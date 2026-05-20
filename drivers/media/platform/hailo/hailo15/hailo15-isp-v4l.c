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
#include "hailo15-isp-v4l.h"
#include "hailo15-isp-hw.h"
#include "hailo15-isp-events.h"
#include "hailo15-media.h"
#include "common.h"
#include <linux/property.h>
#include <trace/events/hailo15_isp.h>

#define CREATE_TRACE_POINTS
#include <trace/events/hailo15_isp_fast_toggle.h>


#define HAILO15_ISP_NAME_SIZE 10
#define HAILO15_ISP_ADDR_SPACE_MAX_SIZE 0x10000
#define HAILO15_ISP_RMEM_SIZE (32 * 1024 * 1024)
#define HAILO15_ISP_MCM_MODE_RMEM_EXT_SIZE (2 * 3840 * 2160 * 2)
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
        .repeat = HAILO15_ISP_SINK_PAD_MAX,

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

/* Forward declarations */
void frame_timeout_handler_0(struct timer_list *t);
void frame_timeout_handler_1(struct timer_list *t);
static int hailo15_isp_init_raw_bufs(struct hailo15_isp_device *isp_dev, int sink_pad_index, size_t size);

struct hailo15_af_kevent af_kevent;
EXPORT_SYMBOL(af_kevent);

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
	int isp_path, sink_pad_index;

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
			hailo15_buf_list_add_tail(buf, &isp_dev->mcm_queue);
			mutex_unlock(&isp_dev->mcm_lock);
			trace_isp_mcm_in_buffer_queue(buf->grp_id, buf->vb.vb2_buf.index,
						     buf->dma[0], ktime_get_ns());
			return;
		}

		isp_dev->cur_buf[buf->grp_id] = buf;
		mutex_unlock(&isp_dev->mcm_lock);

		/* If the output stream hasn't started yet, defer HW configuration.
		 * The daemon configures FE during start_stream on MP, so FE
		 * commands will fail if issued before that. s_stream on MP will
		 * configure cur_buf after the start_stream event completes. */
		sink_pad_index = HAILO15_VID_GRP_TO_ISP_SINK_PAD(buf->grp_id);
		if (isp_dev->stream_enabled[sink_pad_index])
			hailo15_isp_configure_frame_base(isp_dev, buf->dma, buf->grp_id);

		trace_isp_mcm_in_buffer_queue(buf->grp_id, buf->vb.vb2_buf.index,
					     buf->dma[0], ktime_get_ns());
	} else if(isp_path == ISP_MCM_RAW_OUT){
		/* Validate MCM mode */
		if (isp_dev->mcm_mode != ISP_MCM_MODE_RAW_WRITE) {
			pr_err("%s: Invalid MCM mode %d for MCM raw write path\n",
			       __func__, isp_dev->mcm_mode);
			return;
		}
		mutex_lock(&isp_dev->mcm_raw_wr_lock);
		isp_dev->cur_buf[buf->grp_id] = buf;

		/* If the stream on MP is not enabled yet, queue the buffer - we'll configure the buffer to hw after the stream is started */
		sink_pad_index = HAILO15_VID_GRP_TO_ISP_SINK_PAD(buf->grp_id);
		if(!isp_dev->stream_enabled[sink_pad_index]){
			mutex_unlock(&isp_dev->mcm_raw_wr_lock);
			return;
		}

		/* Configure MCM raw frame base for sensor0 (vdid 0) */
		hailo15_isp_configure_mcm_raw_frame_base(isp_dev, buf->dma,
			HAILO15_ISP_SINK_PAD_S0);
		mutex_unlock(&isp_dev->mcm_raw_wr_lock);
		trace_isp_mcm_raw_wr_buffer_process(buf->grp_id, buf->vb.vb2_buf.index,
						    buf->dma[0], ktime_get_ns());
	} else {
		isp_dev->cur_buf[buf->grp_id] = buf;
		/* In MCM mode, defer HW configuration until FE is enabled.
		 * s_stream will call hailo15_isp_configure_buffer() on cur_buf. */
		if (isp_dev->mcm_mode && !isp_dev->fe_enable)
			return;
		hailo15_isp_configure_frame_base(isp_dev, buf->dma, buf->grp_id);
	}
}

static int hailo15_isp_requbufs(struct v4l2_subdev *sd, void *arg)
{
	struct hailo15_reqbufs *pad_requbufs = (struct hailo15_reqbufs *)arg;
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);

	/* if requested buf count is 0, return
	   this happens at the beginning and end of the stream */
	if (pad_requbufs->num_buffers == 0 ||
		hailo15_is_mcm_raw_wr_grp_id(sd->grp_id) ||
		hailo15_is_mcm_in_grp_id(sd->grp_id)) {
		pr_debug("%s - requested buf count is 0, returning\n", __func__);
		return 0;
	}

	isp_dev->prev_reqbufs.pad = pad_requbufs->pad;
	isp_dev->prev_reqbufs.num_buffers = pad_requbufs->num_buffers;

	return hailo15_isp_post_event_requebus(isp_dev, pad_requbufs->pad, pad_requbufs->num_buffers);
}

static int hailo15_isp_apply_prev_reqbufs(struct hailo15_isp_device *isp_dev)
{
	if (isp_dev->prev_reqbufs.num_buffers == 0) {
		pr_err("%s - previous reqbufs num_buffers is 0, nothing to apply\n", __func__);
		return -EAGAIN;
	}

	return hailo15_isp_post_event_requebus(isp_dev, isp_dev->prev_reqbufs.pad, isp_dev->prev_reqbufs.num_buffers);
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
	uint32_t mcm_mode;
	bool streaming = false;
	unsigned long flags;
	int i;

	if (arg == NULL) {
		pr_err("%s - mcm_mode arg is NULL\n", __func__);
		return -EINVAL;
	}

	// If we are currently priming for fast toggle, ignore this call (v4l control call is triggered on stream start)
	// We won't check for streaming state - and simply return
	if (isp_dev->fast_toggle_state != FAST_TOGGLE_NONE && isp_dev->fast_toggle_state < FAST_TOGGLE_STATE_MAX) {
		return 0;
	}

	// The MCM mode passed down from v4l
	mcm_mode = *(uint32_t *)arg;

	if (mcm_mode >= ISP_MCM_MODE_MAX) {
		pr_err("%s - invalid mcm mode %d\n", __func__, mcm_mode);
		return -EINVAL;
	}

	spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
	for (i = 0; i < HAILO15_ISP_SINK_PAD_MAX; i++) {
		streaming |= isp_dev->stream_enabled[i];
	}
	spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);

	if (streaming) {
		pr_err("%s - mcm mode cannot be set while streaming\n", __func__);
		return -EBUSY;
	}

	isp_dev->mcm_mode = mcm_mode;
	return 0;
}

static int hailo15_isp_mcm_set_mode_priming(struct v4l2_subdev *sd, void *arg)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	uint32_t *mcm_mode = (uint32_t *)arg;

	if (*mcm_mode >= ISP_MCM_MODE_MAX) {
		pr_err("%s - invalid mcm mode %d\n", __func__, *mcm_mode);
		return -EINVAL;
	}

	isp_dev->mcm_mode_priming = *mcm_mode;

	return 0;
}

