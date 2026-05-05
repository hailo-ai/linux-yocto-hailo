// SPDX-License-Identifier: GPL-2.0
/*
 * Generic Memory Tracker Framework
 * 
 * Provides common infrastructure for tracking memory allocations
 * across different subsystems (CMA, DMA, vmalloc, etc.)
 * Uses debugfs with seq_file for unlimited output.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/kref.h>
#include <linux/ktime.h>
#include <linux/stacktrace.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/tracepoint-defs.h>
#include "mem_tracker.h"


/* Create tracepoint definitions */
#define CREATE_TRACE_POINTS
#include <trace/events/mem_tracker.h>
#undef CREATE_TRACE_POINTS

/* Root debugfs directory for all memory trackers */
static struct dentry *mem_tracker_root;

/* Global enable flag (default: enabled) */
static atomic_t mem_tracker_enabled = ATOMIC_INIT(1);

/* Session ID for tracking recording sessions - updated by user-space scripts */
static u64 current_session_id = 0;
static DEFINE_SPINLOCK(session_id_lock);

/* Forward declaration */
static void emit_new_session_history(struct mem_tracker *tracker);

/*
 * ============================================================================
 * Helper Functions
 * ============================================================================
 */

static inline u64 get_ftrace_timestamp(void)
{
	/* Use boot clock to match ftrace's default trace_clock setting */
	return ktime_get_boot_fast_ns();
}

/* Capture current call stack */
static void capture_stack_trace(unsigned long *entries, unsigned int *nr_entries,
				 unsigned int max_entries)
{
	if (!entries || !nr_entries)
		return;
	
#ifdef CONFIG_STACKTRACE
	*nr_entries = stack_trace_save(entries, max_entries, 2);
#else
	/* Stack trace support not available - return empty stack */
	*nr_entries = 0;
#endif
}

/* Get current process context - macro to avoid buffer size issues */
#define capture_process_context(tgid_ptr, tid_ptr, comm_buf) do { \
	struct task_struct *__task = current; \
	*(tgid_ptr) = __task->tgid; \
	*(tid_ptr) = __task->pid; \
	get_task_comm((comm_buf), __task); \
} while (0)

/*
 * ============================================================================
 * Private Data Reference Counting (using kernel's standard kref API)
 * ============================================================================
 */

/* Release callback for kref_put() - called when refcount reaches zero */
static void mem_priv_release(struct kref *kref)
{
	struct mem_priv *priv;
	
	if (!kref)
		return;
	
	priv = container_of(kref, struct mem_priv, kref);
	
	if (priv && priv->free_priv)
		priv->free_priv(priv->data);
	priv->data = NULL;
	kfree(priv);
}

/* Create a mem_priv wrapper with type-specific data (helper to avoid duplication) */
static struct mem_priv *mem_priv_create(struct mem_tracker *tracker, void *priv)
{
	struct mem_priv *mem_priv_wrapper;
	void *type_priv;
	
	if (!tracker || !tracker->ops || !tracker->ops->alloc_priv || !priv)
		return NULL;
	
	/* Allocate wrapper */
	mem_priv_wrapper = kzalloc(sizeof(struct mem_priv), GFP_ATOMIC);
	if (!mem_priv_wrapper)
		return NULL;
	
	/* Allocate type-specific data */
	type_priv = tracker->ops->alloc_priv();
	if (!type_priv) {
		kfree(mem_priv_wrapper);
		return NULL;
	}
	
	/* Copy caller's data to allocated storage */
	memcpy(type_priv, priv, tracker->ops->priv_size);
	
	/* Initialize wrapper with kref */
	kref_init(&mem_priv_wrapper->kref);
	mem_priv_wrapper->data = type_priv;
	mem_priv_wrapper->free_priv = tracker->ops->free_priv;
	
	return mem_priv_wrapper;
}

/*
 * ============================================================================
 * Circular Buffer Management
 * ============================================================================
 */

/* Format stack trace as a string based on stack_capture mode
 * - MEM_TRACKER_STACK_SYMBOLIZED: Uses %pS for function+offset format (e.g., "func+0x10/0x20")
 * - MEM_TRACKER_STACK_RAW: Uses 0x%lx for raw hex addresses (smaller trace files)
 * - MEM_TRACKER_STACK_DISABLED: Returns empty string */
static char *format_stack_trace(struct mem_tracker *tracker,
					const unsigned long *entries,
					unsigned int nr_entries,
					char *buffer, size_t buffer_size)
{
	unsigned int i;
	int len = 0;
	
	if (!tracker || !entries || nr_entries == 0 || !buffer || buffer_size == 0) {
		if (buffer && buffer_size > 0)
			buffer[0] = '\0';
		return buffer;
	}
	
	if (tracker->stack_capture == MEM_TRACKER_STACK_SYMBOLIZED) {
		/* Use %pS for function+offset format (e.g., "func+0x10/0x20")
		 * Safety margin of 64 bytes for longer symbolized strings */
		for (i = 0; i < nr_entries && len < (int)(buffer_size - 64); i++) {
			if (i > 0) {
				len += scnprintf(buffer + len, buffer_size - len, ",");
			}
			len += scnprintf(buffer + len, buffer_size - len, "%pS", (void *)entries[i]);
		}
	} else if (tracker->stack_capture == MEM_TRACKER_STACK_RAW) {
		/* Use 0x%lx for raw hex addresses (smaller trace files)
		 * Safety margin of 20 bytes for hex addresses */
		for (i = 0; i < nr_entries && len < (int)(buffer_size - 20); i++) {
			if (i > 0) {
				len += scnprintf(buffer + len, buffer_size - len, ",");
			}
			len += scnprintf(buffer + len, buffer_size - len, "0x%lx", entries[i]);
		}
	}
	/* MEM_TRACKER_STACK_DISABLED: buffer remains empty */
	
	if (len == 0) {
		scnprintf(buffer, buffer_size, "none");
	} else if (len < buffer_size) {
		buffer[len] = '\0';
	} else {
		buffer[buffer_size - 1] = '\0';
	}
	
	return buffer;
}

/* Emit event to ftrace for Perfetto integration */
static void emit_event_to_ftrace(struct mem_tracker *tracker,
				   const struct mem_event *event,
				   enum mem_event_type type,
				   unsigned long address, size_t size)
{
	char stack_buf[800];
	char *stack_str = NULL;
	const char *display_name;
	const char *name;
	
	if (!tracker || !event)
		return;
	
	/* Early exit if tracepoint is not enabled - avoid symbolization overhead */
	if (!trace_mem_tracker_event_enabled())
		return;
	
	emit_new_session_history(tracker);


	/* Format stack trace string only if stack capture mode is symbolized
	 * For raw mode, pass NULL so only the raw_stack_entries array is populated */
	if (tracker->stack_capture == MEM_TRACKER_STACK_SYMBOLIZED &&
		event->nr_stack_entries > 0) {
		format_stack_trace(tracker, event->stack_entries, event->nr_stack_entries,
					stack_buf, sizeof(stack_buf));
		stack_str = stack_buf;
	}
	
	/* Get display name: use callback if available, otherwise use tracker name */
	display_name = tracker->name;
	if (tracker->ops && tracker->ops->get_display_name && event->priv && event->priv->data) {
		name = tracker->ops->get_display_name(event->priv->data);
		if (name)
			display_name = name;
	}
	
	/* Pass tracker->name (generic type like "cma") as tracker_name,
	 * and display_name (specific like "hailo_media_buf,cma") as memory_pool.
	 * For symbolized mode: pass formatted string in symbolized_stack_trace, NULL for raw_stack_entries.
	 * For raw mode: pass NULL for symbolized_stack_trace, raw addresses in raw_stack_entries array. */
	trace_mem_tracker_event(tracker->name,
				type == MEM_EVENT_ALLOC ? "ALLOC" : "FREE",
				address, size, event->allocation_uid, event->comm, event->tgid,
				event->timestamp,
				(tracker->stack_capture == MEM_TRACKER_STACK_RAW) ? event->stack_entries : NULL,
				(tracker->stack_capture == MEM_TRACKER_STACK_RAW) ? event->nr_stack_entries : 0,
				stack_str, display_name);
}

