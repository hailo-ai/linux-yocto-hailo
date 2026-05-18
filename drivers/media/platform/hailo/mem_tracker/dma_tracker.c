// SPDX-License-Identifier: GPL-2.0
/*
 * DMA Allocation Tracker
 *
 * Tracks DMA coherent and page allocations system-wide using kernel tracepoints.
 * Built on top of the generic memory tracker framework.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/dma-direction.h>
#include "mem_tracker.h"
#include <trace/events/dma.h>

/*
 * DMA-specific private data structures
 */

/* DMA allocation type - distinguishes coherent from page allocations */
enum dma_alloc_type {
	DMA_ALLOC_COHERENT,	/* from dma_alloc_attrs / dma_free_attrs */
	DMA_ALLOC_PAGES,	/* from dma_alloc_pages / dma_free_pages */
};

/* Private data for all DMA records (alloc, event, history)
 * Infrastructure wraps this in mem_priv and manages refcounting */
struct dma_record_priv {
	enum dma_alloc_type alloc_type;
	char dev_name[64];		/* device name from dev_name(dev) */
	void *virt_addr;		/* virtual address */
	dma_addr_t dma_addr;		/* DMA bus address */
	phys_addr_t phys_addr;		/* physical address */
	enum dma_data_direction dir;	/* DMA direction */
	gfp_t gfp_flags;		/* GFP flags (only available on alloc) */
	unsigned long attrs;		/* DMA attributes */
};

/* Global DMA tracker instance */
static struct mem_tracker *dma_tracker;

/*
 * ============================================================================
 * Helper Functions
 * ============================================================================
 */

static const char *dma_alloc_type_str(enum dma_alloc_type type)
{
	switch (type) {
	case DMA_ALLOC_COHERENT:
		return "coherent";
	case DMA_ALLOC_PAGES:
		return "pages";
	default:
		return "unknown";
	}
}

static const char *dma_dir_str(enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_BIDIRECTIONAL:
		return "BIDIR";
	case DMA_TO_DEVICE:
		return "TO_DEV";
	case DMA_FROM_DEVICE:
		return "FROM_DEV";
	case DMA_NONE:
		return "NONE";
	default:
		return "?";
	}
}

static inline void fill_dma_record_priv(struct dma_record_priv *priv,
					enum dma_alloc_type alloc_type,
					struct device *dev,
					void *virt_addr,
					dma_addr_t dma_addr,
					enum dma_data_direction dir,
					gfp_t gfp_flags,
					unsigned long attrs)
{
	priv->alloc_type = alloc_type;
	strscpy(priv->dev_name, dev_name(dev), sizeof(priv->dev_name));
	priv->virt_addr = virt_addr;
	priv->dma_addr = dma_addr;
	priv->phys_addr = dma_to_phys(dev, dma_addr);
	priv->dir = dir;
	priv->gfp_flags = gfp_flags;
	priv->attrs = attrs;
}

/*
 * ============================================================================
 * Callback Operations - DMA-specific formatting
 * ============================================================================
 */

/* Allocate type-specific private data (infrastructure will wrap and refcount) */
static void *dma_alloc_priv(void)
{
	return kzalloc(sizeof(struct dma_record_priv), GFP_ATOMIC);
}

/* Free type-specific private data (called by infrastructure when refcount=0) */
static void dma_free_priv(void *priv)
{
	kfree(priv);
}

/* Format DMA-specific summary */
static void dma_format_summary(struct seq_file *m, struct mem_tracker *tracker)
{
	if (!tracker)
		return;

	seq_puts(m, "DMA-specific Information:\n");
	seq_puts(m, "  Tracking: dma_alloc/dma_free (coherent), dma_alloc_pages/dma_free_pages\n");
	seq_puts(m, "  Address key: dma_addr\n\n");
}

/* Format active allocation record - brief */
static void dma_format_active_brief(struct seq_file *m, struct mem_alloc_record *rec)
{
	struct dma_record_priv *priv;

	if (!rec || !rec->priv || !rec->priv->data)
		return;

	priv = MEM_TRACKER_PRIV(rec, struct dma_record_priv);

	seq_printf(m,
		   "uid=%llu type=%s dev=%s dma_addr=0x%llx phys_addr=0x%llx size=%zu dir=%s process=%s[%d] timestamp=%llu\n",
		   rec->allocation_uid, dma_alloc_type_str(priv->alloc_type),
		   priv->dev_name, (u64)priv->dma_addr, (u64)priv->phys_addr,
		   rec->size, dma_dir_str(priv->dir), rec->comm, rec->tgid,
		   rec->timestamp);
}