static int hailo15_isp_get_rxw_subdev(struct v4l2_subdev *sd, struct v4l2_subdev **rxw_subdev)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	int sink_pad_index = HAILO15_VID_GRP_TO_ISP_SINK_PAD(sd->grp_id);
	struct media_pad *pad = &isp_dev->pads[sink_pad_index];

	if (pad && pad->entity) {
		pad = media_entity_remote_pad(pad);
	} else {
		// could not find the rxwrapper pad - report and return error
		pr_err("%s - could not find remote pad for isp pad %d\n", __func__, sink_pad_index);
		return -ENODEV;
	}

	if (!pad->entity || !is_media_entity_v4l2_subdev(pad->entity)) {
		pr_err("%s - remote pad entity is not a v4l2 subdev\n", __func__);
		return -ENODEV;
	}

	*rxw_subdev = media_entity_to_v4l2_subdev(pad->entity);
	if (!*rxw_subdev) {
		pr_err("%s - could not get v4l2 subdev from media entity\n", __func__);
		return -ENODEV;
	}

	(*rxw_subdev)->grp_id = sd->grp_id;
	return 0;
}

// Helper function to send fast toggle state to all recipienents (rxwrapper, sensor, csi2rx)
static int send_fast_toggle_set_state(struct v4l2_subdev *sd, struct fast_toggle_data *toggle_data)
{
	struct v4l2_subdev *csi2rx_sd;
	struct v4l2_subdev *sensor_sd;
	struct v4l2_subdev *subdev = NULL;
	int ret;

	// Get csi2rx subdev - we'll set it's fast toggle status
	csi2rx_sd = hailo15_get_csi2rx_subdev(sd->v4l2_dev->mdev, sd->grp_id);
	if (!csi2rx_sd) {
		pr_err("%s - failed to get csi2rx subdev\n", __func__);
		return -EINVAL;
	}

	// Call the exported function directly - tell csi2rx to apply priming settings
	ret = v4l2_subdev_call(csi2rx_sd, core, ioctl, HAILO15_INTERNAL_CSI2RX_FAST_TOGGLE_SET_STATUS, toggle_data);
	if (ret) {
		pr_err("%s - Failed to apply CSI2RX priming: %d\n", __func__, ret);
		return ret;
	}

	// Get sensor subdev - we'll set it's fast toggle status
	sensor_sd = hailo15_get_sensor_subdev(sd->v4l2_dev->mdev, sd->grp_id);
	if (!sensor_sd) {
		pr_err("%s - failed to get sensor subdev\n", __func__);
		return -EINVAL;
	}

	// Call the ioctl on the sensor subdev to apply priming settings
	ret = v4l2_subdev_call(sensor_sd, core, ioctl, HAILO15_INTERNAL_SENSOR_FAST_TOGGLE_SET_STATUS, &(toggle_data->state));
	if (ret) {
		pr_err("%s - Failed to apply sensor priming: %d\n", __func__, ret);
		return ret;
	}

	ret = hailo15_isp_get_rxw_subdev(sd, &subdev);
	if (ret) {
		pr_err("%s - failed to get rxwrapper subdev: %d\n", __func__, ret);
		return ret;
	}

	ret = v4l2_subdev_call(subdev, core, ioctl, HAILO15_INTERNAL_RXW_FAST_TOGGLE_SET_STATUS, toggle_data);
	if (ret) {
		pr_err("%s - failed to toggle priming on rxwrapper subdev %s: %d\n", __func__, subdev->name, ret);
		return ret;
	}

	return 0;
}

static int hailo15_isp_s_stream(struct v4l2_subdev *sd, int enable);

