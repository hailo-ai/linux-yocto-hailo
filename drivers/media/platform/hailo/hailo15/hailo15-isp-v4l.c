#include <linux/module.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of_graph.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mc.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ctrls.h>
#include <isp_ctrl/hailo15_isp_ctrl.h>
#include <stdbool.h>
#include "hailo15-isp.h"
#include "hailo15-isp-hw.h"
#include "hailo15-isp-events.h"
#include "hailo15-media.h"
#include "common.h"
#include <linux/property.h>
#include <trace/events/hailo15_isp.h>


#define HAILO15_ISP_NAME_SIZE 10
#define HAILO15_ISP_ADDR_SPACE_MAX_SIZE 0x10000
#define HAILO15_ISP_RMEM_SIZE (32 * 1024 * 1024)
#define HAILO15_ISP_MCM_MODE_RMEM_EXT_SIZE (2 * 3840 * 2160 * 2)
#define HAILO15_ISP_FBUF_SIZE (3840 * 2160 * 2)
#define HAILO15_ISP_FAKEBUF_SIZE (3840 * 2160 * 3)
#define HAILO15L_ISP_SP2_ERR_MASK (BIT(23) | BIT(24))
#define HAILO15_ISP_BUF_DONE_TIMEOUT 1000


static const struct isp_wrapper_config hailo15_isp_wrapper_config = {
    .fatal_asf_int_mask_offset = 0x4,
    .fatal_asf_int_mask_value = 0x1,
    .func_int_mask_offset = 0x38,
    .func_int_mask_value = 0x7,
    .err_int_mask_offset = 0x40,
    .err_int_mask_value = 0x3ffff,
    .err_int_status_offset = 0x44,
    .err_int_w1c_offset = 0x48,
    .err_int_w1c_value = 0x1fffffff,

    .shifter_cfg = {
        .first_shifter_offset = 0,
        .shift_value = 0,
        .shifter_regs = 0,
    },

    .line_buf_cfg = {
        .enabled = 0
    },

	.isp_err_interrupt_reg = {
		.name = "isp_err_interrupt",
		.num_errors = 5,
		.errors = (struct error_message[]) {
			{ .mask = (BIT(0) | BIT(1)), .message = "dma_credit_ready_high_rate_int_set" },
			{ .mask = (BIT(2) | BIT(3) | BIT(4) | BIT(5)), .message = "almost_full_int_set" },
			{ .mask = (BIT(6) | BIT(7) | BIT(8) | BIT(9)), .message = "frame_drop_exit_int_set" },
			{ .mask = (BIT(10) | BIT(11) | BIT(12) | BIT(13)), .message = "frame_drop_int_set" },
			{ .mask = (BIT(14) | BIT(15) | BIT(16) | BIT(17)), .message = "overflow_int_set" },
		}
	}
};

static const struct isp_wrapper_config hailo15l_isp_wrapper_config = {
    .fatal_asf_int_mask_offset = 0x4,
    .fatal_asf_int_mask_value = 0xFF,
    .func_int_mask_offset = 0x4c,
    .func_int_mask_value = 0x7,
    .err_int_mask_offset = 0x54,
    .err_int_mask_value = 0x1fffffff,
    .err_int_status_offset = 0x58,
    .err_int_w1c_offset = 0x5c,
    .err_int_w1c_value = 0x1fffffff,

    .shifter_cfg = {
        .first_shifter_offset = 0x24C,
        .shift_value = 0x4,
        .shifter_regs = 3,
    },

    .line_buf_cfg = {
        .enabled = 1,
        .repeat = 1,

        .offsets = {
            .line_buf_cfg = 0x224,
            .line_buf_cfg_line_width = 0x22C,
            .line_buf_cfg_min_vblank_duration = 0x234,
            .line_buf_cfg_min_hblank_duration = 0x23C,
        },

        .values = {
            .vblank_vc = 1,
            // These values come from a VSI recommendation at HM-18 ticket
            .line_buf_cfg_min_vblank_duration = 110, // VSI recommended 100, and we added 10 for safety
            .line_buf_cfg_min_hblank_duration = 100,
        },
    },

	.isp_err_interrupt_reg = {
		.name = "isp_err_interrupt",
		.num_errors = 14,
		.errors = (struct error_message[]) {
			{ .mask = (BIT(0) | BIT(1) | BIT(2) | BIT(3)), .message = "dma_credit_ready_high_rate_int_set" },
			{ .mask = (BIT(4) | BIT(5) | BIT(6) | BIT(7)), .message = "almost_full_int_set" },
			{ .mask = (BIT(8) | BIT(9) | BIT(10) | BIT(11)), .message = "frame_drop_exit_int_set" },
			{ .mask = (BIT(12) | BIT(13) | BIT(14) | BIT(15)), .message = "frame_drop_int_set" },
			{ .mask = (BIT(16) | BIT(17) | BIT(18) | BIT(19)), .message = "overflow_int_set" },
			{ .mask = BIT(20), .message = "pr_timeout" },
			{ .mask = BIT(21), .message = "cr_timeout" },
			{ .mask = BIT(22), .message = "isp_mp_axi_bresp, Received error response on MP AXI interface" },
			{ .mask = BIT(23), .message = "isp_sp2_axi_bresp, Received error response on SP2 Write AXI interface" },
			{ .mask = BIT(24), .message = "isp_sp2_axi_rresp, Received error response on SP2 Read AXI interface" },
			{ .mask = BIT(25), .message = "isp_mcm_axi_bresp, Received error response on MCM Write AXI interface" },
			{ .mask = BIT(26), .message = "isp_mcm_axi_rresp, Received error response on MCM Read AXI interface" },
			{ .mask = BIT(27), .message = "bubble_s0, ISP received bubble on sensor_0 interface" },
			{ .mask = BIT(28), .message = "bubble_s1, ISP received bubble on sensor_1 interface" },
		}		
	}
};

static const struct of_device_id hailo15_isp_of_match[] = {
	{ .compatible = "hailo,hailo15-isp", .data = &hailo15_isp_wrapper_config },
	{ .compatible = "hailo,hailo15l-isp", .data = &hailo15l_isp_wrapper_config },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, hailo15_isp_of_match);

/* Legacy register functions */
extern uint32_t hailo15_isp_read_reg(struct hailo15_isp_device *, uint32_t);
extern void hailo15_isp_write_reg(struct hailo15_isp_device *, uint32_t, uint32_t);

/* New struct-based register operation functions */
extern uint32_t hailo15_isp_read_reg_op(struct hailo15_isp_device *, const struct hailo15_reg_op *);
extern void hailo15_isp_write_reg_op(struct hailo15_isp_device *, const struct hailo15_reg_op *);

/* Convenience functions for register operations with specific vdid */
extern void hailo15_isp_write_reg_with_vdid(struct hailo15_isp_device *, uint32_t, uint32_t, int);
extern uint32_t hailo15_isp_read_reg_with_vdid(struct hailo15_isp_device *, uint32_t, int);
extern void hailo15_isp_configure_frame_base(struct hailo15_isp_device *,
						dma_addr_t *, int);
extern void hailo15_isp_configure_mcm_raw_frame_base(struct hailo15_isp_device *,
				    dma_addr_t *, unsigned int);
extern void hailo15_isp_configure_frame_size(struct hailo15_isp_device *, int);
extern irqreturn_t hailo15_isp_irq_process(struct hailo15_isp_device *);
extern irqreturn_t hailo15_isp_err_irq_process(struct hailo15_isp_device *, int irq);
extern void hailo15_isp_handle_afm_int(struct work_struct *);
extern int hailo15_isp_dma_set_enable(struct hailo15_isp_device *, int, int);
extern void hailo15_fe_get_dev(struct vvcam_fe_dev** dev);
extern void hailo15_fe_set_address_space_base(struct vvcam_fe_dev* fe_dev,void* base);

/* Forward declarations */
void frame_timeout_handler_0(struct timer_list *t);
void frame_timeout_handler_1(struct timer_list *t);
static int hailo15_isp_init_raw_bufs(struct hailo15_isp_device *isp_dev, int sink_pad_index, size_t size);

struct hailo15_af_kevent af_kevent;
EXPORT_SYMBOL(af_kevent);

struct vvcam_fe_dev* fe_dev = NULL;

/* watches HW_INTERRUPTS__ISP_FUNC_INTERRUPT_IRQ */
static irqreturn_t hailo15_isp_irq_handler(int irq, void *isp_dev)
{
    return hailo15_isp_irq_process(isp_dev);
}

/* watches HW_INTERRUPTS__VISION_SUBSYS_ERR_INT_IRQ */
static irqreturn_t hailo15_isp_err_irq_handler(int irq, void *isp_dev)
{
    return hailo15_isp_err_irq_process(isp_dev, irq);
}

static struct hailo15_isp_device* isp_dev_from_v4l2_subdev(struct v4l2_subdev *sd)
{
    struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	return ctx->dev;
}

static inline struct fwnode_handle *
hailo15_isp_pad_get_ep(struct hailo15_isp_device *isp_dev, int pad_nr)
{
	return fwnode_graph_get_endpoint_by_id(dev_fwnode(isp_dev->dev), pad_nr,
						   0, FWNODE_GRAPH_ENDPOINT_NEXT);
}

static inline int hailo15_isp_put_ep(struct fwnode_handle *ep)
{
	fwnode_handle_put(ep);
	return 0;
}

static inline void
hailo15_isp_configure_buffer(struct hailo15_isp_device *isp_dev,
				 struct hailo15_buffer *buf)
{
	// Validate that buffer address is in a specific range?
	int isp_path;

	if(!buf) {
		pr_warn("%s - buf is null, return\n", __func__);
		return;
	}

	isp_path = HAILO15_VID_GRP_TO_ISP_PATH(buf->grp_id);
	if (isp_path < 0 || isp_path >= ISP_MAX_PATH)
		return;
	if(isp_path == ISP_MCM_IN){
		if(!isp_dev->rdma_enable){
			pr_err("trying to configure mcm buffer with rdma disabled!\n");
			return;
		}
		mutex_lock(&isp_dev->mcm_lock);
		if(isp_dev->cur_buf[buf->grp_id]){
			list_add_tail(&buf->irqlist, &isp_dev->mcm_queue);
			mutex_unlock(&isp_dev->mcm_lock);
			return;
		}

		isp_dev->cur_buf[buf->grp_id] = buf;
		mutex_unlock(&isp_dev->mcm_lock);
	}else{
		isp_dev->cur_buf[buf->grp_id] = buf;
	}
	hailo15_isp_configure_frame_base(isp_dev, buf->dma, buf->grp_id);
}

static int hailo15_isp_requbufs(struct v4l2_subdev *sd, void *arg)
{
    struct hailo15_reqbufs *pad_requbufs = (struct hailo15_reqbufs *)arg;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);

	/* if requested buf count is 0, return
	   this happens at the beginning and end of the stream */
	if (pad_requbufs->num_buffers == 0) {
		pr_debug("%s - requested buf count is 0, returning\n", __func__);
		return 0;
	}

    return hailo15_isp_post_event_requebus(isp_dev, pad_requbufs->pad, pad_requbufs->num_buffers);
}

static int hailo15_isp_pad_s_stream(struct v4l2_subdev *sd, void *arg)
{
    struct hailo15_pad_stream_status *pad_stream = (struct hailo15_pad_stream_status *)arg;
    struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);

    isp_dev->pad_data[pad_stream->pad].stream = pad_stream->status;

    /* This function does not actually start the stream on the selected pad at the moment
     * It is only used to set the stream status on the pad_data structure.
     * The stream is started by the hailo15_isp_s_stream_event function,
     * which currently always uses pad 0 */

    return 0;
}

static int hailo15_isp_mcm_extract_mode(struct v4l2_subdev *sd, void *arg)
{
    struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
    uint32_t *mcm_mode = (uint32_t *)arg;

    *mcm_mode = isp_dev->mcm_mode;
    return 0;
}

