#include <linux/module.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of_graph.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mc.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ctrls.h>
#include <isp_ctrl/hailo15_isp_ctrl.h>
#include "hailo15-isp.h"
#include "hailo15-isp-hw.h"
#include "common.h"

#define HAILO15_ISP_NAME "hailo-isp"
#define HAILO15_ISP_NAME_SIZE 10
#define HAILO15_ISP_ADDR_SPACE_MAX_SIZE 0x10000
#define HAILO15_ISP_RMEM_SIZE (32 * 1024 * 1024)
#define HAILO15_ISP_FBUF_SIZE (3840 * 2160 * 2)
#define HAILO15_ISP_FAKEBUF_SIZE (3840 * 2160 * 3)

#define VID_GRP_TO_ISP_PATH(vid_grp)                                           \
	({                                                                     \
		int __path;                                                    \
		do {                                                           \
			switch (vid_grp) {                                     \
			case VID_GRP_ISP_MP:                                   \
				__path = ISP_MP;                               \
				break;                                         \
			case VID_GRP_ISP_SP:                                   \
				__path = ISP_SP2;                              \
				break;                                         \
			default:                                               \
				__path = -EINVAL;                              \
				break;                                         \
			}                                                      \
		} while (0);                                                   \
		__path;                                                        \
	})

#define ISP_PATH_TO_VID_GRP(isp_path)                                          \
	({                                                                     \
		int __path;                                                    \
		do {                                                           \
			switch (isp_path) {                                    \
			case ISP_MP:                                           \
				__path = VID_GRP_ISP_MP;                       \
				break;                                         \
			case ISP_SP2:                                          \
				__path = VID_GRP_ISP_SP;                       \
				break;                                         \
			default:                                               \
				__path = -EINVAL;                              \
				break;                                         \
			}                                                      \
		} while (0);                                                   \
		__path;                                                        \
	})

extern uint32_t hailo15_isp_read_reg(struct hailo15_isp_device *, uint32_t);
extern void hailo15_isp_write_reg(struct hailo15_isp_device *, uint32_t,
				  uint32_t);
extern void hailo15_isp_configure_frame_base(struct hailo15_isp_device *,
					     dma_addr_t *, unsigned int);
extern void hailo15_isp_configure_frame_size(struct hailo15_isp_device *, int);
extern irqreturn_t hailo15_isp_irq_process(struct hailo15_isp_device *);
extern void hailo15_isp_handle_afm_int(struct work_struct *);
extern int hailo15_isp_dma_set_enable(struct hailo15_isp_device *, int, int);

struct hailo15_af_kevent af_kevent;
EXPORT_SYMBOL(af_kevent);

static irqreturn_t hailo15_isp_irq_handler(int irq, void *isp_dev)
{
	return hailo15_isp_irq_process(isp_dev);
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
	/* Validate that buffer address is in a specific range? */
	int isp_path;
	isp_path = VID_GRP_TO_ISP_PATH(buf->grp_id);
	if (isp_path < 0 || isp_path >= ISP_MAX_PATH)
		return;

	isp_dev->cur_buf[isp_path] = buf;
	hailo15_isp_configure_frame_base(isp_dev, buf->dma, isp_path);
}

/******************************/
/* VSI infrastructure support */
/******************************/
static int hailo15_vsi_isp_qcap(struct v4l2_subdev *sd, void *arg)
{
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
	struct v4l2_capability *cap = (struct v4l2_capability *)arg;
	/* SHOULD BE COPY_TO_USER */
	strlcpy((char *)cap->driver, HAILO15_ISP_NAME, sizeof(cap->driver));
	cap->bus_info[0] = isp_dev->id;
	return 0;
}

static int hailo15_isp_queryctrl(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
	struct hailo15_pad_queryctrl *pad_querctrl =
		(struct hailo15_pad_queryctrl *)arg;
	ret = v4l2_queryctrl(&isp_dev->ctrl_handler, pad_querctrl->query_ctrl);

	return ret;
}

static int hailo15_isp_query_ext_ctrl(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
	struct hailo15_pad_query_ext_ctrl *pad_quer_ext_ctrl =
		(struct hailo15_pad_query_ext_ctrl *)arg;
	ret = v4l2_query_ext_ctrl(&isp_dev->ctrl_handler,
				  pad_quer_ext_ctrl->query_ext_ctrl);

	return ret;
}

