
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

/* Diagnostic events emitted when QBUF or buffer_done exceeds a latency
 * threshold. */
TRACE_EVENT(hailo15_slow_qbuf,
	TP_PROTO(struct hailo15_video_out_node *vid_node,
		 unsigned int index, s64 elapsed_ms),
	TP_ARGS(vid_node, index, elapsed_ms),
	TP_STRUCT__entry(
		__string(device_name, vid_node->video_dev->name)
		__field(int, path)
		__field(unsigned int, index)
		__field(s64, elapsed_ms)
	),
	TP_fast_assign(
		__assign_str(device_name, vid_node->video_dev->name);
		__entry->path = vid_node->path;
		__entry->index = index;
		__entry->elapsed_ms = elapsed_ms;
	),
	TP_printk("dev=%s path=%d index=%u elapsed=%lld ms",
		__get_str(device_name),
		__entry->path,
		__entry->index,
		__entry->elapsed_ms
	)
);

TRACE_EVENT(hailo15_slow_buffer_done,
	TP_PROTO(struct hailo15_buffer *buf, int grp_id,
		 s64 elapsed_ms, ktime_t now),
	TP_ARGS(buf, grp_id, elapsed_ms, now),
	TP_STRUCT__entry(
		__field(int, grp_id)
		__field(unsigned int, index)
		__field(s64, elapsed_ms)
		__field(u64, qbuf_start)
		__field(u64, fe_switch_start)
		__field(u64, fe_switch_end)
		__field(u64, rdma_ready)
		__field(u64, frame_end)
		__field(u64, now)
	),
	TP_fast_assign(
		__entry->grp_id = grp_id;
		__entry->index = buf->vb.vb2_buf.index;
		__entry->elapsed_ms = elapsed_ms;
		__entry->qbuf_start = ktime_to_ns(buf->timing.qbuf_start);
		__entry->fe_switch_start = ktime_to_ns(buf->timing.fe_switch_start);
		__entry->fe_switch_end = ktime_to_ns(buf->timing.fe_switch_end);
		__entry->rdma_ready = ktime_to_ns(buf->timing.rdma_ready);
		__entry->frame_end = ktime_to_ns(buf->timing.frame_end);
		__entry->now = ktime_to_ns(now);
	),
	TP_printk("grp_id=%d index=%u elapsed=%lld ms"
		  " qbuf_start=%llu fe_switch_start=%llu fe_switch_end=%llu"
		  " rdma_ready=%llu frame_end=%llu now=%llu",
		__entry->grp_id,
		__entry->index,
		__entry->elapsed_ms,
		__entry->qbuf_start,
		__entry->fe_switch_start,
		__entry->fe_switch_end,
		__entry->rdma_ready,
		__entry->frame_end,
		__entry->now
	)
);

#endif /* _TRACE_HAILO_H */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE hailo15-vid-out-traces /* this file */

#include <trace/define_trace.h>