static int hailo15_isp_mcm_set_mode(struct v4l2_subdev *sd, void *arg)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	uint32_t *mcm_mode = (uint32_t *)arg;
	bool streaming = false;
	unsigned long flags;
	int i;

	if (*mcm_mode >= ISP_MCM_MODE_MAX) {
		pr_err("%s - invalid mcm mode %d\n", __func__, *mcm_mode);
		return -EINVAL;
	}

	for (i = 0; i < HAILO15_ISP_SINK_PAD_MAX; i++) {
		spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
		streaming |= isp_dev->stream_enabled[i];
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);
	}

	if (streaming) {
		pr_err("%s - mcm mode cannot be set while streaming\n", __func__);
		return -EBUSY;
	}

	isp_dev->mcm_mode = *mcm_mode;
	return 0;
}

struct list_head *hailo15_isp_get_empty_queue(struct hailo15_isp_device *isp_dev, int sink_pad_index)
{
	switch (sink_pad_index) {
		case HAILO15_ISP_SINK_PAD_S0:
			return &isp_dev->raw0_empty_queue;
		case HAILO15_ISP_SINK_PAD_S1:
			return &isp_dev->raw1_empty_queue;
		default:
			return NULL;
	}

	return NULL;
}
EXPORT_SYMBOL(hailo15_isp_get_empty_queue);

struct list_head *hailo15_isp_get_full_queue(struct hailo15_isp_device *isp_dev, int sink_pad_index)
{
	switch (sink_pad_index) {
		case HAILO15_ISP_SINK_PAD_S0:
			return &isp_dev->raw0_full_queue;
		case HAILO15_ISP_SINK_PAD_S1:
			return &isp_dev->raw1_full_queue;
		default:
			return NULL;
	}
}
EXPORT_SYMBOL(hailo15_isp_get_full_queue);

struct mutex *hailo15_isp_get_empty_lock(struct hailo15_isp_device *isp_dev, int sink_pad_index)
{
	switch (sink_pad_index) {
		case HAILO15_ISP_SINK_PAD_S0:
			return &isp_dev->raw0_empty_lock;
		case HAILO15_ISP_SINK_PAD_S1:
			return &isp_dev->raw1_empty_lock;
		default:
			return NULL;
	}
}
EXPORT_SYMBOL(hailo15_isp_get_empty_lock);

struct mutex *hailo15_isp_get_full_lock(struct hailo15_isp_device *isp_dev, int sink_pad_index)
{
	switch (sink_pad_index) {
		case HAILO15_ISP_SINK_PAD_S0:
			return &isp_dev->raw0_full_lock;
		case HAILO15_ISP_SINK_PAD_S1:
			return &isp_dev->raw1_full_lock;
		default:
			return NULL;
	}
}
EXPORT_SYMBOL(hailo15_isp_get_full_lock);


/******************************/
/* VSI infrastructure support */
/******************************/
static int hailo15_vsi_isp_qcap(struct v4l2_subdev *sd, void *arg)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct v4l2_capability *cap = (struct v4l2_capability *)arg;
	/* SHOULD BE COPY_TO_USER */
	strlcpy((char *)cap->driver, HAILO15_ISP_NAME, sizeof(cap->driver));
	cap->bus_info[0] = isp_dev->id;
	return 0;
}

static int hailo15_isp_queryctrl(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct hailo15_pad_queryctrl *pad_querctrl =
		(struct hailo15_pad_queryctrl *)arg;
	ret = v4l2_queryctrl(&isp_dev->ctrl_handler, pad_querctrl->query_ctrl);

	return ret;
}

static int hailo15_isp_query_ext_ctrl(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct hailo15_pad_query_ext_ctrl *pad_quer_ext_ctrl =
		(struct hailo15_pad_query_ext_ctrl *)arg;
	ret = v4l2_query_ext_ctrl(&isp_dev->ctrl_handler,
				  pad_quer_ext_ctrl->query_ext_ctrl);

	return ret;
}

static int hailo15_isp_querymenu(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct hailo15_pad_querymenu *pad_quermenu =
		(struct hailo15_pad_querymenu *)arg;
	ret = v4l2_querymenu(&isp_dev->ctrl_handler, pad_quermenu->querymenu);

	return ret;
}

static int hailo15_isp_g_ctrl(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct hailo15_pad_control *pad_ctrl =
		(struct hailo15_pad_control *)arg;

	mutex_lock(&isp_dev->ctrl_lock);
	isp_dev->ctrl_pad = pad_ctrl->pad;
	ret = v4l2_g_ctrl(&isp_dev->ctrl_handler, pad_ctrl->control);
	mutex_unlock(&isp_dev->ctrl_lock);

	return ret;
}

static int hailo15_isp_s_ctrl(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct hailo15_pad_control *pad_ctrl =
		(struct hailo15_pad_control *)arg;

	mutex_lock(&isp_dev->ctrl_lock);
	isp_dev->ctrl_pad = pad_ctrl->pad;
	ret = v4l2_s_ctrl(NULL, &isp_dev->ctrl_handler, pad_ctrl->control);
	mutex_unlock(&isp_dev->ctrl_lock);

	return ret;
}

static int hailo15_isp_g_ext_ctrls(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct hailo15_pad_ext_controls *pad_ext_ctrls =
		(struct hailo15_pad_ext_controls *)arg;

	mutex_lock(&isp_dev->ctrl_lock);
	isp_dev->ctrl_pad = pad_ext_ctrls->pad;
	ret = v4l2_g_ext_ctrls(&isp_dev->ctrl_handler, sd->devnode,
				   sd->v4l2_dev->mdev, pad_ext_ctrls->ext_controls);
	mutex_unlock(&isp_dev->ctrl_lock);

	return ret;
}

static int hailo15_isp_s_ext_ctrls(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct hailo15_pad_ext_controls *pad_ext_ctrls =
		(struct hailo15_pad_ext_controls *)arg;

	mutex_lock(&isp_dev->ctrl_lock);
	isp_dev->ctrl_pad = pad_ext_ctrls->pad;
	ret = v4l2_s_ext_ctrls(NULL, &isp_dev->ctrl_handler, sd->devnode,
				   sd->v4l2_dev->mdev, pad_ext_ctrls->ext_controls);
	mutex_unlock(&isp_dev->ctrl_lock);

	return ret;
}

static int hailo15_isp_try_ext_ctrls(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct hailo15_pad_ext_controls *pad_ext_ctrls =
		(struct hailo15_pad_ext_controls *)arg;
	ret = v4l2_try_ext_ctrls(&isp_dev->ctrl_handler, sd->devnode,
				 sd->v4l2_dev->mdev,
				 pad_ext_ctrls->ext_controls);

	return ret;
}

static int hailo15_isp_stat_subscribe(struct v4l2_subdev *sd, void *arg)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
    struct hailo15_pad_stat_subscribe *pad_sub = (struct hailo15_pad_stat_subscribe*)arg;
    struct hailo15_isp_pad_data *cur_pad = &isp_dev->pad_data[pad_sub->pad];
    int ret = 0;

    if (pad_sub->type != HAILO15_UEVENT_ISP_STAT) {
        return -EINVAL;
    }

    if (pad_sub->id >= HAILO15_UEVENT_ISP_STAT_MAX) {
        ret = -EINVAL;
    } else {
        if (cur_pad->stat_sub[pad_sub->id].type) {
            ret = -EINVAL;
        } else {
            cur_pad->stat_sub[pad_sub->id] = *pad_sub;
        }
    }

    if (ret == 0)
        pr_info("subscribed to isp stat event %d on pad %d\n", pad_sub->id, pad_sub->pad);

    return ret;
}

static int hailo15_isp_stat_unsubscribe(struct v4l2_subdev *sd, void *arg)
{
	int ret = 0;
    struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
    struct hailo15_pad_stat_subscribe *pad_sub = (struct hailo15_pad_stat_subscribe *)arg;
    struct hailo15_isp_pad_data *cur_pad = &isp_dev->pad_data[pad_sub->pad];

    if (pad_sub->type != HAILO15_UEVENT_ISP_STAT) {
        return -EINVAL;
    }

    if (pad_sub->id >= HAILO15_UEVENT_ISP_STAT_MAX) {
        ret = -EINVAL;
    } else {
        if (cur_pad->stat_sub[pad_sub->id].type == 0) {
            pr_err("%s: stat event %d is not subscribed\n", __func__, pad_sub->id);
            return -EINVAL;
        }

        memset(&cur_pad->stat_sub[pad_sub->id], 0, sizeof(*pad_sub));
        pr_info("unsubscribed from isp stat event %d on pad %d\n", pad_sub->id, pad_sub->pad);
    }

    return ret;
}

/* When disabling 3dnr in Pluto we get SP2 err interrupt, since we use an address
that can't be accessed, the writing is diverted to the correct address (the actual null-address).
That is intentional so we don't want to get that interrupt,
therefore we mask the SP2 err interrupt when disabling 3dnr in Pluto. */
static int hailo15l_isp_set_enable_sp2_err(struct v4l2_subdev *sd, void *arg)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	bool enable;
	uint32_t mask;
	bool write_mask = false;
	
	const struct isp_wrapper_config *wrapper_cfg;

	if (!isp_dev) {
		pr_err("isp_dev is null\n");
		return -EINVAL;
	}

	wrapper_cfg = isp_dev->wrapper_cfg;
	if (!wrapper_cfg) {
		pr_err("wrapper_cfg is null\n");
		return -EINVAL;
	}

	if (arg == NULL) {
		pr_err("arg is null\n");
		return -EINVAL;
	}
	enable = *((bool *)arg);

	/* This logic is only relevant for Pluto */
	if (isp_dev->wrapper_cfg != &hailo15l_isp_wrapper_config) {
		return 0;
	}
	
	mask = readl(isp_dev->wrapper_base + wrapper_cfg->err_int_mask_offset);
	if (enable && !(mask & HAILO15L_ISP_SP2_ERR_MASK)) {
		/* Handle cases of enabling/disabling 3dnr in the middle of streaming - 
		make sure we wait for the 3dnr to actually be enabled before clearing the error, 
		since that error constantly jumps when 3dnr disabled. That can be ensured after buffer done
		this is only for pluto so currently assume only single sensor0 */
		if (atomic_read(&isp_dev->streaming_started[HAILO15_ISP_SINK_PAD_S0])) {
			atomic_set(&isp_dev->buf_done_ready, 0);
			if (wait_event_timeout(isp_dev->buf_done_wait_q, atomic_read(&isp_dev->buf_done_ready),
				msecs_to_jiffies(HAILO15_ISP_BUF_DONE_TIMEOUT)) == -ETIMEDOUT) {
				pr_info("buffer done wait timed out so sp2 error wasn't properly cleared, continuing\n");
			}
		}
		mask |= HAILO15L_ISP_SP2_ERR_MASK;
		write_mask = true;

	} else if (!enable && (mask & HAILO15L_ISP_SP2_ERR_MASK)) {
		mask &= ~HAILO15L_ISP_SP2_ERR_MASK;
		write_mask = true;
	}

	if (write_mask) {
		writel(HAILO15L_ISP_SP2_ERR_MASK, isp_dev->wrapper_base + wrapper_cfg->err_int_w1c_offset);
		writel(mask, isp_dev->wrapper_base + wrapper_cfg->err_int_mask_offset);
	}
	
	return 0;
}