/* Emit history record to ftrace for Perfetto integration */
static void emit_hist_to_ftrace(struct mem_tracker *tracker,
				   const struct mem_hist_record *hist)
{
	char alloc_stack_buf[800];
	char free_stack_buf[800];
	const char *display_name;
	const char *name;
	char *alloc_stack_str = NULL;
	char *free_stack_str = NULL;
	
	if (!tracker || !hist)
		return;
	
	/* Early exit if tracepoint is not enabled - avoid symbolization overhead */
	if (!trace_mem_tracker_hist_event_enabled())
		return;

	/* Format stack trace strings ONLY if symbolized mode is enabled
	 * For raw mode, pass NULL so only the raw_stack_entries arrays are populated */
	if (tracker->stack_capture == MEM_TRACKER_STACK_SYMBOLIZED) {
		if (hist->nr_alloc_stack > 0) {
			format_stack_trace(tracker, hist->alloc_stack, hist->nr_alloc_stack,
						  alloc_stack_buf, sizeof(alloc_stack_buf));
			alloc_stack_str = alloc_stack_buf;
		}
		if (hist->freed && hist->nr_free_stack > 0) {
			format_stack_trace(tracker, hist->free_stack, hist->nr_free_stack,
						  free_stack_buf, sizeof(free_stack_buf));
			free_stack_str = free_stack_buf;
		}
	}
	
	/* Get display name: use callback if available, otherwise use tracker name */
	display_name = tracker->name;
	if (tracker->ops && tracker->ops->get_display_name && hist->priv && hist->priv->data) {
		name = tracker->ops->get_display_name(hist->priv->data);
		if (name)
			display_name = name;
	}
	
	/* Emit history tracepoint - pass formatted string if symbolized, otherwise raw addresses */
	pr_debug("mem_tracker: Emitting hist event: addr=0x%lx uid=%llu freed=%d\n",
		 hist->address, hist->allocation_uid, hist->freed);
	trace_mem_tracker_hist_event(tracker->name,
					hist->address, hist->size, hist->allocation_uid,
					hist->alloc_comm, hist->alloc_tgid,
					hist->alloc_timestamp,
					(tracker->stack_capture == MEM_TRACKER_STACK_RAW) ? hist->alloc_stack : NULL,
					(tracker->stack_capture == MEM_TRACKER_STACK_RAW) ? hist->nr_alloc_stack : 0,
					alloc_stack_str,
					display_name,
					hist->freed,
					hist->freed ? hist->free_comm : "",
					hist->freed ? hist->free_tgid : 0,
					hist->freed ? hist->free_timestamp : 0,
					(tracker->stack_capture == MEM_TRACKER_STACK_RAW && hist->freed) ? hist->free_stack : NULL,
					(tracker->stack_capture == MEM_TRACKER_STACK_RAW && hist->freed) ? hist->nr_free_stack : 0,
					free_stack_str);
}

static void build_event_generic(struct mem_tracker *tracker,
				 struct mem_event *event,
				 enum mem_event_type type,
				 unsigned long address, size_t size,
				 u64 allocation_uid,
				 pid_t tgid,
				 pid_t tid,
				 const char *comm,
				 u64 timestamp,
				 const unsigned long *stack_entries,
				 unsigned int nr_stack_entries,
				 struct mem_priv *priv)
{
	if (!tracker || !event)
		return;
	
	event->type = type;
	event->address = address;
	event->size = size;
	event->allocation_uid = allocation_uid;
	event->timestamp = timestamp;
	
	/* Capture process context if comm is NULL */
	if (comm == NULL) {
		capture_process_context(&event->tgid, &event->tid, event->comm);
	} else {
		event->tgid = tgid;
		event->tid = tid;
		memcpy(event->comm, comm, TASK_COMM_LEN);
	}
	
	/* Copy stack trace from alloc record (already captured) */
	if (nr_stack_entries > 0 && stack_entries) {
		memcpy(event->stack_entries, stack_entries,
			nr_stack_entries * sizeof(unsigned long));
		event->nr_stack_entries = nr_stack_entries;
	} else if (tracker->stack_capture != MEM_TRACKER_STACK_DISABLED) {
		/* Capture stack if enabled */
		capture_stack_trace(event->stack_entries, &event->nr_stack_entries,
				MEM_TRACKER_MAX_STACK_DEPTH);
	} else {
		event->nr_stack_entries = 0;
	}
	
	/* Share the same priv pointer (increment refcount) */
	event->priv = priv;
	if (event->priv)
		kref_get(&event->priv->kref);
}

/* Build event struct from allocation record (for alloc events) */
static void build_event_for_alloc(struct mem_tracker *tracker,
				struct mem_event *event,
				const struct mem_alloc_record *alloc_rec)
{
	if (!alloc_rec)
		return;
	
	build_event_generic(tracker, event, MEM_EVENT_ALLOC,
				alloc_rec->address, alloc_rec->size, alloc_rec->allocation_uid,
				alloc_rec->tgid, alloc_rec->tid, alloc_rec->comm, alloc_rec->timestamp,
				alloc_rec->stack_entries, alloc_rec->nr_stack_entries, alloc_rec->priv);
}

/* Build event struct for free event from allocation record (when allocation found) */
static void build_event_for_free(struct mem_tracker *tracker,
					 struct mem_event *event,
					 const struct mem_alloc_record *alloc_rec,
					 u64 timestamp)
{
	if (!alloc_rec)
		return;
	
	build_event_generic(tracker, event, MEM_EVENT_FREE,
				alloc_rec->address, alloc_rec->size, alloc_rec->allocation_uid,
				alloc_rec->tgid, alloc_rec->tid, alloc_rec->comm, timestamp,
				NULL, 0, alloc_rec->priv);
}

/* Build event struct for free event (when allocation not found) */
static void build_event_for_free_no_alloc_found(struct mem_tracker *tracker,
				 struct mem_event *event,
				 unsigned long address, size_t size,
				 struct mem_priv *priv, u64 timestamp)
{
	/* UID is 0 since allocation not found, pass NULL for comm to capture internally */
	build_event_generic(tracker, event, MEM_EVENT_FREE,
		address, size, 0, 0, 0, NULL, timestamp,
		NULL, 0, priv);
}

static void fill_history_record_alloc(struct mem_tracker *tracker,
				 struct mem_hist_record *hist,
				 const struct mem_alloc_record *alloc_rec)
{
	if (!hist || !alloc_rec)
		return;
	
	hist->address = alloc_rec->address;
	hist->size = alloc_rec->size;
	hist->allocation_uid = alloc_rec->allocation_uid;
	
	hist->alloc_tgid = alloc_rec->tgid;
	hist->alloc_tid = alloc_rec->tid;
	
	memcpy(hist->alloc_comm, alloc_rec->comm, TASK_COMM_LEN);
	hist->alloc_timestamp = alloc_rec->timestamp;

	if (alloc_rec->nr_stack_entries > 0 && alloc_rec->stack_entries) {
		memcpy(hist->alloc_stack, alloc_rec->stack_entries,
			alloc_rec->nr_stack_entries * sizeof(unsigned long));
		hist->nr_alloc_stack = alloc_rec->nr_stack_entries;
	} else {
		hist->nr_alloc_stack = 0;
	}

	/* New allocation - no free info yet */
	hist->freed = false;
	hist->nr_free_stack = 0;

	/* Share the same priv pointer (increment refcount) */
	hist->priv = alloc_rec->priv;
	if (hist->priv)
		kref_get(&hist->priv->kref);
}

