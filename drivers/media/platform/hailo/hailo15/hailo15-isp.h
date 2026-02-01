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
#include <linux/atomic.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include "hailo15-events.h"
#include "hailo15-isp-hw-defs.h"
#include "common.h"
#include "fe/fe_dev.h"

#define HAILO15_ISP_NAME "hailo-isp"

/* We double the needed queue size for safety */
#define HAILO15_ISP_EVENT_QUEUE_SIZE (250 * 2)

#define HAILO15_ISP_CHN_MAX 2 // S0/S1 - also called "port" in isp_mcm_buf
#define HAILO15_ISP_PATHS_MAX 2 // MP/SP
#define HAILO15_ISP_RAW_BUFS_NUM 3 /* number of buffers for raw frames from sensors */
#define FRAME_TIMEOUT_MS 1100
#define HAILO15_ISP_IRQ_EVENTS_COUNT 2

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
	HAILO15_ISP_SOURCE_PAD_SP_S0,
	HAILO15_ISP_SOURCE_PAD_MP_S1,
	HAILO15_ISP_SINK_PAD_MCM_IN,
	HAILO15_ISP_SOURCE_PAD_SP_S1,
	HAILO15_ISP_SOURCE_PAD_MCM_RAW_WR,
	HAILO15_ISP_SOURCE_PAD_MAX,
};

static inline int HAILO15_VID_GRP_TO_ISP_SINK_PAD(int grp_id)
{
	switch (grp_id) {
	case HAILO15_VID_GRP_SX_CSI0_ISP_MP:
	case HAILO15_VID_GRP_SX_CSI0_ISP_SP:
	case HAILO15_VID_GRP_MCM_RAW_WR:
		return HAILO15_ISP_SINK_PAD_S0;
	case HAILO15_VID_GRP_SX_CSI1_ISP_MP:
	case HAILO15_VID_GRP_SX_CSI1_ISP_SP:
		return HAILO15_ISP_SINK_PAD_S1;
	default:
		return -1;
	}
}

static inline int HAILO15_VID_GRP_TO_ISP_SOURCE_PAD(int grp_id)
{
	switch (grp_id) {
	case HAILO15_VID_GRP_SX_CSI0_ISP_MP:
	case HAILO15_VID_GRP_MCM_RAW_WR:
		return HAILO15_ISP_SOURCE_PAD_MP_S0;
	case HAILO15_VID_GRP_SX_CSI0_ISP_SP:
		return HAILO15_ISP_SOURCE_PAD_SP_S0;
	case HAILO15_VID_GRP_SX_CSI1_ISP_MP:
		return HAILO15_ISP_SOURCE_PAD_MP_S1;
	case HAILO15_VID_GRP_SX_CSI1_ISP_SP:
		return HAILO15_ISP_SOURCE_PAD_SP_S1;
	case HAILO15_VID_GRP_MCM_IN:
		return HAILO15_ISP_SINK_PAD_MCM_IN;
		// return HAILO15_ISP_SOURCE_PAD_MCM_RAW_WR;
	default:
		return -1;
	}
}

static inline int HAILO15_ISP_SOURCE_PAD_TO_ISP_SINK_PAD(int src_pad)
{
	switch (src_pad) {
	case HAILO15_ISP_SOURCE_PAD_MP_S0:
	case HAILO15_ISP_SOURCE_PAD_SP_S0:
	case HAILO15_ISP_SOURCE_PAD_MCM_RAW_WR:
		return HAILO15_ISP_SINK_PAD_S0;
	case HAILO15_ISP_SOURCE_PAD_MP_S1:
	case HAILO15_ISP_SOURCE_PAD_SP_S1:
		return HAILO15_ISP_SINK_PAD_S1;
	default:
		return -1;
	}
}

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

struct hailo15_isp_pad_data {
	uint32_t stream;
	uint32_t sequence;
	struct hailo15_pad_stat_subscribe stat_sub[HAILO15_UEVENT_ISP_STAT_MAX];
};

struct hailo15_miv2_mis {
	uint32_t miv2_mis;
	struct list_head list;
};

struct hailo15_isp_raw_buf {
	void *virt_addr;
	dma_addr_t phys_addr;
	struct list_head list;
	uint32_t index;
	size_t size;
};

struct hailo15_hw_shifter_config {
    uint32_t first_shifter_offset;
    uint32_t shift_value;
    size_t shifter_regs;
};

struct hailo15_isp_line_buf_config {
    int enabled;
    size_t repeat; // Amount of subsequent repeats for each register (per channel)

    struct {
        uint32_t line_buf_cfg;
        uint32_t line_buf_cfg_line_width;
        uint32_t line_buf_cfg_min_vblank_duration;
        uint32_t line_buf_cfg_min_hblank_duration;
    } offsets;

    struct {
        uint8_t vblank_vc;
        uint32_t line_buf_cfg_min_vblank_duration;
        uint32_t line_buf_cfg_min_hblank_duration;
    } values;
};

struct isp_wrapper_config {
    uint32_t fatal_asf_int_mask_offset;
    uint32_t fatal_asf_int_mask_value;
    uint32_t func_int_mask_offset;
    uint32_t func_int_mask_value;
    uint32_t err_int_mask_offset;
    uint32_t err_int_mask_value;
    uint32_t err_int_status_offset;
    uint32_t err_int_w1c_offset;
    uint32_t err_int_w1c_value;
    struct hailo15_hw_shifter_config shifter_cfg;
    struct hailo15_isp_line_buf_config line_buf_cfg;
    struct err_status_reg isp_err_interrupt_reg;
};

struct hailo15_irq_deffered_work {
	struct work_struct irq_deffered_w;
	struct hailo15_isp_device *isp_dev;
	uint32_t irq_status;
};