static long hailo15_vsi_isp_priv_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
					   void *arg)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct isp_reg_data isp_reg;
	struct v4l2_subdev_format *fmt;
	int port;
	int path;
	int ret = -EINVAL;

	dev_dbg(isp_dev->dev, "hailo15_vsi_isp_priv_ioctl: cmd = %d\n", cmd);

	switch (cmd) {
	case VIDIOC_QUERYCAP:
		ret = hailo15_vsi_isp_qcap(sd, arg);
		break;
	case ISPIOC_V4L2_READ_REG:
		mutex_lock(&isp_dev->mlock);
		memcpy(&isp_reg, arg, sizeof(struct isp_reg_data));
		/* only called when not in mcm mode */
		isp_reg.value = hailo15_isp_read_reg(isp_dev, isp_reg.reg);
		memcpy(arg, &isp_reg, sizeof(struct isp_reg_data));
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	case ISPIOC_V4L2_WRITE_REG:
		mutex_lock(&isp_dev->mlock);
		memcpy(&isp_reg, arg, sizeof(struct isp_reg_data));
		/* only called when not in mcm mode */
		hailo15_isp_write_reg(isp_dev, isp_reg.reg, isp_reg.value);
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	case ISPIOC_V4L2_RMEM:
		port = ((struct hailo15_rmem *)arg)->port;
		if (port >= HAILO15_ISP_SINK_PAD_MAX) {
			dev_err(isp_dev->dev, "invalid port %d\n", port);
			ret = -EINVAL;
			break;
		}
		isp_dev->rmem[port].size = HAILO15_ISP_RMEM_SIZE;
		if(isp_dev->mcm_mode){
			isp_dev->rmem[port].size += HAILO15_ISP_MCM_MODE_RMEM_EXT_SIZE;
		}
		if(!isp_dev->rmem_vaddr[port]){
			isp_dev->rmem_vaddr[port] = dma_alloc_coherent(
			isp_dev->dev, isp_dev->rmem[port].size, &isp_dev->rmem[port].addr, GFP_KERNEL);
			if (!isp_dev->rmem_vaddr[port]) {
				dev_err(isp_dev->dev, "can't allocate rmem buffer for port %d\n", port);
				ret = -ENOMEM;
				break;
			}
		}

		memcpy(arg, &isp_dev->rmem[port], sizeof(isp_dev->rmem[port]));
		ret = 0;
		break;
	case ISPIOC_V4L2_MI_START:
		mutex_lock(&isp_dev->mlock);
		path = HAILO15_VID_GRP_TO_ISP_PATH(isp_dev->cur_buf_path);
		if (!isp_dev->mcm_mode && isp_dev->cur_buf[isp_dev->cur_buf_path] &&
			isp_dev->mi_stopped[path] &&
			hailo15_isp_is_path_enabled(isp_dev, path)) {
			hailo15_isp_configure_frame_size(isp_dev, isp_dev->cur_buf_path);
			hailo15_isp_configure_buffer(
				isp_dev, isp_dev->cur_buf[isp_dev->cur_buf_path]);
			isp_dev->mi_stopped[path] = 0;
		}
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	case ISPIOC_V4L2_MI_STOP:
		mutex_lock(&isp_dev->mlock);
		path = HAILO15_VID_GRP_TO_ISP_PATH(isp_dev->cur_buf_path);
		if (!hailo15_isp_is_path_enabled(isp_dev, path) &&
			!isp_dev->mi_stopped[path])
			isp_dev->mi_stopped[path] = 1;
		mutex_unlock(&isp_dev->mlock);
		// clear error interrupts before closing
		writel(isp_dev->wrapper_cfg->err_int_w1c_value, isp_dev->wrapper_base + isp_dev->wrapper_cfg->err_int_w1c_offset);
		ret = 0;
		break;
	case ISPIOC_V4L2_SET_MCM_MODE:
        ret = hailo15_isp_mcm_set_mode(sd, arg);
		break;
	case ISPIOC_V4L2_MCM_MODE:
        ret = hailo15_isp_mcm_extract_mode(sd, arg);
		break;
	case ISPIOC_V4L2_REQBUFS:
		ret = hailo15_isp_requbufs(sd, arg);
		break;
	case ISPIOC_V4L2_SET_INPUT_FORMAT:
		fmt = (struct v4l2_subdev_format *)arg;
		memcpy(&isp_dev->input_fmt[HAILO15_VID_GRP_TO_ISP_SINK_PAD(fmt->pad)], (void *)arg,
			   sizeof(struct v4l2_subdev_format));
		ret = 0;
		break;
	case ISPIOC_V4L2_GET_NULL_ADDR:
		*((uint32_t*)arg) = isp_dev->null_addr;
		ret = 0;
		break;
	case ISPIOC_V4L2_SET_ENABLE_SP2_ERR:
		ret = hailo15l_isp_set_enable_sp2_err(sd, arg);
		break;
	case ISPIOC_S_MIV_INFO:
	case ISPIOC_S_MIS_IRQADDR:
	case ISPIOC_S_MP_34BIT:
	case ISPIOC_RST_QUEUE:
	case ISPIOC_D_MIS_IRQADDR:
		ret = 0;
		break;

    case HAILO15_PAD_S_STREAM:
        ret = hailo15_isp_pad_s_stream(sd, arg);
        break;
	case HAILO15_PAD_QUERYCTRL:
		ret = hailo15_isp_queryctrl(sd, arg);
		break;
	case HAILO15_PAD_QUERY_EXT_CTRL:
		ret = hailo15_isp_query_ext_ctrl(sd, arg);
		break;
	case HAILO15_PAD_G_CTRL:
		ret = hailo15_isp_g_ctrl(sd, arg);
		break;
	case HAILO15_PAD_S_CTRL:
		ret = hailo15_isp_s_ctrl(sd, arg);
		break;
	case HAILO15_PAD_G_EXT_CTRLS:
		ret = hailo15_isp_g_ext_ctrls(sd, arg);
		break;
	case HAILO15_PAD_S_EXT_CTRLS:
		ret = hailo15_isp_s_ext_ctrls(sd, arg);
		break;
	case HAILO15_PAD_TRY_EXT_CTRLS:
		ret = hailo15_isp_try_ext_ctrls(sd, arg);
		break;
	case HAILO15_PAD_QUERYMENU:
		ret = hailo15_isp_querymenu(sd, arg);
		break;
	case HAILO15_PAD_STAT_SUBSCRIBE:
		ret = hailo15_isp_stat_subscribe(sd, arg);
		break;
	case HAILO15_PAD_STAT_UNSUBSCRIBE:
		ret = hailo15_isp_stat_unsubscribe(sd, arg);
		break;
	case HAILO15_TUNING:
		mutex_lock(&isp_dev->mlock);
		memcpy(&isp_dev->tuning_state, arg, sizeof(bool));
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	default:
		ret = -EINVAL;
		pr_debug("unsupported isp command %x.\n", cmd);
		break;
	}

	return ret;
}

static int hailo15_vsi_isp_init_events(struct hailo15_isp_device *isp_dev)
{
	size_t size = HAILO15_EVENT_RESOURCE_DATA_SIZE + offsetof(struct hailo15_isp_event_pkg, data);
	size_t size_page_align = PAGE_ALIGN(size);

	mutex_init(&isp_dev->event_resource.event_lock);
	isp_dev->event_resource.virt_addr = kmalloc(size_page_align, GFP_KERNEL);
	if (!isp_dev->event_resource.virt_addr)
		return -ENOMEM;

	isp_dev->event_resource.phy_addr =
		virt_to_phys(isp_dev->event_resource.virt_addr);
	isp_dev->event_resource.size = size;
	memset(isp_dev->event_resource.virt_addr, 0,
		   isp_dev->event_resource.size);
	return 0;
}

/*@TODO add clean events function*/

static int hailo15_vsi_isp_subscribe_event(struct v4l2_subdev *subdev,
					   struct v4l2_fh *fh,
					   struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(subdev, fh, sub);
	case HAILO15_DAEMON_ISP_EVENT:
	case HAILO15_ISP_EVENT_IRQ:
		return v4l2_event_subscribe(fh, sub, 8, NULL);
	default:
		return v4l2_event_subscribe(fh, sub,
						HAILO15_ISP_EVENT_QUEUE_SIZE, NULL);
	}
}

