#ifndef __HAILO15_EVENTS_H
#define __HAILO15_EVENTS_H

#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#define HAILO15_VIDEO_EVENT_RESOURCE_SIZE 4096
#define HAILO15_DEAMON_VIDEO_EVENT (V4L2_EVENT_PRIVATE_START + 1000)
#define HAILO15_DAEMON_ISP_EVENT   (V4L2_EVENT_PRIVATE_START + 2000)
#define HAILO15_ISP_IRQ_EVENT      (V4L2_EVENT_PRIVATE_START + 3000)

#define HAILO15_UEVENT_ISP_STAT    HAILO15_DAEMON_ISP_EVENT

enum hailo15_event_stat_id {
    HAILO15_UEVENT_ISP_EXP_STAT,
    HAILO15_UEVENT_ISP_EXPV2_STAT,
    HAILO15_UEVENT_ISP_HIST_STAT,
    HAILO15_UEVENT_ISP_AWB_STAT,
    HAILO15_UEVENT_ISP_AFM_STAT,
    HAILO15_UEVENT_SENSOR_DATALOSS_STAT,
    HAILO15_UEVENT_VSM_DONE_STAT,
    HAILO15_UEVENT_SENSOR_STREAMING_STAT,
    HAILO15_UEVENT_ISP_STAT_MAX
};

struct hailo15_af_kevent {
	wait_queue_head_t wait_q;
	struct mutex data_lock;
	uint32_t sum_a;
	uint32_t sum_b;
	uint32_t sum_c;
	uint32_t lum_a;
	uint32_t lum_b;
	uint32_t lum_c;
	int ready;
	int vdid;
};

extern struct hailo15_af_kevent af_kevent;

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
	HAILO15_DAEMON_ISP_EVENT_S_SELECTION,
	HAILO15_DAEMON_ISP_EVENT_FAST_TOGGLE,
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
	char data[];
};


#endif