static int hailo15_isp_querymenu(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
	struct hailo15_pad_querymenu *pad_quermenu =
		(struct hailo15_pad_querymenu *)arg;
	ret = v4l2_querymenu(&isp_dev->ctrl_handler, pad_quermenu->querymenu);

	return ret;
}

static int hailo15_isp_g_ctrl(struct v4l2_subdev *sd, void *arg)
{
	int ret;
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
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
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
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
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
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
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
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
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
	struct hailo15_pad_ext_controls *pad_ext_ctrls =
		(struct hailo15_pad_ext_controls *)arg;
	ret = v4l2_try_ext_ctrls(&isp_dev->ctrl_handler, sd->devnode,
				 sd->v4l2_dev->mdev,
				 pad_ext_ctrls->ext_controls);

	return ret;
}

static long hailo15_vsi_isp_priv_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
				       void *arg)
{
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
	struct hailo15_reqbufs *req;
	struct isp_reg_data isp_reg;
	int ret = -EINVAL;
	int path;

	switch (cmd) {
	case VIDIOC_QUERYCAP:
		ret = hailo15_vsi_isp_qcap(sd, arg);
		break;
	case ISPIOC_V4L2_READ_REG:
		mutex_lock(&isp_dev->mlock);
		memcpy(&isp_reg, arg, sizeof(struct isp_reg_data));
		isp_reg.value = hailo15_isp_read_reg(isp_dev, isp_reg.reg);
		memcpy(arg, &isp_reg, sizeof(struct isp_reg_data));
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	case ISPIOC_V4L2_WRITE_REG:
		mutex_lock(&isp_dev->mlock);
		memcpy(&isp_reg, arg, sizeof(struct isp_reg_data));
		hailo15_isp_write_reg(isp_dev, isp_reg.reg, isp_reg.value);
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	case ISPIOC_V4L2_RMEM:
		memcpy(arg, &isp_dev->rmem, sizeof(isp_dev->rmem));
		ret = 0;
		break;
	case ISPIOC_V4L2_MI_START:
		mutex_lock(&isp_dev->mlock);
		for (path = ISP_MP; path < ISP_MAX_PATH; ++path) {
			if (isp_dev->cur_buf[path] &&
			    isp_dev->mi_stopped[path] &&
			    hailo15_isp_is_path_enabled(isp_dev, path)) {
				hailo15_isp_configure_frame_size(isp_dev, path);
				hailo15_isp_configure_buffer(
					isp_dev, isp_dev->cur_buf[path]);
				isp_dev->mi_stopped[path] = 0;
			}
		}
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	case ISPIOC_V4L2_MI_STOP:
		mutex_lock(&isp_dev->mlock);
		for (path = ISP_MP; path < ISP_MAX_PATH; ++path) {
			if (!hailo15_isp_is_path_enabled(isp_dev, path) &&
			    !isp_dev->mi_stopped[path])
				isp_dev->mi_stopped[path] = 1;
		}
		mutex_unlock(&isp_dev->mlock);
		ret = 0;
		break;
	case ISPIOC_V4L2_MCM_MODE:
		*((uint32_t *)arg) = 0;
		ret = 0;
		break;
	case ISPIOC_V4L2_REQBUFS:
		req = (struct hailo15_reqbufs *)arg;
		ret = hailo15_video_post_event_requebus(isp_dev, req->pad,
							req->num_buffers);
		break;
	case ISPIOC_V4L2_SET_INPUT_FORMAT:
		memcpy(&isp_dev->input_fmt, (void *)arg,
		       sizeof(struct v4l2_subdev_format));
		ret = 0;
		break;
	case ISPIOC_V4L2_MCM_DQBUF:
	case ISPIOC_S_MIV_INFO:
	case ISPIOC_S_MIS_IRQADDR:
	case ISPIOC_S_MP_34BIT:
	case ISPIOC_RST_QUEUE:
	case ISPIOC_D_MIS_IRQADDR:
		ret = 0;
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
	default:
		ret = -EINVAL;
		pr_debug("unsupported isp command %x.\n", cmd);
		break;
	}

	return ret;
}

