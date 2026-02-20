// SPDX-License-Identifier: GPL-2.0
/*
 * CMA Allocation Tracker
 * 
 * Tracks all CMA allocations system-wide using kernel tracepoints.
 * Built on top of the generic memory tracker framework.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/cma.h>
#include <linux/bitmap.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/ktime.h>
#include <mm/cma.h>  /* Internal header for struct cma definition (bitmap, lock, order_per_bit) */
#include "mem_tracker.h"
#include <trace/events/cma.h>
#include <trace/events/mem_tracker.h>

/* Create tracepoint definitions for CMA tracker */
#define CREATE_TRACE_POINTS
#include <trace/events/cma_tracker.h>
#undef CREATE_TRACE_POINTS

/* CMA heap stats: pages_used mode */
#define CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_DISABLED 0
#define CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_FULL 1
#define CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED 2

/*
 * CMA-specific private data structures
 */

/* CMA heap statistics structure */
struct cma_heap_stats {
	unsigned long total_pages;
	struct {
		unsigned long used;
		unsigned long free;
		bool is_valid;
	} pages_usage;
	unsigned long max_chunk_pages;
};

/* Private data for all CMA records (alloc, event, history) 
 * Infrastructure wraps this in mem_priv and manages refcounting */
struct cma_record_priv {
	unsigned long pfn;		/* physical frame number */
	struct page *page;		/* page pointer (virtual address) */
	unsigned long count;	/* number of pages */
	char cma_name[64];		/* e.g. "linux,cma" */
	struct cma_heap_stats heap_stats; /* Heap stats at allocation time */
	struct cma_heap_stats heap_stats_at_free; /* Heap stats at free time (if freed) */
};

/* Global CMA tracker instance */
static struct mem_tracker *cma_tracker;

/*
 * ============================================================================
 * Performance Overhead Measurement (Test Utility)
 * ============================================================================
 */
#ifdef CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT
/* Timing statistics structure */
struct cma_timing_stats {
	u64 total_calls;
	u64 total_time_ns;
	u64 get_heap_stats_time_ns;
	u64 track_time_ns;
	u64 emit_tracepoint_time_ns;
	u64 min_time_ns;
	u64 max_time_ns;
};

/* Timing stats for alloc and free operations */
static struct cma_timing_stats alloc_timing_stats;
static struct cma_timing_stats free_timing_stats;
static DEFINE_SPINLOCK(timing_stats_lock);

#define INIT_PERFORMANCE_TIMER(name) u64 name##_start = 0, name##_end = 0, name##_time = 0
#define START_TIMER(name) do { name##_start = ktime_get_ns(); } while (0)
#define END_TIMER(name) do { name##_end = ktime_get_ns(); } while (0)
#define GET_TIMER_DURATION(name) ((name##_time = name##_end - name##_start))
#else /* if not defined CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT */
#define INIT_PERFORMANCE_TIMER(name) do { } while (0)
#define START_TIMER(name) do { } while (0)
#define END_TIMER(name) do { } while (0)
#define GET_TIMER_DURATION(name) (0)
#endif /* if not defined CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT */

/* CMA heap stats tracking control (default: disabled) */
static bool cma_heap_stats_enabled;
static DEFINE_SPINLOCK(heap_stats_enabled_lock);

#if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED
/* Cached used_pages count per CMA area (updated incrementally to avoid bitmap_weight scan) */
struct cma_used_pages_cache {
	const char *name;
	struct cma *cma;
	unsigned long cached_used_pages;  /* Cached count in pages (not bits) */
	unsigned long cached_total_pages;
	unsigned int cached_order_per_bit;
};
static struct cma_used_pages_cache used_pages_cache[MAX_CMA_AREAS]; /* Support up to MAX_CMA_AREAS CMA areas */
static unsigned int used_pages_cache_count;
static DEFINE_SPINLOCK(used_pages_cache_lock);

/* Forward declarations */
static bool get_cma_used_pages_from_cache(struct cma *cma, unsigned long *used_pages);
static void update_used_pages_cache_entry(struct cma *cma, const char *heap_name, unsigned long used_pages);
static void update_used_pages_cache_alloc(const char *name, unsigned long count);
static void update_used_pages_cache_free(const char *name, unsigned long count);
#endif /* if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED */

/* Allow this regardless, for the dedicated debugfs file */
static void get_cma_heap_stats(const char *heap_name,
	struct cma_heap_stats *stats, bool ignore_cache);

/*
 * ============================================================================
 * Callback Operations - CMA-specific formatting
 * ============================================================================
 */

/* Allocate type-specific private data (infrastructure will wrap and refcount) */
static void *cma_alloc_priv(void)
{
	return kzalloc(sizeof(struct cma_record_priv), GFP_ATOMIC);
}

/* Free type-specific private data (called by infrastructure when refcount=0) */
static void cma_free_priv(void *priv)
{
	kfree(priv);
}