/* Format active allocation record - full details */
static void dma_format_active_detailed(struct seq_file *m, struct mem_alloc_record *rec)
{
	struct dma_record_priv *priv;
	unsigned int i;

	if (!rec || !rec->priv || !rec->priv->data)
		return;

	priv = MEM_TRACKER_PRIV(rec, struct dma_record_priv);

	seq_printf(m,
		   "---\n"
		   "uid=%llu type=%s dev=%s\n"
		   "dma_addr=0x%llx phys_addr=0x%llx virt_addr=%px size=%zu\n"
		   "dir=%s gfp_flags=0x%x attrs=0x%lx\n"
		   "process=%s[%d] thread=%d timestamp=%llu\n",
		   rec->allocation_uid, dma_alloc_type_str(priv->alloc_type),
		   priv->dev_name, (u64)priv->dma_addr, (u64)priv->phys_addr,
		   priv->virt_addr, rec->size, dma_dir_str(priv->dir),
		   (__force unsigned int)priv->gfp_flags, priv->attrs,
		   rec->comm, rec->tgid, rec->tid, rec->timestamp);

	if (rec->nr_stack_entries > 0) {
		seq_puts(m, "\nstack:\n");
		for (i = 0; i < rec->nr_stack_entries; i++)
			seq_printf(m, "  %pS\n", (void *)rec->stack_entries[i]);
	}
}

/* Format event - brief */
static void dma_format_event_brief(struct seq_file *m, struct mem_event *event)
{
	struct dma_record_priv *priv;

	if (!event->priv) {
		seq_printf(m,
			   "type=%s uid=%llu address=0x%lx size=%zu bytes process=%s[%d] timestamp=%llu [no DMA data]\n",
			   event->type == MEM_EVENT_ALLOC ? "ALLOC" : "FREE",
			   event->allocation_uid, event->address, event->size,
			   event->comm, event->tgid, event->timestamp);
		return;
	}

	priv = MEM_TRACKER_PRIV(event, struct dma_record_priv);

	seq_printf(m,
		   "type=%s uid=%llu dma_type=%s dev=%s dma_addr=0x%llx phys_addr=0x%llx size=%zu dir=%s process=%s[%d] timestamp=%llu\n",
		   event->type == MEM_EVENT_ALLOC ? "ALLOC" : "FREE",
		   event->allocation_uid, dma_alloc_type_str(priv->alloc_type),
		   priv->dev_name, (u64)priv->dma_addr, (u64)priv->phys_addr,
		   event->size, dma_dir_str(priv->dir), event->comm,
		   event->tgid, event->timestamp);
}