static void fill_history_record_free(struct mem_tracker *tracker,
				 struct mem_hist_record *hist,
				 const struct mem_event *free_event)
{
	if (!hist || !free_event)
		return;
	
	hist->freed = true;
	hist->free_tgid = free_event->tgid;
	hist->free_tid = free_event->tid;

	hist->free_timestamp = free_event->timestamp;
	memcpy(hist->free_comm, free_event->comm, TASK_COMM_LEN);

	if (free_event->nr_stack_entries > 0 && free_event->stack_entries) {
		memcpy(hist->free_stack, free_event->stack_entries,
			free_event->nr_stack_entries * sizeof(unsigned long));
		hist->nr_free_stack = free_event->nr_stack_entries;
	} else {
		hist->nr_free_stack = 0;
	}

	/* Set the priv context only if it's not already set */
	if (!hist->priv) {
		hist->priv = free_event->priv;
		/* Share the same priv pointer (increment refcount) */
		if (hist->priv)
			kref_get(&hist->priv->kref);
	}
}

/* Add event to circular buffer (receives pre-built event struct) */
static void add_event(struct mem_tracker *tracker, const struct mem_event *event_src)
{
	struct mem_event *event;
	unsigned long flags;
	
	if (!tracker || !event_src)
		return;
	
	spin_lock_irqsave(&tracker->events_buffer.lock, flags);
	
	event = &tracker->events_buffer.events[tracker->events_buffer.head];
	
	/* Release old event's priv if buffer is full (about to overwrite) */
	if (tracker->events_buffer.count >= tracker->events_buffer.size && event->priv) {
		kref_put(&event->priv->kref, mem_priv_release);
		event->priv = NULL;
	}
	
	/* Update circular buffer pointers */
	tracker->events_buffer.head = (tracker->events_buffer.head + 1) %
				tracker->events_buffer.size;
	if (tracker->events_buffer.count < tracker->events_buffer.size)
		tracker->events_buffer.count++;
	
	/* Copy event data (increment refcount) */
	*event = *event_src;
	if (event->priv)
		kref_get(&event->priv->kref);
	
	spin_unlock_irqrestore(&tracker->events_buffer.lock, flags);
	
	/* Emit to ftrace for Perfetto integration,
	 * use the input instance which is guaranteed lifetime */
	emit_event_to_ftrace(tracker, event_src, event_src->type, event_src->address, event_src->size);
}

/* Find history record by UID (fallback when direct pointer is stale) */
static struct mem_hist_record *find_history_record_by_uid(struct mem_tracker *tracker,
							  u64 allocation_uid)
{
	struct mem_hist_record *hist;
	unsigned int i, idx, start, count;
	
	count = tracker->hist_buffer.count;
	if (count == 0)
		return NULL;
	
	/* Calculate start index (oldest entry) */
	if (count < tracker->hist_buffer.size)
		start = 0;
	else
		start = tracker->hist_buffer.head;
	
	/* Search from oldest to newest */
	for (i = 0; i < count; i++) {
		idx = (start + i) % tracker->hist_buffer.size;
		hist = &tracker->hist_buffer.records[idx];
		
		if (hist->allocation_uid == allocation_uid)
			return hist;
	}
	
	return NULL;
}

/* Add record to history circular buffer or update if freeing */
static void add_to_history(struct mem_tracker *tracker,
			   struct mem_alloc_record *alloc_rec,
			   const struct mem_event *event)
{
	struct mem_hist_record *hist;
	struct mem_hist_record hist_copy;
	unsigned long flags;
	bool found = false;
	
	if (!tracker || !event || !alloc_rec)
		return;

	if (event->type == MEM_EVENT_FREE && !alloc_rec->hist_record)
		return;
	
	spin_lock_irqsave(&tracker->hist_buffer.lock, flags);
	
	if (event->type == MEM_EVENT_FREE) {
		/* Use direct pointer to history record (O(1) instead of O(n) search) */
		hist = alloc_rec->hist_record;
		
		/* Verify it's still valid (not freed and matches UID) */
		/* If buffer wrapped around, this record may have been overwritten with different data */
		if (!hist->freed && hist->allocation_uid == alloc_rec->allocation_uid) {
			/* Update this record with free info from event */
			fill_history_record_free(tracker, hist, event);
			found = true;
			
			/* Copy history record data before unlocking (for ftrace emit, increment refcount) */
			hist_copy = *hist;
			if (hist_copy.priv)
				kref_get(&hist_copy.priv->kref);

		} else {
			/* Pointer is stale (buffer wrapped around and overwrote this record) */
			/* Search for the record by UID */
			hist = find_history_record_by_uid(tracker, alloc_rec->allocation_uid);
			if (hist && !hist->freed) {
				/* Update this record with free info from event */
				fill_history_record_free(tracker, hist, event);
				found = true;
				
				/* Copy history record data before unlocking (for ftrace emit, increment refcount) */
				hist_copy = *hist;
				if (hist_copy.priv)
					kref_get(&hist_copy.priv->kref);
				
				/* Fix the stale pointer by updating alloc_rec->hist_record */
				alloc_rec->hist_record = hist;
			} else {
				/* Record not found in history buffer or already freed - clear stale pointer */
				alloc_rec->hist_record = NULL;
			}
		}
		
		spin_unlock_irqrestore(&tracker->hist_buffer.lock, flags);
		
		/* Emit updated history record to ftrace for Perfetto integration.
		 * Using a copy of the record because the main record may have been overwritten
		 * at any time since we've unlocked hist_buffer.lock */
		if (found) {
			emit_hist_to_ftrace(tracker, &hist_copy);
			/* Release the refcount we took for emit */
			if (hist_copy.priv)
				kref_put(&hist_copy.priv->kref, mem_priv_release);
		}
		
		return;
	}
	
	/* Adding new allocation record */
	hist = &tracker->hist_buffer.records[tracker->hist_buffer.head];
	
	/* If buffer is full, we're about to overwrite an old record */
	if (tracker->hist_buffer.count >= tracker->hist_buffer.size) {
		/* Release old history entry's priv if present */
		if (hist->priv) {
			kref_put(&hist->priv->kref, mem_priv_release);
			hist->priv = NULL;
		}
		
		/* Clear any stale pointers from active allocations pointing to this record */
		/* Note: We need to acquire alloc_lock, but we're already holding hist_buffer.lock */
		/* To avoid deadlock, we'll clear pointers when we detect staleness during free */
		/* The UID check in the free path will catch overwritten records */
	}
	
	/* Fill allocation fields from the record and event (stack trace already captured) */
	fill_history_record_alloc(tracker, hist, alloc_rec);
	
	/* Store pointer to history record in allocation record for O(1) lookup on free */
	alloc_rec->hist_record = hist;
	
	/* Update circular buffer pointers */
	tracker->hist_buffer.head = (tracker->hist_buffer.head + 1) %
				    tracker->hist_buffer.size;
	if (tracker->hist_buffer.count < tracker->hist_buffer.size)
		tracker->hist_buffer.count++;
	
	/* Copy history record data before unlocking (for ftrace emit, increment refcount) */
	hist_copy = *hist;
	if (hist_copy.priv)
		kref_get(&hist_copy.priv->kref);
	
	spin_unlock_irqrestore(&tracker->hist_buffer.lock, flags);
	
	/* Emit to ftrace for Perfetto integration,
	 * use the input instance which is guaranteed lifetime */
	emit_hist_to_ftrace(tracker, &hist_copy);
	
	/* Release the refcount we took for emit */
	if (hist_copy.priv)
		kref_put(&hist_copy.priv->kref, mem_priv_release);
}

