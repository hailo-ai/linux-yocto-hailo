/* SPDX-License-Identifier: GPL-2.0 */
/* TRACE_SYSTEM must be defined before including this file */

#if !defined(TRACE_SYSTEM)
#error "TRACE_SYSTEM must be defined before including this file"
#endif

#if !defined(_TRACE_HAILO_RXWRAPPER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO_RXWRAPPER_H

#include <linux/tracepoint.h>
#include <linux/timekeeping.h>
#include "../../drivers/media/platform/hailo/hailo15/common.h"

/* Forward declarations */
struct hailo15_buffer;

//Invoked in hailo15 irq_work_handle (in IRQ work queue context)
TRACE_EVENT(hailo_buffer_done_sdr,
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

//Invoked in hailo15 irq_work_handle (in IRQ work queue context)
TRACE_EVENT(hailo_buffer_done_hdr,
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


#endif /* _TRACE_HAILO_RXWRAPPER_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/events
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE hailo15_rxw_traces
#include <trace/define_trace.h>