static int
hailo15_vsi_isp_unsubscribe_event(struct v4l2_subdev *subdev,
				  struct v4l2_fh *fh,
				  struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

static int hailo15_isp_enable_clocks(struct hailo15_isp_device *isp_dev)
{
	int ret;

	ret = clk_prepare_enable(isp_dev->p_clk);
	if (ret) {
		pr_err("Couldn't prepare and enable P clock\n");
		return ret;
	}
	isp_dev->is_p_clk_enabled = 1;

	ret = clk_prepare_enable(isp_dev->ip_clk);
	if (ret) {
		pr_err("Couldn't prepare and enable IP clock\n");
		clk_disable_unprepare(isp_dev->p_clk);
		isp_dev->is_p_clk_enabled = 0;
		return ret;
	}
	isp_dev->is_ip_clk_enabled = 1;

	enable_irq(isp_dev->irq[1]);
	return ret;
}

static void hailo15_isp_disable_clocks(struct hailo15_isp_device *isp_dev)
{
	disable_irq(isp_dev->irq[1]);

	if (isp_dev->is_ip_clk_enabled) {
		clk_disable_unprepare(isp_dev->ip_clk);
		isp_dev->is_ip_clk_enabled = 0;
	}

	if (isp_dev->is_p_clk_enabled) {
		clk_disable_unprepare(isp_dev->p_clk);
		isp_dev->is_p_clk_enabled = 0;
	}
}

static struct v4l2_subdev_core_ops hailo15_isp_core_ops = {
	.ioctl = hailo15_vsi_isp_priv_ioctl,
	.subscribe_event = hailo15_vsi_isp_subscribe_event,
	.unsubscribe_event = hailo15_vsi_isp_unsubscribe_event,
};

static int hailo15_isp_refcnt_inc_enable(struct hailo15_isp_device *isp_dev)
{
	mutex_lock(&isp_dev->mlock);

	isp_dev->refcnt++;

	if (isp_dev->refcnt == 1) {
		pm_runtime_get_sync(isp_dev->dev);
		hailo15_isp_enable_clocks(isp_dev);
	}

	mutex_unlock(&isp_dev->mlock);
	return 0;
}

static int hailo15_isp_refcnt_dec_disable(struct hailo15_isp_device *isp_dev)
{
	int ret;

	ret = 0;
	mutex_lock(&isp_dev->mlock);
	if (isp_dev->refcnt == 0) {
		ret = -EINVAL;
		goto out;
	}

	isp_dev->refcnt--;

	if (isp_dev->refcnt == 0) {
		hailo15_isp_disable_clocks(isp_dev);
		pm_runtime_put_sync(isp_dev->dev);
		goto out;
	}

out:
	mutex_unlock(&isp_dev->mlock);
	return ret;
}

static int hailo15_isp_clear_raw_bufs(struct hailo15_isp_device *isp_dev,
				      unsigned int sink_pad_index)
{
	struct hailo15_isp_raw_buf *pos, *npos;
	struct list_head *empty_queue;
	struct list_head *full_queue;
	struct mutex *full_lock;
	struct mutex *empty_lock;
	int ret = 0;

	empty_queue = hailo15_isp_get_empty_queue(isp_dev, sink_pad_index);
	if (!empty_queue) {
		pr_err("%s - no empty queue for raw%d\n", __func__, sink_pad_index);
		return -EINVAL;
	}
	full_queue = hailo15_isp_get_full_queue(isp_dev, sink_pad_index);
	if (!full_queue) {
		pr_err("%s - no full queue for raw%d\n", __func__, sink_pad_index);
		return -EINVAL;
	}
	empty_lock = hailo15_isp_get_empty_lock(isp_dev, sink_pad_index);
	if (!empty_lock) {
		pr_err("%s - no empty lock for raw%d\n", __func__, sink_pad_index);
		return -EINVAL;
	}
	full_lock = hailo15_isp_get_full_lock(isp_dev, sink_pad_index);
	if (!full_lock) {
		pr_err("%s - no full lock for raw%d\n", __func__, sink_pad_index);
		return -EINVAL;
	}

	mutex_lock(full_lock);
	list_for_each_entry_safe(pos, npos, full_queue, list) {
		trace_isp_raw_buffer_full_q_out(sink_pad_index, pos->index, pos->phys_addr);
		list_del(&pos->list);
		dma_free_coherent(isp_dev->dev, pos->size,
				pos->virt_addr, pos->phys_addr);
		kfree(pos);
	}
	mutex_unlock(full_lock);

	isp_dev->raw_frame_available[sink_pad_index] = false;

	mutex_lock(empty_lock);
	list_for_each_entry_safe(pos, npos, empty_queue, list) {
		trace_isp_raw_buffer_empty_q_out(sink_pad_index, pos->index, pos->phys_addr);
		list_del(&pos->list);
		dma_free_coherent(isp_dev->dev, pos->size,
				pos->virt_addr, pos->phys_addr);
		kfree(pos);
	}
	mutex_unlock(empty_lock);

	if (isp_dev->cur_raw_buf[sink_pad_index]) {
		dma_free_coherent(isp_dev->dev, isp_dev->cur_raw_buf[sink_pad_index]->size,
				isp_dev->cur_raw_buf[sink_pad_index]->virt_addr, isp_dev->cur_raw_buf[sink_pad_index]->phys_addr);
		kfree(isp_dev->cur_raw_buf[sink_pad_index]);
		isp_dev->cur_raw_buf[sink_pad_index] = NULL;
	}

	return ret;
}

/****************************/
/* v4l2 subdevice video ops */
/****************************/
static int hailo15_isp_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	int path;
	int pad_index;
	int sink_pad_index;
	int source_pad_index;
	int stream_cnt;
	size_t raw_buf_size;
	unsigned long flags;
	struct v4l2_subdev *subdev;
	struct media_pad *pad;
	struct hailo15_buffer *pos, *npos, *cur_tmp;
	struct list_head *empty_queue;
	struct hailo15_isp_device *isp_dev =
		container_of(sd, struct hailo15_isp_device, sd);
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(&isp_dev->sd);

	pr_debug("%s: enable=%d\n", __func__, enable);

	sink_pad_index = HAILO15_VID_GRP_TO_ISP_SINK_PAD(sd->grp_id);
	source_pad_index = HAILO15_VID_GRP_TO_ISP_SOURCE_PAD(sd->grp_id);

	spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
	for (pad_index = 0; pad_index < HAILO15_ISP_SINK_PAD_MAX; pad_index++) {
		if (isp_dev->stream_enabled[pad_index]) {
			stream_cnt++;
		}
	}
	spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);

	if (enable) {
		path = HAILO15_VID_GRP_TO_ISP_PATH(sd->grp_id);
		if (path < 0 || path >= ISP_MAX_PATH)
			return -EINVAL;
		if(path == ISP_MCM_IN){
			isp_dev->rdma_enable = 1;
			isp_dev->fe_enable = 1;
			isp_dev->dma_ready = 0;
			isp_dev->frame_end = 0;
			isp_dev->fe_ready = 0;
			return 0;
		}

		if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) {

			/* setup the raw buffers */
			if (isp_dev->input_fmt[sink_pad_index].format.width == 0 || isp_dev->input_fmt[sink_pad_index].format.height == 0) {
				pr_err("invalid input format for raw%d\n", sink_pad_index);
				return -EINVAL;
			}

			/* 2 bytes per pixel */
			raw_buf_size = isp_dev->input_fmt[sink_pad_index].format.width * isp_dev->input_fmt[sink_pad_index].format.height * 2;
			if (hailo15_isp_init_raw_bufs(isp_dev, sink_pad_index, raw_buf_size)) {
				pr_err("cannot initialize raw%d buffers\n", sink_pad_index);
				return -ENOMEM;
			}

			/* get the first empty buffer */
			empty_queue = hailo15_isp_get_empty_queue(isp_dev, sink_pad_index);
			if (!empty_queue) {
				pr_err("cannot get empty queue for raw%d\n", sink_pad_index);
				return -EINVAL;
			}

			isp_dev->cur_raw_buf[sink_pad_index] = list_first_entry_or_null(empty_queue, struct hailo15_isp_raw_buf, list);
			if (!isp_dev->cur_raw_buf[sink_pad_index]) {
				pr_err("cannot get cur raw%d buf\n", sink_pad_index);
				return -EINVAL;
			}

			/* remove the buffer from the empty queue */
			trace_isp_raw_buffer_empty_q_out(sink_pad_index, isp_dev->cur_raw_buf[sink_pad_index]->index, isp_dev->cur_raw_buf[sink_pad_index]->phys_addr);
			list_del(&isp_dev->cur_raw_buf[sink_pad_index]->list);

			isp_dev->rdma_enable = 1;
			isp_dev->fe_enable = 1;

			/* if this is the first stream, do initializations */
			spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
			if (stream_cnt == 0) {
				isp_dev->fe_switch.next_vdid[0] = sink_pad_index;
				isp_dev->fe_switch.next_vdid[1] = sink_pad_index;
				isp_dev->fe_dev->fe.curr_vdid = sink_pad_index;

				isp_dev->dma_ready = 0;
				isp_dev->frame_end = 0;
				isp_dev->fe_ready = 0;
				isp_dev->cur_buf_path = sd->grp_id;
			}
			spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);
		} else {
			isp_dev->cur_buf_path = sd->grp_id;
		}

		ret = hailo15_isp_refcnt_inc_enable(isp_dev);
		if (ret)
			return ret;

		/* configure the wrapper only on the first stream */
		if (stream_cnt == 0) {
			hailo15_config_isp_wrapper(isp_dev);
		}

		isp_dev->queue_empty[sd->grp_id] = 0;
		isp_dev->current_vsm_index[sd->grp_id] = -1;

		mutex_lock(&isp_dev->ready_lock);
		isp_dev->output_ready = 1;
		mutex_unlock(&isp_dev->ready_lock);

		ret = hailo15_isp_post_event_start_stream(isp_dev, source_pad_index);
		if (ret) {
			pr_warn("%s - start stream event failed with %d\n", __func__, ret);
			return ret;
		}

		/* set the raw0, raw1 address after posting event so daemon doesn't override the address */
		if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) {

			/* if this is the first stream, set the raw frame base immediately */
			spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
			if (stream_cnt == 0) {
				hailo15_isp_configure_mcm_raw_frame_base(isp_dev,
					&isp_dev->cur_raw_buf[sink_pad_index]->phys_addr, sink_pad_index);
			}
			spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);
		}

		if(isp_dev->mcm_mode){
			hailo15_isp_configure_frame_size(isp_dev, sd->grp_id);
			hailo15_isp_configure_buffer(isp_dev, isp_dev->cur_buf[sd->grp_id]);
		}
	}

	if(!isp_dev->rdma_enable || isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR){
		pad = &isp_dev->pads[sink_pad_index];
		if (pad)
			pad = media_entity_remote_pad(pad);

		if (pad && is_media_entity_v4l2_subdev(pad->entity)) {
			subdev = media_entity_to_v4l2_subdev(pad->entity);
			subdev->grp_id = sd->grp_id;
			ret = v4l2_subdev_call(subdev, video, s_stream, enable);
			if (ret) {
				pr_err("%s - s_stream to subdev %s failed, err = (%pe)\n", __func__, subdev->name, ERR_PTR(ret));
				goto disable;
			}
		}
	}

	// enable frame timeout timer only after subdevs are enabled
	if (enable) {
		spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
		isp_dev->stream_enabled[sink_pad_index] = 1;
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);

		/* if this is not the first stream, toggle the sensors */
		if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR && stream_cnt > 0) {
			isp_dev->toggle_sensors = true;
		}

		timer_setup(&isp_dev->frame_timer[sink_pad_index],
		    sink_pad_index == HAILO15_ISP_SINK_PAD_S0 ? frame_timeout_handler_0 : frame_timeout_handler_1, 0);
		if (mod_timer(&isp_dev->frame_timer[sink_pad_index], jiffies + msecs_to_jiffies(FRAME_TIMEOUT_MS))) {
			pr_warn("frame timer was pending after setup\n");
		}
	}

	// disable clocks only after disabling the stream for all the subdevs
	if (!enable) {
		path = HAILO15_VID_GRP_TO_ISP_PATH(sd->grp_id);
		if (path < 0 || path >= ISP_MAX_PATH)
			return -EINVAL;

		/* in vid-out disable, no need to stop clocks or post stop stream event */
		if (path == ISP_MCM_IN)
			goto disable_rdma;

		del_timer_sync(&isp_dev->frame_timer[sink_pad_index]);
		atomic_set(&isp_dev->streaming_started[sink_pad_index], 0);

disable:
		spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
		isp_dev->stream_enabled[sink_pad_index] = 0;
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);

		/* if we disable a stream in multi sensor mode, and we are in toggle state,
		   we need to wait for the toggle to be disabled at the right time before actually stopping the stream */
		if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR && stream_cnt > 1) {
			ret = wait_event_interruptible(isp_dev->toggle_sensors_wait_q, !isp_dev->toggle_sensors);
			if (ret == -ERESTARTSYS) {
				pr_warn("%s - wait_event_interruptible got interrupted\n", __func__);
			}
		}

		ret = hailo15_isp_post_event_stop_stream(isp_dev, source_pad_index);
		if (ret) {
			pr_warn("%s - stop stream event failed with %d\n", __func__, ret);
		}

		/* these resets are only neccessary for mcm mode
		   when using vid-out, rdma_enable set to zero only when closing the stream on the vid-out */
disable_rdma:
		if ((stream_cnt <= 1 && isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) ||
			(path == ISP_MCM_IN)) {
			isp_dev->rdma_enable = 0;
			isp_dev->dma_ready = 0;
			isp_dev->frame_end = 0;
			isp_dev->fe_ready = 0;
		}

		if(path == ISP_MCM_IN) {
			mutex_lock(&isp_dev->ready_lock);
			isp_dev->mcm_waiting = 0;
			mutex_unlock(&isp_dev->ready_lock);

			mutex_lock(&isp_dev->mcm_lock);
			list_for_each_entry_safe(pos, npos, &isp_dev->mcm_queue, irqlist){
				list_del(&pos->irqlist);
				hailo15_dma_buffer_done(ctx, sd->grp_id, pos);
			}

			cur_tmp = isp_dev->cur_buf[HAILO15_VID_GRP_MCM_IN];
			isp_dev->cur_buf[HAILO15_VID_GRP_MCM_IN] = NULL;
			hailo15_dma_buffer_done(ctx, sd->grp_id, cur_tmp);
			mutex_unlock(&isp_dev->mcm_lock);
			return ret;
		}

		isp_dev->frame_count[sd->grp_id] = 0;
		isp_dev->mi_stopped[path] = 1;

		/* indicate to vid-out to stop queueing in case it's running */
		mutex_lock(&isp_dev->ready_lock);
		isp_dev->output_ready = 0;
		mutex_unlock(&isp_dev->ready_lock);

		isp_dev->cur_buf[sd->grp_id] = NULL;
		memset(&isp_dev->input_fmt[sink_pad_index], 0, sizeof(isp_dev->input_fmt[sink_pad_index]));

		if (stream_cnt <= 1) {
			/* make sure fe completes so no wait gets stuck */
			isp_dev->fe_dev->fe_isp_irq_work(isp_dev->fe_dev);

			if(isp_dev->rmem_vaddr[sink_pad_index]){
				dma_free_coherent(isp_dev->dev, isp_dev->rmem[sink_pad_index].size,
					isp_dev->rmem_vaddr[sink_pad_index], isp_dev->rmem[sink_pad_index].addr);
				isp_dev->rmem_vaddr[sink_pad_index] = 0;
			}

			drain_workqueue(isp_dev->isp_mis_wq);
			if(isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) {
				wake_up_interruptible_all(&isp_dev->raw_frame_available_wait_q);
				drain_workqueue(isp_dev->mcm_wr_raw_wq);
			}

			/* in multi sensor, miv2_mis work can wait on raw_frame_available_wait_q
			so flush only after raw_frame_available_wait_q is woken up */
			drain_workqueue(isp_dev->miv2_mis_wq);
		}

		if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) {
			if (hailo15_isp_clear_raw_bufs(isp_dev, sink_pad_index)) {
				pr_err("%s - failed to clear raw%d bufs\n", __func__, sink_pad_index);
				ret = -ENOMEM;
			}
			if (stream_cnt <= 1) {
				atomic_set(&isp_dev->first_rdma_done, 0);
			}
		}

		if (stream_cnt <= 1) {

			/* set after posting the event */
			isp_dev->fe_enable = 0;

			hailo15_isp_reset_hw(isp_dev);
		}

		hailo15_isp_refcnt_dec_disable(isp_dev);
	}

	return ret;
}

static struct v4l2_subdev_video_ops hailo15_isp_video_ops = {
	.s_stream = hailo15_isp_s_stream,
};

bool hailo15_isp_is_format_hdr(struct v4l2_subdev_format *format)
{
    if (format->format.width == 0 || format->format.height == 0)
        return false;

    if (format->format.width > INPUT_WIDTH ||
        format->format.height > INPUT_HEIGHT) {
        pr_debug("Unsupported resolution %dx%d\n", format->format.width,
             format->format.height);
        return false;
    }

    if (format->format.code == MEDIA_BUS_FMT_SRGGB12_2X12 || format->format.code == MEDIA_BUS_FMT_SRGGB12_3X12) {
        return true;
    }

    return false;
}

