#ifndef __HAILO15_EVENTS_H
#define __HAILO15_EVENTS_H

#define HAILO15_VIDEO_EVENT_RESOURCE_SIZE 4096
#define HAILO15_DEAMON_VIDEO_EVENT (V4L2_EVENT_PRIVATE_START + 1000)
#define HAILO15_DAEMON_ISP_EVENT (V4L2_EVENT_PRIVATE_START + 2000)

enum HAILO15_video_private_event_id {
	HAILO15_DAEMON_VIDEO_EVENT_CREATE_PIPELINE = 0,
	HAILO15_DAEMON_VIDEO_EVENT_DESTROY_PIPELINE,
	HAILO15_DAEMON_VIDEO_EVENT_MAX
};

enum HAILO15_isp_private_event_id {
	HAILO15_DAEMON_ISP_EVENT_SET_FMT = 0,
	HAILO15_DAEMON_ISP_EVENT_REQBUFS,
	HAILO15_DAEMON_ISP_EVENT_QBUF,
	HAILO15_DAEMON_ISP_EVENT_BUF_DONE,
	HAILO15_DAEMON_ISP_EVENT_STREAMON,
	HAILO15_DAEMON_ISP_EVENT_STREAMOFF,
	HAILO15_DAEMON_ISP_EVENT_S_CTRL,
	HAILO15_DAEMON_ISP_EVENT_G_CTRL,
	HAILO15_DAEMON_ISP_EVENT_MAX,
};

typedef struct hailo15_daemon_event_meta {
	unsigned int event_type;
	unsigned int event_id;
} hailo15_daemon_event_meta_t;

enum hailo15_video_bayer_e {
	BAYER_RGGB = 0,
	BAYER_GRBG = 1,
	BAYER_GBRG = 2,
	BAYER_BGGR = 3,
	BAYER_BUTT
};
struct hailo15_video_caps_size {
	uint32_t bounds_width;
	uint32_t bounds_height;
	uint32_t top;
	uint32_t left;
	uint32_t width;
	uint32_t height;
};

struct hailo15_video_caps_mode {
	uint32_t index;
	struct hailo15_video_caps_size size;
	uint32_t hdr_mode;
	uint32_t stitching_mode;
	uint32_t bit_width;
	uint32_t bayer_pattern;
	uint32_t mipi_lanes;
	uint32_t fps;
};

struct hailo15_video_caps_enummode {
	int index;
	struct hailo15_video_caps_mode mode;
};

struct hailo15_isp_ctrl {
	uint32_t cid;
	uint32_t size;
	char data[0];
};

struct hailo15_video_event_data {
	uint32_t pad;
	uint8_t dev;
	uint32_t event_id;
	uint64_t phy_addr;
	uint64_t size;
	uint32_t data_size;
};

struct hailo15_video_event_shm {
	uint32_t complete;
	int32_t result;
	char data[0];
};

#endif