static int hailo15_vsi_isp_init_events(struct hailo15_isp_device *isp_dev)
{
	mutex_init(&isp_dev->event_resource.event_lock);
	isp_dev->event_resource.virt_addr =
		kmalloc(HAILO15_EVENT_RESOURCE_SIZE, GFP_KERNEL);
	if (!isp_dev->event_resource.virt_addr)
		return -ENOMEM;

	isp_dev->event_resource.phy_addr =
		virt_to_phys(isp_dev->event_resource.virt_addr);
	isp_dev->event_resource.size = HAILO15_EVENT_RESOURCE_SIZE;
	memset(isp_dev->event_resource.virt_addr, 0,
	       isp_dev->event_resource.size);
	return 0;
}

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
	pm_runtime_get_sync(isp_dev->dev);

	if (isp_dev->refcnt == 1) {
		hailo15_isp_enable_clocks(isp_dev);
		hailo15_config_isp_wrapper(isp_dev);
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
		pm_runtime_put_sync(isp_dev->dev);
		hailo15_isp_disable_clocks(isp_dev);
		goto out;
	}

out:
	mutex_unlock(&isp_dev->mlock);
	return ret;
}

/****************************/
/* v4l2 subdevice video ops */
/****************************/
static int hailo15_isp_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	int path;
	struct v4l2_subdev *subdev;
	struct media_pad *pad;
	struct hailo15_isp_device *isp_dev =
		container_of(sd, struct hailo15_isp_device, sd);

	if (enable) {
		ret = hailo15_isp_refcnt_inc_enable(isp_dev);
		if (ret)
			return ret;

		hailo15_config_isp_wrapper(isp_dev);
		path = VID_GRP_TO_ISP_PATH(sd->grp_id);
		if (path < 0 || path >= ISP_MAX_PATH)
			return -EINVAL;
		isp_dev->queue_empty[path] = 0;
		isp_dev->current_vsm_index[path] = -1;
	}

	pad = &isp_dev->pads[HAILO15_ISP_SINK_PAD_S0];
	if (pad)
		pad = media_entity_remote_pad(pad);

	if (pad && is_media_entity_v4l2_subdev(pad->entity)) {
		subdev = media_entity_to_v4l2_subdev(pad->entity);
		subdev->grp_id = sd->grp_id;
		v4l2_subdev_call(subdev, video, s_stream, enable);
	}

	// disable clocks only after disabling the stream for all the subdevs
	if (!enable) {
		hailo15_isp_refcnt_dec_disable(isp_dev);
		isp_dev->frame_count = 0;
		path = VID_GRP_TO_ISP_PATH(sd->grp_id);
		if (path < 0 || path >= ISP_MAX_PATH)
			return -EINVAL;

		isp_dev->mi_stopped[path] = 1;
		isp_dev->cur_buf[path] = NULL;
		memset(&isp_dev->input_fmt, 0, sizeof(isp_dev->input_fmt));
		return hailo15_video_post_event_stop_stream(isp_dev);
	}

	return hailo15_video_post_event_start_stream(isp_dev);
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
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
	struct v4l2_subdev *sensor_sd;
	int ret = -EINVAL;
	struct media_pad *pad;
	int isp_path;
	unsigned int sink_pad_idx = HAILO15_ISP_SINK_PAD_MAX;
	struct v4l2_subdev *subdev;
	int max_width = INPUT_WIDTH;
	int max_height = INPUT_HEIGHT;

	if (isp_dev->input_fmt.format.width != 0 &&
	    isp_dev->input_fmt.format.height != 0) {
		max_width = isp_dev->input_fmt.format.width;
		max_height = isp_dev->input_fmt.format.height;
	}

	/*We support only downscaling*/
	if (format->format.width > max_width ||
	    format->format.height > max_height) {
		pr_debug("Unsupported resolution %dx%d\n", format->format.width,
			 format->format.height);
		return -EINVAL;
	}

	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
		return 0;
	}

	isp_path = VID_GRP_TO_ISP_PATH(sd->grp_id);
	if (isp_path < 0 || isp_path >= ISP_MAX_PATH)
		return -EINVAL;

	memcpy(&isp_dev->fmt[isp_path], format,
	       sizeof(struct v4l2_subdev_format));
	ret = hailo15_video_post_event_set_fmt(
		isp_dev, isp_dev->pads[HAILO15_ISP_SOURCE_PAD_MP_S0].index,
		&(format->format));
	if (ret) {
		pr_warn("%s - set_fmt event failed with %d\n", __func__, ret);
		return ret;
	}
	
	sink_pad_idx = (int)(format->pad/2);
	pad = &isp_dev->pads[sink_pad_idx];
	if (pad) {
		pad = media_entity_remote_pad(pad);
	}
	
	if (pad && is_media_entity_v4l2_subdev(pad->entity)) {
		subdev = media_entity_to_v4l2_subdev(pad->entity);
		subdev->grp_id = sd->grp_id;
		ret = v4l2_subdev_call(subdev, pad, set_fmt, NULL, format);
		if (ret)
			return ret;
	}

	// if input_format was not set - no need to update the sensor
    if (isp_dev->input_fmt.format.width == 0 ||
        isp_dev->input_fmt.format.height == 0) {
        pr_debug("%s - input format was not set - no need to update the sensor\n", __func__);
        return 0;
    }
 
    sensor_sd = hailo15_get_sensor_subdev(isp_dev->sd.v4l2_dev->mdev);
    if (!sensor_sd) {
        pr_warn("%s - failed to get sensor subdev\n", __func__);
        return -EINVAL;
    }
 
    return v4l2_subdev_call(sensor_sd, pad, set_fmt, NULL,
                &isp_dev->input_fmt);

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
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;

	return hailo15_isp_refcnt_inc_enable(isp_dev);
}

