#ifndef __HAILO15_ISP_DRIVER__
#define __HAILO15_ISP_DRIVER__

#include <linux/clk.h>
#include <linux/spinlock.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include "common.h"

/* We double the needed queue size for safety */
#define HAILO15_ISP_EVENT_QUEUE_SIZE (250 * 2)

struct isp_mcm_buf {
	uint32_t port;
	uint32_t path;
	uint32_t num_planes;
	uint64_t addr[3];
	uint32_t size[3];
};

enum hailo15_sink_pads {
	HAILO15_ISP_SINK_PAD_S0,
	HAILO15_ISP_SINK_PAD_S1,
	HAILO15_ISP_SINK_PAD_MAX
};

enum hailo15_source_pads {
	HAILO15_ISP_SOURCE_PAD_MP_S0 = HAILO15_ISP_SINK_PAD_MAX,
	HAILO15_ISP_SOURCE_PAD_MP_S1,
	HAILO15_ISP_SOURCE_PAD_SP_S0,
	HAILO15_ISP_SOURCE_PAD_SP_S1,
	HAILO15_ISP_SOURCE_PAD_MAX,
};

#define HAILO15_ISP_PADS_NR (HAILO15_ISP_SOURCE_PAD_MAX)
#define HAILO15_ISP_SOURCE_PADS_NR                                             \
	(HAILO15_ISP_SOURCE_PAD_MAX - HAILO15_ISP_SINK_PAD_MAX)
#define HAILO15_ISP_SINK_PADS_NR (HAILO15_ISP_SINK_PAD_MAX)

enum { ISPIOC_S_MIV_INFO = 0x107,
       ISPIOC_S_MIS_IRQADDR = 0x101,
       ISPIOC_D_MIS_IRQADDR = 0x102,
       ISPIOC_S_MP_34BIT = 0x10a,
       ISPIOC_RST_QUEUE = 0x1c1,
};

enum { ISPIOC_V4L2_TUNING_ENABLE,
       ISPIOC_V4L2_TUNING_DISABLE,
};

struct hailo15_isp_irq_status {
	uint32_t isp_mis;
	uint32_t isp_miv2_mis;
	uint32_t isp_miv2_mis1;
};

struct hailo15_isp_irq_status_event {
	uint32_t port;
	uint32_t irq_status;
};

struct hailo15_isp_src_pad_handle {
	struct media_pad *pad;
	struct v4l2_subdev_format format;
};

struct hailo15_isp_sink_pad_handle {
	struct media_pad *pad;
	int remote_pad;
};
enum { ISP_MP,
       ISP_SP2,
       ISP_MAX_PATH,
};

struct hailo15_isp_device {
	struct device *dev;
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler ctrl_handler;
	uint32_t ctrl_pad;
	struct media_pad pads[HAILO15_ISP_PADS_NR];
	struct hailo15_isp_src_pad_handle
		src_pad_handles[HAILO15_ISP_SOURCE_PADS_NR];
	struct hailo15_isp_sink_pad_handle
		sink_pad_handles[HAILO15_ISP_SINK_PADS_NR];
	struct v4l2_async_notifier notifier;
	uint32_t id;
	void __iomem *base;
	void __iomem *wrapper_base;
	int irq;
	struct tasklet_struct tasklet;
	struct mutex mlock;
	struct mutex ctrl_lock;
	spinlock_t slock;
	int32_t refcnt;
	struct hailo15_video_event_resource event_resource;
	struct hailo15_isp_irq_status irq_status;
	struct hailo15_rmem rmem;
	struct hailo15_buf_ctx *buf_ctx;
	struct hailo15_buffer *cur_buf[ISP_MAX_PATH];
	struct v4l2_subdev_format fmt[ISP_MAX_PATH];
	struct clk *ip_clk;
	struct clk *p_clk;
	uint64_t frame_count;
	int is_ip_clk_enabled;
	int is_p_clk_enabled;
	void *rmem_vaddr;
	int mi_stopped[ISP_MAX_PATH];
	dma_addr_t fbuf_phys;
	void *fbuf_vaddr;
	void *private_data[ISP_MAX_PATH];
	int current_vsm_index[ISP_MAX_PATH];
	struct hailo15_vsm current_vsm;
	struct hailo15_vsm vsm_list[HAILO15_MAX_BUFFERS][ISP_MAX_PATH];
	int queue_empty[ISP_MAX_PATH];
	/* used for empty buffer queue */
	dma_addr_t fakebuf_phys;
	void *fakebuf_vaddr;
};

void hailo15_isp_private_entity_init(struct hailo15_isp_device *isp_dev);
void hailo15_isp_pad_handle_init(struct hailo15_isp_device *isp_dev);
irqreturn_t isp_irq_process(struct hailo15_isp_device *isp_dev);
int isp_hal_set_pad_stream(struct hailo15_isp_device *isp_dev,
			   uint32_t pad_index, int status);
void hailo15_isp_buffer_done(struct hailo15_isp_device *, int path);
void hailo15_config_isp_wrapper(struct hailo15_isp_device *isp_dev);
int hailo15_isp_is_path_enabled(struct hailo15_isp_device *, int);
int hailo15_isp_dma_set_enable(struct hailo15_isp_device *, int, int);
int hailo15_video_post_event_set_fmt(struct hailo15_isp_device *isp_dev,
				     int pad,
				     struct v4l2_mbus_framefmt *format);
int hailo15_video_post_event_start_stream(struct hailo15_isp_device *isp_dev);
int hailo15_video_post_event_stop_stream(struct hailo15_isp_device *isp_dev);
int hailo15_video_post_event_requebus(struct hailo15_isp_device *isp_dev,
				      int pad, uint32_t num_buffers);
int hailo15_isp_s_ctrl_event(struct hailo15_isp_device *isp_dev, int pad,
			     struct v4l2_ctrl *ctrl);
int hailo15_isp_g_ctrl_event(struct hailo15_isp_device *isp_dev, int pad,
			     struct v4l2_ctrl *ctrl);
#endif