/* Format CMA heap stats for seq_file (takes pre-computed stats) */
/* Lightweight one-line format for detailed views */
static void format_cma_heap_stats_inline(struct seq_file *m,
					const struct cma_heap_stats *stats)
{
	unsigned long usage_percent;
	
	if (!stats)
		return;

	if (!stats->pages_usage.is_valid) {
		seq_printf(m, "heap_stats: total=%lu max_chunk=%lu\n", stats->total_pages, stats->max_chunk_pages);
		return;
	}
	
	usage_percent = stats->total_pages > 0 ?
		(stats->pages_usage.used * 100) / stats->total_pages : 0;
	seq_printf(m, "heap_stats: total=%lu used=%lu free=%lu max_chunk=%lu usage=%lu%%\n",
			stats->total_pages, stats->pages_usage.used, stats->pages_usage.free,
			stats->max_chunk_pages, usage_percent);
}

/* Full multi-line format for summary views */
static void format_cma_heap_stats(struct seq_file *m, const char *heap_name,
				  const struct cma_heap_stats *stats)
{
	if (!heap_name) {
		seq_puts(m, "No CMA heap found\n");
		return;
	}
	
	if (!stats)
		return;

	seq_printf(m, "CMA Heap Statistics: %s\n", heap_name);
	seq_puts(m, "============================================\n");
	seq_printf(m, "Total pages:    %lu (%lu MB, %zu bytes)\n",
		   stats->total_pages, (stats->total_pages << PAGE_SHIFT) / (1024 * 1024), stats->total_pages << PAGE_SHIFT);
	if (stats->pages_usage.is_valid) {
		seq_printf(m, "Used pages:     %lu (%lu MB, %zu bytes)\n",
			stats->pages_usage.used, (stats->pages_usage.used << PAGE_SHIFT) / (1024 * 1024), stats->pages_usage.used << PAGE_SHIFT);
		seq_printf(m, "Free pages:     %lu (%lu MB, %zu bytes)\n",
			stats->pages_usage.free, (stats->pages_usage.free << PAGE_SHIFT) / (1024 * 1024), stats->pages_usage.free << PAGE_SHIFT);
	}
	
	seq_printf(m, "Max chunk:      %lu pages (%lu MB, %zu bytes)\n",
		   stats->max_chunk_pages, (stats->max_chunk_pages << PAGE_SHIFT) / (1024 * 1024), stats->max_chunk_pages << PAGE_SHIFT);
	/* Calculate percentage using integer math (avoid floating point) */
	if (stats->pages_usage.is_valid && stats->total_pages > 0) {
		seq_printf(m, "Usage:          %lu.%02lu%%\n",
				(stats->pages_usage.used * 100) / stats->total_pages,
				((stats->pages_usage.used * 10000) / stats->total_pages) % 100);
	}
}

/* Callback to serialize CMA heap stats to seq_file for a specific CMA area */
static int serialize_cma_heap_stats(struct cma *cma, void *data)
{
	struct seq_file *m = (struct seq_file *)data;
	struct cma_heap_stats stats;
	const char *heap_name;
	
	if (!m)
		return -EINVAL;
	
	heap_name = cma_get_name(cma);

	/* Since this function is indended for user prompt, we ignore the cache.
	* The reason for that, is that we want accurate result here,
	* even if heap_stats_enabled is disabled (cache would be invalid). */
	get_cma_heap_stats(heap_name, &stats, true);
	format_cma_heap_stats(m, heap_name, &stats);
	seq_puts(m, "\n");
	return 0; /* Continue iteration */
}

#ifdef CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT
/* Format timing statistics */
static void format_timing_stats(struct seq_file *m, const char *op_name,
				const struct cma_timing_stats *stats)
{
	u64 avg_time_ns = 0;
	u64 avg_get_heap_stats_ns = 0;
	u64 avg_track_ns = 0;
	u64 avg_emit_ns = 0;

	if (!stats)
		return;

	if (stats->total_calls > 0) {
		avg_time_ns = stats->total_time_ns / stats->total_calls;
		avg_get_heap_stats_ns = stats->get_heap_stats_time_ns / stats->total_calls;
		avg_track_ns = stats->track_time_ns / stats->total_calls;
		avg_emit_ns = stats->emit_tracepoint_time_ns / stats->total_calls;
	}

	seq_printf(m, "\n%s Overhead Statistics:\n", op_name);
	seq_puts(m, "============================================\n");
	seq_printf(m, "Total calls:           %llu\n", stats->total_calls);
	seq_printf(m, "Total overhead:        %llu ns (%llu us, %llu ms)\n",
		   stats->total_time_ns,
		   stats->total_time_ns / 1000,
		   stats->total_time_ns / 1000000);
	seq_printf(m, "Average overhead:      %llu ns (%llu us)\n",
		   avg_time_ns, avg_time_ns / 1000);
	seq_printf(m, "Min overhead:         %llu ns (%llu us)\n",
		   stats->min_time_ns, stats->min_time_ns / 1000);
	seq_printf(m, "Max overhead:         %llu ns (%llu us)\n",
		   stats->max_time_ns, stats->max_time_ns / 1000);
	seq_puts(m, "\nBreakdown by operation:\n");
	seq_printf(m, "  get_heap_stats:     avg %llu ns (%llu us)\n",
		   avg_get_heap_stats_ns, avg_get_heap_stats_ns / 1000);
	seq_printf(m, "  track operation:     avg %llu ns (%llu us)\n",
		   avg_track_ns, avg_track_ns / 1000);
	seq_printf(m, "  emit_tracepoint:     avg %llu ns (%llu us)\n",
		   avg_emit_ns, avg_emit_ns / 1000);
	seq_puts(m, "\n");
}
#endif /* CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT */

/* Format CMA-specific summary */
static void cma_format_summary(struct seq_file *m, struct mem_tracker *tracker)
{
	bool heap_stats_enabled;

	if (!tracker)
		return;

	seq_puts(m, "CMA-specific Information:\n");
	seq_puts(m, "  Tracking allocations via builtin CMA tracepoints\n");

	/* Show heap stats tracking status */
	spin_lock(&heap_stats_enabled_lock);
	heap_stats_enabled = cma_heap_stats_enabled;
	spin_unlock(&heap_stats_enabled_lock);
	seq_printf(m, "  Heap stats tracking: %s\n", heap_stats_enabled ? "enabled" : "disabled");
	seq_puts(m, "\n");

	/* Show stats for all CMA heaps */
	if (heap_stats_enabled) {
		cma_for_each_area(serialize_cma_heap_stats, m);
	}

#ifdef CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT
	{
		unsigned long flags;
		/* Show timing statistics (only when summary is called) */
		spin_lock_irqsave(&timing_stats_lock, flags);
		if (alloc_timing_stats.total_calls > 0)
			format_timing_stats(m, "ALLOC", &alloc_timing_stats);
		if (free_timing_stats.total_calls > 0)
			format_timing_stats(m, "FREE", &free_timing_stats);
		spin_unlock_irqrestore(&timing_stats_lock, flags);
	}
#endif /* CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT */
}

/* Debugfs file for heap stats */
static int heap_stats_seq_show(struct seq_file *m, void *v)
{
	/* Show stats for all CMA heaps */
	cma_for_each_area(serialize_cma_heap_stats, m);

	return 0;
}

static int heap_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, heap_stats_seq_show, inode->i_private);
}