/* Format event - full details */
static void dma_format_event_detailed(struct seq_file *m, struct mem_event *event)
{
	struct dma_record_priv *priv;
	unsigned int i;

	seq_puts(m, "---\n");
	seq_printf(m, "type=%s\n", event->type == MEM_EVENT_ALLOC ? "ALLOC" : "FREE");

	if (!event->priv) {
		seq_printf(m,
			   "uid=%llu address=0x%lx size=%zu bytes [no DMA data]\n"
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

	priv = MEM_TRACKER_PRIV(event, struct dma_record_priv);

	seq_printf(m,
		   "uid=%llu dma_type=%s dev=%s\n"
		   "dma_addr=0x%llx phys_addr=0x%llx virt_addr=%px size=%zu\n"
		   "dir=%s gfp_flags=0x%x attrs=0x%lx\n"
		   "process=%s[%d] thread=%d timestamp=%llu\n",
		   event->allocation_uid, dma_alloc_type_str(priv->alloc_type),
		   priv->dev_name, (u64)priv->dma_addr, (u64)priv->phys_addr,
		   priv->virt_addr, event->size, dma_dir_str(priv->dir),
		   (__force unsigned int)priv->gfp_flags, priv->attrs,
		   event->comm, event->tgid, event->tid, event->timestamp);

	if (event->nr_stack_entries > 0) {
		seq_puts(m, "\nstack:\n");
		for (i = 0; i < event->nr_stack_entries; i++)
			seq_printf(m, "  %pS\n", (void *)event->stack_entries[i]);
	}
}

/* Format history record - brief */
static void dma_format_history_brief(struct seq_file *m, struct mem_hist_record *hist)
{
	struct dma_record_priv *priv;

	if (!hist || !hist->priv || !hist->priv->data)
		return;

	priv = MEM_TRACKER_PRIV(hist, struct dma_record_priv);

	if (hist->freed) {
		seq_printf(m,
			   "status=FREED uid=%llu dma_type=%s dev=%s dma_addr=0x%llx phys_addr=0x%llx size=%zu dir=%s alloc=%s[%d]@%llu free=%s[%d]@%llu duration=%llu ns\n",
			   hist->allocation_uid, dma_alloc_type_str(priv->alloc_type),
			   priv->dev_name, (u64)priv->dma_addr, (u64)priv->phys_addr,
			   hist->size, dma_dir_str(priv->dir),
			   hist->alloc_comm, hist->alloc_tgid, hist->alloc_timestamp,
			   hist->free_comm, hist->free_tgid, hist->free_timestamp,
			   hist->free_timestamp - hist->alloc_timestamp);
	} else {
		seq_printf(m,
			   "status=ACTIVE uid=%llu dma_type=%s dev=%s dma_addr=0x%llx phys_addr=0x%llx size=%zu dir=%s alloc=%s[%d]@%llu\n",
			   hist->allocation_uid, dma_alloc_type_str(priv->alloc_type),
			   priv->dev_name, (u64)priv->dma_addr, (u64)priv->phys_addr,
			   hist->size, dma_dir_str(priv->dir),
			   hist->alloc_comm, hist->alloc_tgid, hist->alloc_timestamp);
	}
}

/* Format history record - full details */
static void dma_format_history_detailed(struct seq_file *m, struct mem_hist_record *hist)
{
	struct dma_record_priv *priv;
	unsigned int i;

	if (!hist || !hist->priv || !hist->priv->data)
		return;

	priv = MEM_TRACKER_PRIV(hist, struct dma_record_priv);

	seq_printf(m,
		   "---\n"
		   "uid=%llu dma_type=%s dev=%s\n"
		   "dma_addr=0x%llx phys_addr=0x%llx virt_addr=%px size=%zu\n"
		   "dir=%s attrs=0x%lx\n"
		   "alloc: process=%s[%d] thread=%d timestamp=%llu\n",
		   hist->allocation_uid, dma_alloc_type_str(priv->alloc_type),
		   priv->dev_name, (u64)priv->dma_addr, (u64)priv->phys_addr,
		   priv->virt_addr, hist->size, dma_dir_str(priv->dir),
		   priv->attrs, hist->alloc_comm, hist->alloc_tgid,
		   hist->alloc_tid, hist->alloc_timestamp);

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
static const char *dma_get_display_name(void *priv_data)
{
	struct dma_record_priv *priv;

	if (!priv_data)
		return NULL;

	priv = (struct dma_record_priv *)priv_data;
	return priv->dev_name;
}

/*
 * ============================================================================
 * Tracepoint Probes
 * ============================================================================
 */

/* Alloc probe (shared by dma_alloc and dma_alloc_pages) */
static void probe_dma_alloc_common(enum dma_alloc_type alloc_type,
				   struct device *dev, void *virt_addr,
				   dma_addr_t dma_addr, size_t size,
				   enum dma_data_direction dir, gfp_t flags,
				   unsigned long attrs)
{
	struct dma_record_priv priv = {0};

	if (!dma_tracker || !mem_tracker_is_enabled(dma_tracker))
		return;

	/* Skip failed allocations */
	if (!virt_addr)
		return;

	fill_dma_record_priv(&priv, alloc_type, dev, virt_addr,
			     dma_addr, dir, flags, attrs);

	mem_tracker_alloc(dma_tracker, (unsigned long)dma_addr, size, &priv);
}

/* Free probe (shared by dma_free and dma_free_pages) */
static void probe_dma_free_common(enum dma_alloc_type alloc_type,
				  struct device *dev, void *virt_addr,
				  dma_addr_t dma_addr, size_t size,
				  enum dma_data_direction dir,
				  unsigned long attrs)
{
	struct dma_record_priv priv = {0};

	if (!dma_tracker || !mem_tracker_is_enabled(dma_tracker))
		return;

	fill_dma_record_priv(&priv, alloc_type, dev, virt_addr,
			     dma_addr, dir, 0, attrs);

	mem_tracker_free(dma_tracker, (unsigned long)dma_addr, size, &priv);
}

/* dma_alloc (coherent) */
static void probe_dma_alloc(void *data, struct device *dev,
			    void *virt_addr, dma_addr_t dma_addr,
			    size_t size, enum dma_data_direction dir,
			    gfp_t flags, unsigned long attrs)
{
	probe_dma_alloc_common(DMA_ALLOC_COHERENT, dev, virt_addr,
			       dma_addr, size, dir, flags, attrs);
}

/* dma_alloc_pages */
static void probe_dma_alloc_pages(void *data, struct device *dev,
				  void *virt_addr, dma_addr_t dma_addr,
				  size_t size, enum dma_data_direction dir,
				  gfp_t flags, unsigned long attrs)
{
	probe_dma_alloc_common(DMA_ALLOC_PAGES, dev, virt_addr,
			       dma_addr, size, dir, flags, attrs);
}

/* dma_free (coherent) */
static void probe_dma_free(void *data, struct device *dev,
			   void *virt_addr, dma_addr_t dma_addr,
			   size_t size, enum dma_data_direction dir,
			   unsigned long attrs)
{
	probe_dma_free_common(DMA_ALLOC_COHERENT, dev, virt_addr,
			      dma_addr, size, dir, attrs);
}

/* dma_free_pages */
static void probe_dma_free_pages(void *data, struct device *dev,
				 void *virt_addr, dma_addr_t dma_addr,
				 size_t size, enum dma_data_direction dir,
				 unsigned long attrs)
{
	probe_dma_free_common(DMA_ALLOC_PAGES, dev, virt_addr,
			      dma_addr, size, dir, attrs);
}

/*
 * ============================================================================
 * DMA tracker configuration
 * ============================================================================
 */

static struct mem_tracker_ops dma_ops = {
	.priv_size = sizeof(struct dma_record_priv),
	.alloc_priv = dma_alloc_priv,
	.free_priv = dma_free_priv,
	.format_summary = dma_format_summary,
	.format_active_brief = dma_format_active_brief,
	.format_active_detailed = dma_format_active_detailed,
	.format_event_brief = dma_format_event_brief,
	.format_event_detailed = dma_format_event_detailed,
	.format_history_brief = dma_format_history_brief,
	.format_history_detailed = dma_format_history_detailed,
	.get_display_name = dma_get_display_name,
};

static struct mem_tracker_config dma_config = {
	.name = "dma",
	.buffer_size = MEM_TRACKER_DEFAULT_BUFFER_SIZE,
	.stack_capture = MEM_TRACKER_STACK_DISABLED,
	.ops = &dma_ops,
	.enable = true,
};

/*
 * ============================================================================
 * Module Init/Exit
 * ============================================================================
 */

static int __init dma_tracker_init(void)
{
	int ret;

	/* Register with generic tracker framework */
	dma_tracker = mem_tracker_register(&dma_config);
	if (IS_ERR(dma_tracker)) {
		pr_err("Failed to register DMA tracker: %ld\n",
		       PTR_ERR(dma_tracker));
		return PTR_ERR(dma_tracker);
	}

	/* Register tracepoint probes */
	ret = register_trace_dma_alloc(probe_dma_alloc, NULL);
	if (ret) {
		pr_err("Failed to register dma_alloc probe: %d\n", ret);
		goto err_unregister_tracker;
	}

	ret = register_trace_dma_free(probe_dma_free, NULL);
	if (ret) {
		pr_err("Failed to register dma_free probe: %d\n", ret);
		goto err_unreg_alloc;
	}

	ret = register_trace_dma_alloc_pages(probe_dma_alloc_pages, NULL);
	if (ret) {
		pr_err("Failed to register dma_alloc_pages probe: %d\n", ret);
		goto err_unreg_free;
	}

	ret = register_trace_dma_free_pages(probe_dma_free_pages, NULL);
	if (ret) {
		pr_err("Failed to register dma_free_pages probe: %d\n", ret);
		goto err_unreg_alloc_pages;
	}

	pr_info("DMA tracker initialized (coherent + pages)\n");
	return 0;

err_unreg_alloc_pages:
	unregister_trace_dma_alloc_pages(probe_dma_alloc_pages, NULL);
err_unreg_free:
	unregister_trace_dma_free(probe_dma_free, NULL);
err_unreg_alloc:
	unregister_trace_dma_alloc(probe_dma_alloc, NULL);
err_unregister_tracker:
	mem_tracker_unregister(dma_tracker);
	dma_tracker = NULL;
	return ret;
}

static void __exit dma_tracker_exit(void)
{
	/* Unregister tracepoint probes */
	unregister_trace_dma_free_pages(probe_dma_free_pages, NULL);
	unregister_trace_dma_alloc_pages(probe_dma_alloc_pages, NULL);
	unregister_trace_dma_free(probe_dma_free, NULL);
	unregister_trace_dma_alloc(probe_dma_alloc, NULL);

	/* Unregister from generic framework */
	mem_tracker_unregister(dma_tracker);
	dma_tracker = NULL;

	pr_info("DMA tracker removed\n");
}

module_init(dma_tracker_init);
module_exit(dma_tracker_exit);

MODULE_AUTHOR("Hailo");
MODULE_DESCRIPTION("DMA Allocation Tracker (generic framework)");
MODULE_LICENSE("GPL v2");
