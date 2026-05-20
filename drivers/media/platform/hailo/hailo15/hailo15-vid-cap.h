#ifndef _HAILO_VIDEO_H_
#define _HAILO_VIDEO_H_

#include <linux/list.h>
#include <linux/spinlock.h>

#include <linux/videodev2.h>
#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>

#include <linux/dma-buf.h>
#include <media/media-entity.h>
#include "common.h"
#include "hailo15-events.h"

#define MAX_NUM_FORMATS (8)
#define MAX_SUBDEVS_NUM (8)
#define MAX_VIDEO_NODE_NUM (64)
#define CONFIG_HAILO_DEVICE_TPG
#define CONFIG_HAILO_DEVICE_TPG_USE_FILE

struct dev_node {
	struct device_node *node;
	int id;
};

struct hailo15_video_node {
	struct device *dev;
	int id;
	struct video_device *video_dev;
	struct v4l2_device *v4l2_dev;
	struct v4l2_async_subdev *asd;
	struct v4l2_subdev *direct_sd;
	struct media_device *mdev;
	struct v4l2_ctrl *ctrl;
	struct media_pad pad;
	struct v4l2_format fmt;
	struct v4l2_rect crop, compose;
	struct hailo15_video_fmt formats[MAX_NUM_FORMATS];
	int formatscount;

	struct hailo15_event_resource event_resource;

	struct mutex qlock;
	struct list_head buf_queue; /* protected by qlock*/
	bool skip_first_list_entry;

	struct vb2_queue queue;
	struct mutex buffer_mutex;

	struct hailo15_buffer *prev_buf; /* protected by qlock*/

	struct hailo15_buf_ctx buf_ctx;
	struct mutex ioctl_mutex;

	int streaming;
	bool queue_accepting_buffers; /* Prevents buffer queuing during stream stop/cleanup */
	int path;
	int sequence;
	int pipeline_init;
	int hdr_timestamp_mode;
	u8 tuning_state;
	bool is_first_buffer_processed; /* If false, vid-cap didn't pass (enqueued) any buf to subdev yet */
	wait_queue_head_t stream_wait;

	enum fast_toggle_state fast_toggle_state;
	struct hailo15_buffer *fast_toggle_priming_buf; // on fast toggle, one buffer must be available immediately - this is it
	bool drop_prev_buf; // fast toggle: drop stale prev_buf instead of returning to userspace
};

struct hailo15_vid_cap_device {
	struct device *dev;
	struct media_device mdev;
	struct v4l2_async_notifier subdev_notifier;
	struct hailo15_video_node *vid_nodes[MAX_VIDEO_NODE_NUM];
};

struct hailo15_get_vsm_params {
	int index;
	struct hailo15_vsm vsm;
};

#define queue_to_node(__q) container_of(__q, struct hailo15_video_node, queue)

int hailo15_video_post_event_create_pipeline(struct hailo15_video_node *vid_node);
int hailo15_video_post_event_release_pipeline(struct hailo15_video_node *vid_node);

int hailo15_video_post_event_create_pipeline_fast_toggle(struct hailo15_video_node *vid_node);
int hailo15_video_post_event_release_pipeline_fast_toggle(struct hailo15_video_node *vid_node);

#endif /*_HAILO_VIDEO_H */