static int hailo15_isp_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_isp_device *isp_dev = ctx->dev;
	return hailo15_isp_refcnt_dec_disable(isp_dev);
}

static struct v4l2_subdev_internal_ops hailo15_isp_internal_ops = {
	.open = hailo15_isp_open,
	.close = hailo15_isp_close,
};

/**********************************************************/
/* Hailo15 buffer operations.                             */
/* this ops are defining the API between the video device */
/* and the isp device                                     */
/**********************************************************/
inline void hailo15_isp_buffer_done(struct hailo15_isp_device *isp_dev,
				    int path)
{
	struct hailo15_buffer *buf;
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(&isp_dev->sd);

	++isp_dev->frame_count;

	if (path < 0 || path >= ISP_MAX_PATH)
		return;

	buf = isp_dev->cur_buf[path];
	isp_dev->cur_buf[path] = NULL;

	if (isp_dev->current_vsm_index[path] >= 0 &&
	    isp_dev->current_vsm_index[path] < HAILO15_MAX_BUFFERS) {
		isp_dev->vsm_list[path][isp_dev->current_vsm_index[path]] =
			isp_dev->current_vsm;
		memset(&isp_dev->current_vsm, 0, sizeof(struct hailo15_vsm));
	}

	if (buf) {
		isp_dev->current_vsm_index[path] = buf->vb.vb2_buf.index;
	} else {
		isp_dev->current_vsm_index[path] = -1;
	}

	hailo15_dma_buffer_done(ctx, buf, ISP_PATH_TO_VID_GRP(path));
}

static int hailo15_isp_buffer_process(struct hailo15_dma_ctx *ctx,
				      struct hailo15_buffer *buf)
{
	struct v4l2_subdev *sd;
	struct hailo15_isp_device *isp_dev;
	int path;

	sd = buf->sd;
	isp_dev = container_of(sd, struct hailo15_isp_device, sd);

	/*configure buffer to hw*/
	hailo15_isp_configure_buffer(isp_dev, buf);

	path = VID_GRP_TO_ISP_PATH(buf->grp_id);
	if (path < 0 || path >= ISP_MAX_PATH)
		return -EINVAL;

	if (isp_dev->queue_empty[path]) {
		hailo15_isp_dma_set_enable(isp_dev, path, 1);
		isp_dev->queue_empty[path] = 0;
	}

	return 0;
}

static int hailo15_isp_get_frame_count(struct hailo15_dma_ctx *dma_ctx,
				       uint64_t *fc)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)dma_ctx->dev;
	if (!fc)
		return -EINVAL;
	*fc = isp_dev->frame_count;
	return 0;
}

static int hailo15_isp_get_rmem(struct hailo15_dma_ctx *dma_ctx,
				struct hailo15_rmem *rmem)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)dma_ctx->dev;
	memcpy(rmem, &isp_dev->rmem, sizeof(struct hailo15_rmem));
	return 0;
}