/**************************/
/* v4l2 subdevice pad ops */
/**************************/
static int hailo15_isp_set_fmt(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_format *format)

{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct v4l2_subdev *sensor_sd;
	int ret = -EINVAL;
	struct media_pad *pad;
	int isp_path;
	int sink_pad_idx = HAILO15_ISP_SINK_PAD_MAX;
	int src_pad_idx = HAILO15_ISP_SOURCE_PAD_MAX;
	struct v4l2_subdev *subdev;
	int max_width = INPUT_WIDTH;
	int max_height = INPUT_HEIGHT;

	sink_pad_idx = HAILO15_VID_GRP_TO_ISP_SINK_PAD(sd->grp_id);
	if (sink_pad_idx < 0) {
		pr_warn("%s - invalid sink pad index %d\n", __func__, sink_pad_idx);
		return -EINVAL;
	}

	if (isp_dev->input_fmt[sink_pad_idx].format.width != 0 &&
		isp_dev->input_fmt[sink_pad_idx].format.height != 0) {
		max_width = isp_dev->input_fmt[sink_pad_idx].format.width;
		max_height = isp_dev->input_fmt[sink_pad_idx].format.height;
	}

	/* We don't support width upscaling */
	if (format->format.width > max_width) {
		return -EINVAL;
	}

	/* if larger height than the input is requested
	   set the format to the input height, output buffer
	   will be allocated according to requested resolution */
	if (format->format.height > max_height) {
		format->format.height = max_height;
	}

	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
		return 0;
	}

	isp_path = HAILO15_VID_GRP_TO_ISP_PATH(sd->grp_id);
	if (isp_path < 0 || isp_path >= ISP_MAX_PATH) {
		pr_err("set_fmt: invalid isp path %d\n", isp_path);
		return -EINVAL;
	}

	src_pad_idx = HAILO15_VID_GRP_TO_ISP_SOURCE_PAD(sd->grp_id);
	if (src_pad_idx < 0) {
		pr_err("%s - invalid source pad index %d\n", __func__, src_pad_idx);
		return -EINVAL;
	}

	memcpy(&isp_dev->fmt[isp_path], format,
		   sizeof(struct v4l2_subdev_format));
	ret = hailo15_isp_post_event_set_fmt(
		isp_dev, isp_dev->pads[src_pad_idx].index,
		&(format->format));
	if (ret) {
		pr_err("%s - set_fmt event failed with %d\n", __func__, ret);
		return ret;
	}

	if(isp_dev->rdma_enable && isp_dev->mcm_mode != ISP_MCM_MODE_MULTI_SENSOR){
		return 0;
	}

	pad = &isp_dev->pads[sink_pad_idx];
	if (pad) {
		pad = media_entity_remote_pad(pad);
	}

	if (pad && is_media_entity_v4l2_subdev(pad->entity)) {
		subdev = media_entity_to_v4l2_subdev(pad->entity);
		subdev->grp_id = sd->grp_id;
		ret = v4l2_subdev_call(subdev, pad, set_fmt, NULL, format);
		if (ret) {
			pr_err("%s - set_fmt to subdev %s failed, err = (%pe)\n", __func__, subdev->name, ERR_PTR(ret));
			return ret;
		}
	}

	/* if input_format was not set - no need to update the sensor */
	if (isp_dev->input_fmt[sink_pad_idx].format.width == 0 ||
		isp_dev->input_fmt[sink_pad_idx].format.height == 0) {
		pr_debug("%s - input format was not set - no need to update the sensor\n", __func__);
		return 0;
	}

	sensor_sd = hailo15_get_sensor_subdev(isp_dev->sd.v4l2_dev->mdev, sd->grp_id);
	if (!sensor_sd) {
		pr_err("%s - failed to get sensor subdev\n", __func__);
		return -EINVAL;
	}

	/* the sensor only has a single pad, set to 0 */
	isp_dev->input_fmt[sink_pad_idx].pad = 0;
	ret = v4l2_subdev_call(sensor_sd, pad, set_fmt, NULL, &isp_dev->input_fmt[sink_pad_idx]);
	if (ret) {
		pr_err("%s - set_fmt to subdev %s failed, err = (%pe)\n", __func__, sensor_sd->name, ERR_PTR(ret));
		return ret;
	}

    isp_dev->hdr_enabled = hailo15_isp_is_format_hdr(&isp_dev->input_fmt[sink_pad_idx]);

	return ret;
}

static const struct v4l2_subdev_pad_ops hailo15_isp_pad_ops = {
	.set_fmt = hailo15_isp_set_fmt,
};


/**********************/
/* v4l2 subdevice ops */
/**********************/

/* This is the parent structure that contains all supported v4l2 subdevice operations */
struct v4l2_subdev_ops hailo15_isp_subdev_ops = {
	.core = &hailo15_isp_core_ops,
	.video = &hailo15_isp_video_ops,
	.pad = &hailo15_isp_pad_ops,
};

/**********************/
/* media entity ops   */
/**********************/
static int hailo15_isp_link_setup(struct media_entity *entity,
				  const struct media_pad *local,
				  const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations hailo15_isp_entity_ops = {
	.link_setup = hailo15_isp_link_setup,
};


/********************************/
/*  v4l2 subdevice internal ops */
/********************************/
static int hailo15_isp_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);

	return hailo15_isp_refcnt_inc_enable(isp_dev);
}

static int hailo15_isp_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	return hailo15_isp_refcnt_dec_disable(isp_dev);
}

static int hailo15_isp_registered(struct v4l2_subdev* sd){
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;

	return hailo15_media_create_links(isp_dev->dev, &sd->entity, -1);
}

static struct v4l2_subdev_internal_ops hailo15_isp_internal_ops = {
	.open = hailo15_isp_open,
	.close = hailo15_isp_close,
	.registered = hailo15_isp_registered,
};

/**********************************************************/
/* Hailo15 buffer operations.                             */
/* this ops are defining the API between the video device */
/* and the isp device                                     */
/**********************************************************/
inline void hailo15_isp_buffer_done(struct hailo15_isp_device *isp_dev,
					int grp_id)
{
	struct hailo15_buffer *buf, *next_buf = NULL;
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(&isp_dev->sd);
	int path;

	path = HAILO15_VID_GRP_TO_ISP_PATH(grp_id);
	if (path < 0 || path >= ISP_MAX_PATH)
		return;

	++isp_dev->frame_count[grp_id];

	if(path == ISP_MCM_IN){
		mutex_lock(&isp_dev->mcm_lock);
		buf = isp_dev->cur_buf[grp_id];
		isp_dev->cur_buf[grp_id] = list_first_entry_or_null(&isp_dev->mcm_queue, struct hailo15_buffer, irqlist);
		if(!buf){
			mutex_unlock(&isp_dev->mcm_lock);
			return;
		}
		if(isp_dev->cur_buf[grp_id]){
			list_del(&isp_dev->cur_buf[grp_id]->irqlist);
			next_buf = isp_dev->cur_buf[grp_id];
		}
		mutex_unlock(&isp_dev->mcm_lock);
		if(next_buf){
			hailo15_isp_configure_frame_base(isp_dev, next_buf->dma, grp_id);
		}

	} else{

		buf = isp_dev->cur_buf[grp_id];
		isp_dev->cur_buf[grp_id] = NULL;

		if (isp_dev->current_vsm_index[grp_id] >= 0 &&
			isp_dev->current_vsm_index[grp_id] < HAILO15_MAX_BUFFERS) {
			isp_dev->vsm_list[isp_dev->current_vsm_index[grp_id]][grp_id] =
				isp_dev->current_vsm;
			memset(&isp_dev->current_vsm, 0, sizeof(struct hailo15_vsm));
		}

		if (buf) {
			isp_dev->current_vsm_index[grp_id] = buf->vb.vb2_buf.index;

			/* Read the timestamp of the MCM buffer (sent from hdr_manager via DMA). */
			mutex_lock(&isp_dev->mcm_lock);
			if (isp_dev->cur_buf[HAILO15_VID_GRP_MCM_IN]) {
				buf->vb.vb2_buf.timestamp = isp_dev->cur_buf[HAILO15_VID_GRP_MCM_IN]->vb.vb2_buf.timestamp;
			} else {
				buf->vb.vb2_buf.timestamp = 0;
			}
			mutex_unlock(&isp_dev->mcm_lock);
		} else {
			isp_dev->current_vsm_index[grp_id] = -1;
		}

	}

	if (buf) {
		if(hailo15_dma_buffer_done(ctx, buf->grp_id, buf)) {
			pr_err("%s: hailo15_dma_buffer_done failed for grp_id %d\n", __func__, buf->grp_id);
		}
	} else {
		if(hailo15_dma_buffer_done(ctx, grp_id, buf)) {
			pr_err("%s: hailo15_dma_buffer_done failed for grp_id %d\n", __func__, grp_id);
		}
	}
}

static int hailo15_isp_buffer_process(struct hailo15_dma_ctx *ctx,
					  struct hailo15_buffer *buf, bool is_buf_time_synced)
{
	struct v4l2_subdev *sd;
	struct hailo15_isp_device *isp_dev;
	int path;

	sd = buf->sd;
	isp_dev = container_of(sd, struct hailo15_isp_device, sd);

	if (!is_buf_time_synced && isp_dev->mcm_mode != ISP_MCM_MODE_INJECTION) {
		/* If buffer is processed on wrong time, this is allowed only in injection mode.
		Because in injection mode, we can't always process buffers right after
		the latest buffer_done of the previous one (buffer_done can't be called with null buffer in injection mode).
		*/
		return -EAGAIN;
	}

	/*configure buffer to hw*/
	hailo15_isp_configure_buffer(isp_dev, buf);

	path = HAILO15_VID_GRP_TO_ISP_PATH(buf->grp_id);
	if (path < 0 || path >= ISP_MAX_PATH)
		return -EINVAL;

	if (isp_dev->queue_empty[buf->grp_id]) {
		hailo15_isp_dma_set_enable(isp_dev, path, 1);
		isp_dev->queue_empty[buf->grp_id] = 0;
		if (isp_dev->mcm_mode == ISP_MCM_MODE_INJECTION) {
			mutex_lock(&isp_dev->ready_lock);
			if (isp_dev->mcm_waiting && path == ISP_MP) {
				 /* indicates that the MCM can be done with a buffer */
				isp_dev->output_ready = 1;
				hailo15_isp_buffer_done(isp_dev, HAILO15_VID_GRP_MCM_IN);
				isp_dev->mcm_waiting = 0;
			}
			mutex_unlock(&isp_dev->ready_lock);
		}
	}

	return 0;
}

static int hailo15_isp_get_frame_count(struct hailo15_dma_ctx *dma_ctx, int grp_id,
					   uint64_t *fc)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)dma_ctx->dev;

	if (!fc)
		return -EINVAL;

	*fc = isp_dev->frame_count[grp_id];
	return 0;
}

static int hailo15_isp_get_rmem(struct hailo15_dma_ctx *dma_ctx,
				struct hailo15_rmem *rmem)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)dma_ctx->dev;
	int port = HAILO15_VID_GRP_TO_ISP_SINK_PAD(((struct hailo15_rmem *)rmem)->port);
	if (port >= HAILO15_ISP_SINK_PAD_MAX) {
		dev_err(isp_dev->dev, "%s - invalid port %d\n", __func__, port);
		return -EINVAL;
	}
	memcpy(rmem, &isp_dev->rmem[port], sizeof(struct hailo15_rmem));
	return 0;
}

static int
hailo15_isp_get_event_resource(struct hailo15_dma_ctx *dma_ctx,
				   struct hailo15_event_resource *resource)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)dma_ctx->dev;
	memcpy(resource, &isp_dev->event_resource,
		   sizeof(struct hailo15_event_resource));
	return 0;
}

static int hailo15_isp_get_fbuf(struct hailo15_dma_ctx *ctx,
				struct v4l2_framebuffer *fbuf)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)ctx->dev;
	fbuf->base = (void *)isp_dev->fbuf_phys;
	fbuf->fmt.sizeimage = HAILO15_ISP_FBUF_SIZE;
	return 0;
}

