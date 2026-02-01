/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM hailo15_vid_cap_fast_toggle

#if !defined(_TRACE_HAILO15_VID_CAP_FAST_TOGGLE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO15_VID_CAP_FAST_TOGGLE_H

#include <linux/tracepoint.h>

/* Video Capture fast toggle operation traces */
TRACE_EVENT(vid_cap_fast_toggle_priming_start,
	TP_PROTO(u32 grp_id),
	TP_ARGS(grp_id),

	TP_STRUCT__entry(
		__field(u32, grp_id)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
	),

	TP_printk("grp_id=%u (FAST_TOGGLE_PRIMING ioctl received)",
		  __entry->grp_id
	)
);

TRACE_EVENT(vid_cap_fast_toggle_priming_complete,
	TP_PROTO(u32 grp_id, int result),
	TP_ARGS(grp_id, result),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(int, result)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->result = result;
	),

	TP_printk("grp_id=%u, result=%d (FAST_TOGGLE_PRIMING operation %s)",
		  __entry->grp_id,
		  __entry->result,
		  __entry->result == 0 ? "SUCCESS" : "FAILED"
	)
);

TRACE_EVENT(vid_cap_fast_toggle_start,
	TP_PROTO(u32 grp_id, u32 toggle_type),
	TP_ARGS(grp_id, toggle_type),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
	),

	TP_printk("grp_id=%u, toggle_type=%u (VIDEO_FAST_TOGGLE ioctl received)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(vid_cap_fast_toggle_complete,
	TP_PROTO(u32 grp_id, u32 toggle_type, int result),
	TP_ARGS(grp_id, toggle_type, result),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
		__field(int, result)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
		__entry->result = result;
	),

	TP_printk("grp_id=%u, toggle_type=%u, result=%d (VIDEO_FAST_TOGGLE operation %s)",
		  __entry->grp_id,
		  __entry->toggle_type,
		  __entry->result,
		  __entry->result == 0 ? "SUCCESS" : "FAILED"
	)
);

/* Detailed fast toggle steps */
TRACE_EVENT(vid_cap_fast_toggle_teardown,
	TP_PROTO(u32 grp_id, u32 toggle_type),
	TP_ARGS(grp_id, toggle_type),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
	),

	TP_printk("grp_id=%u, toggle_type=%u (Starting teardown)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(vid_cap_fast_toggle_stream_off,
	TP_PROTO(u32 grp_id, u32 toggle_type),
	TP_ARGS(grp_id, toggle_type),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
	),

	TP_printk("grp_id=%u, toggle_type=%u (Stream off)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(vid_cap_fast_toggle_release_pipeline,
	TP_PROTO(u32 grp_id, u32 toggle_type),
	TP_ARGS(grp_id, toggle_type),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
	),

	TP_printk("grp_id=%u, toggle_type=%u (Release pipeline)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(vid_cap_fast_toggle_apply_priming,
	TP_PROTO(u32 grp_id, u32 toggle_type),
	TP_ARGS(grp_id, toggle_type),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
	),

	TP_printk("grp_id=%u, toggle_type=%u (Apply priming)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(vid_cap_fast_toggle_create_pipeline,
	TP_PROTO(u32 grp_id, u32 toggle_type),
	TP_ARGS(grp_id, toggle_type),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
	),

	TP_printk("grp_id=%u, toggle_type=%u (Create pipeline)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(vid_cap_fast_toggle_stream_on,
	TP_PROTO(u32 grp_id, u32 toggle_type),
	TP_ARGS(grp_id, toggle_type),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
	),

	TP_printk("grp_id=%u, toggle_type=%u (Stream on)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

/* Fast toggle priming buffer lifecycle traces */
TRACE_EVENT(vid_cap_fast_toggle_priming_buf_saved,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address),
	TP_ARGS(grp_id, buffer_index, buffer_address),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, buffer_index)
		__field(u64, buffer_address)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->buffer_index = buffer_index;
		__entry->buffer_address = buffer_address;
	),

	TP_printk("grp_id=%u, index=%u, addr=0x%llx (Priming buffer saved in hailo15_buffer_queue)",
		  __entry->grp_id,
		  __entry->buffer_index,
		  __entry->buffer_address
	)
);

TRACE_EVENT(vid_cap_fast_toggle_priming_buf_released,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address),
	TP_ARGS(grp_id, buffer_index, buffer_address),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, buffer_index)
		__field(u64, buffer_address)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->buffer_index = buffer_index;
		__entry->buffer_address = buffer_address;
	),

	TP_printk("grp_id=%u, index=%u, addr=0x%llx (Priming buffer released in hailo15_video_node_queue_clean)",
		  __entry->grp_id,
		  __entry->buffer_index,
		  __entry->buffer_address
	)
);

TRACE_EVENT(vid_cap_fast_toggle_priming_buf_used,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u32 toggle_type),
	TP_ARGS(grp_id, buffer_index, buffer_address, toggle_type),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, buffer_index)
		__field(u64, buffer_address)
		__field(u32, toggle_type)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->buffer_index = buffer_index;
		__entry->buffer_address = buffer_address;
		__entry->toggle_type = toggle_type;
	),

	TP_printk("grp_id=%u, index=%u, addr=0x%llx, toggle_type=%u (Priming buffer used in do_fast_toggle)",
		  __entry->grp_id,
		  __entry->buffer_index,
		  __entry->buffer_address,
		  __entry->toggle_type
	)
);

TRACE_EVENT(vid_cap_fast_toggle_priming_buf_error,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address),
	TP_ARGS(grp_id, buffer_index, buffer_address),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, buffer_index)
		__field(u64, buffer_address)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->buffer_index = buffer_index;
		__entry->buffer_address = buffer_address;
	),

	TP_printk("grp_id=%u, index=%u, addr=0x%llx (Priming buffer still exists after error, returning to user)",
		  __entry->grp_id,
		  __entry->buffer_index,
		  __entry->buffer_address
	)
);

#endif /* if !defined(_TRACE_HAILO15_VID_CAP_FAST_TOGGLE_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