static int
hailo15_isp_get_event_resource(struct hailo15_dma_ctx *dma_ctx,
			       struct hailo15_video_event_resource *resource)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)dma_ctx->dev;
	memcpy(resource, &isp_dev->event_resource,
	       sizeof(struct hailo15_video_event_resource));
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
	int path;

	path = VID_GRP_TO_ISP_PATH(grp_id);
	if (path < 0 || path >= ISP_MAX_PATH)
		return -EINVAL;

	isp_dev->private_data[path] = data;
	return 0;
}

static int hailo15_isp_get_private_data(struct hailo15_dma_ctx *ctx, int grp_id,
					void **data)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)ctx->dev;
	int path;

	path = VID_GRP_TO_ISP_PATH(grp_id);

	if (!data)
		return -EINVAL;

	if (path < 0 || path >= ISP_MAX_PATH)
		return -EINVAL;

	*data = isp_dev->private_data[path];
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

	isp_path = VID_GRP_TO_ISP_PATH(grp_id);
	if (isp_path < 0 || isp_path >= ISP_MAX_PATH) {
		return -EINVAL;
	}

	memcpy(vsm, &isp_dev->vsm_list[isp_path][index],
	       sizeof(struct hailo15_vsm));
	return 0;
}

static int hailo15_isp_queue_empty(struct hailo15_dma_ctx *ctx, int path)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)ctx->dev;
	dma_addr_t fakebuf_arr[FMT_MAX_PLANES];
	int i;

	if (path < 0 || path >= ISP_MAX_PATH)
		return -EINVAL;

	for (i = 0; i < FMT_MAX_PLANES; ++i) {
		fakebuf_arr[i] = isp_dev->fakebuf_phys;
	}

	hailo15_isp_configure_frame_base(isp_dev, fakebuf_arr, path);
	hailo15_isp_dma_set_enable(isp_dev, path, 0);
	isp_dev->queue_empty[path] = 1;
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

/***************************/
/* v4l2 async notifier ops */
/***************************/
static int hailo15_isp_notifier_bound(struct v4l2_async_notifier *notifier,
				      struct v4l2_subdev *sd,
				      struct v4l2_async_subdev *asd)
{
	struct hailo15_isp_device *isp_dev =
		container_of(notifier, struct hailo15_isp_device, notifier);
	return hailo15_v4l2_notifier_bound(notifier, sd, asd,
					   &isp_dev->sd.entity);
}

static void hailo15_isp_notifier_unbound(struct v4l2_async_notifier *notifier,
					 struct v4l2_subdev *sd,
					 struct v4l2_async_subdev *asd)
{
	return;
}

static const struct v4l2_async_notifier_operations hailo15_isp_notify_ops = {
	.bound = hailo15_isp_notifier_bound,
	.unbind = hailo15_isp_notifier_unbound,
};

/* Parse the device tree node of the given pad, and register and construct the remote */
/* subdevice to the isp device notifier                                               */
/* TODO: MSW-2716: this function is unused                                            */
int hailo15_isp_configure_async_subdev(struct hailo15_isp_device *isp_dev,
				       int pad)
{
	struct fwnode_handle *ep, *remote_ep;
	struct v4l2_async_subdev *asd;
	int ret;
	ret = 0;

	ep = hailo15_isp_pad_get_ep(isp_dev, pad);
	if (!ep) {
		dev_err(isp_dev->dev, "can't get sink pad fwnode for pad %d\n",
			pad);
		return -EINVAL;
	}

	remote_ep = fwnode_graph_get_remote_endpoint(ep);
	if (!remote_ep) {
		dev_err(isp_dev->dev, "can't get remote endpoint for pad %d\n",
			pad);
		ret = -EINVAL;
		goto err_remote_ep;
	}

	hailo15_isp_put_ep(remote_ep);

	asd = v4l2_async_notifier_add_fwnode_remote_subdev(
		&isp_dev->notifier, ep, struct v4l2_async_subdev);

	if (IS_ERR(asd)) {
		dev_err(isp_dev->dev,
			"cant add remote subdev to notifier! %ld\n",
			PTR_ERR(asd));
		ret = PTR_ERR(asd);
		goto err_notifier_add;
	}

	ret = v4l2_async_subdev_notifier_register(&isp_dev->sd,
						  &isp_dev->notifier);

	if (ret) {
		dev_err(isp_dev->dev,
			"Async notifier register error for pad %d\n", pad);
		goto err_notifier_register;
	}
	goto out;
err_notifier_register:
	v4l2_async_notifier_cleanup(&isp_dev->notifier);
err_notifier_add:
err_remote_ep:
out:
	hailo15_isp_put_ep(ep);
	return ret;
}

