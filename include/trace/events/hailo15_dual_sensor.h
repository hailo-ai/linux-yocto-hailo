/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM hailo15_dual_sensor

#if !defined(_TRACE_HAILO15_DUAL_SENSOR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO15_DUAL_SENSOR_H

#include <linux/tracepoint.h>

/* Full queue count change event */
DECLARE_EVENT_CLASS(hailo15_dual_sensor_full_q_count_class,
	TP_PROTO(u32 sensor, s32 old_count, s32 new_count),
	TP_ARGS(sensor, old_count, new_count),

	TP_STRUCT__entry(
		__field(u32, sensor)
		__field(s32, old_count)
		__field(s32, new_count)
	),

	TP_fast_assign(
		__entry->sensor = sensor;
		__entry->old_count = old_count;
		__entry->new_count = new_count;
	),

	TP_printk("sensor=%u, old_count=%d, new_count=%d",
		  __entry->sensor,
		  __entry->old_count,
		  __entry->new_count
	)
);

DEFINE_EVENT(hailo15_dual_sensor_full_q_count_class, hailo15_dual_sensor_full_q_count_inc,
	TP_PROTO(u32 sensor, s32 old_count, s32 new_count),
	TP_ARGS(sensor, old_count, new_count)
);

DEFINE_EVENT(hailo15_dual_sensor_full_q_count_class, hailo15_dual_sensor_full_q_count_dec,
	TP_PROTO(u32 sensor, s32 old_count, s32 new_count),
	TP_ARGS(sensor, old_count, new_count)
);

/* Buffer drop event */
TRACE_EVENT(hailo15_dual_sensor_buffers_dropped,
	TP_PROTO(u32 sensor, u32 dropped_count, s32 count_before, s32 count_after, s32 count_other_sensor),
	TP_ARGS(sensor, dropped_count, count_before, count_after, count_other_sensor),

	TP_STRUCT__entry(
		__field(u32, sensor)
		__field(u32, dropped_count)
		__field(s32, count_before)
		__field(s32, count_after)
		__field(s32, count_other_sensor)
	),

	TP_fast_assign(
		__entry->sensor = sensor;
		__entry->dropped_count = dropped_count;
		__entry->count_before = count_before;
		__entry->count_after = count_after;
		__entry->count_other_sensor = count_other_sensor;
	),

	TP_printk("sensor=%u, dropped=%u, count_before=%d, count_after=%d, count_other=%d",
		  __entry->sensor,
		  __entry->dropped_count,
		  __entry->count_before,
		  __entry->count_after,
		  __entry->count_other_sensor
	)
);

#endif /* if !defined(_TRACE_HAILO15_DUAL_SENSOR_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>

