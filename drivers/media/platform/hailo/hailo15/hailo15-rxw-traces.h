#undef TRACE_SYSTEM
#define TRACE_SYSTEM hailo15_rxw

#if !defined(_TRACE_HAILO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO_H

#include <linux/tracepoint.h>
#include <linux/timekeeping.h>
#include "common.h"

// hailo video events tracepoints
// Tracks event mechanism between driver and user space
// Invoked in hailo15_video_event_post
// Wrapped by trace_hailo_video_event_fmt to format an arbitrary message
TRACE_EVENT(hailo_video_event,
	TP_PROTO(struct video_device *vdev, const char* msg),
	TP_ARGS(vdev, msg),
	TP_STRUCT__entry(
		__string(device_name, vdev->name)
		__string(msg, msg)
	),
	TP_fast_assign(
		__assign_str(device_name, vdev->name);
		__assign_str(msg, msg);
	),
	TP_printk("dev=[%s]: %s", __get_str(device_name), __get_str(msg))
);

// rx_wrapper (RXW) tracepoints
// Traces can be defined using a blueprint (class) for the tracepoint invocation
// and then instantiated with a specific event name.
DECLARE_EVENT_CLASS(hailo_rx_event_class,
	TP_PROTO(struct hailo15_buffer* hb),
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
)

//Invoked in hailo15_rxwrapper_buffer_queue
DEFINE_EVENT(hailo_rx_event_class, hailo_buffer_queued,
	TP_PROTO(struct hailo15_buffer* hb),
	TP_ARGS(hb)
);

//Invoked in hailo15_irq_work_enqueue (still in interrupt context)
DEFINE_EVENT(hailo_rx_event_class, hailo_buffer_deferred,
	TP_PROTO(struct hailo15_buffer* hb),
	TP_ARGS(hb)
);

//Invoked in hailo15_rxwrapper_irq_work_handle (in IRQ work queue context)
DEFINE_EVENT(hailo_rx_event_class, hailo_buffer_done_sdr,
	TP_PROTO(struct hailo15_buffer* hb),
	TP_ARGS(hb)
);

//Invoked in hailo15_rxwrapper_irq_work_handle (in IRQ work queue context)
DEFINE_EVENT(hailo_rx_event_class, hailo_buffer_done_hdr,
	TP_PROTO(struct hailo15_buffer* hb),
	TP_ARGS(hb)
);

//Invoked  from hailo15_rxwrapper_buffer_done and from set_stream
DEFINE_EVENT(hailo_rx_event_class, hailo_buffer_hw,
	TP_PROTO(struct hailo15_buffer* hb),
	TP_ARGS(hb)
);

DEFINE_EVENT(hailo_rx_event_class, hailo_buffer_tick,
	TP_PROTO(struct hailo15_buffer* hb),
	TP_ARGS(hb)
);



#endif /* _TRACE_HAILO_H */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE hailo15-rxw-traces /* this file */

#include <trace/define_trace.h>