static int hailo15_isp_set_private_data(struct hailo15_dma_ctx *ctx, int grp_id,
					void *data)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)ctx->dev;

	if (grp_id < 0 || grp_id >= HAILO15_VID_GRP_SX_MAX)
		return -EINVAL;

	isp_dev->private_data[grp_id] = data;
	return 0;
}

static int hailo15_isp_get_private_data(struct hailo15_dma_ctx *ctx, int grp_id,
					void **data)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)ctx->dev;

	if (!data)
		return -EINVAL;

	if (grp_id < 0 || grp_id >= HAILO15_VID_GRP_SX_MAX)
		return -EINVAL;

	*data = isp_dev->private_data[grp_id];
	return 0;
}
static int hailo15_isp_get_vsm(struct hailo15_dma_ctx *ctx, int grp_id,
				   int index, struct hailo15_vsm *vsm)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)ctx->dev;
	int isp_path;
	if (index < 0 || index >= HAILO15_MAX_BUFFERS)
		return -EINVAL;

	isp_path = HAILO15_VID_GRP_TO_ISP_PATH(grp_id);
	if (isp_path < 0 || isp_path >= ISP_MAX_PATH) {
		return -EINVAL;
	}

	memcpy(vsm, &isp_dev->vsm_list[index][grp_id],
		   sizeof(struct hailo15_vsm));
	return 0;
}

static int hailo15_isp_queue_empty(struct hailo15_dma_ctx *ctx, int grp_id)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)ctx->dev;
	dma_addr_t fakebuf_arr[FMT_MAX_PLANES];
	int i;
	int path = HAILO15_VID_GRP_TO_ISP_PATH(grp_id);

	if (path < 0 || path >= ISP_MAX_PATH)
		return -EINVAL;

	for (i = 0; i < FMT_MAX_PLANES; ++i) {
		fakebuf_arr[i] = isp_dev->fakebuf_phys;
	}

	hailo15_isp_configure_frame_base(isp_dev, fakebuf_arr, grp_id);
	hailo15_isp_dma_set_enable(isp_dev, path, 0);
	isp_dev->queue_empty[grp_id] = 1;
	isp_dev->output_ready = 0;
	return 0;
}

static struct hailo15_buf_ops hailo15_isp_buf_ops = {
	.buffer_process =
		hailo15_isp_buffer_process, /*should it return a value once we validate things?*/
	.get_frame_count = hailo15_isp_get_frame_count,
	.get_rmem = hailo15_isp_get_rmem,
	.get_event_resource = hailo15_isp_get_event_resource,
	.get_fbuf = hailo15_isp_get_fbuf,
	.set_private_data = hailo15_isp_set_private_data,
	.get_private_data = hailo15_isp_get_private_data,
	.get_vsm = hailo15_isp_get_vsm,
	.queue_empty = hailo15_isp_queue_empty,
};

/* Perform any platform device related initialization.      */
/* These include irq line request and address space mapping */
static int hailo15_isp_init_platdev(struct hailo15_isp_device *isp_dev)
{
	int ret;
	struct device *dev = isp_dev->dev;
	struct platform_device *pdev;
	struct resource *mem_res;
	struct resource *wrapper_mem_res;
	pdev = container_of(isp_dev->dev, struct platform_device, dev);

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	wrapper_mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);

	if (!mem_res) {
		dev_err(dev, "can't get memory resource for isp\n");
		return -ENOENT;
	}

	if (!wrapper_mem_res) {
		dev_err(dev, "can't get memory resource for isp wrapper\n");
		kfree(mem_res);
		return -ENOENT;
	}


	isp_dev->base = devm_ioremap_resource(&pdev->dev, mem_res);
	if (IS_ERR(isp_dev->base)) {
		dev_err(dev, "can't get reg mem resource for isp\n");
		return -ENOMEM;
	}

	isp_dev->wrapper_base =
		devm_ioremap_resource(&pdev->dev, wrapper_mem_res);
	if (IS_ERR(isp_dev->wrapper_base)) {
		dev_err(dev, "can't get reg mem resource for isp wrapper\n");
		return -ENOMEM;
	}

	isp_dev->p_clk = devm_clk_get(&pdev->dev, "p_clk");
	if (IS_ERR(isp_dev->p_clk)) {
		dev_err(&pdev->dev, "Couldn't get p clock\n");
		return PTR_ERR(isp_dev->p_clk);
	}

	isp_dev->ip_clk = devm_clk_get(&pdev->dev, "ip_clk");
	if (IS_ERR(isp_dev->ip_clk)) {
		dev_err(&pdev->dev, "Couldn't get ip clock\n");
		return PTR_ERR(isp_dev->ip_clk);
	}

	isp_dev->irq[0] = platform_get_irq(pdev, HAILO15_ISP_IRQ_EVENT_ISP_MIS);
	if (isp_dev->irq[0] < 0) {
		dev_err(dev, "can't get isp irq resource\n");
		return -ENXIO;
	}

	ret = devm_request_irq(dev, isp_dev->irq[0], hailo15_isp_irq_handler, 0,
				   dev_name(dev), isp_dev);
	if (ret) {
		dev_err(dev, "request isp irq error\n");
		return ret;
	}

	isp_dev->irq[1] = platform_get_irq(pdev, 1);
	if (isp_dev->irq[1] < 0) {
		dev_err(dev, "can't get isp irq resource\n");
		return -ENXIO;
	}

	ret = devm_request_irq(dev, isp_dev->irq[1], hailo15_isp_err_irq_handler, IRQF_NO_AUTOEN,
				   dev_name(dev), isp_dev);
	if (ret) {
		dev_err(dev, "request isp irq error\n");
		return ret;
	}

	return 0;
}

static void hailo15_isp_destroy_platdev(struct hailo15_isp_device *isp_dev)
{
	int port;
	hailo15_isp_disable_clocks(isp_dev);
	for (port = 0; port < HAILO15_ISP_SINK_PAD_MAX; port++) {
		if(isp_dev->rmem_vaddr[port]){
			dma_free_coherent(isp_dev->dev, isp_dev->rmem[port].size,
			  isp_dev->rmem_vaddr[port], isp_dev->rmem[port].addr);
			isp_dev->rmem_vaddr[port] = 0;
		}
	}
}

/* Perform v4l2 subdevice specific initializations.            */
/* This function must be called before the actual registration */
static void hailo15_isp_init_v4l2_subdev(struct v4l2_subdev *sd)
{
	struct hailo15_isp_device *isp_dev =
		container_of(sd, struct hailo15_isp_device, sd);
	v4l2_subdev_init(sd, &hailo15_isp_subdev_ops);
	sd->dev = isp_dev->dev;
	sd->fwnode = dev_fwnode(isp_dev->dev);
	sd->internal_ops = &hailo15_isp_internal_ops;
	snprintf(sd->name, HAILO15_ISP_NAME_SIZE, "%s", HAILO15_ISP_NAME);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	sd->owner = THIS_MODULE;

	sd->entity.obj_type = MEDIA_ENTITY_TYPE_V4L2_SUBDEV;
	sd->entity.function = MEDIA_ENT_F_IO_V4L;

	sd->entity.ops = &hailo15_isp_entity_ops;
}

/* Initializes media pads type and pad handles. */
static inline int
hailo15_isp_init_pads(struct hailo15_isp_device *isp_dev)
{
	int pad, ret;

    // Initialize pad flags for sink and source pads
	for (pad = 0; pad < HAILO15_ISP_SINK_PAD_MAX; ++pad) {
		isp_dev->pads[pad].flags = MEDIA_PAD_FL_SINK;
	}

	for (; pad < HAILO15_ISP_SOURCE_PAD_MAX; ++pad) {
		isp_dev->pads[pad].flags = MEDIA_PAD_FL_SOURCE;
	}

	ret = media_entity_pads_init(&isp_dev->sd.entity, HAILO15_ISP_PADS_NR,
					 isp_dev->pads);
	if (ret) {
		dev_err(isp_dev->dev, "media entity init error\n");
	}

	return ret;
}

static inline void
hailo15_isp_destroy_media_pads(struct hailo15_isp_device *isp_dev)
{
	hailo15_media_entity_clean(&isp_dev->sd.entity);
}

static int hailo15_isp_parse_null_addr(struct hailo15_isp_device* isp_dev) {
	struct fwnode_handle *isp_node = NULL, *parent_node = NULL;
	int ret = -EINVAL;

	if (!isp_dev || !isp_dev->dev) {
		pr_err("%s: Invalid isp_dev or isp_dev->dev pointer\n", __func__);
		return -EINVAL;
	}

	isp_node = dev_fwnode(isp_dev->dev);
	if (!isp_node) {
		dev_err(isp_dev->dev, "Failed to get fwnode for ISP device\n");
		return -ENODEV;
	}

	parent_node = fwnode_get_parent(isp_node);
	if (!parent_node) {
		dev_err(isp_dev->dev, "Failed to get parent fwnode (vision_subsys)\n");
		return -ENODEV;
	}

	// Read the property from the *parent* node
	ret = fwnode_property_read_u32(parent_node, "null-addr", &isp_dev->null_addr);
	if (ret) {
		dev_err(isp_dev->dev, "Failed to read 'null-addr' from parent node. ret: %d\n", ret);
	} else {
		dev_dbg(isp_dev->dev, "Successfully read null_addr=0x%x from parent node\n", isp_dev->null_addr);
	}

	fwnode_handle_put(parent_node);

	return ret;
}

static int hailo15_isp_init_raw_bufs(struct hailo15_isp_device *isp_dev, int sink_pad_index, size_t size) {
	struct hailo15_isp_raw_buf *raw_buf;
	struct list_head *empty_queue;
	int i;

	empty_queue = hailo15_isp_get_empty_queue(isp_dev, sink_pad_index);
	if (!empty_queue) {
		dev_err(isp_dev->dev, "cannot get empty queue for raw%d\n", sink_pad_index);
		return -EINVAL;
	}

	for (i = 0; i < HAILO15_ISP_RAW_BUFS_NUM; ++i) {
		raw_buf = kzalloc(sizeof(struct hailo15_isp_raw_buf), GFP_KERNEL);
		if (!raw_buf) {
			dev_err(isp_dev->dev, "cannot allocate raw%d buf struct %d\n", sink_pad_index, i);
			goto err_alloc_raw;
		}
		raw_buf->virt_addr =
			dma_alloc_coherent(isp_dev->dev, size,
					&(raw_buf->phys_addr), GFP_USER);
		if (!raw_buf->virt_addr) {
			dev_err(isp_dev->dev, "cannot allocate raw%d buf %d\n", sink_pad_index, i);
			goto err_alloc_raw;
		}
		raw_buf->index = i;
		raw_buf->size = size;
		trace_isp_raw_buffer_empty_q_in(sink_pad_index, raw_buf->index, raw_buf->phys_addr);
		list_add_tail(&raw_buf->list, empty_queue);
	}

	return 0;

/* free the allocated raw buffers dma addresses */
err_alloc_raw:
	list_for_each_entry_safe(raw_buf, raw_buf, empty_queue, list) {
		list_del(&raw_buf->list);
		dma_free_coherent(isp_dev->dev, size, raw_buf->virt_addr, raw_buf->phys_addr);
		kfree(raw_buf);
	}
	return -ENOMEM;

}