/* Emit all existing active allocations to ftrace for a new recording session (for Perfetto integration) */
static void emit_new_session_history(struct mem_tracker *tracker)
{
	struct mem_alloc_record *rec;
	struct mem_hist_record hist;
	unsigned long flags;
	u64 session_id;
	bool new_session = false;
	
	if (!tracker)
		return;
	
	/* Check if session_id changed (new recording session) */
	/* Note: session_id of 0 means "not set yet", so we only consider non-zero session_ids */
	spin_lock(&session_id_lock);
	session_id = current_session_id;
	
	if (session_id != 0 && session_id != tracker->last_seen_session_id) {
		pr_warn("[mem_tracker] New tracing session detected! session_id=%llu (last_seen was %llu)\n", 
			session_id, tracker->last_seen_session_id);
		new_session = true;
		tracker->last_seen_session_id = session_id;
	}
	spin_unlock(&session_id_lock);
	
	/* Return early if this is not a new session */
	if (!new_session) {
		return;
	}
	
	/* Iterate over active allocations list (only contains active allocations) */
	spin_lock_irqsave(&tracker->alloc_lock, flags);
	
	list_for_each_entry(rec, &tracker->alloc_list, list) {
		/* Convert mem_alloc_record to mem_hist_record for emit */
		hist.address = rec->address;
		hist.size = rec->size;
		hist.allocation_uid = rec->allocation_uid;
		hist.alloc_tgid = rec->tgid;
		hist.alloc_tid = rec->tid;
		memcpy(hist.alloc_comm, rec->comm, TASK_COMM_LEN);
		hist.alloc_timestamp = rec->timestamp;
		
		if (rec->nr_stack_entries > 0) {
			memcpy(hist.alloc_stack, rec->stack_entries,
				rec->nr_stack_entries * sizeof(unsigned long));
			hist.nr_alloc_stack = rec->nr_stack_entries;
		} else {
			hist.nr_alloc_stack = 0;
		}
		
		hist.priv = rec->priv;
		
		/* Not freed yet */
		hist.freed = false;
		hist.nr_free_stack = 0;
		
		/* Increment refcount on hist's priv before unlocking (emit will access it) */
		if (hist.priv)
			kref_get(&hist.priv->kref);
		
		/* Emit this record and re-lock for next iteration */
		spin_unlock_irqrestore(&tracker->alloc_lock, flags);
		emit_hist_to_ftrace(tracker, &hist);
		/* Release the refcount we took for emit */
		if (hist.priv)
			kref_put(&hist.priv->kref, mem_priv_release);
		spin_lock_irqsave(&tracker->alloc_lock, flags);
	}
	
	spin_unlock_irqrestore(&tracker->alloc_lock, flags);
}

/*
 * ============================================================================
 * RB-tree helpers for O(log n) address lookup
 * ============================================================================
 */

/* Find allocation record by address using RB-tree (exported for use by type-specific trackers) */
struct mem_alloc_record *mem_tracker_find_by_address(struct mem_tracker *tracker,
							unsigned long address)
{
	struct rb_node *node;
	
	if (!tracker)
		return NULL;
	
	node = tracker->alloc_tree.rb_node;
	
	while (node) {
		struct mem_alloc_record *rec = rb_entry(node, struct mem_alloc_record, rb_node);
		
		if (address < rec->address)
			node = node->rb_left;
		else if (address > rec->address)
			node = node->rb_right;
		else
			return rec;  /* Found */
	}
	return NULL;  /* Not found */
}
EXPORT_SYMBOL_GPL(mem_tracker_find_by_address);

/* Insert allocation record into RB-tree */
static void insert_alloc_to_tree(struct mem_tracker *tracker,
				  struct mem_alloc_record *rec)
{
	struct rb_node **new = &tracker->alloc_tree.rb_node;
	struct rb_node *parent = NULL;
	
	while (*new) {
		struct mem_alloc_record *this = rb_entry(*new, struct mem_alloc_record, rb_node);
		
		parent = *new;
		if (rec->address < this->address)
			new = &((*new)->rb_left);
		else if (rec->address > this->address)
			new = &((*new)->rb_right);
		else
			/* Duplicate address - should not happen, but handle gracefully */
			return;
	}
	
	rb_link_node(&rec->rb_node, parent, new);
	rb_insert_color(&rec->rb_node, &tracker->alloc_tree);
}

/* Remove allocation record from RB-tree */
static void remove_alloc_from_tree(struct mem_tracker *tracker,
				   struct mem_alloc_record *rec)
{
	rb_erase(&rec->rb_node, &tracker->alloc_tree);
}

/*
 * ============================================================================
 * Core Tracking Functions
 * ============================================================================
 */

