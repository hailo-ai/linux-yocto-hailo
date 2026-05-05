/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Generic Memory Tracker Framework
 *
 * Provides a common infrastructure for tracking memory allocations
 * across different memory subsystems (CMA, DMA, vmalloc, etc.)
 */

#ifndef _LINUX_MEM_TRACKER_H
#define _LINUX_MEM_TRACKER_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/seq_file.h>

/* MEM_TRACKER_MAX_STACK_DEPTH is defined in the trace mem_tracker.h */
#include <trace/events/mem_tracker.h>

#define MEM_TRACKER_DEFAULT_BUFFER_SIZE 1000

/* Forward declarations */
struct mem_tracker;
struct mem_alloc_record;
struct mem_event;
struct mem_hist_record;

/*
 * Event types for circular buffer
 */
enum mem_event_type {
	MEM_EVENT_ALLOC,
	MEM_EVENT_FREE
};

/**
 * enum mem_tracker_stack_mode - Stack trace capture and formatting modes
 * @MEM_TRACKER_STACK_DISABLED: No stack capture (best performance)
 * @MEM_TRACKER_STACK_RAW: Capture raw addresses (small Perfetto traces, allows post-processing)
 * @MEM_TRACKER_STACK_SYMBOLIZED: Capture symbolized names (large traces, human-readable)
 *
 * Controls both whether stacks are captured and how they're formatted in Perfetto traces.
 * Debugfs output (events_detailed) always shows symbolized stacks when capture is enabled.
 * Configurable via: /sys/kernel/debug/mem_trackers/<name>/stack_capture
 */
enum mem_tracker_stack_mode {
	MEM_TRACKER_STACK_DISABLED = 0,
	MEM_TRACKER_STACK_RAW = 1,
	MEM_TRACKER_STACK_SYMBOLIZED = 2
};

/*
 * Private data wrapper with reference counting
 * Uses kernel's standard kref API for reference counting.
 */
struct mem_priv {
	struct kref kref;     /* Kernel's standard refcounting */
	void *data;           /* Type-specific data */
	void (*free_priv)(void *);  /* Callback to free type-specific data */
};

/*
 * Generic allocation record (active allocations)
 * 
 * Contains common fields for all memory types.
 * Type-specific implementations provide their own private data.
 */
struct mem_alloc_record {
	struct list_head list;		/* For chronological iteration */
	struct rb_node rb_node;		/* For O(log n) address lookup */
	
	/* Common fields */
	unsigned long address;		/* Start address/PFN */
	size_t size;			/* Size in bytes */
	u64 allocation_uid;		/* Unique identifier for this allocation */
	
	/* Process context */
	pid_t tgid;			/* Process ID */
	pid_t tid;			/* Thread ID */
	char comm[TASK_COMM_LEN];	/* Process name */
	
	/* Timing */
	u64 timestamp;			/* Allocation timestamp (ns) */
	
	/* Call stack */
	unsigned long stack_entries[MEM_TRACKER_MAX_STACK_DEPTH];
	unsigned int nr_stack_entries;
	
	/* Private data wrapper (refcount + type-specific data) */
	struct mem_priv *priv;
	
	/* Pointer to corresponding history record (for O(1) update on free) */
	struct mem_hist_record *hist_record;
};

/* Helper macro for type-safe priv access in format callbacks */
#define MEM_TRACKER_PRIV(record, type) ((type *)((record)->priv->data))

/*
 * Generic event record (circular buffer of alloc/free events)
 */
struct mem_event {
	enum mem_event_type type;
	
	/* Common fields */
	unsigned long address;
	size_t size;
	u64 allocation_uid;		/* Unique identifier for this allocation */
	
	/* Process context */
	pid_t tgid;
	pid_t tid;
	char comm[TASK_COMM_LEN];
	
	/* Timing */
	u64 timestamp;
	
	/* Call stack */
	unsigned long stack_entries[MEM_TRACKER_MAX_STACK_DEPTH];
	unsigned int nr_stack_entries;
	
	/* Private data wrapper (shared with mem_alloc_record) */
	struct mem_priv *priv;
};

/*
 * Generic history record (full lifecycle tracking)
 */
struct mem_hist_record {
	/* Common fields */
	unsigned long address;
	size_t size;
	u64 allocation_uid;		/* Unique identifier for this allocation */
	
	/* Allocation context */
	pid_t alloc_tgid;
	pid_t alloc_tid;
	char alloc_comm[TASK_COMM_LEN];
	u64 alloc_timestamp;
	
	/* Free context */
	bool freed;
	pid_t free_tgid;
	pid_t free_tid;
	char free_comm[TASK_COMM_LEN];
	u64 free_timestamp;
	
	/* Allocation call stack */
	unsigned long alloc_stack[MEM_TRACKER_MAX_STACK_DEPTH];
	unsigned int nr_alloc_stack;
	
	/* Free call stack */
	unsigned long free_stack[MEM_TRACKER_MAX_STACK_DEPTH];
	unsigned int nr_free_stack;
	
