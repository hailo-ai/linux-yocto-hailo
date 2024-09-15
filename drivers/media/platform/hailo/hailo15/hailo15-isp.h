#ifndef __HAILO15_ISP_DRIVER__
#define __HAILO15_ISP_DRIVER__

#include <linux/list.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include "hailo15-events.h"
#include "common.h"
#include "fe/fe_dev.h"

#define HAILO15_ISP_NAME "hailo-isp"

/* We double the needed queue size for safety */
#define HAILO15_ISP_EVENT_QUEUE_SIZE (250 * 2)

#define HAILO15_ISP_CHN_MAX 2 // S0/S1 - also called "port" in isp_mcm_buf
#define HAILO15_ISP_PATHS_MAX 2 // MP/SP

struct isp_mcm_buf {
	uint32_t port; // channel
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
#define HAILO15_ISP_SOURCE_PADS_NR (HAILO15_ISP_SOURCE_PAD_MAX - HAILO15_ISP_SINK_PAD_MAX)
#define HAILO15_ISP_SINK_PADS_NR (HAILO15_ISP_SINK_PAD_MAX)

#define HAILO15_ISP_SINK_PAD_BEGIN (HAILO15_ISP_SINK_PAD_S0)
#define HAILO15_ISP_SINK_PAD_END (HAILO15_ISP_SINK_PAD_MAX)
#define HAILO15_ISP_SOURCE_PAD_BEGIN (HAILO15_ISP_SINK_PAD_MAX)
#define HAILO15_ISP_SOURCE_PAD_END (HAILO15_ISP_SOURCE_PAD_MAX)

#define HAILO15_ISP_SOURCE_PAD_MP_BEGIN (HAILO15_ISP_SOURCE_PAD_MP_S0)
#define HAILO15_ISP_SOURCE_PAD_MP_END (HAILO15_ISP_SOURCE_PAD_MP_BEGIN + HAILO15_ISP_CHN_MAX)
#define HAILO15_ISP_SOURCE_PAD_SP_BEGIN (HAILO15_ISP_SOURCE_PAD_SP_S0)
#define HAILO15_ISP_SOURCE_PAD_SP_END (HAILO15_ISP_SOURCE_PAD_SP_BEGIN + HAILO15_ISP_CHN_MAX)


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
	uint32_t isp_fe;
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

enum hailo15_isp_path {
	ISP_MP,
	ISP_SP2,
	ISP_MCM_IN,
	ISP_MAX_PATH,
};

struct hailo15_isp_pad_data {
	uint32_t stream;
	uint32_t sequence;
	struct hailo15_pad_stat_subscribe stat_sub[HAILO15_UEVENT_ISP_STAT_MAX];
};

struct hailo15_miv2_mis {
	uint32_t miv2_mis;
	struct list_head list;
};

struct hailo15_isp_device {
	struct device *dev;
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler ctrl_handler;
	uint32_t ctrl_pad;
	struct media_pad pads[HAILO15_ISP_PADS_NR];
    struct hailo15_isp_pad_data pad_data[HAILO15_ISP_PADS_NR];
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
	struct hailo15_event_resource event_resource;
	struct hailo15_isp_irq_status irq_status;
	struct hailo15_rmem rmem;
	struct hailo15_buf_ctx *buf_ctx[ISP_MAX_PATH];
	struct hailo15_buffer *cur_buf[ISP_MAX_PATH];
	struct v4l2_subdev_format fmt[ISP_MAX_PATH];
	struct clk *ip_clk;
	struct clk *p_clk;
	uint64_t frame_count[ISP_MAX_PATH];
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
	uint32_t null_addr;
	struct hailo15_af_kevent *af_kevent;
	struct workqueue_struct *af_wq;
	struct work_struct af_w;
	struct vvcam_fe_dev* fe_dev;
	int mcm_mode;
	struct v4l2_subdev_format input_fmt;
	struct list_head mcm_queue;
	spinlock_t mcm_lock;
	int rdma_enable;
	int dma_ready;
	int frame_end;
	int fe_ready;
	int fe_enable;
	struct tasklet_struct fe_tasklet;
	struct list_head miv2_mis_queue;
	spinlock_t miv2_mis_lock;
	struct workqueue_struct* miv2_mis_wq;
	struct work_struct miv2_mis_w;
};


void hailo15_isp_private_entity_init(struct hailo15_isp_device *isp_dev);
void hailo15_isp_pad_handle_init(struct hailo15_isp_device *isp_dev);
int isp_hal_set_pad_stream(struct hailo15_isp_device *isp_dev,
			   uint32_t pad_index, int status);
void hailo15_isp_buffer_done(struct hailo15_isp_device *, int path);
void hailo15_config_isp_wrapper(struct hailo15_isp_device *isp_dev);
int hailo15_isp_is_path_enabled(struct hailo15_isp_device *, int);
int hailo15_isp_dma_set_enable(struct hailo15_isp_device *, int, int);
void hailo15_isp_reset_hw(struct hailo15_isp_device*);
int hailo15_isp_post_event_set_fmt(struct hailo15_isp_device *isp_dev,
				     int pad,
				     struct v4l2_mbus_framefmt *format);
int hailo15_isp_post_event_start_stream(struct hailo15_isp_device *isp_dev);
int hailo15_isp_post_event_stop_stream(struct hailo15_isp_device *isp_dev);
int hailo15_isp_post_event_requebus(struct hailo15_isp_device *isp_dev,
				      int pad, uint32_t num_buffers);
int hailo15_isp_s_stream_event(struct hailo15_isp_device *isp_dev, int pad, uint32_t status);
int hailo15_isp_s_ctrl_event(struct hailo15_isp_device *isp_dev, int pad,
			     struct v4l2_ctrl *ctrl);
int hailo15_isp_g_ctrl_event(struct hailo15_isp_device *isp_dev, int pad,
			     struct v4l2_ctrl *ctrl);
irqreturn_t isp_irq_process(struct hailo15_isp_device *isp_dev);
irqreturn_t hailo15_process_irq_stats_events(struct hailo15_isp_device *isp_dev,
    int event_id, uint32_t mis);
void mcm_fe_irq_tasklet(unsigned long);
void hailo15_isp_handle_frame_rx(struct work_struct*);
#endif
