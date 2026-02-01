
#if !defined(_TRACE_HAILO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hailo15_video_out

#include <linux/tracepoint.h>
#include <linux/timekeeping.h>
#include "common.h"

// hailo15-vid-out tracepoints
// Traces can be defined using a blueprint (class) for the tracepoint invocation
// and then instantiated with a specific event name.
DECLARE_EVENT_CLASS(hailo15_buffer_vidout_event_class,
	TP_PROTO(struct hailo15_buffer* hb, struct hailo15_video_out_node* vid_node),
	TP_ARGS(hb, vid_node),
	TP_STRUCT__entry(
		__string(device_name, vid_node->video_dev->name)
		__field(u64, grp_id)
		__field(u64, dq_timestamp)
		__field(u64, queue_sequence)
		__field(u64, ev_timestamp)
		__field(u64, dma_address)
	),
	TP_fast_assign(
		__assign_str(device_name, vid_node->video_dev->name);
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

//Invoked in hailo15 driver buffer_queue
DEFINE_EVENT(hailo15_buffer_vidout_event_class, hailo_buffer_queued,
	TP_PROTO(struct hailo15_buffer* hb, struct hailo15_video_out_node* vid_node),
	TP_ARGS(hb, vid_node)
);

//Invoked in hailo15 driver buffer_queue
DEFINE_EVENT(hailo15_buffer_vidout_event_class, hailo_buffer_dequeued,
	TP_PROTO(struct hailo15_buffer* hb, struct hailo15_video_out_node* vid_node),
	TP_ARGS(hb, vid_node)
);

//Invoked in hailo15 irq_work_handle (in IRQ work queue context)
DEFINE_EVENT(hailo15_buffer_vidout_event_class, hailo_buffer_done,
	TP_PROTO(struct hailo15_buffer* hb, struct hailo15_video_out_node* vid_node),
	TP_ARGS(hb, vid_node)
);


DECLARE_EVENT_CLASS(hailo15_vidout_event_class,
	TP_PROTO(struct hailo15_video_out_node* vid_node),
	TP_ARGS(vid_node),
	TP_STRUCT__entry(
		__string(device_name, vid_node->video_dev->name)
		__field(u64, ev_timestamp)
	),
	TP_fast_assign(
		__assign_str(device_name, vid_node->video_dev->name);
		__entry->ev_timestamp = ktime_get_ns();
	),
	TP_printk("dev=%s ev=%llu",
		__get_str(device_name),
		__entry->ev_timestamp
	)
)

DEFINE_EVENT(hailo15_vidout_event_class, hailo15_vidout_start_streaming,
	TP_PROTO(struct hailo15_video_out_node* vid_node),
	TP_ARGS(vid_node)
);

DEFINE_EVENT(hailo15_vidout_event_class, hailo15_vidout_stop_streaming,
	TP_PROTO(struct hailo15_video_out_node* vid_node),
	TP_ARGS(vid_node)
);

DEFINE_EVENT(hailo15_vidout_event_class, hailo15_vidout_start_streaming_error,
	TP_PROTO(struct hailo15_video_out_node* vid_node),
	TP_ARGS(vid_node)
);

#endif /* _TRACE_HAILO_H */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE hailo15-vid-out-traces /* this file */

#include <trace/define_trace.h>
