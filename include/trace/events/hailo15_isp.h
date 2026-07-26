/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM hailo15_isp

#if !defined(_TRACE_HAILO15_ISP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO15_ISP_H

#include <linux/tracepoint.h>
#include <linux/dma-mapping.h>

DECLARE_EVENT_CLASS(hailo15_isp_raw_buffer_class,
	TP_PROTO(u32 sensor_index, u32 buffer_index, dma_addr_t buffer_address),
	TP_ARGS(sensor_index, buffer_index, buffer_address),

	TP_STRUCT__entry(
		__field(u32, sensor_index)
		__field(u32, buffer_index)
		__field(u64, buffer_address)
	),

	TP_fast_assign(
		__entry->sensor_index = sensor_index;
		__entry->buffer_index = buffer_index;
		__entry->buffer_address = buffer_address;
	),

	TP_printk("sensor=%u, index=%u, addr=0x%llx",
		  __entry->sensor_index,
		  __entry->buffer_index,
		  __entry->buffer_address
	)
)

DEFINE_EVENT(hailo15_isp_raw_buffer_class, isp_raw_buffer_empty_q_in,
	TP_PROTO(u32 sensor_index, u32 buffer_index, dma_addr_t buffer_address),
	TP_ARGS(sensor_index, buffer_index, buffer_address)
);

DEFINE_EVENT(hailo15_isp_raw_buffer_class, isp_raw_buffer_empty_q_out,
	TP_PROTO(u32 sensor_index, u32 buffer_index, dma_addr_t buffer_address),
	TP_ARGS(sensor_index, buffer_index, buffer_address)
);

DEFINE_EVENT(hailo15_isp_raw_buffer_class, isp_raw_buffer_full_q_in,
	TP_PROTO(u32 sensor_index, u32 buffer_index, dma_addr_t buffer_address),
	TP_ARGS(sensor_index, buffer_index, buffer_address)
);

DEFINE_EVENT(hailo15_isp_raw_buffer_class, isp_raw_buffer_full_q_out,
	TP_PROTO(u32 sensor_index, u32 buffer_index, dma_addr_t buffer_address),
	TP_ARGS(sensor_index, buffer_index, buffer_address)
);

DECLARE_EVENT_CLASS(hailo15_isp_output_buffer_class,
	TP_PROTO(u32 buffer_index, u32 grp_id, dma_addr_t buffer_address),
	TP_ARGS(buffer_index, grp_id, buffer_address),

	TP_STRUCT__entry(
		__field(u32, buffer_index)
		__field(u32, grp_id)
		__field(u64, buffer_address)
	),

	TP_fast_assign(
		__entry->buffer_index = buffer_index;
		__entry->grp_id = grp_id;
		__entry->buffer_address = buffer_address;
	),

	TP_printk("index=%u, grp_id=%u, addr=0x%llx",
		  __entry->buffer_index,
		  __entry->grp_id,
		  __entry->buffer_address
	)
);

DEFINE_EVENT(hailo15_isp_output_buffer_class, isp_output_buffer_process,
	TP_PROTO(u32 buffer_index, u32 grp_id, dma_addr_t buffer_address),
	TP_ARGS(buffer_index, grp_id, buffer_address)
);

/* MCM raw write buffer flow traces */
DECLARE_EVENT_CLASS(hailo15_isp_mcm_raw_wr_buffer_class,
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

DEFINE_EVENT(hailo15_isp_mcm_raw_wr_buffer_class, isp_mcm_raw_wr_buffer_process,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u64 timestamp),
	TP_ARGS(grp_id, buffer_index, buffer_address, timestamp)
);

DEFINE_EVENT(hailo15_isp_mcm_raw_wr_buffer_class, isp_mcm_raw_wr_buffer_done,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u64 timestamp),
	TP_ARGS(grp_id, buffer_index, buffer_address, timestamp)
);

