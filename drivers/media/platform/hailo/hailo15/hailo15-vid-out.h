#ifndef _HAILO_VIDEO_OUT_H_
#define _HAILO_VIDEO_OUT_H_

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

#define MAX_NUM_FORMATS (8)
#define MAX_SUBDEVS_NUM (2)
#define MAX_VIDEO_NODE_NUM (2)

struct dev_node {
	struct device_node *node;
	int id;
};

struct hailo15_video_out_node {
	struct device *dev;
	int id;
	struct video_device *video_dev;
	struct v4l2_device *v4l2_dev;
	struct v4l2_subdev *direct_sd;
	struct media_device *mdev;
	struct v4l2_ctrl *ctrl;
	struct media_pad pad;
	struct v4l2_format fmt;
	struct v4l2_rect crop, compose;
	struct hailo15_video_fmt formats[MAX_NUM_FORMATS];
	int formatscount;

	spinlock_t qlock;
	struct list_head buf_queue;

	struct vb2_queue queue;
	struct mutex buffer_mutex;

	struct hailo15_buf_ctx buf_ctx;
	struct mutex ioctl_mutex;

	int streaming;
	int path;
	int sequence;
	int reqbufs; 
};

struct hailo15_vid_out_device {
	struct device *dev;
	struct media_device mdev;
	struct v4l2_async_notifier subdev_notifier;
	struct hailo15_video_out_node *vid_nodes[MAX_VIDEO_NODE_NUM];
	struct list_head subdevs;
};

#define queue_to_node(__q) container_of(__q, struct hailo15_video_out_node, queue)

#endif /*_HAILO_VIDEO_OUT_H */
