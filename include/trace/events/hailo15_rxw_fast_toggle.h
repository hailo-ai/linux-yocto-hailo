/* SPDX-License-Identifier: GPL-2.0 */
/* TRACE_SYSTEM must be defined before including this file */

#if !defined(TRACE_SYSTEM)
#error "TRACE_SYSTEM must be defined before including this file"
#endif

#if !defined(_TRACE_HAILO15_RXW_FAST_TOGGLE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO15_RXW_FAST_TOGGLE_H

#include <linux/tracepoint.h>

/* RXWrapper fast toggle operation traces */
TRACE_EVENT(rxw_fast_toggle_set_status,
	TP_PROTO(u32 grp_id, u32 toggle_type, u32 toggle_state),
	TP_ARGS(grp_id, toggle_type, toggle_state),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, toggle_type)
		__field(u32, toggle_state)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->toggle_type = toggle_type;
		__entry->toggle_state = toggle_state;
	),

	TP_printk("grp_id=%u, toggle_type=%u, state=%u (RXWrapper fast_toggle_set_status)",
		  __entry->grp_id,
		  __entry->toggle_type,
		  __entry->toggle_state
	)
);

TRACE_EVENT(rxw_apply_set_pad_format,
	TP_PROTO(u32 grp_id, u32 num_exposures),
	TP_ARGS(grp_id, num_exposures),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u32, num_exposures)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->num_exposures = num_exposures;
	),

	TP_printk("grp_id=%u, num_exposures=%u (RXWrapper applying set_pad_format with DOL%u)",
		  __entry->grp_id,
		  __entry->num_exposures,
		  __entry->num_exposures
	)
);

TRACE_EVENT(rxw_set_subdev_sensor_format,
	TP_PROTO(u32 grp_id, const char *subdev_name),
	TP_ARGS(grp_id, subdev_name),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__string(subdev_name, subdev_name)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__assign_str(subdev_name, subdev_name);
	),

	TP_printk("grp_id=%u, subdev=%s (RXWrapper calling subdev set_fmt)",
		  __entry->grp_id,
		  __get_str(subdev_name)
	)
);

TRACE_EVENT(rxw_apply_set_stream,
	TP_PROTO(u32 grp_id, int enable),
	TP_ARGS(grp_id, enable),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(int, enable)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->enable = enable;
	),

	TP_printk("grp_id=%u, enable=%d (RXWrapper %s stream via p2a)",
		  __entry->grp_id,
		  __entry->enable,
		  __entry->enable ? "STARTING" : "STOPPING"
	)
);

#endif /* if !defined(_TRACE_HAILO15_RXW_FAST_TOGGLE_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/events
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE hailo15_rxw_fast_toggle
#include <trace/define_trace.h>