/* Init the isp device.                               */
/* These include any previous initialization function */
static int hailo15_init_isp_device(struct hailo15_isp_device *isp_dev)
{
	int ret;
	int path;

	if (!isp_dev)
		return -EINVAL;

	ret = hailo15_isp_parse_null_addr(isp_dev);
	if(ret){
		dev_err(isp_dev->dev, "can't parse null address\n");
		return ret;
	}

	mutex_init(&isp_dev->mlock);
	mutex_init(&isp_dev->ctrl_lock);
	mutex_init(&isp_dev->raw0_full_lock);
	mutex_init(&isp_dev->raw0_empty_lock);
	mutex_init(&isp_dev->raw1_full_lock);
	mutex_init(&isp_dev->raw1_empty_lock);

	ret = hailo15_isp_init_platdev(isp_dev);
	if (ret) {
		dev_err(isp_dev->dev, "cannot parse platform device\n");
		goto err_init_platdev;
	}

    isp_dev->hdr_enabled = false;
	hailo15_isp_init_v4l2_subdev(&isp_dev->sd);

	ret = hailo15_isp_init_pads(isp_dev);
	if (ret) {
		dev_err(isp_dev->dev, "cannot initialize media pads\n");
		goto err_init_media_pads;
	}

	pm_runtime_enable(isp_dev->dev);
	isp_dev->fbuf_vaddr =
		dma_alloc_coherent(isp_dev->dev, HAILO15_ISP_FBUF_SIZE,
				   &isp_dev->fbuf_phys, GFP_USER);
	if (!isp_dev->fbuf_vaddr) {
		dev_err(isp_dev->dev, "cannot allocate framebuffer\n");
		goto err_alloc_fbuf;
	}

	INIT_LIST_HEAD(&isp_dev->raw0_full_queue);
	INIT_LIST_HEAD(&isp_dev->raw0_empty_queue);

	INIT_LIST_HEAD(&isp_dev->raw1_full_queue);
	INIT_LIST_HEAD(&isp_dev->raw1_empty_queue);

	memset(&isp_dev->fe_switch, 0, sizeof(isp_dev->fe_switch));
	isp_dev->fe_switch.vd_mode = 1;
	isp_dev->fe_switch.next_vdid[0] = 0;
	isp_dev->fe_switch.next_vdid[1] = 0;

	atomic_set(&isp_dev->first_rdma_done, 0);

	for (path = ISP_MP; path < ISP_MAX_PATH; ++path)
		isp_dev->mi_stopped[path] = 1;

	isp_dev->fakebuf_vaddr =
		dma_alloc_coherent(isp_dev->dev, HAILO15_ISP_FAKEBUF_SIZE,
				   &isp_dev->fakebuf_phys, GFP_USER);
	if (!isp_dev->fakebuf_vaddr) {
		dev_err(isp_dev->dev, "cannot allocate fakebuf\n");
		goto err_alloc_fakebuf;
	}

	/*queue_empty field should not be set to 1 as the software assumes empty queue on initialization*/
	INIT_LIST_HEAD(&isp_dev->mcm_queue);
	mutex_init(&isp_dev->mcm_lock);
	mutex_init(&isp_dev->ready_lock);
	isp_dev->output_ready = 1;
	isp_dev->mcm_waiting = 0;

	isp_dev->miv2_mis_wq = alloc_ordered_workqueue("miv2_mis_wq", WQ_HIGHPRI);
	isp_dev->mcm_wr_raw_wq = alloc_ordered_workqueue("mcm_wr_raw_wq", WQ_HIGHPRI);
	isp_dev->isp_mis_wq = alloc_ordered_workqueue("isp_mis_wq", WQ_HIGHPRI);

	init_waitqueue_head(&isp_dev->raw_frame_available_wait_q);
	isp_dev->raw_frame_available[HAILO15_ISP_SINK_PAD_S0] = false;
	isp_dev->raw_frame_available[HAILO15_ISP_SINK_PAD_S1] = false;

	init_waitqueue_head(&isp_dev->toggle_sensors_wait_q);
	isp_dev->toggle_sensors = false;

	spin_lock_init(&isp_dev->stream_state_lock);

	tasklet_init(&isp_dev->fe_tasklet, mcm_fe_irq_tasklet, (unsigned long)isp_dev);
	init_waitqueue_head(&isp_dev->buf_done_wait_q);
	atomic_set(&isp_dev->buf_done_ready, 0);

	goto out;


err_alloc_fakebuf:
	dma_free_coherent(isp_dev->dev, HAILO15_ISP_FBUF_SIZE,
			  isp_dev->fbuf_vaddr, isp_dev->fbuf_phys);
err_alloc_fbuf:
	pm_runtime_disable(isp_dev->dev);
err_init_media_pads:
	hailo15_isp_destroy_platdev(isp_dev);
err_init_platdev:
	mutex_destroy(&isp_dev->mlock);
	mutex_destroy(&isp_dev->ctrl_lock);
	mutex_destroy(&isp_dev->raw0_full_lock);
	mutex_destroy(&isp_dev->raw0_empty_lock);
	mutex_destroy(&isp_dev->raw1_full_lock);
	mutex_destroy(&isp_dev->raw1_empty_lock);
out:
	return ret;
}

static void hailo15_clean_isp_device(struct hailo15_isp_device *isp_dev)
{
	struct hailo15_isp_raw_buf *raw_buf;
	dma_free_coherent(isp_dev->dev, HAILO15_ISP_FAKEBUF_SIZE,
			  isp_dev->fakebuf_vaddr, isp_dev->fakebuf_phys);
	dma_free_coherent(isp_dev->dev, HAILO15_ISP_FBUF_SIZE,
			  isp_dev->fbuf_vaddr, isp_dev->fbuf_phys);

	list_for_each_entry_safe(raw_buf, raw_buf, &isp_dev->raw0_empty_queue, list) {
		list_del(&raw_buf->list);
		dma_free_coherent(isp_dev->dev, HAILO15_ISP_FBUF_SIZE,
				  raw_buf->virt_addr, raw_buf->phys_addr);
		kfree(raw_buf);
	}

	list_for_each_entry_safe(raw_buf, raw_buf, &isp_dev->raw1_empty_queue, list) {
		list_del(&raw_buf->list);
		dma_free_coherent(isp_dev->dev, HAILO15_ISP_FBUF_SIZE,
				  raw_buf->virt_addr, raw_buf->phys_addr);
		kfree(raw_buf);
	}

	list_for_each_entry_safe(raw_buf, raw_buf, &isp_dev->raw0_full_queue, list) {
		list_del(&raw_buf->list);
		dma_free_coherent(isp_dev->dev, HAILO15_ISP_FBUF_SIZE,
				  raw_buf->virt_addr, raw_buf->phys_addr);
		kfree(raw_buf);
	}

	list_for_each_entry_safe(raw_buf, raw_buf, &isp_dev->raw1_full_queue, list) {
		list_del(&raw_buf->list);
		dma_free_coherent(isp_dev->dev, HAILO15_ISP_FBUF_SIZE,
				  raw_buf->virt_addr, raw_buf->phys_addr);
		kfree(raw_buf);
	}

	pm_runtime_disable(isp_dev->dev);
	hailo15_isp_destroy_media_pads(isp_dev);
	v4l2_device_unregister_subdev(&isp_dev->sd);
	hailo15_isp_destroy_platdev(isp_dev);
	mutex_destroy(&isp_dev->mlock);
	mutex_destroy(&isp_dev->ctrl_lock);
	mutex_destroy(&isp_dev->af_kevent->data_lock);
	mutex_destroy(&isp_dev->mcm_lock);
	mutex_destroy(&isp_dev->ready_lock);
	mutex_destroy(&isp_dev->raw0_full_lock);
	mutex_destroy(&isp_dev->raw0_empty_lock);
	mutex_destroy(&isp_dev->raw1_full_lock);
	mutex_destroy(&isp_dev->raw1_empty_lock);
	destroy_workqueue(isp_dev->af_wq);
	destroy_workqueue(isp_dev->isp_mis_wq);
	destroy_workqueue(isp_dev->miv2_mis_wq);
	destroy_workqueue(isp_dev->mcm_wr_raw_wq);
}

/* Initialize the dma context.                                                  */
/* The dma context holds the required information for proper buffer management. */
static int hailo15_init_dma_ctx(struct hailo15_dma_ctx *ctx,
				struct hailo15_isp_device *isp_dev)
{
	int index;
	int ret = 0;

	ctx->dev = (void *)isp_dev;
	for(index = 0; index < HAILO15_VID_GRP_MAX; ++index){
		if(hailo15_is_isp_grp_id(index)){
			ctx->buf_ctx[index].ops = kzalloc(sizeof(struct hailo15_buf_ops), GFP_KERNEL);
			if (!ctx->buf_ctx[index].ops){
				pr_err("%s - failed to allocate buffer ops\n", __func__);
				ret = -ENOMEM;
				goto err_alloc_ops;
			}

			memcpy(ctx->buf_ctx[index].ops, &hailo15_isp_buf_ops,
				sizeof(struct hailo15_buf_ops));
		}
	}
	v4l2_set_subdevdata(&isp_dev->sd, ctx);
	goto out;
err_alloc_ops:
	index--;
	for(;index >= 0; --index){
		if(hailo15_is_isp_grp_id(index)){
			kfree(ctx->buf_ctx[index].ops);
			ctx->buf_ctx[index].ops = NULL;
		}
	}
out:
	return ret;
}

static void hailo15_clean_dma_ctx(struct hailo15_dma_ctx *ctx)
{
	int index;
	for(index = 0; index < HAILO15_VID_GRP_MAX; ++index){
		if(hailo15_is_isp_grp_id(index)){
			kfree(ctx->buf_ctx[index].ops);
			ctx->buf_ctx[index].ops = NULL;
		}
	}

	return;
}

static int hailo15_isp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hailo15_isp_device *isp_dev;
	struct hailo15_dma_ctx *dma_ctx;
	int ret, sink_pad;

	dev_dbg(dev, "hailo15 isp driver probe started\n");
	ret = hailo15_media_get_sink_endpoints_status(&pdev->dev);
	if(ret){
		dev_dbg(dev, "endpoints not ready: %d\n", ret);
		return ret;
	}

	isp_dev = devm_kzalloc(&pdev->dev, sizeof(struct hailo15_isp_device),
				   GFP_KERNEL);
	if (!isp_dev)
		return -ENOMEM;

	isp_dev->dev = dev;

	isp_dev->wrapper_cfg  = (const struct isp_wrapper_config *)of_device_get_match_data(&pdev->dev);
	if (!isp_dev->wrapper_cfg ) {
		dev_err(&pdev->dev, "No isp_wrapper_config match found\n");
		goto err_init_isp_dev;
	}

	platform_set_drvdata(pdev, isp_dev);

	ret = hailo15_init_isp_device(isp_dev);
	if (ret) {
		dev_err(dev, "can't init isp device\n");
		goto err_init_isp_dev;
	}

	dma_ctx = devm_kzalloc(&pdev->dev, sizeof(struct hailo15_dma_ctx),
				   GFP_KERNEL);
	if (!dma_ctx) {
		ret = -ENOMEM;
		goto err_alloc_dma_ctx;
	}

	ret = hailo15_init_dma_ctx(dma_ctx, isp_dev);
	if (ret) {
		dev_err(dev, "can't init dma context\n");
		goto err_init_dma_ctx;
	}

	ret = hailo15_vsi_isp_init_events(isp_dev);
	if (ret) {
		dev_err(dev, "can't init isp events memory\n");
		goto err_init_events;
	}

	ret = hailo15_isp_ctrl_init(isp_dev);
	if (ret) {
		dev_err(dev, "can't init isp ctrls\n");
		goto err_init_ctrl;
	}

	init_waitqueue_head(&af_kevent.wait_q);
	mutex_init(&af_kevent.data_lock);
	isp_dev->af_kevent = &af_kevent;

	isp_dev->af_wq = alloc_ordered_workqueue("af_wq", WQ_HIGHPRI);
	if (!isp_dev->af_wq) {
		dev_err(dev, "can't create af workqueue\n");
		goto err_create_wq;
	}

	for (sink_pad = 0; sink_pad < HAILO15_ISP_SINK_PAD_MAX; sink_pad++) {
		atomic_set(&isp_dev->streaming_started[sink_pad], 0);
		atomic_set(&isp_dev->frame_received[sink_pad], 0);
	}

	hailo15_fe_get_dev(&isp_dev->fe_dev);
	hailo15_fe_set_address_space_base(isp_dev->fe_dev, isp_dev->base);

	ret = hailo15_media_create_connections(isp_dev->dev, &isp_dev->sd);
	if(ret){
		dev_err(isp_dev->dev, "can't create media connections\n");
		/*@TODO seperate err label (destroy wq)*/
		goto err_create_wq;
	}

	dev_info(dev, "hailo15 isp driver probed successfully\n");
	goto out;

err_create_wq:
	mutex_destroy(&af_kevent.data_lock);
	hailo15_isp_ctrl_destroy(isp_dev);
err_init_ctrl:
err_init_events:
	hailo15_clean_dma_ctx(dma_ctx);
err_init_dma_ctx:
	kfree(dma_ctx);
err_alloc_dma_ctx:
	hailo15_clean_isp_device(isp_dev);
err_init_isp_dev:
	kfree(isp_dev);
out:
	return ret;
}