/* clean function for registered async subdevice */
static void
hailo15_isp_destroy_async_subdevs(struct hailo15_isp_device *isp_dev)
{
	v4l2_async_notifier_unregister(&isp_dev->notifier);
	v4l2_async_notifier_cleanup(&isp_dev->notifier);
}

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

	isp_dev->rmem.size = HAILO15_ISP_RMEM_SIZE;
	isp_dev->rmem_vaddr = dma_alloc_coherent(
		dev, HAILO15_ISP_RMEM_SIZE, &isp_dev->rmem.addr, GFP_KERNEL);
	if (!isp_dev->rmem_vaddr) {
		dev_err(dev, "can't allocate rmem buffer\n");
		return -ENOMEM;
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

	isp_dev->irq = platform_get_irq(pdev, 0);
	if (isp_dev->irq < 0) {
		dev_err(dev, "can't get irq resource\n");
		return -ENXIO;
	}

	ret = devm_request_irq(dev, isp_dev->irq, hailo15_isp_irq_handler, 0,
			       dev_name(dev), isp_dev);

	if (ret) {
		dev_err(dev, "request irq error\n");
		return ret;
	}

	return 0;
}

static void hailo15_isp_destroy_platdev(struct hailo15_isp_device *isp_dev)
{
	devm_free_irq(isp_dev->dev, isp_dev->irq, isp_dev);
	hailo15_isp_disable_clocks(isp_dev);
	dma_free_coherent(isp_dev->dev, HAILO15_ISP_RMEM_SIZE,
			  isp_dev->rmem_vaddr, isp_dev->rmem.addr);
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
hailo15_isp_init_media_pads(struct hailo15_isp_device *isp_dev)
{
	int pad, ret;

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
	media_entity_cleanup(&isp_dev->sd.entity);
}

/* Init the isp device.                               */
/* These include any previous initialization function */
static int hailo15_init_isp_device(struct hailo15_isp_device *isp_dev)
{
	int ret;
	int path;

	if (!isp_dev)
		return -EINVAL;

	mutex_init(&isp_dev->mlock);
	mutex_init(&isp_dev->ctrl_lock);

	ret = hailo15_isp_init_platdev(isp_dev);
	if (ret) {
		dev_err(isp_dev->dev, "cannot parse platform device\n");
		goto err_init_platdev;
	}

	hailo15_isp_init_v4l2_subdev(&isp_dev->sd);

	ret = hailo15_isp_init_media_pads(isp_dev);
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

	for (path = ISP_MP; path < ISP_MAX_PATH; ++path)
		isp_dev->mi_stopped[path] = 1;

	isp_dev->fakebuf_vaddr =
		dma_alloc_coherent(isp_dev->dev, HAILO15_ISP_FAKEBUF_SIZE,
				   &isp_dev->fakebuf_phys, GFP_USER);
	if (!isp_dev->fakebuf_vaddr) {
		dev_err(isp_dev->dev, "cannot allocate fakebuf\n");
		goto err_alloc_fakebuf;
	}

	for (path = ISP_MP; path < ISP_MAX_PATH; ++path)
		isp_dev->mi_stopped[path] = 1;
	/*queue_empty field should not be set to 1 as the software assumes empty queue on initialization*/
	goto out;

err_alloc_fakebuf:
	dma_free_coherent(isp_dev->dev, HAILO15_ISP_FBUF_SIZE,
			  isp_dev->fbuf_vaddr, isp_dev->fbuf_phys);
err_alloc_fbuf:
	pm_runtime_disable(isp_dev->dev);
err_init_media_pads:
	hailo15_isp_destroy_platdev(isp_dev);
err_init_platdev:
	hailo15_isp_destroy_async_subdevs(isp_dev);
	mutex_destroy(&isp_dev->mlock);
	mutex_destroy(&isp_dev->ctrl_lock);
out:
	return ret;
}

static inline int
hailo15_register_isp_subdevice(struct hailo15_isp_device *isp_dev)
{
	return v4l2_async_register_subdev(&isp_dev->sd);
}

static void hailo15_clean_isp_device(struct hailo15_isp_device *isp_dev)
{
	dma_free_coherent(isp_dev->dev, HAILO15_ISP_FAKEBUF_SIZE,
			  isp_dev->fakebuf_vaddr, isp_dev->fakebuf_phys);
	dma_free_coherent(isp_dev->dev, HAILO15_ISP_FBUF_SIZE,
			  isp_dev->fbuf_vaddr, isp_dev->fbuf_phys);
	pm_runtime_disable(isp_dev->dev);
	hailo15_isp_destroy_media_pads(isp_dev);
	hailo15_isp_destroy_platdev(isp_dev);
	hailo15_isp_destroy_async_subdevs(isp_dev);
	mutex_destroy(&isp_dev->mlock);
	mutex_destroy(&isp_dev->ctrl_lock);
	mutex_destroy(&isp_dev->af_kevent->data_lock);
	destroy_workqueue(isp_dev->af_wq);
}

/* Initialize the dma context.                                                  */
/* The dma context holds the required information for proper buffer management. */
static int hailo15_init_dma_ctx(struct hailo15_dma_ctx *ctx,
				struct hailo15_isp_device *isp_dev)
{
	ctx->dev = (void *)isp_dev;
	spin_lock_init(&ctx->buf_ctx.irqlock);
	INIT_LIST_HEAD(&ctx->buf_ctx.dmaqueue);
	ctx->buf_ctx.ops = kzalloc(sizeof(struct hailo15_buf_ops), GFP_KERNEL);
	if (!ctx->buf_ctx.ops)
		return -ENOMEM;
	memcpy(ctx->buf_ctx.ops, &hailo15_isp_buf_ops,
	       sizeof(struct hailo15_buf_ops));
	isp_dev->buf_ctx = &ctx->buf_ctx;
	v4l2_set_subdevdata(&isp_dev->sd, ctx);
	return 0;
}

static void hailo15_clean_dma_ctx(struct hailo15_dma_ctx *ctx)
{
	kfree(ctx->buf_ctx.ops);
	return;
}

static int hailo15_isp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hailo15_isp_device *isp_dev;
	struct hailo15_dma_ctx *dma_ctx;
	int ret;

	isp_dev = devm_kzalloc(&pdev->dev, sizeof(struct hailo15_isp_device),
			       GFP_KERNEL);
	if (!isp_dev)
		return -ENOMEM;

	dma_ctx = devm_kzalloc(&pdev->dev, sizeof(struct hailo15_dma_ctx),
			       GFP_KERNEL);

	if (!dma_ctx) {
		ret = -ENOMEM;
		goto err_alloc_dma_ctx;
	}

	isp_dev->dev = dev;
	platform_set_drvdata(pdev, isp_dev);

	ret = hailo15_init_isp_device(isp_dev);
	if (ret) {
		dev_err(dev, "can't init isp device\n");
		goto err_init_isp_dev;
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

	ret = hailo15_register_isp_subdevice(isp_dev);
	if (ret) {
		dev_err(dev, "can't register isp device\n");
		goto err_register_subdev;
	}

	init_waitqueue_head(&af_kevent.wait_q);
	mutex_init(&af_kevent.data_lock);
	isp_dev->af_kevent = &af_kevent;

	isp_dev->af_wq = create_singlethread_workqueue("af_wq");
	if (!isp_dev->af_wq) {
		dev_err(dev, "can't create af workqueue\n");
		goto err_create_wq;
	}

	INIT_WORK(&isp_dev->af_w, hailo15_isp_handle_afm_int);

	dev_info(dev, "hailo15 isp driver probed\n");
	goto out;

err_create_wq:
	mutex_destroy(&af_kevent.data_lock);
err_register_subdev:
	hailo15_isp_ctrl_destroy(isp_dev);
err_init_ctrl:
err_init_events:
	hailo15_clean_dma_ctx(dma_ctx);
err_init_dma_ctx:
	hailo15_clean_isp_device(isp_dev);
err_init_isp_dev:
	kfree(dma_ctx);
err_alloc_dma_ctx:
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

static const struct of_device_id hailo15_isp_of_match[] = {
	{
		.compatible = "hailo,isp",
	},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, hailo15_isp_of_match);

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
