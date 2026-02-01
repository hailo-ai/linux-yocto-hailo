/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mem_tracker

#if !defined(_TRACE_MEM_TRACKER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MEM_TRACKER_H

#include <linux/tracepoint.h>

#define MEM_TRACKER_MAX_STACK_DEPTH 16

TRACE_EVENT(mem_tracker_event,

	TP_PROTO(const char *tracker_name, const char *event_type,
		 unsigned long address, size_t size, u64 allocation_uid,
		 const char *process, int pid, u64 timestamp,
		 const unsigned long *raw_stack_entries, unsigned int nr_raw_stack_entries,
		 const char *symbolized_stack_trace, const char *memory_pool),

	TP_ARGS(tracker_name, event_type, address, size, allocation_uid, process, pid, timestamp,
		raw_stack_entries, nr_raw_stack_entries, symbolized_stack_trace, memory_pool),

	TP_STRUCT__entry(
		__string(tracker_name, tracker_name)
		__string(event_type, event_type)
		__field(unsigned long, address)
		__field(size_t, size)
		__field(u64, allocation_uid)
		__string(process, process)
		__field(int, pid)
		__field(u64, timestamp)
		__array(unsigned long, raw_stack_entries, MEM_TRACKER_MAX_STACK_DEPTH)
		__field(unsigned int, nr_raw_stack_entries)
		__string(symbolized_stack_trace, symbolized_stack_trace)
		__string(memory_pool, memory_pool)
	),

	TP_fast_assign(
		__assign_str(tracker_name, tracker_name);
		__assign_str(event_type, event_type);
		__entry->address = address;
		__entry->size = size;
		__entry->allocation_uid = allocation_uid;
		__assign_str(process, process);
		__entry->pid = pid;
		__entry->timestamp = timestamp;
		__entry->nr_raw_stack_entries = nr_raw_stack_entries;
		/* Always populate array (even if empty) so ftrace serializes it */
		{
			unsigned int i;
			if (raw_stack_entries && nr_raw_stack_entries > 0) {
				unsigned int copy_count = nr_raw_stack_entries < MEM_TRACKER_MAX_STACK_DEPTH ? nr_raw_stack_entries : MEM_TRACKER_MAX_STACK_DEPTH;
				for (i = 0; i < copy_count; i++) {
					__entry->raw_stack_entries[i] = raw_stack_entries[i];
				}
				/* Zero out remaining array elements to ensure clean serialization */
				for (i = copy_count; i < MEM_TRACKER_MAX_STACK_DEPTH; i++)
					__entry->raw_stack_entries[i] = 0;
			} else {
				/* Zero out entire array if not populated */
				for (i = 0; i < MEM_TRACKER_MAX_STACK_DEPTH; i++)
					__entry->raw_stack_entries[i] = 0;
			}
		}
		__assign_str(symbolized_stack_trace, symbolized_stack_trace ? symbolized_stack_trace : "");
		__assign_str(memory_pool, memory_pool);
	),

	TP_printk("[%s] type=%s addr=0x%lx size=%zu uid=%llu %s[%d] ts=%llu raw_stack_entries=%u memory_pool=%s",
		__get_str(tracker_name), __get_str(event_type),
		__entry->address, __entry->size, __entry->allocation_uid,
		__get_str(process), __entry->pid, __entry->timestamp,
		__entry->nr_raw_stack_entries, __get_str(memory_pool))
);