struct hailo15_mcm_raw_wr_buffer_done_work {
	struct work_struct work;
	struct hailo15_isp_device *isp_dev;
	int grp_id;
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
	int irq[HAILO15_ISP_IRQ_EVENTS_COUNT];
	struct tasklet_struct tasklet;
	struct mutex mlock;
	struct mutex ctrl_lock;
	spinlock_t slock;
	int32_t refcnt;
	struct hailo15_event_resource event_resource;
	struct hailo15_isp_irq_status irq_status;
	struct hailo15_rmem rmem[HAILO15_ISP_SINK_PAD_MAX];
	void *rmem_vaddr[HAILO15_ISP_SINK_PAD_MAX];
	struct hailo15_buffer *cur_buf[HAILO15_VID_GRP_SX_MAX];
	uint32_t cur_buf_path;
	struct v4l2_subdev_format fmt[ISP_MAX_PATH];
	struct clk *ip_clk;
	struct clk *p_clk;
	uint64_t frame_count[HAILO15_VID_GRP_SX_MAX];
	int is_ip_clk_enabled;
	int is_p_clk_enabled;
	int mi_stopped[ISP_MAX_PATH];
	void *private_data[HAILO15_VID_GRP_SX_MAX];
	int current_vsm_index[HAILO15_VID_GRP_SX_MAX];
	struct hailo15_vsm current_vsm;
	struct hailo15_vsm vsm_list[HAILO15_MAX_BUFFERS][HAILO15_VID_GRP_SX_MAX];
	int queue_empty[HAILO15_VID_GRP_SX_MAX];
	/* used for empty buffer queue */
	dma_addr_t fakebuf_phys;
	void *fakebuf_vaddr;
	uint32_t null_addr;
	struct hailo15_af_kevent *af_kevent;
	struct workqueue_struct *af_wq;
	struct vvcam_fe_dev* fe_dev;
	int mcm_mode;
	struct v4l2_subdev_format input_fmt[HAILO15_ISP_SINK_PAD_MAX];
	struct list_head mcm_queue;
	struct list_head mcm_raw_wr_queue;
	struct mutex mcm_raw_wr_lock;
	struct list_head raw0_full_queue;
	struct list_head raw0_empty_queue;
	struct list_head raw1_full_queue;
	struct list_head raw1_empty_queue;
	struct mutex raw0_full_lock;
	struct mutex raw0_empty_lock;
	struct mutex raw1_full_lock;
	struct mutex raw1_empty_lock;
	struct hailo15_isp_raw_buf *cur_raw_buf[2];
	struct hailo15_isp_raw_buf *cur_rdma_buf;
	bool stream_enabled[HAILO15_ISP_SINK_PAD_MAX];
	struct mutex mcm_lock;
	struct isp_fe_switch_t fe_switch;
	int rdma_enable;
	int dma_ready;
	int frame_end;
	int fe_ready;
	int fe_enable;
	int mcm_waiting;
	int output_ready;
	struct mutex ready_lock;
	struct tasklet_struct fe_tasklet;
	spinlock_t stream_state_lock;
	struct workqueue_struct* isp_mis_wq;
	struct workqueue_struct* miv2_mis_wq;
	struct workqueue_struct* mcm_wr_raw_wq;
	wait_queue_head_t raw_frame_available_wait_q;
	bool raw_frame_available[HAILO15_ISP_SINK_PAD_MAX];
	wait_queue_head_t toggle_sensors_wait_q;
	bool toggle_sensors;
	const struct isp_wrapper_config *wrapper_cfg;
	struct timer_list frame_timer[HAILO15_ISP_SINK_PAD_MAX];
	atomic_t frame_received[HAILO15_ISP_SINK_PAD_MAX];
	atomic_t streaming_started[HAILO15_ISP_SINK_PAD_MAX];
	atomic_t first_rdma_done;
	atomic_t full_queue_count[HAILO15_ISP_SINK_PAD_MAX]; /* count of buffers in each sensor's full queue */
	bool tuning_state;
	bool hdr_enabled;
	wait_queue_head_t buf_done_wait_q;
	atomic_t buf_done_ready;

	struct hailo15_reqbufs prev_reqbufs;
	enum isp_mcm_mode mcm_mode_priming;
	enum fast_toggle_state fast_toggle_state;
};


void hailo15_isp_private_entity_init(struct hailo15_isp_device *isp_dev);
void hailo15_isp_pad_handle_init(struct hailo15_isp_device *isp_dev);
int isp_hal_set_pad_stream(struct hailo15_isp_device *isp_dev,
			   uint32_t pad_index, int status);
void hailo15_isp_buffer_done(struct hailo15_isp_device *, int grp_id);
void hailo15_config_isp_wrapper(struct hailo15_isp_device *isp_dev);
int hailo15_isp_is_path_enabled(struct hailo15_isp_device *, int);
void hailo15_isp_reset_hw(struct hailo15_isp_device*);
int hailo15_isp_post_event_set_fmt(struct hailo15_isp_device *isp_dev,
				     int pad,
				     struct v4l2_mbus_framefmt *format);
int hailo15_isp_post_event_start_stream(struct hailo15_isp_device *isp_dev, int pad);
int hailo15_isp_post_event_stop_stream(struct hailo15_isp_device *isp_dev, int pad);
int hailo15_isp_post_event_requebus(struct hailo15_isp_device *isp_dev,
				      int pad, uint32_t num_buffers);
int hailo15_isp_post_event_fast_toggle(struct hailo15_isp_device *isp_dev, int pad);
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
void hailo15_isp_handle_mcm_raw_frame_rx(struct work_struct *work);

#endif
