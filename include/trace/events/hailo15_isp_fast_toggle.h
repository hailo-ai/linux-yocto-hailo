/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM hailo15_isp_fast_toggle

#if !defined(_TRACE_HAILO15_ISP_FAST_TOGGLE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO15_ISP_FAST_TOGGLE_H

#include <linux/tracepoint.h>

/* ISP fast toggle operation traces */
TRACE_EVENT(isp_fast_toggle_stream_start,
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

	TP_printk("grp_id=%u, toggle_type=%u (ISP fast toggle stream starting)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(isp_fast_toggle_set_format_start,
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

	TP_printk("grp_id=%u, toggle_type=%u (Calling fast_toggle_set_format)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(isp_fast_toggle_post_event_set_fmt_start,
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

	TP_printk("grp_id=%u, toggle_type=%u (Calling hailo15_isp_post_event_set_fmt)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(isp_fast_toggle_post_event_set_fmt_complete,
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

	TP_printk("grp_id=%u, toggle_type=%u, result=%d (hailo15_isp_post_event_set_fmt %s)",
		  __entry->grp_id,
		  __entry->toggle_type,
		  __entry->result,
		  __entry->result == 0 ? "SUCCESS" : "FAILED"
	)
);

TRACE_EVENT(isp_fast_toggle_subdev_set_fmt,
	TP_PROTO(u32 grp_id, u32 toggle_type, const char *subdev_name),
	TP_ARGS(grp_id, toggle_type, subdev_name),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
		__string(subdev_name, subdev_name)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
		__assign_str(subdev_name, subdev_name);
	),

	TP_printk("grp_id=%u, toggle_type=%u, subdev=%s (Calling subdev set_fmt)",
		  __entry->grp_id,
		  __entry->toggle_type,
		  __get_str(subdev_name)
	)
);

TRACE_EVENT(isp_fast_toggle_mcm_in_start,
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

	TP_printk("grp_id=%u, toggle_type=%u (Starting MCM in)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

TRACE_EVENT(isp_fast_toggle_apply_prev_reqbufs_start,
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

	TP_printk("grp_id=%u, toggle_type=%u (Calling hailo15_isp_apply_prev_reqbufs)",
		  __entry->grp_id,
		  __entry->toggle_type
	)
);

#endif /* if !defined(_TRACE_HAILO15_ISP_FAST_TOGGLE_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
