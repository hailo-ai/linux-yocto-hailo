/* SPDX-License-Identifier: GPL-2.0 */
/* Common Hailo15 trace events - TRACE_SYSTEM must be defined before including this file */

#if !defined(TRACE_SYSTEM)
#error "TRACE_SYSTEM must be defined before including hailo15_events.h"
#endif

#if !defined(_TRACE_HAILO15_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO15_EVENTS_H

#include <linux/tracepoint.h>
#include "../../drivers/media/platform/hailo/hailo15/common.h"
#include <trace/events/hailo15_events_map.h>

//Invoked in hailo15 driver buffer_queue
TRACE_EVENT(hailo_buffer_queued,
	TP_PROTO(struct hailo15_buffer *hb),
	TP_ARGS(hb),
	TP_STRUCT__entry(
		__string(device_name, hb->sd->name)
		__field(u64, grp_id)
		__field(u64, dq_timestamp)
		__field(u64, queue_sequence)
		__field(u64, ev_timestamp)
		__field(u64, dma_address)
	),
	TP_fast_assign(
		__assign_str(device_name, hb->sd->name);
		__entry->grp_id = hb->grp_id;
		__entry->dq_timestamp = hb->vb.vb2_buf.timestamp;
		__entry->queue_sequence = hb->queue_sequence;
		__entry->ev_timestamp = ktime_get_ns();
		__entry->dma_address = hb->dma[0];
	),
	TP_printk("dev=%s grp_id=0x%llx irq_ts=%llu qs=%llu ev=%llu dma=0x%016llx",
		__get_str(device_name),
		__entry->grp_id,
		__entry->dq_timestamp,
		__entry->queue_sequence,
		__entry->ev_timestamp,
		__entry->dma_address
	)
);

//Invoked in hailo15_irq_work_enqueue (still in interrupt context)
TRACE_EVENT(hailo_buffer_deferred,
	TP_PROTO(struct hailo15_buffer *hb),
	TP_ARGS(hb),
	TP_STRUCT__entry(
		__string(device_name, hb->sd->name)
		__field(u64, grp_id)
		__field(u64, dq_timestamp)
		__field(u64, queue_sequence)
		__field(u64, ev_timestamp)
		__field(u64, dma_address)
	),
	TP_fast_assign(
		__assign_str(device_name, hb->sd->name);
		__entry->grp_id = hb->grp_id;
		__entry->dq_timestamp = hb->vb.vb2_buf.timestamp;
		__entry->queue_sequence = hb->queue_sequence;
		__entry->ev_timestamp = ktime_get_ns();
		__entry->dma_address = hb->dma[0];
	),
	TP_printk("dev=%s grp_id=0x%llx irq_ts=%llu qs=%llu ev=%llu dma=0x%016llx",
		__get_str(device_name),
		__entry->grp_id,
		__entry->dq_timestamp,
		__entry->queue_sequence,
		__entry->ev_timestamp,
		__entry->dma_address
	)
);

//Invoked from hailo15 buffer_done and from set_stream
TRACE_EVENT(hailo_buffer_hw,
	TP_PROTO(struct hailo15_buffer *hb),
	TP_ARGS(hb),
	TP_STRUCT__entry(
		__string(device_name, hb->sd->name)
		__field(u64, grp_id)
		__field(u64, dq_timestamp)
		__field(u64, queue_sequence)
		__field(u64, ev_timestamp)
		__field(u64, dma_address)
	),
	TP_fast_assign(
		__assign_str(device_name, hb->sd->name);
		__entry->grp_id = hb->grp_id;
		__entry->dq_timestamp = hb->vb.vb2_buf.timestamp;
		__entry->queue_sequence = hb->queue_sequence;
		__entry->ev_timestamp = ktime_get_ns();
		__entry->dma_address = hb->dma[0];
	),
	TP_printk("dev=%s grp_id=0x%llx irq_ts=%llu qs=%llu ev=%llu dma=0x%016llx",
		__get_str(device_name),
		__entry->grp_id,
		__entry->dq_timestamp,
		__entry->queue_sequence,
		__entry->ev_timestamp,
		__entry->dma_address
	)
);

TRACE_EVENT(hailo15_driver_error_raw,
	TP_PROTO(const char *name, int error_code, const char *error_msg),
	TP_ARGS(name, error_code, error_msg),

	TP_STRUCT__entry(
		__string(name, name)
		__field(int, error_code)
		__string(error_msg, error_msg)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->error_code = error_code;
		__assign_str(error_msg, error_msg);
	),

	TP_printk("name=%s error_code=%d error_msg=%s",
		__get_str(name),
		__entry->error_code,
		__get_str(error_msg)
	)
);

TRACE_EVENT(hailo15_driver_s_stream,
	TP_PROTO(const char *name),
	TP_ARGS(name),

	TP_STRUCT__entry(
		__string(name, name)
	),

	TP_fast_assign(
		__assign_str(name, name);
	),

	TP_printk("driver=%s",
		__get_str(name)
	)
);

TRACE_EVENT(hailo15_driver_t_stream,
	TP_PROTO(const char *name),
	TP_ARGS(name),

	TP_STRUCT__entry(
		__string(name, name)
	),

	TP_fast_assign(
		__assign_str(name, name);
	),

	TP_printk("driver=%s",
		__get_str(name)
	)
);

#endif /* if !defined(_TRACE_HAILO15_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ) */

#define TRACE_INCLUDE_PATH trace/events
#define TRACE_INCLUDE_FILE hailo15_events

/* This part must be outside protection */
#include <trace/define_trace.h>