TRACE_EVENT(mem_tracker_hist_event,

	TP_PROTO(const char *tracker_name, unsigned long address, size_t size,
		 u64 allocation_uid, const char *alloc_process, int alloc_pid,
		 u64 alloc_timestamp, const unsigned long *alloc_raw_stack_entries,
		 unsigned int nr_alloc_raw_stack_entries, const char *alloc_symbolized_stack_trace,
		 const char *memory_pool, bool freed, const char *free_process, int free_pid,
		 u64 free_timestamp, const unsigned long *free_raw_stack_entries,
		 unsigned int nr_free_raw_stack_entries, const char *free_symbolized_stack_trace),

	TP_ARGS(tracker_name, address, size, allocation_uid, alloc_process, alloc_pid,
		alloc_timestamp, alloc_raw_stack_entries, nr_alloc_raw_stack_entries, alloc_symbolized_stack_trace,
		memory_pool, freed, free_process, free_pid, free_timestamp, free_raw_stack_entries,
		nr_free_raw_stack_entries, free_symbolized_stack_trace),

	TP_STRUCT__entry(
		__string(tracker_name, tracker_name)
		__field(unsigned long, address)
		__field(size_t, size)
		__field(u64, allocation_uid)
		__string(alloc_process, alloc_process)
		__field(int, alloc_pid)
		__field(u64, alloc_timestamp)
		__array(unsigned long, alloc_raw_stack_entries, MEM_TRACKER_MAX_STACK_DEPTH)
		__field(unsigned int, nr_alloc_raw_stack_entries)
		__string(alloc_symbolized_stack_trace, alloc_symbolized_stack_trace)
		__string(memory_pool, memory_pool)
		__field(bool, freed)
		__string(free_process, free_process)
		__field(int, free_pid)
		__field(u64, free_timestamp)
		__array(unsigned long, free_raw_stack_entries, MEM_TRACKER_MAX_STACK_DEPTH)
		__field(unsigned int, nr_free_raw_stack_entries)
		__string(free_symbolized_stack_trace, free_symbolized_stack_trace)
	),

	TP_fast_assign(
		__assign_str(tracker_name, tracker_name);
		__entry->address = address;
		__entry->size = size;
		__entry->allocation_uid = allocation_uid;
		__assign_str(alloc_process, alloc_process);
		__entry->alloc_pid = alloc_pid;
		__entry->alloc_timestamp = alloc_timestamp;
		__entry->nr_alloc_raw_stack_entries = nr_alloc_raw_stack_entries;
		{
			unsigned int i;
			if (alloc_raw_stack_entries && nr_alloc_raw_stack_entries > 0) {
				unsigned int copy_count = nr_alloc_raw_stack_entries < MEM_TRACKER_MAX_STACK_DEPTH ? nr_alloc_raw_stack_entries : MEM_TRACKER_MAX_STACK_DEPTH;
				for (i = 0; i < copy_count; i++)
					__entry->alloc_raw_stack_entries[i] = alloc_raw_stack_entries[i];
				/* Zero out remaining array elements */
				for (i = copy_count; i < MEM_TRACKER_MAX_STACK_DEPTH; i++)
					__entry->alloc_raw_stack_entries[i] = 0;
			} else {
				/* Zero out entire array if not populated */
				for (i = 0; i < MEM_TRACKER_MAX_STACK_DEPTH; i++)
					__entry->alloc_raw_stack_entries[i] = 0;
			}
		}
		__assign_str(alloc_symbolized_stack_trace, alloc_symbolized_stack_trace ? alloc_symbolized_stack_trace : "");
		__assign_str(memory_pool, memory_pool);
		__entry->freed = freed;
		__assign_str(free_process, free_process);
		__entry->free_pid = free_pid;
		__entry->free_timestamp = free_timestamp;
		__entry->nr_free_raw_stack_entries = nr_free_raw_stack_entries;
		{
			unsigned int i;
			if (free_raw_stack_entries && nr_free_raw_stack_entries > 0) {
				unsigned int copy_count = nr_free_raw_stack_entries < MEM_TRACKER_MAX_STACK_DEPTH ? nr_free_raw_stack_entries : MEM_TRACKER_MAX_STACK_DEPTH;
				for (i = 0; i < copy_count; i++)
					__entry->free_raw_stack_entries[i] = free_raw_stack_entries[i];
				/* Zero out remaining array elements */
				for (i = copy_count; i < MEM_TRACKER_MAX_STACK_DEPTH; i++)
					__entry->free_raw_stack_entries[i] = 0;
			} else {
				/* Zero out entire array if not populated */
				for (i = 0; i < MEM_TRACKER_MAX_STACK_DEPTH; i++)
					__entry->free_raw_stack_entries[i] = 0;
			}
		}
		__assign_str(free_symbolized_stack_trace, free_symbolized_stack_trace ? free_symbolized_stack_trace : "");
	),

	TP_printk("[%s] addr=0x%lx size=%zu uid=%llu alloc=%s[%d]@%llu alloc_raw_stack_entries=%u memory_pool=%s freed=%d free=%s[%d]@%llu free_raw_stack_entries=%u",
		__get_str(tracker_name), __entry->address, __entry->size, __entry->allocation_uid,
		__get_str(alloc_process), __entry->alloc_pid, __entry->alloc_timestamp,
		__entry->nr_alloc_raw_stack_entries, __get_str(memory_pool),
		__entry->freed, __get_str(free_process), __entry->free_pid,
		__entry->free_timestamp, __entry->nr_free_raw_stack_entries)
);

#endif /* _TRACE_MEM_TRACKER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

