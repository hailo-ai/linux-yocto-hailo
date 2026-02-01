
#if !defined(_TRACE_HAILO_VIDEO_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HAILO_VIDEO_EVENTS_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hailo15_video_event

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


#endif /* _TRACE_HAILO_H */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE hailo15-video-events-traces /* this file */
#include <trace/define_trace.h>