static int hailo15_isp_fast_toggle_set_state(struct v4l2_subdev *sd, struct fast_toggle_data *toggle_data)
{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	int ret;
	u32 orig_grp_id;

	if (toggle_data->state < 0 || toggle_data->state >= FAST_TOGGLE_STATE_MAX) {
		pr_err("%s - invalid fast_toggle_state %d\n", __func__, toggle_data->state);
		return -EINVAL;
	}

	isp_dev->fast_toggle_state = toggle_data->state;

	// If apply priming - do it for this driver, then propagate to other devs
	if (isp_dev->fast_toggle_state == FAST_TOGGLE_APPLY_PRIMING) {
		if (isp_dev->mcm_mode_priming >= ISP_MCM_MODE_MAX) {
			pr_err("%s - invalid mcm_mode_priming %d during fast toggle\n", __func__, isp_dev->mcm_mode_priming);
			return -EINVAL;
		}
		isp_dev->mcm_mode = isp_dev->mcm_mode_priming;
		isp_dev->mcm_mode_priming = ISP_MCM_MODE_MAX;
	}

	// Stream teardown handle - stop vid10 stream if needed
	if (isp_dev->fast_toggle_state == FAST_TOGGLE_TEARDOWN) {
		if (toggle_data->type == TOGGLE_MERCURY_HDR_SDR || toggle_data->type == TOGGLE_MERCURY_PREISP_SDR
			|| toggle_data->type == TOGGLE_PLUTO_PREISP_SDR || toggle_data->type == TOGGLE_PLUTO_PREISP_HDR) {
			// stop stream from vid10 path
			orig_grp_id = sd->grp_id;
			sd->grp_id = HAILO15_VID_GRP_MCM_IN;
			ret = hailo15_isp_s_stream(sd, 0);
			sd->grp_id = orig_grp_id;
			if (ret) {
				pr_err("%s - failed to stop vid10 stream during fast toggle teardown: %d\n", __func__, ret);
				return ret;
			}
		}

	}

	// pass state change to rest of the subdevs
	ret = send_fast_toggle_set_state(&isp_dev->sd, toggle_data);
	if (ret) {
		pr_err("%s - Failed to send status to subdevs: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

/*
* this function returns the empty queue for the given sensor -
* the queue of buffers that are avaialble to write raw frames in to
*/
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

/*
* this function returns the full queue for the given sensor -
* the queue of buffers containing raw frames from the sensor to be processed
*/
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

/*
* this function returns the empty lock for the given sensor -
* must use lock before accessing the empty queue
*/
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

/*
* this function returns the full lock for the given sensor -
* must use lock before accessing the full queue
*/
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
	u8 enable;
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
	enable = *((u8 *)arg);

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
		ret = hailo15_isp_ioctl_read_reg(isp_dev, isp_reg.reg, &isp_reg.value);
		if (ret == 0) {
			memcpy(arg, &isp_reg, sizeof(struct isp_reg_data));
		}
		mutex_unlock(&isp_dev->mlock);
		break;
	case ISPIOC_V4L2_WRITE_REG:
		mutex_lock(&isp_dev->mlock);
		memcpy(&isp_reg, arg, sizeof(struct isp_reg_data));
		/* only called when not in mcm mode */
		ret = hailo15_isp_ioctl_write_reg(isp_dev, isp_reg.reg, isp_reg.value);
		mutex_unlock(&isp_dev->mlock);
		break;
	case ISPIOC_V4L2_RMEM:
		port = ((struct hailo15_rmem *)arg)->port;
		if (port >= HAILO15_ISP_SINK_PAD_MAX) {
			dev_err(isp_dev->dev, "invalid port %d\n", port);
			ret = -EINVAL;
			break;
		}
		{
		size_t needed_size = HAILO15_ISP_RMEM_SIZE;
		if(isp_dev->mcm_mode){
			needed_size += HAILO15_ISP_MCM_MODE_RMEM_EXT_SIZE;
		}
		/* Grow-only — keep the existing rmem if already >= needed_size,
		 * (re)allocate only to grow. Avoids the per-toggle free+realloc
		 * that fragments CMA across SDR<->MCM fast_toggles. */
		if (isp_dev->rmem_vaddr[port] && isp_dev->rmem[port].size < needed_size) {
			dma_free_coherent(isp_dev->dev, isp_dev->rmem[port].size,
				isp_dev->rmem_vaddr[port], isp_dev->rmem[port].addr);
			isp_dev->rmem_vaddr[port] = NULL;
		}
		if (!isp_dev->rmem_vaddr[port]) {
			isp_dev->rmem[port].size = needed_size;
			isp_dev->rmem_vaddr[port] = dma_alloc_coherent(
				isp_dev->dev, isp_dev->rmem[port].size,
				&isp_dev->rmem[port].addr, GFP_KERNEL);
			if (!isp_dev->rmem_vaddr[port]) {
				dev_err(isp_dev->dev, "can't allocate rmem buffer for port %d\n", port);
				ret = -ENOMEM;
				break;
			}
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
	case ISPIOC_V4L2_SET_MCM_MODE_PRIMING:
		ret = hailo15_isp_mcm_set_mode_priming(sd, arg);
		break;
	case ISPIOC_V4L2_MCM_MODE:
        ret = hailo15_isp_mcm_extract_mode(sd, arg);
		break;
	case ISPIOC_V4L2_SET_HDR_COMPRESSION: {
		struct hdr_comp_ctrl *ctrl = (struct hdr_comp_ctrl *)arg;
		mutex_lock(&isp_dev->mlock);
		if (ctrl->compression_enabled > 1 || ctrl->decompression_enabled > 1) {
			dev_err(isp_dev->dev, "invalid hdr comp/decomp values %u/%u\n",
				ctrl->compression_enabled, ctrl->decompression_enabled);
			ret = -EINVAL;
			mutex_unlock(&isp_dev->mlock);
			break;
		}
		isp_dev->hdr_compression_enabled = !!ctrl->compression_enabled;
		isp_dev->hdr_decompression_enabled = !!ctrl->decompression_enabled;
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	}
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
		memcpy(&isp_dev->tuning_state, arg, sizeof(isp_dev->tuning_state));
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	case HAILO15_INTERNAL_ISP_FAST_TOGGLE_SET_STATUS:
		ret = hailo15_isp_fast_toggle_set_state(sd, (struct fast_toggle_data *)arg);
		break;
	case ISPIOC_V4L2_EVENT_COMPLETE:
		/* Daemon calls this after writing ack to shared memory.
		 * Wake the kernel thread waiting in event_wait_complete. */
		wake_up(&isp_dev->event_resource.wait_q);
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
	init_waitqueue_head(&isp_dev->event_resource.wait_q);
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
	struct hailo15_isp_device *isp_dev;

	switch (sub->type) {
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subdev_subscribe_event(subdev, fh, sub);
	case HAILO15_DAEMON_ISP_EVENT:
		/* Same cross-restart seq-leak concern as the video channel:
		 * a previous daemon may have died with kernel_seq advanced
		 * past complete. Reset both counters here on the new daemon's
		 * first subscribe. The daemon subscribes ~12 times in a startup
		 * loop (one per event id); gate on list_empty(&fh->subscribed)
		 * so subsequent subscribes on the same fd are no-ops and a
		 * mid-life re-subscribe can't swallow an in-flight kernel_seq.
		 * Note: HAILO15_UEVENT_ISP_STAT aliases to the same numeric
		 * value (see hailo15-events.h) and would land here too; the
		 * list_empty guard makes that future case safe by construction. */
		if (list_empty(&fh->subscribed)) {
			isp_dev = isp_dev_from_v4l2_subdev(subdev);
			hailo15_isp_event_reset_seq(&isp_dev->event_resource);
		}
		return v4l2_event_subscribe(fh, sub, 8, NULL);
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
	return ret;
}

static void hailo15_isp_disable_clocks(struct hailo15_isp_device *isp_dev)
{
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
		hailo15_config_isp_wrapper_interrupts(isp_dev);
		enable_irq(isp_dev->irq[0]);
		enable_irq(isp_dev->irq[1]);
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
		disable_irq(isp_dev->irq[0]);
		disable_irq(isp_dev->irq[1]);
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
		/* If cur_rdma_buf is in the full queue, clear it to avoid double-free */
		if (isp_dev->cur_rdma_buf == pos) {
			isp_dev->cur_rdma_buf = NULL;
		}
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
		/* If cur_rdma_buf is in the empty queue, clear it to avoid double-free */
		if (isp_dev->cur_rdma_buf == pos) {
			isp_dev->cur_rdma_buf = NULL;
		}
		trace_isp_raw_buffer_empty_q_out(sink_pad_index, pos->index, pos->phys_addr);
		list_del(&pos->list);
		dma_free_coherent(isp_dev->dev, pos->size,
				pos->virt_addr, pos->phys_addr);
		kfree(pos);
	}
	mutex_unlock(empty_lock);

	if (isp_dev->cur_raw_buf[sink_pad_index]) {
		/* If cur_rdma_buf points to the same buffer, clear it to avoid double-free */
		if (isp_dev->cur_rdma_buf == isp_dev->cur_raw_buf[sink_pad_index]) {
			isp_dev->cur_rdma_buf = NULL;
		}
		dma_free_coherent(isp_dev->dev, isp_dev->cur_raw_buf[sink_pad_index]->size,
				isp_dev->cur_raw_buf[sink_pad_index]->virt_addr, isp_dev->cur_raw_buf[sink_pad_index]->phys_addr);
		kfree(isp_dev->cur_raw_buf[sink_pad_index]);
		isp_dev->cur_raw_buf[sink_pad_index] = NULL;
	}

	/* Check if cur_rdma_buf belongs to this sink_pad_index and free it if so.
	 * cur_rdma_buf can point to a buffer that's not in any queue when stream stops.
	 * This happens when RDMA is actively processing a buffer that hasn't been returned
	 * to the empty queue yet. We can only safely free it when:
	 * 1. RDMA is disabled (rdma_enable == 0), meaning all streams are stopping
	 * 2. All streams are disabled (both sensors stopped)
	 * 3. It belongs to this sink_pad_index (check via fe_switch.next_vdid[0])
	 * 4. It's not the same as cur_raw_buf (already handled above)
	 *
	 * Note: We must ensure workqueues are drained and all streams are stopped before
	 * freeing to avoid freeing a buffer that's still being processed by hardware.
	 */
	if (isp_dev->cur_rdma_buf &&
	    !isp_dev->rdma_enable) {
		unsigned long flags;
		bool all_streams_stopped;

		spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
		all_streams_stopped = !isp_dev->stream_enabled[HAILO15_ISP_SINK_PAD_S0] &&
				      !isp_dev->stream_enabled[HAILO15_ISP_SINK_PAD_S1];
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);

		if (all_streams_stopped &&
		    isp_dev->fe_switch.next_vdid[0] == sink_pad_index &&
		    isp_dev->cur_rdma_buf != isp_dev->cur_raw_buf[sink_pad_index]) {
			trace_isp_raw_buffer_empty_q_out(sink_pad_index,
				isp_dev->cur_rdma_buf->index,
				isp_dev->cur_rdma_buf->phys_addr);
			dma_free_coherent(isp_dev->dev, isp_dev->cur_rdma_buf->size,
					isp_dev->cur_rdma_buf->virt_addr, isp_dev->cur_rdma_buf->phys_addr);
			kfree(isp_dev->cur_rdma_buf);
			isp_dev->cur_rdma_buf = NULL;
		}
	}

	return ret;
}

static void hailo15_isp_start_mcm_in(struct hailo15_isp_device *isp_dev) {
	isp_dev->rdma_enable = 1;
	isp_dev->fe_enable = 1;
	isp_dev->dma_ready = 0;
	isp_dev->frame_end = 0;
	isp_dev->fe_ready = 0;
}

/****************************/
/* v4l2 subdevice video ops */
/****************************/
static int hailo15_isp_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	int path;
	int pad_index;
	int sink_pad_index;
	int source_pad_index;
	int stream_cnt = 0;
	size_t raw_buf_size;
	unsigned long flags;
	struct v4l2_subdev *subdev = NULL;
	struct media_pad *pad = NULL;
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

	path = HAILO15_VID_GRP_TO_ISP_PATH(sd->grp_id);
	if (path < 0 || path >= ISP_MAX_PATH)
		return -EINVAL;

	// Fast toggle - MCM in should be started on fast toggle, not when user sends this
	if (isp_dev->fast_toggle_state != FAST_TOGGLE_NONE && path == ISP_MCM_IN && enable) {
		return 0;
	}

	if (enable) {
		ret = hailo15_isp_refcnt_inc_enable(isp_dev);
		if (ret)
			return ret;

		/* For (MCM_RAW_WR), defer streaming until MP starts */
		if (path == ISP_MCM_RAW_OUT) {
			isp_dev->queue_empty[sd->grp_id] = 0;
			trace_isp_queue_empty_exit(sd->grp_id, ktime_get_ns());
			return 0;
		}

		if (path == ISP_MCM_IN){
			hailo15_isp_start_mcm_in(isp_dev);
			return 0;
		}
		if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) {

			/* setup the raw buffers */
			if (isp_dev->input_fmt[sink_pad_index].format.width == 0 ||
				isp_dev->input_fmt[sink_pad_index].format.height == 0) {
				pr_err("invalid input format for raw%d\n", sink_pad_index);
				ret = -EINVAL;
				goto err_dec_refcnt;
			}

			/* 2 bytes per pixel */
			raw_buf_size = isp_dev->input_fmt[sink_pad_index].format.width *
				isp_dev->input_fmt[sink_pad_index].format.height * 2;
			if (hailo15_isp_init_raw_bufs(isp_dev, sink_pad_index, raw_buf_size)) {
				pr_err("cannot initialize raw%d buffers\n", sink_pad_index);
				ret = -ENOMEM;
				goto err_dec_refcnt;
			}

			/* get the first empty buffer */
			empty_queue = hailo15_isp_get_empty_queue(isp_dev, sink_pad_index);
			if (!empty_queue) {
				pr_err("cannot get empty queue for raw%d\n", sink_pad_index);
				ret = -EINVAL;
				goto err_dec_refcnt;
			}

			isp_dev->cur_raw_buf[sink_pad_index] =
				list_first_entry_or_null(empty_queue, struct hailo15_isp_raw_buf, list);
			if (!isp_dev->cur_raw_buf[sink_pad_index]) {
				pr_err("cannot get cur raw%d buf\n", sink_pad_index);
				ret = -EINVAL;
				goto err_dec_refcnt;
			}

			/* remove the buffer from the empty queue */
			trace_isp_raw_buffer_empty_q_out(sink_pad_index,
				isp_dev->cur_raw_buf[sink_pad_index]->index,
				isp_dev->cur_raw_buf[sink_pad_index]->phys_addr);
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

		/* Re-run per stream-on to pick up each sink pad's input_fmt. */
		hailo15_config_isp_wrapper_datapath(isp_dev);

		/* Enable the HW ISP stitcher statistics path on first stream. */
		if (stream_cnt == 0 && isp_dev->hdr_enabled)
			hailo15_isp_stitcher_hw_enable(isp_dev,
				isp_dev->input_fmt[sink_pad_index].format.width,
				isp_dev->input_fmt[sink_pad_index].format.height);

		isp_dev->queue_empty[sd->grp_id] = 0;
		trace_isp_queue_empty_exit(sd->grp_id, ktime_get_ns());
		isp_dev->current_vsm_index[sd->grp_id] = -1;

		mutex_lock(&isp_dev->ready_lock);
		isp_dev->output_ready = 1;
		mutex_unlock(&isp_dev->ready_lock);

		ret = hailo15_isp_post_event_start_stream(isp_dev, source_pad_index, isp_dev->fast_toggle_state != FAST_TOGGLE_NONE);
		if (ret) {
			pr_warn("%s - start stream event failed with %d\n", __func__, ret);
			mutex_lock(&isp_dev->ready_lock);
			isp_dev->output_ready = 0;
			mutex_unlock(&isp_dev->ready_lock);
			hailo15_isp_refcnt_dec_disable(isp_dev);
			return ret;
		}

		/* set the raw0, raw1 address after posting event so daemon doesn't override the address */
		if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) {
			/* if this is the first stream, set the raw frame base immediately */
			if (stream_cnt == 0) {
				hailo15_isp_configure_mcm_raw_frame_base(isp_dev,
					&isp_dev->cur_raw_buf[sink_pad_index]->phys_addr, sink_pad_index);
			}
		} else if (isp_dev->mcm_mode == ISP_MCM_MODE_RAW_WRITE) {
			/* we reach this point for grp_ids for MP,
			 * so after the start event was posted,
			 * the fe is set and we can configure the raw frame base */
			if (!isp_dev->cur_buf[HAILO15_VID_GRP_MCM_RAW_WR]) {
				/* this should give a chance to queue a buffer after the stream is started */
				pr_warn_ratelimited("%s - no raw buffer for grp_id %d\n", __func__, HAILO15_VID_GRP_MCM_RAW_WR);
			} else {
				hailo15_isp_configure_mcm_raw_frame_base(isp_dev, &isp_dev->cur_buf[HAILO15_VID_GRP_MCM_RAW_WR]->dma[0], sink_pad_index);
				trace_isp_mcm_raw_wr_buffer_process(HAILO15_VID_GRP_MCM_RAW_WR, isp_dev->cur_buf[HAILO15_VID_GRP_MCM_RAW_WR]->vb.vb2_buf.index,
									isp_dev->cur_buf[HAILO15_VID_GRP_MCM_RAW_WR]->dma[0], ktime_get_ns());
			}
		}

		if (isp_dev->mcm_mode) {
			hailo15_isp_configure_frame_size(isp_dev, sd->grp_id);
			hailo15_isp_configure_buffer(isp_dev, isp_dev->cur_buf[sd->grp_id]);

			/* Configure deferred MCM_IN buffer if one was queued before
			 * the MP stream started. Now that the daemon has configured
			 * the FE (MP start_stream event completed), it is safe to issue. */
			if (isp_dev->cur_buf[HAILO15_VID_GRP_MCM_IN]) {
				hailo15_isp_configure_frame_base(isp_dev,
					isp_dev->cur_buf[HAILO15_VID_GRP_MCM_IN]->dma,
					HAILO15_VID_GRP_MCM_IN);
			}
		}
	}

	/* For ISP_MCM_IN, we don't call s_stream on upstream devices
	 * but for paths that should trigger the sensor stream, we do.
	 */
	if (path != ISP_MCM_IN &&
		(!isp_dev->rdma_enable ||
		 isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR ||
		 isp_dev->mcm_mode == ISP_MCM_MODE_RAW_WRITE)) {
		pad = &isp_dev->pads[sink_pad_index];
		if (pad && pad->entity) {
			pad = media_entity_remote_pad(pad);
		} else {
			pad = NULL;
		}

		if (pad && pad->entity && is_media_entity_v4l2_subdev(pad->entity)) {
			subdev = media_entity_to_v4l2_subdev(pad->entity);
			if (subdev) {
				subdev->grp_id = sd->grp_id;
				ret = v4l2_subdev_call(subdev, video, s_stream, enable);
				if (ret) {
					pr_err("%s - s_stream to subdev %s failed, err = (%pe)\n", __func__, subdev->name, ERR_PTR(ret));
					goto disable;
				}
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

		if (mod_timer(&isp_dev->frame_timer[sink_pad_index], jiffies + msecs_to_jiffies(FRAME_TIMEOUT_MS))) {
			pr_warn("frame timer was pending after setup\n");
		}
	}

	// disable clocks only after disabling the stream for all the subdevs
	if (!enable) {
		/* path is already set above */
		if (path == ISP_MCM_IN || path == ISP_MCM_RAW_OUT)
			goto disable_rdma;

		if (path != ISP_MCM_RAW_OUT) {
			del_timer_sync(&isp_dev->frame_timer[sink_pad_index]);
			atomic_set(&isp_dev->streaming_started[sink_pad_index], 0);
		}

disable:
		/* If stream was never fully enabled, do minimal cleanup only.
		 * Only undo resources when coming from a failed enable (goto disable),
		 * not from a disable call via the err: path in start_streaming. */
		if (!isp_dev->stream_enabled[sink_pad_index]) {
			if (enable) {
				hailo15_isp_post_event_stop_stream(isp_dev,
					source_pad_index,
					isp_dev->fast_toggle_state != FAST_TOGGLE_NONE);
				mutex_lock(&isp_dev->ready_lock);
				isp_dev->output_ready = 0;
				mutex_unlock(&isp_dev->ready_lock);
				hailo15_isp_refcnt_dec_disable(isp_dev);
			}
			return ret;
		}

		spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
		isp_dev->stream_enabled[sink_pad_index] = 0;
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);

		/* Drain miv2_mis workqueue immediately after disabling stream.
		 * In-flight work items will either complete normally (if already
		 * past the stream_enabled check) or skip buffer processing (if
		 * they haven't started yet). After drain returns, no work items
		 * are running, so it's safe to clean up buffers below. */
		drain_workqueue(isp_dev->miv2_mis_wq);

		/* if we disable a stream in multi sensor mode, and we are in toggle state,
		   we need to wait for the toggle to be disabled at the right time before actually stopping the stream */
		if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR && stream_cnt > 1) {
			ret = wait_event_interruptible(isp_dev->toggle_sensors_wait_q, !isp_dev->toggle_sensors);
			if (ret == -ERESTARTSYS) {
				pr_warn("%s - wait_event_interruptible got interrupted\n", __func__);
			}
		}

		ret = hailo15_isp_post_event_stop_stream(isp_dev, source_pad_index, isp_dev->fast_toggle_state != FAST_TOGGLE_NONE);
		if (ret) {
			pr_warn("%s - stop stream event failed with %d\n", __func__, ret);
		}

		/* Sensor is now off. Wait for the last in-flight frame to
		 * complete ISP processing before tearing down the engine. */
		{
			unsigned int fc = isp_dev->frame_count[sd->grp_id];
			wait_event_timeout(isp_dev->buf_done_wait_q,
				isp_dev->frame_count[sd->grp_id] != fc,
				msecs_to_jiffies(35));
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

		if (path == ISP_MCM_IN) {
			mutex_lock(&isp_dev->ready_lock);
			isp_dev->mcm_waiting = 0;
			mutex_unlock(&isp_dev->ready_lock);

			mutex_lock(&isp_dev->mcm_lock);
			list_for_each_entry_safe(pos, npos, &isp_dev->mcm_queue, irqlist){
				hailo15_buf_list_del(pos);
				trace_isp_mcm_in_buffer_dequeue(sd->grp_id, pos->vb.vb2_buf.index,
							       pos->dma[0], ktime_get_ns());
				hailo15_dma_buffer_done(ctx, sd->grp_id, pos);
			}

			cur_tmp = isp_dev->cur_buf[HAILO15_VID_GRP_MCM_IN];
			isp_dev->cur_buf[HAILO15_VID_GRP_MCM_IN] = NULL;
			if (cur_tmp) {
				trace_isp_mcm_in_buffer_dequeue(sd->grp_id, cur_tmp->vb.vb2_buf.index,
								cur_tmp->dma[0], ktime_get_ns());
			}
			hailo15_dma_buffer_done(ctx, sd->grp_id, cur_tmp);
			mutex_unlock(&isp_dev->mcm_lock);

			/* Drain deferred ISP work to prevent in-flight work items
			 * from calling buffer_done after vb2_queue is released */
			drain_workqueue(isp_dev->miv2_mis_wq);

			hailo15_isp_refcnt_dec_disable(isp_dev);
			return ret;
		}

		if (path == ISP_MCM_RAW_OUT) {
			/* Drain workqueue before clearing cur_buf to prevent
			 * work items from accessing freed buffers */
			if (isp_dev->mcm_wr_raw_wq) {
				drain_workqueue(isp_dev->mcm_wr_raw_wq);
			}

			/* Clear cur_buf and queue. Return all buffers to userspace */
			mutex_lock(&isp_dev->mcm_raw_wr_lock);
			list_for_each_entry_safe(pos, npos, &isp_dev->mcm_raw_wr_queue, irqlist){
				hailo15_buf_list_del(pos);
				trace_isp_mcm_raw_wr_buffer_dequeue(sd->grp_id, pos->vb.vb2_buf.index,
								    pos->dma[0], ktime_get_ns());
				hailo15_dma_buffer_done(ctx, sd->grp_id, pos);
			}
			cur_tmp = isp_dev->cur_buf[HAILO15_VID_GRP_MCM_RAW_WR];
			isp_dev->cur_buf[HAILO15_VID_GRP_MCM_RAW_WR] = NULL;
			mutex_unlock(&isp_dev->mcm_raw_wr_lock);
			if (cur_tmp) {
				trace_isp_mcm_raw_wr_buffer_dequeue(sd->grp_id, cur_tmp->vb.vb2_buf.index,
								    cur_tmp->dma[0], ktime_get_ns());
				hailo15_dma_buffer_done(ctx, sd->grp_id, cur_tmp);
			}
			hailo15_isp_refcnt_dec_disable(isp_dev);
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
			drain_workqueue(isp_dev->isp_mis_wq);
			if(isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) {
				wake_up_interruptible_all(&isp_dev->raw_frame_available_wait_q);
				drain_workqueue(isp_dev->mcm_wr_raw_wq);
			}

			/* in multi sensor, miv2_mis work can wait on raw_frame_available_wait_q
			so flush only after raw_frame_available_wait_q is woken up */
			drain_workqueue(isp_dev->miv2_mis_wq);

			hailo15_isp_stitcher_hw_disable(isp_dev);

			WRITE_ONCE(isp_dev->stitcher_stats_enable, false);
			drain_workqueue(isp_dev->stitcher_stats_wq);
			mutex_lock(&isp_dev->stitcher_stats_lock);
			memset(isp_dev->stitcher_stats_buf, 0,
			       sizeof(isp_dev->stitcher_stats_buf));
			mutex_unlock(&isp_dev->stitcher_stats_lock);
		}

		/* Free rmem only when the last stream stops (stream_cnt <= 1).
		 * At this point vid-out has already stopped and VB2 has cleaned up
		 * its buffers, so the CMA region is no longer referenced.
		 * Do NOT free when stream_cnt > 1 — vid-out VB2 buffers may still
		 * reference this CMA region. */
		if (stream_cnt <= 1 &&
		    isp_dev->rmem_vaddr[sink_pad_index] &&
		    isp_dev->fast_toggle_state == FAST_TOGGLE_NONE) {
			dma_free_coherent(isp_dev->dev, isp_dev->rmem[sink_pad_index].size,
				isp_dev->rmem_vaddr[sink_pad_index], isp_dev->rmem[sink_pad_index].addr);
			isp_dev->rmem_vaddr[sink_pad_index] = NULL;
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

err_dec_refcnt:
	hailo15_isp_refcnt_dec_disable(isp_dev);
	return ret;
}

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

    if (format->format.code == MEDIA_BUS_FMT_SRGGB12_2X12 || format->format.code == MEDIA_BUS_FMT_SRGGB12_3X12 ||
		format->format.code == MEDIA_BUS_FMT_SGBRG12_2X12 || format->format.code == MEDIA_BUS_FMT_SGBRG12_3X12) {
        return true;
    }

    return false;
}

static void hailo15_isp_stitcher_stats_set_dol(struct hailo15_isp_device *isp_dev,
					       struct v4l2_subdev_format *format)
{
	uint32_t n = 0, mask = 0;

	if (format) {
		switch (format->format.code) {
		case MEDIA_BUS_FMT_SRGGB12_2X12:
		case MEDIA_BUS_FMT_SGBRG12_2X12:
			n = ISP_HDR_EXP_STATISTICS_2DOL;
			mask = ISP_STITCHING_MIS_EXP_STAT_S_RDY;
			break;
		case MEDIA_BUS_FMT_SRGGB12_3X12:
		case MEDIA_BUS_FMT_SGBRG12_3X12:
			n = ISP_HDR_EXP_STATISTICS_3DOL;
			mask = ISP_STITCHING_MIS_EXP_STAT_VS_RDY;
			break;
		}
	}
	isp_dev->stitcher_stats_n = n;
	isp_dev->stitcher_ready_mask = mask;
}

static int fast_toggle_set_format(struct v4l2_subdev *sd,
				   struct v4l2_subdev_format *format, enum fast_toggle_type toggle_type)

{
	struct hailo15_isp_device *isp_dev = isp_dev_from_v4l2_subdev(sd);
	struct v4l2_subdev *sensor_sd;
	int ret = -EINVAL;
	struct media_pad *pad;
	int isp_path;
	int sink_pad_idx = HAILO15_ISP_SINK_PAD_MAX;
	int src_pad_idx = HAILO15_ISP_SOURCE_PAD_MAX;
	struct v4l2_subdev *subdev;

	sink_pad_idx = HAILO15_VID_GRP_TO_ISP_SINK_PAD(sd->grp_id);
	if (sink_pad_idx < 0) {
		pr_warn("%s - invalid sink pad index %d\n", __func__, sink_pad_idx);
		return -EINVAL;
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

	if (!hailo15_is_mcm_raw_wr_grp_id(sd->grp_id)) {
		trace_isp_fast_toggle_post_event_set_fmt_start(sd->grp_id, toggle_type);
		ret = hailo15_isp_post_event_set_fmt(
			isp_dev, isp_dev->pads[src_pad_idx].index,
			&(format->format));
		trace_isp_fast_toggle_post_event_set_fmt_complete(sd->grp_id, toggle_type, ret);
		if (ret) {
			pr_err("%s - set_fmt event failed with %d\n", __func__, ret);
			return ret;
		}
	}

	// in case of p2a: set_fmt is done from vid2->rxwrapper - don't override
	// if toggline into non-p2a profile, vid0 calls this (vid2 does nothing), so here we should call the sensor/rxw set_fmt
	if (toggle_type == TOGGLE_MERCURY_HDR_SDR || toggle_type == TOGGLE_MERCURY_PREISP_SDR
		|| toggle_type == TOGGLE_PLUTO_HDR_SDR || toggle_type == TOGGLE_PLUTO_SDR_HDR
		|| toggle_type == TOGGLE_PLUTO_PREISP_HDR) {
		pad = &isp_dev->pads[sink_pad_idx];
		if (pad && pad->entity) {
			pad = media_entity_remote_pad(pad);
		} else {
			pad = NULL;
		}

		if (pad && pad->entity && is_media_entity_v4l2_subdev(pad->entity)) {
			subdev = media_entity_to_v4l2_subdev(pad->entity);
			if (subdev) {
				subdev->grp_id = sd->grp_id;
				trace_isp_fast_toggle_subdev_set_fmt(sd->grp_id, toggle_type, subdev->name);
				ret = v4l2_subdev_call(subdev, pad, set_fmt, NULL, format);
				if (ret) {
					pr_err("%s - set_fmt to subdev %s failed, err = (%pe)\n", __func__, subdev->name, ERR_PTR(ret));
					return ret;
				}
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
		trace_isp_fast_toggle_subdev_set_fmt(sd->grp_id, toggle_type, sensor_sd->name);
		ret = v4l2_subdev_call(sensor_sd, pad, set_fmt, NULL, &isp_dev->input_fmt[sink_pad_idx]);
		if (ret) {
			pr_err("%s - set_fmt to subdev %s failed, err = (%pe)\n", __func__, sensor_sd->name, ERR_PTR(ret));
			return ret;
		}
	}

    isp_dev->hdr_enabled = hailo15_isp_is_format_hdr(&isp_dev->input_fmt[sink_pad_idx]);
    hailo15_isp_stitcher_stats_set_dol(isp_dev, &isp_dev->input_fmt[sink_pad_idx]);

	return ret;
}

// toggle type is enum, but we can only pass types that v4l2 is aware of, so we pass int and convert it
static int hailo15_isp_fast_toggle_stream(struct hailo15_dma_ctx *dma_ctx, int grp_id, int toggle_type_int)
{
	int isp_path;
	int ret;
	struct hailo15_isp_device *isp_dev = (struct hailo15_isp_device *)dma_ctx->dev;
	struct fast_toggle_data toggle_data;

	enum fast_toggle_type toggle_type = (enum fast_toggle_type)toggle_type_int;

	if (toggle_type < 0 || toggle_type >= FAST_TOGGLE_TYPE_MAX) {
		pr_err("%s - invalid fast toggle type %d\n", __func__, toggle_type);
		return -EINVAL;
	}

	isp_path = HAILO15_VID_GRP_TO_ISP_PATH(grp_id);

	trace_isp_fast_toggle_stream_start(grp_id, toggle_type);

	trace_isp_fast_toggle_set_format_start(grp_id, toggle_type);
	ret = fast_toggle_set_format(&isp_dev->sd, &isp_dev->fmt[isp_path], toggle_type);
	if (ret) {
		pr_err("%s - fast toggle set_fmt event failed with %d\n", __func__, ret);
		return ret;
	}

	if (toggle_type_int != TOGGLE_MERCURY_SDR_SDR) {
		// Send set state of FAST_TOGGLE_ACTIVE - in order to activate rxwrapper
		toggle_data.type = toggle_type;
		toggle_data.state = FAST_TOGGLE_ACTIVE;
		ret = hailo15_isp_fast_toggle_set_state(&isp_dev->sd, &toggle_data);
		if (ret) {
			pr_err("%s - failed to activate fast toggle in subdevs, subdev call returned %d\n", __func__, ret);
			return ret;
		}
	}

	if (toggle_type_int == TOGGLE_MERCURY_SDR_HDR || toggle_type_int == TOGGLE_MERCURY_SDR_PREISP
		|| toggle_type_int == TOGGLE_PLUTO_SDR_PREISP || toggle_type_int == TOGGLE_PLUTO_HDR_PREISP) {
		// start mcm in (should be started after vid2 is started, but before vid0)
		trace_isp_fast_toggle_mcm_in_start(grp_id, toggle_type);
		hailo15_isp_start_mcm_in(isp_dev);
	}

	trace_isp_fast_toggle_apply_prev_reqbufs_start(grp_id, toggle_type);
	ret = hailo15_isp_apply_prev_reqbufs(isp_dev);
	if (ret) {
		pr_err("%s - fast toggle apply prev requbufs failed with %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static struct v4l2_subdev_video_ops hailo15_isp_video_ops = {
	.s_stream = hailo15_isp_s_stream,
};

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

	if (hailo15_is_mcm_raw_wr_grp_id(sd->grp_id)) {
		return 0;
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

	if (!hailo15_is_mcm_raw_wr_grp_id(sd->grp_id)) {
		ret = hailo15_isp_post_event_set_fmt(
			isp_dev, isp_dev->pads[src_pad_idx].index,
			&(format->format));
		if (ret) {
			pr_err("%s - set_fmt event failed with %d\n", __func__, ret);
			return ret;
		}
	}

	if(isp_dev->rdma_enable &&
	   isp_dev->mcm_mode != ISP_MCM_MODE_MULTI_SENSOR &&
	   isp_dev->mcm_mode != ISP_MCM_MODE_RAW_WRITE){
		return 0;
	}

	pad = &isp_dev->pads[sink_pad_idx];
	if (pad && pad->entity) {
		pad = media_entity_remote_pad(pad);
	} else {
		pad = NULL;
	}

	if (pad && pad->entity && is_media_entity_v4l2_subdev(pad->entity)) {
		subdev = media_entity_to_v4l2_subdev(pad->entity);
		if (subdev) {
			subdev->grp_id = sd->grp_id;
			ret = v4l2_subdev_call(subdev, pad, set_fmt, NULL, format);
			if (ret) {
				pr_err("%s - set_fmt to subdev %s failed, err = (%pe)\n", __func__, subdev->name, ERR_PTR(ret));
				return ret;
			}
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
	hailo15_isp_stitcher_stats_set_dol(isp_dev, &isp_dev->input_fmt[sink_pad_idx]);

	return ret;
}

static const struct v4l2_subdev_pad_ops hailo15_isp_pad_ops = {
	.set_fmt = hailo15_isp_set_fmt,
};


/**********************/
/* v4l2 subdevice ops */
/**********************/

/* This is the parent structure that contains all supported v4l2 subdevice operations */
static struct v4l2_subdev_ops hailo15_isp_subdev_ops = {
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
			hailo15_buf_list_del(isp_dev->cur_buf[grp_id]);
			next_buf = isp_dev->cur_buf[grp_id];
		}
		mutex_unlock(&isp_dev->mcm_lock);
		if(buf){
			trace_isp_mcm_in_buffer_dequeue(grp_id, buf->vb.vb2_buf.index,
							buf->dma[0], ktime_get_ns());
		}
		if(next_buf){
			hailo15_isp_configure_frame_base(isp_dev, next_buf->dma, grp_id);
			trace_isp_mcm_in_buffer_queue(grp_id, next_buf->vb.vb2_buf.index,
						    next_buf->dma[0], ktime_get_ns());
		}

	} else if(path == ISP_MCM_RAW_OUT){
		mutex_lock(&isp_dev->mcm_raw_wr_lock);
		buf = isp_dev->cur_buf[grp_id];
		isp_dev->cur_buf[grp_id] = NULL;
		mutex_unlock(&isp_dev->mcm_raw_wr_lock);
		if(!buf){
			/* vid-cap might need to release the previous buffer to user space
			   trace and pass flow to vid-cap */
			trace_isp_mcm_raw_wr_buffer_done_null(grp_id, ktime_get_ns());
		} else {
			buf->vb.vb2_buf.timestamp = ktime_get_ns();
			trace_isp_mcm_raw_wr_buffer_done(grp_id, buf->vb.vb2_buf.index,
								buf->dma[0], buf->vb.vb2_buf.timestamp);
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

	path = HAILO15_VID_GRP_TO_ISP_PATH(buf->grp_id);
	if (path < 0 || path >= ISP_MAX_PATH)
		return -EINVAL;

	if (!is_buf_time_synced && isp_dev->mcm_mode != ISP_MCM_MODE_INJECTION) {
		/* If buffer is processed on wrong time, this is allowed only in injection mode.
		Because in injection mode, we can't always process buffers right after
		the latest buffer_done of the previous one (buffer_done can't be called with null buffer in injection mode).
		*/
		return -EAGAIN;
	}

	/*configure buffer to hw*/
	hailo15_isp_configure_buffer(isp_dev, buf);

	/* If queue was empty, enable DMA and clear the flag */
	if (isp_dev->queue_empty[buf->grp_id]) {
		isp_dev->queue_empty[buf->grp_id] = 0;
		trace_isp_queue_empty_exit(buf->grp_id, ktime_get_ns());
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

	if (path == ISP_MCM_RAW_OUT) {
		/* For MCM_RAW_OUT, configure fakebuf using mcm_raw_frame_base */
		fakebuf_arr[PLANE_Y] = isp_dev->fakebuf_phys;
		mutex_lock(&isp_dev->mcm_raw_wr_lock);
		isp_dev->cur_buf[grp_id] = NULL;
		hailo15_isp_configure_mcm_raw_frame_base(isp_dev, fakebuf_arr,
							  HAILO15_ISP_SINK_PAD_S0);
		mutex_unlock(&isp_dev->mcm_raw_wr_lock);
	} else {
		/* For other paths, configure fakebuf using frame_base */
		for (i = 0; i < FMT_MAX_PLANES; ++i) {
			fakebuf_arr[i] = isp_dev->fakebuf_phys;
		}
		hailo15_isp_configure_frame_base(isp_dev, fakebuf_arr, grp_id);
	}

	isp_dev->queue_empty[grp_id] = 1;
	trace_isp_queue_empty_enter(grp_id, ktime_get_ns());
	isp_dev->output_ready = 0;
	return 0;
}

static struct hailo15_buf_ops hailo15_isp_buf_ops = {
	.buffer_process = hailo15_isp_buffer_process,
	.get_frame_count = hailo15_isp_get_frame_count,
	.get_rmem = hailo15_isp_get_rmem,
	.get_event_resource = hailo15_isp_get_event_resource,
	.set_private_data = hailo15_isp_set_private_data,
	.get_private_data = hailo15_isp_get_private_data,
	.get_vsm = hailo15_isp_get_vsm,
	.queue_empty = hailo15_isp_queue_empty,
	.fast_toggle_stream = hailo15_isp_fast_toggle_stream,
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
		return -ENXIO;
	}

	ret = devm_request_irq(dev, isp_dev->irq[0], hailo15_isp_irq_handler, IRQF_NO_AUTOEN,
				   dev_name(dev), isp_dev);
	if (ret) {
		dev_err(dev, "request isp irq error\n");
		return ret;
	}

	isp_dev->irq[1] = platform_get_irq(pdev, 1);
	if (isp_dev->irq[1] < 0) {
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
			isp_dev->rmem_vaddr[port] = NULL;
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
	INIT_LIST_HEAD(&isp_dev->raw0_full_queue);
	INIT_LIST_HEAD(&isp_dev->raw0_empty_queue);

	INIT_LIST_HEAD(&isp_dev->raw1_full_queue);
	INIT_LIST_HEAD(&isp_dev->raw1_empty_queue);

	memset(&isp_dev->fe_switch, 0, sizeof(isp_dev->fe_switch));
	isp_dev->fe_switch.vd_mode = 1;
	isp_dev->fe_switch.next_vdid[0] = 0;
	isp_dev->fe_switch.next_vdid[1] = 0;

	atomic_set(&isp_dev->first_rdma_done, 0);
	for (path = 0; path < HAILO15_ISP_SINK_PAD_MAX; path++) {
		atomic_set(&isp_dev->full_queue_count[path], 0);
	}

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
	INIT_LIST_HEAD(&isp_dev->mcm_raw_wr_queue);
	mutex_init(&isp_dev->mcm_lock);
	mutex_init(&isp_dev->mcm_raw_wr_lock);
	mutex_init(&isp_dev->ready_lock);
	isp_dev->output_ready = 1;
	isp_dev->mcm_waiting = 0;

	isp_dev->miv2_mis_wq = alloc_ordered_workqueue("miv2_mis_wq", WQ_HIGHPRI);
	isp_dev->mcm_wr_raw_wq = alloc_ordered_workqueue("mcm_wr_raw_wq", WQ_HIGHPRI);
	isp_dev->isp_mis_wq = alloc_ordered_workqueue("isp_mis_wq", WQ_HIGHPRI);
	isp_dev->stitcher_stats_wq = alloc_ordered_workqueue("stitcher_stats_wq", 0);
	INIT_WORK(&isp_dev->stitcher_stats_work, hailo15_isp_stitcher_stats_work);
	mutex_init(&isp_dev->stitcher_stats_lock);

	init_waitqueue_head(&isp_dev->raw_frame_available_wait_q);
	isp_dev->raw_frame_available[HAILO15_ISP_SINK_PAD_S0] = false;
	isp_dev->raw_frame_available[HAILO15_ISP_SINK_PAD_S1] = false;

	init_waitqueue_head(&isp_dev->toggle_sensors_wait_q);
	isp_dev->toggle_sensors = false;

	isp_dev->hdr_compression_enabled = false;
	isp_dev->hdr_decompression_enabled = false;
	spin_lock_init(&isp_dev->stream_state_lock);

	init_waitqueue_head(&isp_dev->buf_done_wait_q);
	atomic_set(&isp_dev->buf_done_ready, 0);

	memset(&isp_dev->prev_reqbufs, 0, sizeof(isp_dev->prev_reqbufs));

	isp_dev->mcm_mode_priming = ISP_MCM_MODE_MAX;
	isp_dev->fast_toggle_state = FAST_TOGGLE_NONE;

	goto out;


err_alloc_fakebuf:
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

	list_for_each_entry_safe(raw_buf, raw_buf, &isp_dev->raw0_empty_queue, list) {
		list_del(&raw_buf->list);
		dma_free_coherent(isp_dev->dev, raw_buf->size,
				  raw_buf->virt_addr, raw_buf->phys_addr);
		kfree(raw_buf);
	}

	list_for_each_entry_safe(raw_buf, raw_buf, &isp_dev->raw1_empty_queue, list) {
		list_del(&raw_buf->list);
		dma_free_coherent(isp_dev->dev, raw_buf->size,
				  raw_buf->virt_addr, raw_buf->phys_addr);
		kfree(raw_buf);
	}

	list_for_each_entry_safe(raw_buf, raw_buf, &isp_dev->raw0_full_queue, list) {
		list_del(&raw_buf->list);
		dma_free_coherent(isp_dev->dev, raw_buf->size,
				  raw_buf->virt_addr, raw_buf->phys_addr);
		kfree(raw_buf);
	}

	list_for_each_entry_safe(raw_buf, raw_buf, &isp_dev->raw1_full_queue, list) {
		list_del(&raw_buf->list);
		dma_free_coherent(isp_dev->dev, raw_buf->size,
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
	mutex_destroy(&isp_dev->mcm_raw_wr_lock);
	mutex_destroy(&isp_dev->ready_lock);
	mutex_destroy(&isp_dev->raw0_full_lock);
	mutex_destroy(&isp_dev->raw0_empty_lock);
	mutex_destroy(&isp_dev->raw1_full_lock);
	mutex_destroy(&isp_dev->raw1_empty_lock);
	mutex_destroy(&isp_dev->stitcher_stats_lock);
	destroy_workqueue(isp_dev->af_wq);
	destroy_workqueue(isp_dev->isp_mis_wq);
	destroy_workqueue(isp_dev->miv2_mis_wq);
	destroy_workqueue(isp_dev->mcm_wr_raw_wq);
	destroy_workqueue(isp_dev->stitcher_stats_wq);
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
			/* Use unified buffer ops for all ISP paths including MCM_RAW_WR */
			ctx->buf_ctx[index].ops = kmemdup(&hailo15_isp_buf_ops, sizeof(struct hailo15_buf_ops), GFP_KERNEL);
			if (!ctx->buf_ctx[index].ops){
				pr_err("%s - failed to allocate buffer ops\n", __func__);
				ret = -ENOMEM;
				goto err_alloc_ops;
			}
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

	isp_dev->wrapper_cfg = (const struct isp_wrapper_config *)of_device_get_match_data(&pdev->dev);
	if (!isp_dev->wrapper_cfg) {
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
		timer_setup(&isp_dev->frame_timer[sink_pad],
			    sink_pad == HAILO15_ISP_SINK_PAD_S0 ? frame_timeout_handler_0 : frame_timeout_handler_1,
			    0);
	}

	hailo15_fe_get_dev(&isp_dev->fe_dev);
	if (WARN(!isp_dev->fe_dev, "FE device not available\n")) {
		ret = -ENODEV;
		goto err_create_wq;
	}
	hailo15_fe_set_address_space_base(isp_dev->fe_dev, isp_dev->base);
	isp_dev->fe_dev->fe.isp_irq = isp_dev->irq[0];

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
err_alloc_dma_ctx:
	hailo15_clean_isp_device(isp_dev);
err_init_isp_dev:
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
					   .of_match_table = hailo15_isp_of_match,
					   .pm = &hailo15_isp_pm_ops,
				   } };

module_platform_driver(hailo15_isp_driver);

MODULE_DESCRIPTION("Hailo 15 isp driver");
MODULE_AUTHOR("Hailo Imaging SW Team");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("Hailo15-ISP");