/* Track a new allocation */
int mem_tracker_alloc(struct mem_tracker *tracker,
			unsigned long address,
			size_t size,
			void *priv)
{
	struct mem_alloc_record *rec;
	struct mem_event event;
	unsigned long flags;
	s64 current_alloc, peak;
	
	if (!tracker || !tracker->initialized)
		return -EINVAL;
	
	/* Allocate record */
	rec = kzalloc(sizeof(*rec), GFP_ATOMIC);
	if (!rec)
		return -ENOMEM;

	rec->timestamp = get_ftrace_timestamp();
	
	/* Generate unique allocation UID */
	rec->allocation_uid = atomic64_inc_return(&tracker->next_allocation_uid);
	
	/* Fill common fields */
	rec->address = address;
	rec->size = size;
	capture_process_context(&rec->tgid, &rec->tid, rec->comm);
	
	/* Capture stack if enabled */
	if (tracker->stack_capture != MEM_TRACKER_STACK_DISABLED) {
		capture_stack_trace(rec->stack_entries, &rec->nr_stack_entries,
				   MEM_TRACKER_MAX_STACK_DEPTH);
	} else {
		rec->nr_stack_entries = 0;
	}
	
	/* Allocate and wrap type-specific private data */
	rec->priv = mem_priv_create(tracker, priv);
	if (priv && !rec->priv) {
		kfree(rec);
		return -ENOMEM;
	}
	
	/* Add to active allocations: both list (chronological) and RB-tree (fast lookup) */
	spin_lock_irqsave(&tracker->alloc_lock, flags);
	list_add_tail(&rec->list, &tracker->alloc_list);
	insert_alloc_to_tree(tracker, rec);
	spin_unlock_irqrestore(&tracker->alloc_lock, flags);
	
	/* Update statistics */
	atomic64_add(size, &tracker->total_allocated);
	current_alloc = atomic64_add_return(size, &tracker->current_allocated);
	atomic_inc(&tracker->alloc_count);
	
	/* Update peak if necessary */
	do {
		peak = atomic64_read(&tracker->peak_allocated);
		if (current_alloc <= peak)
			break;
	} while (atomic64_cmpxchg(&tracker->peak_allocated, peak, current_alloc) != peak);
	
	/* Build event struct once (reuses stack trace already captured in rec) */
	build_event_for_alloc(tracker, &event, rec);
	
	/* Add to circular buffers (both reuse the same stack trace from rec) */
	add_event(tracker, &event);
	add_to_history(tracker, rec, &event);

	/* Release the temporary event's priv (from build_event) */
	if (event.priv) {
		kref_put(&event.priv->kref, mem_priv_release);
		event.priv = NULL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mem_tracker_alloc);

/* Track a deallocation */
int mem_tracker_free(struct mem_tracker *tracker,
			unsigned long address,
			size_t size,
			void *priv)
{
	struct mem_alloc_record *rec;
	struct mem_priv *temp_priv_wrapper = NULL;
	struct mem_event event;
	unsigned long flags;
	u64 free_timestamp;
	bool found = false;
	
	if (!tracker || !tracker->initialized)
		return -EINVAL;

	/* Capture timestamp once for free event */
	free_timestamp = get_ftrace_timestamp();
	
	/* Find and remove from active allocations using O(log n) RB-tree lookup */
	spin_lock_irqsave(&tracker->alloc_lock, flags);
	rec = mem_tracker_find_by_address(tracker, address);
	if (rec) {
		list_del(&rec->list);
		remove_alloc_from_tree(tracker, rec);
		found = true;
	}
	spin_unlock_irqrestore(&tracker->alloc_lock, flags);
	
	if (found) {
		/* Update statistics */
		atomic64_add(size, &tracker->total_freed);
		atomic64_sub(size, &tracker->current_allocated);
		atomic_inc(&tracker->free_count);
		
		/* Add free event and update history before freeing record */
		build_event_for_free(tracker, &event, rec, free_timestamp);
		add_event(tracker, &event);
		add_to_history(tracker, rec, &event);

		/* Release the temporary event's priv (from build_event) */
		if (event.priv) {
			kref_put(&event.priv->kref, mem_priv_release);
			event.priv = NULL;
		}
		
		/* Release reference to private data (remove the active list's reference) */
		if (rec->priv) {
			kref_put(&rec->priv->kref, mem_priv_release);
			rec->priv = NULL;
		}
		
		kfree(rec);
		rec = NULL;
		return 0;
	}

	/* Allocation not found - create a new priv wrapper for the free event */
	temp_priv_wrapper = mem_priv_create(tracker, priv);
	if (!temp_priv_wrapper) {
		return -ENOMEM;
	}

	build_event_for_free_no_alloc_found(tracker, &event, address, size, temp_priv_wrapper, free_timestamp);
	add_event(tracker, &event);

	/* Release the temporary event's priv (from build_event) */
	if (event.priv) {
		kref_put(&event.priv->kref, mem_priv_release);
		event.priv = NULL;
	}

	/* Release our initial reference from mem_priv_create (event now owns it) */
	if (temp_priv_wrapper) {
		kref_put(&temp_priv_wrapper->kref, mem_priv_release);
		temp_priv_wrapper = NULL;
	}

	return -ENOENT;
}
EXPORT_SYMBOL_GPL(mem_tracker_free);

/*
 * ============================================================================
 * seq_file Implementations for debugfs Files
 * ============================================================================
 */

/* summary */
static int summary_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	unsigned long flags;
	int active_count = 0;
	struct mem_alloc_record *rec;
	s64 total_alloc, total_freed, current_alloc, peak_alloc;
	
	total_alloc = atomic64_read(&tracker->total_allocated);
	total_freed = atomic64_read(&tracker->total_freed);
	current_alloc = atomic64_read(&tracker->current_allocated);
	peak_alloc = atomic64_read(&tracker->peak_allocated);
	
	seq_puts(m, "============================================\n");
	seq_printf(m, "Memory Tracker Statistics: %s\n", tracker->name);
	seq_puts(m, "============================================\n");
	
	seq_printf(m, "Total allocated:   %lld bytes (%lld MB)\n",
		   total_alloc, total_alloc / (1024 * 1024));
	seq_printf(m, "Total freed:       %lld bytes (%lld MB)\n",
		   total_freed, total_freed / (1024 * 1024));
	seq_printf(m, "Current allocated: %lld bytes (%lld MB)\n",
		   current_alloc, current_alloc / (1024 * 1024));
	seq_printf(m, "Peak allocated:    %lld bytes (%lld MB)\n",
		   peak_alloc, peak_alloc / (1024 * 1024));
	seq_printf(m, "Allocation count:  %d\n",
		   atomic_read(&tracker->alloc_count));
	seq_printf(m, "Free count:        %d\n",
		   atomic_read(&tracker->free_count));
	
	/* Count active allocations */
	spin_lock_irqsave(&tracker->alloc_lock, flags);
	list_for_each_entry(rec, &tracker->alloc_list, list)
		active_count++;
	spin_unlock_irqrestore(&tracker->alloc_lock, flags);
	
	seq_printf(m, "Active allocations: %d\n\n", active_count);
	
	/* Circular buffer status */
	seq_printf(m, "Memory event buffers:  %u/%u entries\n",
		   tracker->events_buffer.count, tracker->events_buffer.size);
	seq_printf(m, "Allocation history buffers: %u/%u entries\n",
		   tracker->hist_buffer.count, tracker->hist_buffer.size);
	
	seq_printf(m, "Current trace session ID: %llu\n", current_session_id);
	seq_printf(m, "Stack capture: %s\n",
		   tracker->stack_capture == MEM_TRACKER_STACK_DISABLED ? "disabled" :
		   tracker->stack_capture == MEM_TRACKER_STACK_RAW ? "raw addresses" : "symbolized");
	seq_printf(m, "  (0=disabled, 1=raw addresses, 2=symbolized)\n");
	
	/* Type-specific summary */
	if (tracker->ops && tracker->ops->format_summary) {
		seq_puts(m, "\n============================================\n\n");
		tracker->ops->format_summary(m, tracker);
		seq_puts(m, "============================================\n");
	}
	
	return 0;
}

/* Active allocations */
static int active_brief_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	struct mem_alloc_record *rec;
	unsigned long flags;
	int count = 0;
	
	spin_lock_irqsave(&tracker->alloc_lock, flags);
	
	list_for_each_entry(rec, &tracker->alloc_list, list) {
		if (tracker->ops && tracker->ops->format_active_brief)
			tracker->ops->format_active_brief(m, rec);
		count++;
	}
	
	spin_unlock_irqrestore(&tracker->alloc_lock, flags);
	
	if (count == 0)
		seq_puts(m, "No active allocations\n");
	else
		seq_printf(m, "\nTotal: %d active allocations\n", count);
	
	return 0;
}

static int active_detailed_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	struct mem_alloc_record *rec;
	unsigned long flags;
	int count = 0;
	
	spin_lock_irqsave(&tracker->alloc_lock, flags);
	
	list_for_each_entry(rec, &tracker->alloc_list, list) {
		if (tracker->ops && tracker->ops->format_active_detailed)
			tracker->ops->format_active_detailed(m, rec);
		count++;
	}
	
	spin_unlock_irqrestore(&tracker->alloc_lock, flags);
	
	if (count == 0)
		seq_puts(m, "No active allocations\n");
	else
		seq_printf(m, "\nTotal: %d active allocations\n", count);
	
	return 0;
}

