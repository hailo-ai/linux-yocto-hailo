#ifndef __HAILO15_ISP_EVENTS_H__
#define __HAILO15_ISP_EVENTS_H__

#include "hailo15-events.h"


struct hailo15_isp_event_pkg_head {
	uint32_t pad;
	uint8_t dev;
	uint32_t event_id;
	uint64_t phy_addr;
	uint32_t size;
	uint32_t data_size;
};

struct hailo15_isp_event_pkg {
	struct hailo15_isp_event_pkg_head head;
	uint8_t complete;
	int32_t result;
	uint8_t data[];
};

int hailo15_isp_post_event(struct video_device *vdev,
				 hailo15_daemon_event_meta_t event_meta,
				 struct hailo15_event_resource *event_resource,
				 int pad, void *data, size_t data_size);

void hailo15_isp_event_reset_seq(struct hailo15_event_resource *event_resource);


#endif /* __HAILO15_ISP_EVENTS_H__ */
