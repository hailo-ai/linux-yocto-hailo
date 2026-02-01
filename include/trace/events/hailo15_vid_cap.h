/* SPDX-License-Identifier: GPL-2.0 */

#if !defined(_TRACE_HAILO15_VID_CAP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO15_VID_CAP_H

#include <linux/tracepoint.h>
#include <linux/dma-mapping.h>

/* Video capture buffer initialization and release traces */
DECLARE_EVENT_CLASS(hailo15_vid_cap_buffer_class,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u64 timestamp),
	TP_ARGS(grp_id, buffer_index, buffer_address, timestamp),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, buffer_index)
		__field(u64, buffer_address)
		__field(u64, timestamp)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->buffer_index = buffer_index;
		__entry->buffer_address = buffer_address;
		__entry->timestamp = timestamp;
	),

	TP_printk("grp_id=%u, index=%u, addr=0x%llx, ts=%llu",
		  __entry->grp_id,
		  __entry->buffer_index,
		  __entry->buffer_address,
		  __entry->timestamp
	)
);

DEFINE_EVENT(hailo15_vid_cap_buffer_class, vid_cap_buffer_queue,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u64 timestamp),
	TP_ARGS(grp_id, buffer_index, buffer_address, timestamp)
);

DEFINE_EVENT(hailo15_vid_cap_buffer_class, vid_cap_buffer_dequeue,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u64 timestamp),
	TP_ARGS(grp_id, buffer_index, buffer_address, timestamp)
);

#endif /* if !defined(_TRACE_HAILO15_VID_CAP_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