	/* Private data wrapper (shared with mem_alloc_record) */
	struct mem_priv *priv;
};

/*
 * Callback operations for type-specific implementations
 *
 * These callbacks are invoked by the generic layer to format
 * type-specific data for display via seq_file.
 * 
 * IMPORTANT: alloc_priv/free_priv manage ONLY the type-specific data.
 *            The infrastructure handles reference counting automatically.
 *            
 *            All three record types (mem_alloc_record, mem_event, mem_hist_record)
 *            share the same priv instance via mem_priv wrapper.
 *            
 *            free_priv is called by infrastructure only when refcount reaches 0.
 */
struct mem_tracker_ops {
	/* Size of type-specific private data structure */
	size_t priv_size;
	
	/* Allocate/free type-specific private data (infrastructure will wrap it) */
	void *(*alloc_priv)(void);
	void (*free_priv)(void *priv_data);
	
	/* Format type-specific summary information */
	void (*format_summary)(struct seq_file *m, struct mem_tracker *tracker);
	
	/* Format active allocation record */
	void (*format_active_brief)(struct seq_file *m, struct mem_alloc_record *rec);
	void (*format_active_detailed)(struct seq_file *m, struct mem_alloc_record *rec);
	
	/* Format event */
	void (*format_event_brief)(struct seq_file *m, struct mem_event *event);
	void (*format_event_detailed)(struct seq_file *m, struct mem_event *event);
	
	/* Format history record */
	void (*format_history_brief)(struct seq_file *m, struct mem_hist_record *hist);
	void (*format_history_detailed)(struct seq_file *m, struct mem_hist_record *hist);
	
	/* Get display name from private data (optional, for tracepoint)
	 * @priv_data: Pointer to type-specific private data (not the mem_priv wrapper)
	 * Returns: Display name string, or NULL to use tracker name
	 */
	const char *(*get_display_name)(void *priv_data);
};

/*
 * Configuration for memory tracker
 */
struct mem_tracker_config {
	const char *name;		/* Tracker name (e.g., "cma", "dma") */
	unsigned int buffer_size;	/* Size of circular buffers */
	unsigned int stack_capture;	/* 0=off, 1=raw addresses, 2=symbolized */
	struct mem_tracker_ops *ops;	/* Type-specific operations */
	bool enable;
};

/*
 * Main memory tracker structure (opaque to users)
 */
struct mem_tracker {
	/* Configuration */
	char name[32];
	struct mem_tracker_ops *ops;
	
	/* Active allocations: list for chronological order, RB-tree for fast lookup */
	struct list_head alloc_list;	/* For chronological iteration */
	struct rb_root alloc_tree;	/* For O(log n) address lookup */
	spinlock_t alloc_lock;
	
	/* Statistics */
	atomic64_t total_allocated;
	atomic64_t total_freed;
	atomic64_t current_allocated;
	atomic64_t peak_allocated;
	atomic_t alloc_count;
	atomic_t free_count;
	
	/* Unique ID generation */
	atomic64_t next_allocation_uid;

	/* Session tracking */
	u64 last_seen_session_id;
	
	/* Circular buffer for events */
	struct {
		struct mem_event *events;
		unsigned int size;
		unsigned int head;
		unsigned int count;
		spinlock_t lock;
	} events_buffer;
	
	/* Circular buffer for history */
	struct {
		struct mem_hist_record *records;
		unsigned int size;
		unsigned int head;
		unsigned int count;
		spinlock_t lock;
	} hist_buffer;
	
	/* Configuration */
	unsigned int buffer_size;
	unsigned int stack_capture;	/* 0=off, 1=raw addresses, 2=symbolized */
	atomic_t enabled;		/* Per-tracker enable flag */
	bool initialized;
	
	/* Debugfs */
	struct dentry *debugfs_dir;
};

/*
 * Public API
 */

/* Register a new memory tracker */
struct mem_tracker *mem_tracker_register(struct mem_tracker_config *config);

/* Unregister and free a memory tracker */
void mem_tracker_unregister(struct mem_tracker *tracker);

/* Track an allocation */
int mem_tracker_alloc(struct mem_tracker *tracker,
		      unsigned long address,
		      size_t size,
		      void *priv);

/* Track a deallocation */
int mem_tracker_free(struct mem_tracker *tracker,
		     unsigned long address,
		     size_t size,
		     void *priv);

/* Find allocation record by address */
struct mem_alloc_record *mem_tracker_find_by_address(struct mem_tracker *tracker,
						      unsigned long address);

/* Configuration updates */
int mem_tracker_set_buffer_size(struct mem_tracker *tracker, unsigned int size);
unsigned int mem_tracker_get_buffer_size(struct mem_tracker *tracker);
void mem_tracker_set_stack_capture(struct mem_tracker *tracker, unsigned int mode);
unsigned int mem_tracker_get_stack_capture(struct mem_tracker *tracker);

/* Is memory tracket enabled/disabled */
bool mem_tracker_is_enabled(struct mem_tracker *tracker);

#endif /* _LINUX_MEM_TRACKER_H */