/* Events */
static int events_brief_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	unsigned long flags;
	unsigned int i, idx, count, start;
	
	spin_lock_irqsave(&tracker->events_buffer.lock, flags);
	
	count = tracker->events_buffer.count;
	
	if (count == 0) {
		spin_unlock_irqrestore(&tracker->events_buffer.lock, flags);
		seq_puts(m, "No events recorded\n");
		return 0;
	}
	
	/* Calculate start index (oldest entry) */
	if (count < tracker->events_buffer.size)
		start = 0;
	else
		start = tracker->events_buffer.head;
	
	/* Iterate from oldest to newest */
	for (i = 0; i < count; i++) {
		idx = (start + i) % tracker->events_buffer.size;
		if (tracker->ops && tracker->ops->format_event_brief)
			tracker->ops->format_event_brief(m,
				&tracker->events_buffer.events[idx]);
	}
	
	spin_unlock_irqrestore(&tracker->events_buffer.lock, flags);
	
	seq_printf(m, "\nTotal: %d events\n", count);
	
	return 0;
}

static int events_detailed_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	unsigned long flags;
	unsigned int i, idx, count, start;
	
	spin_lock_irqsave(&tracker->events_buffer.lock, flags);
	
	count = tracker->events_buffer.count;
	
	if (count == 0) {
		spin_unlock_irqrestore(&tracker->events_buffer.lock, flags);
		seq_puts(m, "No events recorded\n");
		return 0;
	}
	
	/* Calculate start index (oldest entry) */
	if (count < tracker->events_buffer.size)
		start = 0;
	else
		start = tracker->events_buffer.head;
	
	/* Iterate from oldest to newest */
	for (i = 0; i < count; i++) {
		idx = (start + i) % tracker->events_buffer.size;
		if (tracker->ops && tracker->ops->format_event_detailed)
			tracker->ops->format_event_detailed(m,
				&tracker->events_buffer.events[idx]);
	}
	
	spin_unlock_irqrestore(&tracker->events_buffer.lock, flags);
	
	seq_printf(m, "\nTotal: %d events\n", count);
	
	return 0;
}

/* Allocations History */
static int history_brief_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	unsigned long flags;
	unsigned int i, idx, count, start;
	
	spin_lock_irqsave(&tracker->hist_buffer.lock, flags);
	
	count = tracker->hist_buffer.count;
	
	if (count == 0) {
		spin_unlock_irqrestore(&tracker->hist_buffer.lock, flags);
		seq_puts(m, "No allocation history recorded\n");
		return 0;
	}
	
	/* Calculate start index (oldest entry) */
	if (count < tracker->hist_buffer.size)
		start = 0;
	else
		start = tracker->hist_buffer.head;
	
	/* Iterate from oldest to newest */
	for (i = 0; i < count; i++) {
		idx = (start + i) % tracker->hist_buffer.size;
		if (tracker->ops && tracker->ops->format_history_brief)
			tracker->ops->format_history_brief(m,
				&tracker->hist_buffer.records[idx]);
	}
	
	spin_unlock_irqrestore(&tracker->hist_buffer.lock, flags);
	
	seq_printf(m, "\nTotal: %d allocations\n", count);
	
	return 0;
}

static int history_detailed_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	unsigned long flags;
	unsigned int i, idx, count, start;
	
	spin_lock_irqsave(&tracker->hist_buffer.lock, flags);
	
	count = tracker->hist_buffer.count;
	
	if (count == 0) {
		spin_unlock_irqrestore(&tracker->hist_buffer.lock, flags);
		seq_puts(m, "No allocation history recorded\n");
		return 0;
	}
	
	/* Calculate start index (oldest entry) */
	if (count < tracker->hist_buffer.size)
		start = 0;
	else
		start = tracker->hist_buffer.head;
	
	/* Iterate from oldest to newest */
	for (i = 0; i < count; i++) {
		idx = (start + i) % tracker->hist_buffer.size;
		if (tracker->ops && tracker->ops->format_history_detailed)
			tracker->ops->format_history_detailed(m,
				&tracker->hist_buffer.records[idx]);
	}
	
	spin_unlock_irqrestore(&tracker->hist_buffer.lock, flags);
	
	seq_printf(m, "\nTotal: %d allocations\n", count);
	
	return 0;
}

/* Other options */
static int buffer_size_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	seq_printf(m, "%u\n", tracker->buffer_size);
	return 0;
}

static ssize_t buffer_size_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct mem_tracker *tracker = m->private;
	unsigned int new_size;
	char kbuf[32];
	int ret;
	
	if (count >= sizeof(kbuf))
		return -EINVAL;
	
	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;
	kbuf[count] = '\0';
	
	ret = kstrtouint(kbuf, 0, &new_size);
	if (ret)
		return ret;
	
	ret = mem_tracker_set_buffer_size(tracker, new_size);
	if (ret)
		return ret;
	
	return count;
}

static int stack_capture_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	seq_printf(m, "%u\n", tracker->stack_capture);
	return 0;
}

static ssize_t stack_capture_write(struct file *file,
				    const char __user *buf,
				    size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct mem_tracker *tracker = m->private;
	unsigned int mode;
	char kbuf[32];
	int ret;
	
	if (count >= sizeof(kbuf))
		return -EINVAL;
	
	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;
	kbuf[count] = '\0';
	
	ret = kstrtouint(kbuf, 0, &mode);
	if (ret)
		return ret;
	
	if (mode > MEM_TRACKER_STACK_SYMBOLIZED) {
		pr_err("mem_tracker: Invalid stack_capture mode %u (valid: 0=disabled, 1=raw, 2=symbolized)\n",
				mode);
		return -EINVAL;
	}
	
	tracker->stack_capture = mode;
	
	return count;
}

/* Session ID debugfs handlers */
static int session_id_seq_show(struct seq_file *m, void *v)
{
	unsigned long flags;
	u64 session_id;
	
	spin_lock_irqsave(&session_id_lock, flags);
	session_id = current_session_id;
	spin_unlock_irqrestore(&session_id_lock, flags);
	
	seq_printf(m, "%llu\n", session_id);
	return 0;
}

static ssize_t session_id_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	char kbuf[32];
	u64 new_session_id;
	int ret;
	
	if (count >= sizeof(kbuf))
		return -EINVAL;
	
	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;
	kbuf[count] = '\0';
	
	ret = kstrtoull(kbuf, 0, &new_session_id);
	if (ret)
		return ret;
	
	spin_lock(&session_id_lock);
	current_session_id = new_session_id;
	spin_unlock(&session_id_lock);
	
	return count;
}

/* Global enable debugfs handlers */
static int global_enable_seq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", atomic_read(&mem_tracker_enabled));
	return 0;
}

static ssize_t global_enable_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	char kbuf[32];
	unsigned int enabled;
	int ret;

	if (count >= sizeof(kbuf))
		return -EINVAL;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;
	kbuf[count] = '\0';

	ret = kstrtouint(kbuf, 0, &enabled);
	if (ret)
		return ret;

	if (enabled > 1)
		return -EINVAL;

	atomic_set(&mem_tracker_enabled, enabled ? 1 : 0);

	return count;
}

/* Per-tracker enable debugfs handlers */
static int enable_seq_show(struct seq_file *m, void *v)
{
	struct mem_tracker *tracker = m->private;
	seq_printf(m, "%d\n", atomic_read(&tracker->enabled));
	return 0;
}

static ssize_t enable_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct mem_tracker *tracker = m->private;
	char kbuf[32];
	unsigned int enabled;
	int ret;

	if (count >= sizeof(kbuf))
		return -EINVAL;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;
	kbuf[count] = '\0';

	ret = kstrtouint(kbuf, 0, &enabled);
	if (ret)
		return ret;

	if (enabled > 1)
		return -EINVAL;

	atomic_set(&tracker->enabled, enabled ? 1 : 0);

	return count;
}