static const struct file_operations heap_stats_fops = {
	.owner = THIS_MODULE,
	.open = heap_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* Debugfs file for enabling/disabling heap stats tracking */
static ssize_t heap_stats_enabled_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char buf[32];
	size_t len;
	bool enabled;

	spin_lock(&heap_stats_enabled_lock);
	enabled = cma_heap_stats_enabled;
	spin_unlock(&heap_stats_enabled_lock);

	len = snprintf(buf, sizeof(buf), "%d\n", enabled ? 1 : 0);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t heap_stats_enabled_write(struct file *file, const char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	char buf[32];
	unsigned long val;
	int ret;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = '\0';
	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	spin_lock(&heap_stats_enabled_lock);
	cma_heap_stats_enabled = (val != 0);
	spin_unlock(&heap_stats_enabled_lock);

	/* If heap_stats were turned on, we need to invalidate the cache for each CMA area. */
	if (cma_heap_stats_enabled) {
		struct cma_heap_stats stats;
		int i;
		for (i = 0; i < cma_area_count; i++) {
			/* We use ignore_cache=true to compute the stats from the bitmap. */
			get_cma_heap_stats(cma_areas[i].name, &stats, true);
		}
	}

	return count;
}

static const struct file_operations heap_stats_enabled_fops = {
	.owner = THIS_MODULE,
	.read = heap_stats_enabled_read,
	.write = heap_stats_enabled_write,
	.llseek = default_llseek,
};

/* Format active allocation record - brief */
static void cma_format_active_brief(struct seq_file *m, struct mem_alloc_record *rec)
{
	struct cma_record_priv *priv;
	
	if (!rec || !rec->priv || !rec->priv->data)
		return;
	
	priv = MEM_TRACKER_PRIV(rec, struct cma_record_priv);
	
	seq_printf(m,
		   "uid=%llu pfn=0x%lx cma=%s size=%zu bytes process=%s[%d] timestamp=%llu\n",
		   rec->allocation_uid, priv->pfn, priv->cma_name, rec->size,
		   rec->comm, rec->tgid, rec->timestamp);
}

/* Format active allocation record - full details */
static void cma_format_active_detailed(struct seq_file *m, struct mem_alloc_record *rec)
{
	struct cma_record_priv *priv;
	unsigned int i;
	
	if (!rec || !rec->priv || !rec->priv->data)
		return;
	
	priv = MEM_TRACKER_PRIV(rec, struct cma_record_priv);
	
	seq_printf(m,
		   "---\n"
		   "uid=%llu cma=%s pfn=0x%lx phys=0x%llx page=%px count=%lu size=%zu bytes\n"
		   "process=%s[%d] thread=%d timestamp=%llu\n",
		   rec->allocation_uid, priv->cma_name, priv->pfn,
		   (unsigned long long)(priv->pfn << PAGE_SHIFT),
		   priv->page, priv->count, rec->size,
		   rec->comm, rec->tgid, rec->tid, rec->timestamp);
	
	/* Show heap stats at allocation time (inline) only if available */
	if (priv->heap_stats.total_pages > 0) {
		format_cma_heap_stats_inline(m, &priv->heap_stats);
	}
	
	if (rec->nr_stack_entries > 0) {
		seq_puts(m, "\nstack:\n");
		for (i = 0; i < rec->nr_stack_entries; i++)
			seq_printf(m, "  %pS\n", (void *)rec->stack_entries[i]);
	}
}

/* Format event - brief */
static void cma_format_event_brief(struct seq_file *m, struct mem_event *event)
{
	struct cma_record_priv *priv;
	
	/* Safety check for NULL priv (shouldn't happen, but be defensive) */
	if (!event->priv) {
		seq_printf(m,
			   "type=%s uid=%llu address=0x%lx size=%zu bytes process=%s[%d] timestamp=%llu [no CMA data]\n",
			   event->type == MEM_EVENT_ALLOC ? "ALLOC" : "FREE",
			   event->allocation_uid, event->address, event->size,
			   event->comm, event->tgid, event->timestamp);
		return;
	}
	
	priv = MEM_TRACKER_PRIV(event, struct cma_record_priv);
	
	seq_printf(m,
		   "type=%s uid=%llu pfn=0x%lx cma=%s size=%zu bytes process=%s[%d] timestamp=%llu\n",
		   event->type == MEM_EVENT_ALLOC ? "ALLOC" : "FREE",
		   event->allocation_uid, priv->pfn, priv->cma_name, event->size,
		   event->comm, event->tgid, event->timestamp);
}

/* Format event - full details */
static void cma_format_event_detailed(struct seq_file *m, struct mem_event *event)
{
	struct cma_record_priv *priv;
	unsigned int i;
	
	seq_puts(m, "---\n");
	seq_printf(m, "type=%s\n", event->type == MEM_EVENT_ALLOC ? "ALLOC" : "FREE");
	
	/* Safety check for NULL priv (shouldn't happen, but be defensive) */
	if (!event->priv) {
		seq_printf(m,
			   "uid=%llu address=0x%lx size=%zu bytes [no CMA data]\n"
			   "process=%s[%d] thread=%d timestamp=%llu\n",
			   event->allocation_uid, event->address, event->size,
			   event->comm, event->tgid, event->tid, event->timestamp);
		
		if (event->nr_stack_entries > 0) {
			seq_puts(m, "stack:\n");
			for (i = 0; i < event->nr_stack_entries; i++)
				seq_printf(m, "  %pS\n", (void *)event->stack_entries[i]);
		}
		return;
	}
	
	priv = MEM_TRACKER_PRIV(event, struct cma_record_priv);
	
	seq_printf(m,
		   "uid=%llu cma=%s pfn=0x%lx phys=0x%llx page=%px count=%lu size=%zu bytes\n"
		   "process=%s[%d] thread=%d timestamp=%llu\n",
		   event->allocation_uid, priv->cma_name, priv->pfn,
		   (unsigned long long)(priv->pfn << PAGE_SHIFT),
		   priv->page, priv->count, event->size,
		   event->comm, event->tgid, event->tid, event->timestamp);
	
	/* Show heap stats at event time (inline) only if available */
	/* For FREE events, show free-time stats if available, otherwise alloc-time stats */
	if (event->type == MEM_EVENT_FREE && priv->heap_stats_at_free.total_pages > 0) {
		format_cma_heap_stats_inline(m, &priv->heap_stats_at_free);
	} else if (priv->heap_stats.total_pages > 0) {
		format_cma_heap_stats_inline(m, &priv->heap_stats);
	}
	
	if (event->nr_stack_entries > 0) {
		seq_puts(m, "\nstack:\n");
		for (i = 0; i < event->nr_stack_entries; i++)
			seq_printf(m, "  %pS\n", (void *)event->stack_entries[i]);
	}
}

/* Format history record - brief */
static void cma_format_history_brief(struct seq_file *m, struct mem_hist_record *hist)
{
	struct cma_record_priv *priv;
	
	if (!hist || !hist->priv || !hist->priv->data)
		return;
	
	priv = MEM_TRACKER_PRIV(hist, struct cma_record_priv);
	
	if (hist->freed) {
		seq_printf(m,
			   "status=FREED uid=%llu pfn=0x%lx cma=%s size=%zu bytes alloc=%s[%d]@%llu free=%s[%d]@%llu duration=%llu ns\n",
			   hist->allocation_uid, priv->pfn, priv->cma_name, hist->size,
			   hist->alloc_comm, hist->alloc_tgid, hist->alloc_timestamp,
			   hist->free_comm, hist->free_tgid, hist->free_timestamp,
			   hist->free_timestamp - hist->alloc_timestamp);
	} else {
		seq_printf(m,
			   "status=ACTIVE uid=%llu pfn=0x%lx cma=%s size=%zu bytes alloc=%s[%d]@%llu\n",
			   hist->allocation_uid, priv->pfn, priv->cma_name, hist->size,
			   hist->alloc_comm, hist->alloc_tgid, hist->alloc_timestamp);
	}
}

/* Format history record - full details */
static void cma_format_history_detailed(struct seq_file *m, struct mem_hist_record *hist)
{
	struct cma_record_priv *priv;
	unsigned int i;
	
	if (!hist || !hist->priv || !hist->priv->data)
		return;
	
	priv = MEM_TRACKER_PRIV(hist, struct cma_record_priv);
	
	seq_printf(m,
		   "---\n"
		   "uid=%llu cma=%s pfn=0x%lx phys=0x%llx page=%px count=%lu size=%zu bytes\n"
		   "alloc: process=%s[%d] thread=%d timestamp=%llu\n",
		   hist->allocation_uid, priv->cma_name, priv->pfn,
		   (unsigned long long)(priv->pfn << PAGE_SHIFT),
		   priv->page, priv->count, hist->size,
		   hist->alloc_comm, hist->alloc_tgid, hist->alloc_tid,
		   hist->alloc_timestamp);
	
	/* Show heap stats at allocation time only if available */
	if (priv->heap_stats.total_pages > 0) {
		format_cma_heap_stats_inline(m, &priv->heap_stats);
	}
	
	if (hist->nr_alloc_stack > 0) {
		seq_puts(m, "alloc_stack:\n");
		for (i = 0; i < hist->nr_alloc_stack; i++)
			seq_printf(m, "  %pS\n", (void *)hist->alloc_stack[i]);
	}
	
	if (hist->freed) {
		seq_printf(m,
			   "\nfree: process=%s[%d] thread=%d timestamp=%llu duration=%llu ns\n",
			   hist->free_comm, hist->free_tgid, hist->free_tid,
			   hist->free_timestamp,
			   hist->free_timestamp - hist->alloc_timestamp);

		/* Show heap stats at free time only if available */
		if (priv->heap_stats_at_free.total_pages > 0) {
			format_cma_heap_stats_inline(m, &priv->heap_stats_at_free);
		}
		
		if (hist->nr_free_stack > 0) {
			seq_puts(m, "free_stack:\n");
			for (i = 0; i < hist->nr_free_stack; i++)
				seq_printf(m, "  %pS\n", (void *)hist->free_stack[i]);
		}
	} else {
		seq_puts(m, "status: still allocated\n");
	}
}

/* Get display name from private data for tracepoint */
static const char *cma_get_display_name(void *priv_data)
{
	struct cma_record_priv *priv;
	
	if (!priv_data)
		return NULL;
	
	priv = (struct cma_record_priv *)priv_data;
	return priv->cma_name;
}

/*
 * ============================================================================
 * Tracepoint Probes
 * ============================================================================
 */

/* Fill CMA-specific data (helper to avoid duplication) */
static inline void fill_cma_record_priv(struct cma_record_priv *priv,
					  const char *name,
					  unsigned long pfn,
					  const struct page *page,
					  unsigned long count,
					  const struct cma_heap_stats *heap_stats)
{
	priv->pfn = pfn;
	priv->page = (struct page *)page;
	priv->count = count;
	strncpy(priv->cma_name, name, sizeof(priv->cma_name) - 1);
	priv->cma_name[sizeof(priv->cma_name) - 1] = '\0';
	if (heap_stats)
		priv->heap_stats = *heap_stats;
}

/* Helper struct for cma_for_each_area callback */
struct cma_find_by_name_data {
	const char *heap_name;
	struct cma *found;
};

/* Callback to find CMA area by name */
static int find_cma_by_name(struct cma *cma, void *data)
{
	struct cma_find_by_name_data *find_data = data;
	
	if (!find_data || !find_data->heap_name)
		return -EINVAL;
	
	if (strcmp(cma_get_name(cma), find_data->heap_name) == 0) {
		find_data->found = cma;
		return 1; /* Stop iteration */
	}
	return 0; /* Continue */
}

#if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED
/* Get cached used_pages count for a CMA area, or return false if cache miss/invalid */
/* This avoids calling bitmap_weight() which scans the entire bitmap (expensive for large bitmaps) */
static bool get_cma_used_pages_from_cache(struct cma *cma, unsigned long *used_pages)
{
	unsigned int i;
	bool cache_valid = false;
	
	if (!cma || !used_pages)
		return false;
	
	/* Check cache (without holding cma->lock to avoid deadlock) */
	spin_lock(&used_pages_cache_lock);
	for (i = 0; i < used_pages_cache_count; i++) {
		if (used_pages_cache[i].cma == cma &&
			used_pages_cache[i].cached_total_pages == cma->count &&
			used_pages_cache[i].cached_order_per_bit == cma->order_per_bit) {
			*used_pages = used_pages_cache[i].cached_used_pages;
			cache_valid = true;
			break;
		}
	}
	spin_unlock(&used_pages_cache_lock);
	
	return cache_valid;
}

/* Update or create cache entry for a CMA area */
static void update_used_pages_cache_entry(struct cma *cma, const char *heap_name, unsigned long used_pages)
{
	unsigned int i;
	
	if (!cma)
		return;
	
	spin_lock(&used_pages_cache_lock);
	/* Find existing entry and update it */
	for (i = 0; i < used_pages_cache_count; i++) {
		if (used_pages_cache[i].cma == cma) {
			used_pages_cache[i].cached_used_pages = used_pages;
			used_pages_cache[i].cached_total_pages = cma->count;
			used_pages_cache[i].cached_order_per_bit = cma->order_per_bit;
			break;
		}
	}

	/* If the cache entry is not found, create a new entry */
	if (i == used_pages_cache_count && i < ARRAY_SIZE(used_pages_cache)) {
		used_pages_cache[i].name = heap_name;
		used_pages_cache[i].cma = cma;
		used_pages_cache[i].cached_used_pages = used_pages;
		used_pages_cache[i].cached_total_pages = cma->count;
		used_pages_cache[i].cached_order_per_bit = cma->order_per_bit;
		used_pages_cache_count++;
	}
	spin_unlock(&used_pages_cache_lock);
}
#endif /* if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED */

static unsigned long get_cma_used_pages_from_bitmap(
		struct cma *cma, unsigned long bitmap_maxno)
{
	unsigned long used_pages = bitmap_weight(cma->bitmap, (int)bitmap_maxno);
	used_pages <<= cma->order_per_bit;

	return used_pages;
}

/* Get CMA heap statistics (duplicates logic from cma_debug.c) */
static void get_cma_heap_stats(const char *heap_name,
	struct cma_heap_stats *stats, bool ignore_cache)
{
	struct cma *cma = NULL;
	unsigned long bitmap_maxno;
	unsigned long start, end;
	struct cma_find_by_name_data find_data = {
		.heap_name = heap_name,
		.found = NULL
	};

	/* Find CMA area by name using exported API */
	cma_for_each_area(find_cma_by_name, &find_data);
	cma = find_data.found;

	if (!cma) {
		stats->total_pages = stats->pages_usage.used = stats->pages_usage.free = stats->max_chunk_pages = 0;
		return;
	}

	stats->total_pages = cma->count;
	bitmap_maxno = cma_bitmap_maxno(cma);

	/* Calculate used pages using cached count (updated incrementally) */
	spin_lock_irq(&cma->lock);

#if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_DISABLED
	stats->pages_usage.used = 0;
    stats->pages_usage.is_valid = false;
#elif CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_FULL
	stats->pages_usage.used = get_cma_used_pages_from_bitmap(cma, bitmap_maxno);
	stats->pages_usage.is_valid = true;
#else /* if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED */
	/* If cache is not ignored, use it.
	 * If it's either ignored or invalid, recalculate using the bitmap */
	if (ignore_cache || !get_cma_used_pages_from_cache(cma, &stats->pages_usage.used)) {
		stats->pages_usage.used = get_cma_used_pages_from_bitmap(cma, bitmap_maxno);
		update_used_pages_cache_entry(cma, heap_name, stats->pages_usage.used);
	}
	stats->pages_usage.is_valid = true;
#endif /* if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED */
	
	stats->pages_usage.free = stats->total_pages - stats->pages_usage.used;

	/* Calculate max chunk (same logic as cma_debug.c:cma_maxchunk_get) */
	stats->max_chunk_pages = 0;
	for (end = 0; end < bitmap_maxno; ) {
		unsigned long chunk_size;
		
		start = find_next_zero_bit(cma->bitmap, bitmap_maxno, end);
		if (start >= bitmap_maxno)
			break;
		
		end = find_next_bit(cma->bitmap, bitmap_maxno, start);
		if (end > bitmap_maxno)
			end = bitmap_maxno;
		
		chunk_size = (end - start) << cma->order_per_bit;
		stats->max_chunk_pages = max(chunk_size, stats->max_chunk_pages);
		
		if (end == bitmap_maxno)
			break;
	}
	spin_unlock_irq(&cma->lock);
}

/* Get CMA heap statistics and emit tracepoint */
static void emit_cma_heap_stats(const char *heap_name, const struct cma_heap_stats *stats)
{
	/* Emit tracepoint */
	trace_cma_heap_stats(heap_name, stats->total_pages, stats->pages_usage.used,
				stats->pages_usage.free, stats->max_chunk_pages);
}

#if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED
/* Update cached used_pages count on allocation */
static void update_used_pages_cache_alloc(const char *name, unsigned long count)
{
	unsigned int i;
	
	spin_lock(&used_pages_cache_lock);
	for (i = 0; i < used_pages_cache_count; i++) {
		if (used_pages_cache[i].name && strcmp(used_pages_cache[i].name, name) == 0) {
			/* Increment cached count by the number of pages allocated */
			used_pages_cache[i].cached_used_pages += count;
			break;
		}
	}
	spin_unlock(&used_pages_cache_lock);
}

/* Update cached used_pages count on free */
static void update_used_pages_cache_free(const char *name, unsigned long count)
{
	unsigned int i;
	
	spin_lock(&used_pages_cache_lock);
	for (i = 0; i < used_pages_cache_count; i++) {
		if (used_pages_cache[i].name && strcmp(used_pages_cache[i].name, name) == 0) {
			/* Decrement cached count */
			if (used_pages_cache[i].cached_used_pages >= count)
				used_pages_cache[i].cached_used_pages -= count;
			else
				used_pages_cache[i].cached_used_pages = 0; /* Underflow protection */
			break;
		}
	}
	spin_unlock(&used_pages_cache_lock);
}
#endif /* if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED */

/* Update free-time heap stats in the allocation record's priv */
static void update_free_time_heap_stats(struct mem_tracker *tracker,
					unsigned long address,
					const struct cma_heap_stats *heap_stats)
{
	struct mem_alloc_record *rec;
	unsigned long flags;
	struct cma_record_priv *priv;
	
	if (!tracker || !heap_stats)
		return;
	
	/* Find the allocation record using O(log n) RB-tree lookup and update its priv */
	spin_lock_irqsave(&tracker->alloc_lock, flags);
	rec = mem_tracker_find_by_address(tracker, address);
	if (rec && rec->priv && rec->priv->data) {
		priv = (struct cma_record_priv *)rec->priv->data;
		priv->heap_stats_at_free = *heap_stats;
	}
	spin_unlock_irqrestore(&tracker->alloc_lock, flags);
}

/* Track a CMA allocation */
static void track_cma_alloc(const char *name, unsigned long pfn,
				const struct page *page, unsigned long count,
				const struct cma_heap_stats *heap_stats)
{
	struct cma_record_priv priv = {0};
	
	/* heap_stats can be NULL if heap stats tracking is disabled */
	fill_cma_record_priv(&priv, name, pfn, page, count, heap_stats);
	mem_tracker_alloc(cma_tracker, pfn << PAGE_SHIFT, count << PAGE_SHIFT, &priv);
}

/* Track a CMA free */
static void track_cma_free(const char *name, unsigned long pfn,
				const struct page *page, unsigned long count,
				const struct cma_heap_stats *heap_stats)
{
	struct cma_record_priv priv = {0};
	
	/* heap_stats can be NULL if heap stats tracking is disabled */
	fill_cma_record_priv(&priv, name, pfn, page, count, heap_stats);
	
	/* Update free-time stats in the allocation record before freeing (only if enabled) */
	if (heap_stats)
		update_free_time_heap_stats(cma_tracker, pfn << PAGE_SHIFT, heap_stats);

	mem_tracker_free(cma_tracker, pfn << PAGE_SHIFT, count << PAGE_SHIFT, &priv);
}

/* Tracepoint probe for cma_alloc_finish */
static void probe_cma_alloc_finish(void *data, const char *name,
					unsigned long pfn, const struct page *page,
					unsigned long count, unsigned int align)
{
	struct cma_heap_stats stats;
	struct cma_heap_stats *stats_ptr = NULL;
	bool heap_stats_enabled;

	INIT_PERFORMANCE_TIMER(total);
	INIT_PERFORMANCE_TIMER(get_stats);
	INIT_PERFORMANCE_TIMER(main_track);
	INIT_PERFORMANCE_TIMER(emit_stats); 
	
	if (!cma_tracker || !mem_tracker_is_enabled())
		return;

	START_TIMER(total);

    /* First make sure the allocation succeeded */
    if (page == NULL) {
        pr_err("CMA allocation failed for %s with pfn %lu, count %lu, align %u\n", name, pfn, count, align);
        return;
    }
	
	spin_lock(&heap_stats_enabled_lock);
	heap_stats_enabled = cma_heap_stats_enabled;
	spin_unlock(&heap_stats_enabled_lock);

	/* Get heap stats only if enabled */
	if (heap_stats_enabled) {
		START_TIMER(get_stats);
#if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED
		/* Update cache BEFORE getting stats so we capture post-alloc state */
		update_used_pages_cache_alloc(name, count);
#endif /* if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED */
		get_cma_heap_stats(name, &stats, false);
		END_TIMER(get_stats);
		stats_ptr = &stats;
	}
	
	START_TIMER(main_track);
	track_cma_alloc(name, pfn, page, count, stats_ptr);
	END_TIMER(main_track);
	
	/* Emit heap stats after allocation only if enabled */
	if (heap_stats_enabled) {
		START_TIMER(emit_stats);
		emit_cma_heap_stats(name, &stats);
		END_TIMER(emit_stats);
	}
	
	END_TIMER(total);
	
#ifdef CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT
	{
		unsigned long flags;
		/* Update timing statistics */
		spin_lock_irqsave(&timing_stats_lock, flags);
		alloc_timing_stats.total_calls++;
		alloc_timing_stats.total_time_ns += GET_TIMER_DURATION(total);
		alloc_timing_stats.get_heap_stats_time_ns += GET_TIMER_DURATION(get_stats);
		alloc_timing_stats.track_time_ns += GET_TIMER_DURATION(main_track);
		alloc_timing_stats.emit_tracepoint_time_ns += GET_TIMER_DURATION(emit_stats);
		if (GET_TIMER_DURATION(total) < alloc_timing_stats.min_time_ns || alloc_timing_stats.total_calls == 1)
			alloc_timing_stats.min_time_ns = GET_TIMER_DURATION(total);
		if (GET_TIMER_DURATION(total) > alloc_timing_stats.max_time_ns)
			alloc_timing_stats.max_time_ns = GET_TIMER_DURATION(total);
		spin_unlock_irqrestore(&timing_stats_lock, flags);
	}
#endif /* CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT */
}

/* Tracepoint probe for cma_release */
static void probe_cma_release(void *data, const char *name,
					unsigned long pfn, const struct page *page,
					unsigned long count)
{
	struct cma_heap_stats stats;
	struct cma_heap_stats *stats_ptr = NULL;
	bool heap_stats_enabled;

	INIT_PERFORMANCE_TIMER(total);
	INIT_PERFORMANCE_TIMER(get_stats);
	INIT_PERFORMANCE_TIMER(main_track);
	INIT_PERFORMANCE_TIMER(emit_stats);
	
	if (!cma_tracker || !mem_tracker_is_enabled())
		return;
	
	START_TIMER(total);	

	spin_lock(&heap_stats_enabled_lock);
	heap_stats_enabled = cma_heap_stats_enabled;
	spin_unlock(&heap_stats_enabled_lock);
	
	/* Get heap stats only if enabled */
	if (heap_stats_enabled) {
		START_TIMER(get_stats);
#if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED
		/* Update cache BEFORE getting stats so we capture post-free state */
		update_used_pages_cache_free(name, count);
#endif /* if CONFIG_CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE == CMA_TRACKER_HEAP_STATS_USED_PAGES_MODE_CACHED */
		get_cma_heap_stats(name, &stats, false);
		END_TIMER(get_stats);
		stats_ptr = &stats;
	}

	START_TIMER(main_track);
	track_cma_free(name, pfn, page, count, stats_ptr);
	END_TIMER(main_track);
	
	/* Emit heap stats after free only if enabled */
	if (heap_stats_enabled) {
		START_TIMER(emit_stats);
		emit_cma_heap_stats(name, &stats);
		END_TIMER(emit_stats);
	}

	END_TIMER(total);
	
#ifdef CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT
	{
		unsigned long flags;
		/* Update timing statistics */
		spin_lock_irqsave(&timing_stats_lock, flags);
		free_timing_stats.total_calls++;
		free_timing_stats.total_time_ns += GET_TIMER_DURATION(total);
		free_timing_stats.get_heap_stats_time_ns += GET_TIMER_DURATION(get_stats);
		free_timing_stats.track_time_ns += GET_TIMER_DURATION(main_track);
		free_timing_stats.emit_tracepoint_time_ns += GET_TIMER_DURATION(emit_stats);
		if (GET_TIMER_DURATION(total) < free_timing_stats.min_time_ns || free_timing_stats.total_calls == 1)
			free_timing_stats.min_time_ns = GET_TIMER_DURATION(total);
		if (GET_TIMER_DURATION(total) > free_timing_stats.max_time_ns)
			free_timing_stats.max_time_ns = GET_TIMER_DURATION(total);
		spin_unlock_irqrestore(&timing_stats_lock, flags);
	}
#endif /* CONFIG_CMA_TRACKER_PERFORMANCE_MEASUREMENT */
}

/*
 * ============================================================================
 * CMA tracker configuration
 * ============================================================================
 */

 static struct mem_tracker_ops cma_ops = {
	.priv_size = sizeof(struct cma_record_priv),
	.alloc_priv = cma_alloc_priv,
	.free_priv = cma_free_priv,
	.format_summary = cma_format_summary,
	.format_active_brief = cma_format_active_brief,
	.format_active_detailed = cma_format_active_detailed,
	.format_event_brief = cma_format_event_brief,
	.format_event_detailed = cma_format_event_detailed,
	.format_history_brief = cma_format_history_brief,
	.format_history_detailed = cma_format_history_detailed,
	.get_display_name = cma_get_display_name,
};

static struct mem_tracker_config cma_config = {
	.name = "cma",
	.buffer_size = MEM_TRACKER_DEFAULT_BUFFER_SIZE,
	.stack_capture = MEM_TRACKER_STACK_DISABLED,  /* Default: disabled */
	.ops = &cma_ops,
};

/*
 * ============================================================================
 * Module Init/Exit
 * ============================================================================
 */

static int __init cma_tracker_init(void)
{
	int ret;
	
	/* Register with generic tracker framework */
	cma_tracker = mem_tracker_register(&cma_config);
	if (!IS_ERR(cma_tracker) && cma_tracker->debugfs_dir) {
		/* Add custom heap_stats debugfs file */
		debugfs_create_file("heap_stats", 0444, cma_tracker->debugfs_dir,
				   cma_tracker, &heap_stats_fops);
		/* Add heap_stats_enabled control file */
		debugfs_create_file("heap_stats_enabled", 0644, cma_tracker->debugfs_dir,
				   NULL, &heap_stats_enabled_fops);
	}
	if (IS_ERR(cma_tracker)) {
		pr_err("Failed to register CMA tracker: %ld\n", PTR_ERR(cma_tracker));
		return PTR_ERR(cma_tracker);
	}
	
	/* Register tracepoint probes */
	ret = register_trace_cma_alloc_finish(probe_cma_alloc_finish, NULL);
	if (ret) {
		pr_err("Failed to register cma_alloc_finish probe: %d\n", ret);
		goto err_unregister_tracker;
	}
	
	ret = register_trace_cma_release(probe_cma_release, NULL);
	if (ret) {
		pr_err("Failed to register cma_release probe: %d\n", ret);
		goto err_unregister_alloc;
	}

	pr_info("CMA tracker initialized (via generic framework)\n");
	return 0;

err_unregister_alloc:
	unregister_trace_cma_alloc_finish(probe_cma_alloc_finish, NULL);
err_unregister_tracker:
	mem_tracker_unregister(cma_tracker);
	cma_tracker = NULL;
	return ret;
}

static void __exit cma_tracker_exit(void)
{
	/* Unregister tracepoint probes */
	unregister_trace_cma_release(probe_cma_release, NULL);
	unregister_trace_cma_alloc_finish(probe_cma_alloc_finish, NULL);
	
	/* Unregister from generic framework */
	mem_tracker_unregister(cma_tracker);
	cma_tracker = NULL;
	
	pr_info("CMA tracker removed\n");
}

module_init(cma_tracker_init);
module_exit(cma_tracker_exit);

MODULE_AUTHOR("Hailo");
MODULE_DESCRIPTION("CMA Allocation Tracker (generic framework)");
MODULE_LICENSE("GPL v2");