/* Simple event trace class - for events that only need grp_id and timestamp */
DECLARE_EVENT_CLASS(hailo15_isp_simple_event_class,
	TP_PROTO(u32 grp_id, u64 timestamp),
	TP_ARGS(grp_id, timestamp),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u64, timestamp)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->timestamp = timestamp;
	),

	TP_printk("grp_id=%u, ts=%llu",
		  __entry->grp_id,
		  __entry->timestamp
	)
);

/* Simplified frame end IRQ trace - only grp_id and timestamp */
DEFINE_EVENT(hailo15_isp_simple_event_class, isp_mcm_raw_wr_frame_end_irq,
	TP_PROTO(u32 grp_id, u64 timestamp),
	TP_ARGS(grp_id, timestamp)
);

/* Work started processing trace */
DEFINE_EVENT(hailo15_isp_simple_event_class, isp_mcm_raw_wr_frame_irq_work,
	TP_PROTO(u32 grp_id, u64 timestamp),
	TP_ARGS(grp_id, timestamp)
);

/* MCM raw write buffer_done with null cur_buf trace */
DEFINE_EVENT(hailo15_isp_simple_event_class, isp_mcm_raw_wr_buffer_done_null,
	TP_PROTO(u32 grp_id, u64 timestamp),
	TP_ARGS(grp_id, timestamp)
);

/* MCM IN buffer_done gated off — MP queue empty and stall flag set. */
DEFINE_EVENT(hailo15_isp_simple_event_class, isp_mcm_in_no_mp_stall,
	TP_PROTO(u32 grp_id, u64 timestamp),
	TP_ARGS(grp_id, timestamp)
);

/* MCM_IN buffer initialization and release traces */
DECLARE_EVENT_CLASS(hailo15_isp_mcm_in_buffer_class,
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

DEFINE_EVENT(hailo15_isp_mcm_in_buffer_class, isp_mcm_in_buffer_queue,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u64 timestamp),
	TP_ARGS(grp_id, buffer_index, buffer_address, timestamp)
);

DEFINE_EVENT(hailo15_isp_mcm_in_buffer_class, isp_mcm_in_buffer_dequeue,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u64 timestamp),
	TP_ARGS(grp_id, buffer_index, buffer_address, timestamp)
);

/* MCM_RAW_OUT buffer queue and dequeue traces */
DEFINE_EVENT(hailo15_isp_mcm_raw_wr_buffer_class, isp_mcm_raw_wr_buffer_queue,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u64 timestamp),
	TP_ARGS(grp_id, buffer_index, buffer_address, timestamp)
);

DEFINE_EVENT(hailo15_isp_mcm_raw_wr_buffer_class, isp_mcm_raw_wr_buffer_dequeue,
	TP_PROTO(u32 grp_id, u32 buffer_index, dma_addr_t buffer_address, u64 timestamp),
	TP_ARGS(grp_id, buffer_index, buffer_address, timestamp)
);

/* Queue empty state transition traces */
DECLARE_EVENT_CLASS(hailo15_isp_queue_empty_class,
	TP_PROTO(u32 grp_id, u64 timestamp),
	TP_ARGS(grp_id, timestamp),

	TP_STRUCT__entry(
		__field(u32, grp_id)
		__field(u64, timestamp)
	),

	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->timestamp = timestamp;
	),

	TP_printk("grp_id=%u, ts=%llu",
		  __entry->grp_id,
		  __entry->timestamp
	)
);

DEFINE_EVENT(hailo15_isp_queue_empty_class, isp_queue_empty_enter,
	TP_PROTO(u32 grp_id, u64 timestamp),
	TP_ARGS(grp_id, timestamp)
);

DEFINE_EVENT(hailo15_isp_queue_empty_class, isp_queue_empty_exit,
	TP_PROTO(u32 grp_id, u64 timestamp),
	TP_ARGS(grp_id, timestamp)
);

#endif /* if !defined(_TRACE_HAILO15_ISP_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