/*
 * ============================================================================
 * file_operations for debugfs
 * ============================================================================
 */

#define DEFINE_SEQ_FOPS(name) \
static int name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, name##_seq_show, inode->i_private); \
} \
static const struct file_operations name##_fops = { \
	.owner = THIS_MODULE, \
	.open = name##_open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define DEFINE_SEQ_FOPS_RW(name) \
static int name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, name##_seq_show, inode->i_private); \
} \
static const struct file_operations name##_fops = { \
	.owner = THIS_MODULE, \
	.open = name##_open, \
	.read = seq_read, \
	.write = name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

DEFINE_SEQ_FOPS(summary);
DEFINE_SEQ_FOPS(active_brief);
DEFINE_SEQ_FOPS(active_detailed);
DEFINE_SEQ_FOPS(events_brief);
DEFINE_SEQ_FOPS(events_detailed);
DEFINE_SEQ_FOPS(history_brief);
DEFINE_SEQ_FOPS(history_detailed);
DEFINE_SEQ_FOPS_RW(buffer_size);
DEFINE_SEQ_FOPS_RW(stack_capture);
DEFINE_SEQ_FOPS_RW(session_id);
DEFINE_SEQ_FOPS_RW(global_enable);
DEFINE_SEQ_FOPS_RW(enable);

/*
 * ============================================================================
 * Configuration Management
 * ============================================================================
 */

/* Calculate start index for copying circular buffer, skipping oldest entries if shrinking */
static unsigned int calculate_circular_start_idx(unsigned int old_count,
						   unsigned int old_size,
						   unsigned int new_size,
						   unsigned int old_head)
{
	unsigned int skip_count;
	
	skip_count = (old_count > new_size) ? (old_count - new_size) : 0;
	
	/* Linear buffer (not yet full) */
	if (old_count < old_size)
		return skip_count;
	
	/* Wrapped buffer (circular) */
	return (old_head + skip_count) % old_size;
}

/* Helper: Copy events buffer, preserving oldest-to-newest order */
static unsigned int copy_events_buffer(struct mem_event *new_events,
					struct mem_event *old_events,
					unsigned int old_count, unsigned int old_size,
					unsigned int new_size, unsigned int old_head)
{
	unsigned int copy_count, start_idx, i, j;
	
	if (!new_events || !old_events)
		return 0;
	
	copy_count = min(old_count, new_size);
	if (copy_count == 0)
		return 0;
	
	start_idx = calculate_circular_start_idx(old_count, old_size, new_size, old_head);
	
	/* Copy entries and increment refcounts */
	for (i = 0; i < copy_count; i++) {
		j = (start_idx + i) % old_size;
		new_events[i] = old_events[j];
		if (new_events[i].priv)
			kref_get(&new_events[i].priv->kref);
	}
	
	return copy_count;
}

/* Helper: Copy history buffer, preserving oldest-to-newest order */
static unsigned int copy_hist_buffer(struct mem_hist_record *new_hist,
					struct mem_hist_record *old_hist,
					unsigned int old_count, unsigned int old_size,
					unsigned int new_size, unsigned int old_head)
{
	unsigned int copy_count, start_idx, i, j;
	
	if (!new_hist || !old_hist)
		return 0;
	
	copy_count = min(old_count, new_size);
	if (copy_count == 0)
		return 0;
	
	start_idx = calculate_circular_start_idx(old_count, old_size, new_size, old_head);
	
	/* Copy entries and increment refcounts */
	for (i = 0; i < copy_count; i++) {
		j = (start_idx + i) % old_size;
		new_hist[i] = old_hist[j];
		if (new_hist[i].priv)
			kref_get(&new_hist[i].priv->kref);
	}
	
	return copy_count;
}

/* Helper: Release all priv references in events buffer and free it */
static void free_events_buffer(struct mem_event *events, unsigned int count)
{
	unsigned int i;
	
	if (!events)
		return;
	
	for (i = 0; i < count; i++) {
		if (events[i].priv) {
			kref_put(&events[i].priv->kref, mem_priv_release);
			events[i].priv = NULL;
		}
	}
	kfree(events);
}

/* Helper: Release all priv references in history buffer and free it */
static void free_hist_buffer(struct mem_hist_record *records, unsigned int count)
{
	unsigned int i;
	
	if (!records)
		return;
	
	for (i = 0; i < count; i++) {
		if (records[i].priv) {
			kref_put(&records[i].priv->kref, mem_priv_release);
			records[i].priv = NULL;
		}
	}
	kfree(records);
}

int mem_tracker_set_buffer_size(struct mem_tracker *tracker, unsigned int size)
{
	struct mem_event *new_events, *old_events;
	struct mem_hist_record *new_hist, *old_hist;
	unsigned int old_count, copy_count;
	unsigned long flags;
	
	if (!tracker || size == 0 || size > 100000)
		return -EINVAL;
	
	/* Allocate new buffers */
	new_events = kcalloc(size, sizeof(*new_events), GFP_KERNEL);
	if (!new_events)
		return -ENOMEM;
	
	new_hist = kcalloc(size, sizeof(*new_hist), GFP_KERNEL);
	if (!new_hist) {
		kfree(new_events);
		return -ENOMEM;
	}
	
	/* Replace events buffer, preserving existing data */
	spin_lock_irqsave(&tracker->events_buffer.lock, flags);
	old_events = tracker->events_buffer.events;
	old_count = tracker->events_buffer.count;
	
	copy_count = copy_events_buffer(new_events, old_events, old_count,
					 tracker->events_buffer.size, size,
					 tracker->events_buffer.head);
	
	tracker->events_buffer.events = new_events;
	tracker->events_buffer.size = size;
	tracker->events_buffer.head = copy_count % size;
	tracker->events_buffer.count = copy_count;
	spin_unlock_irqrestore(&tracker->events_buffer.lock, flags);
	
	free_events_buffer(old_events, old_count);
	
	/* Replace history buffer, preserving existing data */
	spin_lock_irqsave(&tracker->hist_buffer.lock, flags);
	old_hist = tracker->hist_buffer.records;
	old_count = tracker->hist_buffer.count;
	
	copy_count = copy_hist_buffer(new_hist, old_hist, old_count,
					tracker->hist_buffer.size, size,
					tracker->hist_buffer.head);
	
	tracker->hist_buffer.records = new_hist;
	tracker->hist_buffer.size = size;
	tracker->hist_buffer.head = copy_count % size;
	tracker->hist_buffer.count = copy_count;
	spin_unlock_irqrestore(&tracker->hist_buffer.lock, flags);
	
	free_hist_buffer(old_hist, old_count);
	
	tracker->buffer_size = size;
	
	return 0;
}
EXPORT_SYMBOL_GPL(mem_tracker_set_buffer_size);

unsigned int mem_tracker_get_buffer_size(struct mem_tracker *tracker)
{
	return tracker ? tracker->buffer_size : 0;
}
EXPORT_SYMBOL_GPL(mem_tracker_get_buffer_size);

void mem_tracker_set_stack_capture(struct mem_tracker *tracker, unsigned int mode)
{
	if (tracker && mode <= MEM_TRACKER_STACK_SYMBOLIZED)
		tracker->stack_capture = mode;
}
EXPORT_SYMBOL_GPL(mem_tracker_set_stack_capture);

unsigned int mem_tracker_get_stack_capture(struct mem_tracker *tracker)
{
	return tracker ? tracker->stack_capture : 0;
}
EXPORT_SYMBOL_GPL(mem_tracker_get_stack_capture);