static int hailo15_isp_remove(struct platform_device *pdev)
{
	struct hailo15_isp_device *isp_dev = platform_get_drvdata(pdev);
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(&isp_dev->sd);
	hailo15_clean_dma_ctx(
		ctx); /* need to stop interrupts before doing thath */
	hailo15_clean_isp_device(isp_dev);
	kfree(ctx);
	kfree(isp_dev);
	dev_info(isp_dev->dev, "hailo15 isp driver removed\n");
	return 0;
}

static int hailo15_isp_hal_pad_stat_done(struct hailo15_isp_device *isp_dev,
        struct hailo15_pad_stat *pad_stat)
{
    struct media_pad *pad;
    struct video_device *video;
    struct v4l2_event event;

    static_assert(sizeof(event.u.data) == sizeof(pad_stat->reserved),
        "event.u.data and pad_stat->reserved are not the same size");

    pad = media_entity_remote_pad(&isp_dev->pads[pad_stat->pad]);
    if (!pad)
        return -EINVAL;

    if (!is_media_entity_v4l2_video_device(pad->entity)) {
        pr_err("received unexpected pad entity type %d\n", pad->entity->obj_type);
        return -EINVAL;
    }

    video = media_entity_to_video_device(pad->entity);
    event.id = pad_stat->id;
    event.type = pad_stat->type;
    memcpy(event.u.data, pad_stat->reserved, sizeof(pad_stat->reserved));

    v4l2_event_queue(video, &event);
    return 0;
}

static bool hailo15_isp_pad_is_sink(struct hailo15_isp_device *isp_dev, int pad)
{
    bool by_flags = (isp_dev->pads[pad].flags & MEDIA_PAD_FL_SINK);
    bool by_index = (pad < HAILO15_ISP_SINK_PAD_END);

    if (by_flags != by_index)
        pr_err("pad %d bad sink pad check! by_flags %d, by_index %d\n",
            pad, by_flags, by_index);

    return by_index;
}


static int hailo15_process_pad_stat_sub(
    struct hailo15_isp_device *isp_dev, uint8_t pad, uint32_t mis_reg,
    enum hailo15_event_stat_id stat_id, uint32_t mis_mask,
    void *data, size_t data_size)
{
    struct hailo15_pad_stat_subscribe *stat_sub = &isp_dev->pad_data[pad].stat_sub[stat_id];
    struct hailo15_pad_stat pad_stat;

    switch (stat_sub->type) {
        case HAILO15_UEVENT_ISP_STAT:
            break; // stat is subscribed, continue
        case 0:
            return 0; // stat is not subscribed
        default:
            pr_err("received unexpected stat sub type %x for pad %d\n",
                stat_sub->type, pad);
            return -EINVAL;
    }

    if (stat_sub->id != stat_id) {
        pr_err("received unexpected stat sub id %d for pad %d, expected stat_id %d\n",
            stat_sub->id, pad, stat_id);
        return -EINVAL;
    }

    if ((mis_reg & mis_mask) == 0)
        return 0; // stat is not ready


    memset(&pad_stat, 0, sizeof(pad_stat));
    pad_stat.pad = pad;
    pad_stat.type = HAILO15_UEVENT_ISP_STAT;
    pad_stat.id = stat_id;

    if (data) {
        if (data_size == 0 || data_size > sizeof(pad_stat.reserved)) {
            pr_err("unexpected data size %ld for pad %d\n", data_size, pad);
            return -EINVAL;
        }

        memcpy(pad_stat.reserved, data, data_size);
    }

    return hailo15_isp_hal_pad_stat_done(isp_dev, &pad_stat);
}

static int hailo15_process_isp_pad_stat_sub(
    struct hailo15_isp_device *isp_dev, uint8_t pad, uint32_t mis_reg,
    enum hailo15_event_stat_id stat_id, uint32_t mis_mask)
{
    uint32_t *sequence = &isp_dev->pad_data[pad].sequence;

    return hailo15_process_pad_stat_sub(isp_dev, pad, mis_reg,
        stat_id, mis_mask, sequence, sizeof(*sequence));
}

static int hailo15_process_sensor_dataloss_stat_sub(
    struct hailo15_isp_device *isp_dev, uint8_t pad, uint32_t mis_reg)
{
    int ret = 0;
    uint32_t sensor_index = 0;

    for (sensor_index = 0; sensor_index < ISP_MIS_SENSORS_COUNT; sensor_index++) {
        uint32_t sensor_dataloss_mask = ISP_MIS_SENSOR_DATALOSS_LAST_BIT >> sensor_index;
        ret |= hailo15_process_pad_stat_sub(
            isp_dev, pad, mis_reg, HAILO15_UEVENT_SENSOR_DATALOSS_STAT,
            sensor_dataloss_mask, &sensor_index, sizeof(sensor_index));
    }

    return ret;
}

static int hailo15_isp_irq_stat_process(struct hailo15_isp_device *isp_dev,
                uint32_t id, uint32_t mis)
{
    uint8_t pad = 0;
    int ret = 0;

    if (id == HAILO15_ISP_IRQ_EVENT_ISP_MIS) {
        for (pad = 0; pad < HAILO15_ISP_PADS_NR; pad++) {
            // Skip sink pads and pads without streams
            if (hailo15_isp_pad_is_sink(isp_dev, pad) ||
                !isp_dev->pad_data[pad].stream)
                continue;

            ret |= hailo15_process_isp_pad_stat_sub(isp_dev, pad, mis,
                HAILO15_UEVENT_ISP_EXP_STAT, ISP_MIS_EXP_END_MASK);
            ret |= hailo15_process_isp_pad_stat_sub(isp_dev, pad, mis,
                HAILO15_UEVENT_ISP_HIST_STAT, ISP_MIS_HIST_MEASURE_RDY_MASK);
            ret |= hailo15_process_isp_pad_stat_sub(isp_dev, pad, mis,
                HAILO15_UEVENT_ISP_AWB_STAT, ISP_MIS_AWB_DONE_MASK);
            ret |= hailo15_process_isp_pad_stat_sub(isp_dev, pad, mis,
                HAILO15_UEVENT_ISP_AFM_STAT, ISP_MIS_AFM_FIN_MASK);
            ret |= hailo15_process_isp_pad_stat_sub(isp_dev, pad, mis,
                HAILO15_UEVENT_VSM_DONE_STAT, ISP_MIS_VSM_DONE);
        }
    } else if (id == HAILO15_ISP_IRQ_EVENT_MI_MIS) {
        for (pad = 0; pad < HAILO15_ISP_PADS_NR; pad++) {
            // Skip sink pads and pads without streams
            if (hailo15_isp_pad_is_sink(isp_dev, pad) ||
                !isp_dev->pad_data[pad].stream)
                continue;

            ret |= hailo15_process_isp_pad_stat_sub(isp_dev, pad, mis,
                HAILO15_UEVENT_ISP_EXPV2_STAT, ISP_MP_JDP_FRAME_END_MASK);
        }
    }

    return ret;
}

irqreturn_t hailo15_process_irq_stats_events(struct hailo15_isp_device *isp_dev,
    int event_id, uint32_t mis)
{
    int ret = 0;

    if (mis == 0) {
        return IRQ_NONE;
    }

    ret = hailo15_isp_irq_stat_process(isp_dev, event_id, mis);
    if (ret) {
        pr_err("hailo15_isp_irq_stat_process with id %d and mis %x failed with ret %d\n",
            event_id, mis, ret);
    }

    return IRQ_HANDLED;
}

static void frame_timeout_handler_common(struct hailo15_isp_device *isp_dev, int sink_pad)
{
	uint32_t sensor_mask = 0;
	int source_pad;

	if (!isp_dev) {
		pr_err("%s - no device\n", __func__);
		return;
	}

	if (sink_pad < 0 || sink_pad >= HAILO15_ISP_SINK_PAD_MAX) {
		pr_err("%s - invalid sink_pad from timer: %d\n", __func__, sink_pad);
		return;
	}

	if (!atomic_read(&isp_dev->frame_received[sink_pad])) {
		/* if the transition is from streaming to no frame received -
		 * send dataloss stat and reset the streaming_started flag
		 */
		if (atomic_cmpxchg(&isp_dev->streaming_started[sink_pad], 1, 0)) {
			pr_warn("%s - no frame received on sink %d, possible disconnection\n",
				__func__, sink_pad);

			/* Send dataloss stat for all sensors - only subscribed sensors will be processed */
			for (source_pad = HAILO15_ISP_SINK_PAD_MAX; source_pad < HAILO15_ISP_SOURCE_PAD_MAX; ++source_pad) {
				if (HAILO15_ISP_SOURCE_PAD_TO_ISP_SINK_PAD(source_pad) != sink_pad)
					continue;

				/* match the sensor mask to the pad - assumes the first pad is sensor 0 and so on */
				sensor_mask = ISP_MIS_SENSOR_DATALOSS_LAST_BIT >> (source_pad - HAILO15_ISP_SINK_PAD_MAX);
				if (hailo15_process_sensor_dataloss_stat_sub(isp_dev, source_pad, sensor_mask)) {
					pr_err("%s - failed to send dataloss stat for pad %d and sensor mask 0x%x\n",
					       __func__, source_pad, sensor_mask);
				}
			}
		}
	} else {
		/* if the transition is from not streaming to frame received -
		 * send stream stat and set the streaming_started flag
		 */
		if (!atomic_cmpxchg(&isp_dev->streaming_started[sink_pad], 0, 1)) {
			/* Send stream stat for all sensors - only subscribed sensors will be processed */
			for (source_pad = HAILO15_ISP_SINK_PAD_MAX; source_pad < HAILO15_ISP_SOURCE_PAD_MAX; ++source_pad) {
				if (HAILO15_ISP_SOURCE_PAD_TO_ISP_SINK_PAD(source_pad) != sink_pad)
					continue;

				if (hailo15_process_isp_pad_stat_sub(isp_dev, source_pad, 1,
				    HAILO15_UEVENT_SENSOR_STREAMING_STAT, 1)) {
					pr_err("%s - failed to send stream stat for pad %d\n",
					       __func__, source_pad);
				}
			}
		}

		/* Reset frame_received for the next interval */
		atomic_set(&isp_dev->frame_received[sink_pad], 0);
	}

	/* Restart the timer */
	mod_timer(&isp_dev->frame_timer[sink_pad], jiffies + msecs_to_jiffies(FRAME_TIMEOUT_MS));
}

void frame_timeout_handler_0(struct timer_list *t)
{
	struct hailo15_isp_device *isp_dev =
		container_of(t, struct hailo15_isp_device, frame_timer[HAILO15_ISP_SINK_PAD_S0]);
	frame_timeout_handler_common(isp_dev, HAILO15_ISP_SINK_PAD_S0);
}

void frame_timeout_handler_1(struct timer_list *t)
{
	struct hailo15_isp_device *isp_dev =
		container_of(t, struct hailo15_isp_device, frame_timer[HAILO15_ISP_SINK_PAD_S1]);
	frame_timeout_handler_common(isp_dev, HAILO15_ISP_SINK_PAD_S1);
}

/********************/
/* PM subsystem ops */
/********************/
static int isp_system_suspend(struct device *dev)
{
	return pm_runtime_force_suspend(dev);
}

static int isp_system_resume(struct device *dev)
{
	return pm_runtime_force_resume(dev);
	;
}

static int isp_runtime_suspend(struct device *dev)
{
	return 0;
}

static int isp_runtime_resume(struct device *dev)
{
	return 0;
}
static const struct dev_pm_ops hailo15_isp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(isp_system_suspend, isp_system_resume)
		SET_RUNTIME_PM_OPS(isp_runtime_suspend, isp_runtime_resume,
				   NULL)
};

static struct platform_driver
	hailo15_isp_driver = { .probe = hailo15_isp_probe,
				   .remove = hailo15_isp_remove,
				   .driver = {
					   .name = HAILO15_ISP_NAME,
					   .owner = THIS_MODULE,
					   .of_match_table = hailo15_isp_of_match,
					   .pm = &hailo15_isp_pm_ops,
				   } };

module_platform_driver(hailo15_isp_driver);

MODULE_DESCRIPTION("Hailo 15 isp driver");
MODULE_AUTHOR("Hailo Imaging SW Team");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("Hailo15-ISP");
