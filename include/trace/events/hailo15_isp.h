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

#endif /* if !defined(_TRACE_HAILO15_ISP_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