bool mem_tracker_is_enabled(struct mem_tracker *tracker)
{
	if (!tracker)
		return false;
	return atomic_read(&mem_tracker_enabled) != 0 &&
	       atomic_read(&tracker->enabled) != 0;
}
EXPORT_SYMBOL_GPL(mem_tracker_is_enabled);

/*
 * ============================================================================
 * Registration / Initialization
 * ============================================================================
 */

struct mem_tracker *mem_tracker_register(struct mem_tracker_config *config)
{
	struct mem_tracker *tracker;
	int ret;
	
	if (!config || !config->name || !config->ops)
		return ERR_PTR(-EINVAL);
	
	/* Allocate tracker */
	tracker = kzalloc(sizeof(*tracker), GFP_KERNEL);
	if (!tracker)
		return ERR_PTR(-ENOMEM);
	
	/* Initialize configuration */
	strncpy(tracker->name, config->name, sizeof(tracker->name) - 1);
	tracker->ops = config->ops;
	tracker->buffer_size = config->buffer_size ? : MEM_TRACKER_DEFAULT_BUFFER_SIZE;
	tracker->stack_capture = config->stack_capture;
	atomic_set(&tracker->enabled, config->enable ? 1 : 0);	

	/* Initialize lists, RB-tree, and locks */
	INIT_LIST_HEAD(&tracker->alloc_list);
	tracker->alloc_tree = RB_ROOT;
	spin_lock_init(&tracker->alloc_lock);
	
	/* Initialize statistics */
	atomic64_set(&tracker->total_allocated, 0);
	atomic64_set(&tracker->total_freed, 0);
	atomic64_set(&tracker->current_allocated, 0);
	atomic64_set(&tracker->peak_allocated, 0);
	atomic_set(&tracker->alloc_count, 0);
	atomic_set(&tracker->free_count, 0);
	
	/* Initialize allocation UID counter
	 * (0 means invalid/unknown, it will start from 1 because of atomic64_inc_return) */
	atomic64_set(&tracker->next_allocation_uid, 0);
	
	/* Initialize events buffer */
	tracker->events_buffer.events = kcalloc(tracker->buffer_size,
						sizeof(struct mem_event),
						GFP_KERNEL);
	if (!tracker->events_buffer.events) {
		ret = -ENOMEM;
		goto err_free_tracker;
	}
	tracker->events_buffer.size = tracker->buffer_size;
	tracker->events_buffer.head = 0;
	tracker->events_buffer.count = 0;
	spin_lock_init(&tracker->events_buffer.lock);
	
	/* Initialize history buffer */
	tracker->hist_buffer.records = kcalloc(tracker->buffer_size,
						sizeof(struct mem_hist_record),
						GFP_KERNEL);
	if (!tracker->hist_buffer.records) {
		ret = -ENOMEM;
		goto err_free_events;
	}
	tracker->hist_buffer.size = tracker->buffer_size;
	tracker->hist_buffer.head = 0;
	tracker->hist_buffer.count = 0;
	spin_lock_init(&tracker->hist_buffer.lock);
	
	/* Create debugfs directory */
	if (!mem_tracker_root) {
		mem_tracker_root = debugfs_create_dir("mem_trackers", NULL);
		
		/* Create files only if debugfs directory is available */
		if (mem_tracker_root) {
			/* Create session_id file at root level */
			debugfs_create_file("session_id", 0644, mem_tracker_root, NULL, &session_id_fops);
			
			/* Create enable file at root level */
			debugfs_create_file("enable", 0644, mem_tracker_root, NULL, &global_enable_fops);
		} else {
			pr_warn("mem_tracker: debugfs not available, continuing without debugfs interface\n");
		}
	}
	
	if (mem_tracker_root) {
		tracker->debugfs_dir = debugfs_create_dir(tracker->name, mem_tracker_root);
		/* Create files only if debugfs directory is available */
		if (tracker->debugfs_dir) {
			/* Create debugfs files */
			debugfs_create_file("summary", 0444, tracker->debugfs_dir, tracker, &summary_fops);
			debugfs_create_file("active", 0444, tracker->debugfs_dir, tracker, &active_brief_fops);
			debugfs_create_file("active_detailed", 0444, tracker->debugfs_dir, tracker, &active_detailed_fops);
			debugfs_create_file("events", 0444, tracker->debugfs_dir, tracker, &events_brief_fops);
			debugfs_create_file("events_detailed", 0444, tracker->debugfs_dir, tracker, &events_detailed_fops);
			debugfs_create_file("history", 0444, tracker->debugfs_dir, tracker, &history_brief_fops);
			debugfs_create_file("history_detailed", 0444, tracker->debugfs_dir, tracker, &history_detailed_fops);
			debugfs_create_file("buffer_size", 0644, tracker->debugfs_dir, tracker, &buffer_size_fops);
			debugfs_create_file("stack_capture", 0644, tracker->debugfs_dir, tracker, &stack_capture_fops);
			debugfs_create_file("enable", 0644, tracker->debugfs_dir, tracker, &enable_fops);

			pr_info("Memory tracker '%s' registered at /sys/kernel/debug/mem_trackers/%s/\n",
				tracker->name, tracker->name);
		} else {
			pr_warn("mem_tracker: Failed to create debugfs directory for '%s', continuing without debugfs interface\n",
				tracker->name);
		}
	}
	
	tracker->initialized = true;
	
	return tracker;

err_free_events:
	kfree(tracker->events_buffer.events);
err_free_tracker:
	kfree(tracker);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(mem_tracker_register);

void mem_tracker_unregister(struct mem_tracker *tracker)
{
	struct mem_alloc_record *rec, *tmp;
	unsigned long flags;
	unsigned int i;
	
	if (!tracker)
		return;
	
	tracker->initialized = false;
	
	/* Remove debugfs entries (if debugfs was available) */
	if (tracker->debugfs_dir)
		debugfs_remove_recursive(tracker->debugfs_dir);
	
	/* Free all active allocations */
	spin_lock_irqsave(&tracker->alloc_lock, flags);
	list_for_each_entry_safe(rec, tmp, &tracker->alloc_list, list) {
		list_del(&rec->list);
		remove_alloc_from_tree(tracker, rec);
		if (rec->priv) {
			kref_put(&rec->priv->kref, mem_priv_release);
			rec->priv = NULL;
		}
		kfree(rec);
		rec = NULL;
	}
	spin_unlock_irqrestore(&tracker->alloc_lock, flags);
	
	/* Release all priv references in event buffer */
	for (i = 0; i < tracker->events_buffer.count; i++) {
		if (tracker->events_buffer.events[i].priv) {
			kref_put(&tracker->events_buffer.events[i].priv->kref,
				 mem_priv_release);
			tracker->events_buffer.events[i].priv = NULL;
		}
	}
	
	/* Release all priv references in history buffer */
	for (i = 0; i < tracker->hist_buffer.count; i++) {
		if (tracker->hist_buffer.records[i].priv) {
			kref_put(&tracker->hist_buffer.records[i].priv->kref,
				 mem_priv_release);
			tracker->hist_buffer.records[i].priv = NULL;
		}
	}
	
	/* Free buffers */
	kfree(tracker->events_buffer.events);
	kfree(tracker->hist_buffer.records);
	
	pr_info("Memory tracker '%s' unregistered\n", tracker->name);
	
	kfree(tracker);
}
EXPORT_SYMBOL_GPL(mem_tracker_unregister);

MODULE_AUTHOR("Hailo");
MODULE_DESCRIPTION("Generic Memory Tracker Framework");
MODULE_LICENSE("GPL v2");

