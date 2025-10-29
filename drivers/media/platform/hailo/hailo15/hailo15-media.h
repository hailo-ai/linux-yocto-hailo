#ifndef __HAILO15_MEDIA_H
#define __HAILO15_MEDIA_H
#include <linux/of_graph.h>
#include <linux/property.h>
#include <linux/of_platform.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-async.h>
#include <stdbool.h>
#include "common.h"

struct hailo15_media_device {
	struct v4l2_subdev* sd;
	struct fwnode_handle* endpoint;
	struct list_head link;
};

struct hailo15_media_connection {
	struct hailo15_media_device* sink;
	struct hailo15_media_device* source;
	struct list_head connection;
};

int hailo15_media_create_links(struct device* dev, struct media_entity* entity, int id);
int hailo15_media_get_sink_endpoints_status(struct device* dev);
int hailo15_media_create_connections(struct device* dev, struct v4l2_subdev* sd);
int hailo15_media_register_v4l2_device(struct v4l2_device* v4l2_dev, int id);
void hailo15_media_init_media_device(struct device* dev);
struct media_device* hailo15_media_get_media_device(void);
int hailo15_media_get_subdev(struct device *dev, int id, struct v4l2_subdev **sd);
int hailo15_media_register_video_subdev_nodes(struct v4l2_device *video_dev);
void hailo15_media_entity_clean(struct media_entity *entity);
void hailo15_media_clean_media_device(void);
int hailo15_media_device_initialized(void);
#endif

