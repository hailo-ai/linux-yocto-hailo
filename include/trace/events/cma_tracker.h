/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM cma_tracker

#if !defined(_TRACE_CMA_TRACKER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CMA_TRACKER_H

#include <linux/tracepoint.h>

TRACE_EVENT(cma_heap_stats,

	TP_PROTO(const char *heap_name, unsigned long total_pages,
		 unsigned long used_pages, unsigned long free_pages,
		 unsigned long max_chunk_pages),

	TP_ARGS(heap_name, total_pages, used_pages, free_pages, max_chunk_pages),

	TP_STRUCT__entry(
		__string(heap_name, heap_name)
		__field(unsigned long, total_pages)
		__field(unsigned long, used_pages)
		__field(unsigned long, free_pages)
		__field(unsigned long, max_chunk_pages)
	),

	TP_fast_assign(
		__assign_str(heap_name, heap_name);
		__entry->total_pages = total_pages;
		__entry->used_pages = used_pages;
		__entry->free_pages = free_pages;
		__entry->max_chunk_pages = max_chunk_pages;
	),

	TP_printk("heap_name=%s total=%lu used=%lu free=%lu max_chunk=%lu",
		__get_str(heap_name), __entry->total_pages, __entry->used_pages,
		__entry->free_pages, __entry->max_chunk_pages)
);

#endif /* _TRACE_CMA_TRACKER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

