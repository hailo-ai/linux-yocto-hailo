#include "hailo15-vid-cap.h"

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include "hailo15-events.h"

#define HAILO_VID_NAME "hailo_video"

#define MIN_BUFFERS_NEEDED 5
#define MAX_NUM_FRAMES HAILO15_MAX_BUFFERS
#define NUM_PLANES                                                             \
	1 /*working on packed mode, this should be determined from the format*/

#define STREAM_OFF 0
#define STREAM_ON 1

#define VIDEO_INDEX_VALIDATE(index, do_fail)                                   \
	do {                                                                   \
		if ((index) >= MAX_VIDEO_NODE_NUM) {                           \
			pr_err("%s: id %d is too large (id > %d) \n",          \
			       __func__, index, MAX_VIDEO_NODE_NUM);           \
			do_fail;                                               \
		}                                                              \
	} while (0);
#define VALIDATE_STREAM_OFF(vid_node, do_fail)                                     \
	do {                                                                       \
		if ((vid_node)->streaming) {                                       \
			pr_err("video_device: tried to call %s while steaming!\n", \
			       __func__);                                          \
			do_fail;                                                   \
		}                                                                  \
	} while (0);

struct mutex sd_mutex;

#define hailo15_subdev_call(vid_node, ...)                                     \
	({                                                                     \
		int __ret;                                                     \
		do {                                                           \
			mutex_lock(&sd_mutex);                                 \
			vid_node->direct_sd->grp_id = vid_node->path;          \
			__ret = v4l2_subdev_call(vid_node->direct_sd,          \
						 __VA_ARGS__);                 \
			mutex_unlock(&sd_mutex);                               \
		} while (0);                                                   \
		__ret;                                                         \
	})

static int hailo15_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	if (WARN_ON(!vid_node))
		return -EINVAL;

	strcpy(cap->driver, "hailo_video");
	strcpy(cap->card, "HAILO");
	cap->bus_info[0] = 0;
	snprintf((char *)cap->bus_info, sizeof(cap->bus_info),
		 "platform:hailo%d", vid_node->id);

	return 0;
}

static int hailo15_video_create_pipeline(struct hailo15_video_node *vid_node)
{
	int ret = 0;

	if (vid_node->pipeline_init == 0 && vid_node->path != VID_GRP_P2A) {
		ret = hailo15_video_post_event_create_pipeline(vid_node);
		if (ret) {
			pr_err("%s - post event failed and returned: %d\n",
			       __func__, ret);
			return ret;
		}
	}

	return ret;
}

static int hailo15_enum_fmt_vid_cap(struct file *file, void *priv,
				    struct v4l2_fmtdesc *f)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	int ret = 0;

	if (WARN_ON(!vid_node))
		return -EINVAL;

	if (!vid_node->pipeline_init) {
		ret = hailo15_video_create_pipeline(vid_node);
		if (ret) {
			return ret;
		}
		vid_node->pipeline_init = 1;
	}

	if (f->index < hailo15_get_formats_count()) {
		f->pixelformat = hailo15_get_formats()[f->index].fourcc;
		return 0;
	}

	return -EINVAL;
}

static int hailo15_g_fmt_vid_cap(struct file *file, void *priv,
				 struct v4l2_format *f)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	int ret = 0;

	if (WARN_ON(!vid_node))
		return -EINVAL;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (!vid_node->pipeline_init) {
		ret = hailo15_video_create_pipeline(vid_node);
		if (ret) {
			return ret;
		}
		vid_node->pipeline_init = 1;
	}

	/* is it really a user-space struct ?*/
	memcpy(f, &vid_node->fmt, sizeof(struct v4l2_format));
	return 0;
}

static inline void init_v4l2_subdev_fmt(struct v4l2_subdev_format *subdev_fmt,
					struct v4l2_pix_format_mplane *fmt,
					int code, int active)
{
	subdev_fmt->format.width = fmt->width;
	subdev_fmt->format.height = fmt->height;
	subdev_fmt->format.code = code;
	subdev_fmt->which =
		active ? V4L2_SUBDEV_FORMAT_ACTIVE : V4L2_SUBDEV_FORMAT_TRY;
}

static int _hailo15_try_fmt_vid_cap(struct file *file, void *priv,
				    struct v4l2_format *f, int active)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	const struct hailo15_video_fmt *format = NULL;
	struct v4l2_subdev_format fmt;
	struct v4l2_subdev_state try_fmt;
	struct v4l2_subdev_pad_config pad_cfg;
	int ret;

	pr_debug("%s - enter - active = %d, width: %u, height: %u\n", __func__,
		 active, pix_mp->width, pix_mp->height);

	memset(&fmt, 0, sizeof(fmt));
	memset(&try_fmt, 0, sizeof(try_fmt));
	memset(&pad_cfg, 0, sizeof(pad_cfg));

	if (WARN_ON(!vid_node))
		return -EINVAL;

	VALIDATE_STREAM_OFF(vid_node, return -EINVAL);

	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type))
		return -EINVAL;

	format = hailo15_fourcc_get_format(pix_mp->pixelformat);

	if (format == NULL) {
		return -EINVAL;
	}

	if (pix_mp->width % format->width_modulus)
		return -EINVAL;

	ret = hailo15_fill_planes_fmt(format, pix_mp);
	if (ret) {
		pr_err("%s - fill_planes_fmt failed\n", __func__);
		return ret;
	}

	pix_mp->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	pix_mp->colorspace = V4L2_COLORSPACE_DEFAULT;
	init_v4l2_subdev_fmt(&fmt, pix_mp, format->code, active);

	if (!active) {
		pad_cfg.try_fmt = fmt.format;
		try_fmt.pads = &pad_cfg;
		ret = hailo15_subdev_call(vid_node, pad, set_fmt, &try_fmt,
					  &fmt);
		if (ret)
			return ret;
	} else {
		ret = hailo15_subdev_call(vid_node, pad, set_fmt, NULL, &fmt);
		if (ret) {
			pr_err("%s - set_fmt active failed\n", __func__);
			return ret;
		}
	}

	return ret;
}

static int hailo15_try_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_format *f)
{
	return _hailo15_try_fmt_vid_cap(file, priv, f, 0);
}

static int hailo15_s_fmt_vid_cap(struct file *file, void *priv,
				 struct v4l2_format *f)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	int ret = 0;

	if (WARN_ON(!vid_node))
		return -EINVAL;

	VALIDATE_STREAM_OFF(vid_node, return -EINVAL);

	if (!vid_node->pipeline_init) {
		ret = hailo15_video_create_pipeline(vid_node);
		if (ret) {
			return ret;
		}
		vid_node->pipeline_init = 1;
	}
	ret = _hailo15_try_fmt_vid_cap(file, priv, f, 1);
	if (ret) {
		pr_err("%s - try set fmt failed with: %d\n", __func__, ret);
		return ret;
	}

	memcpy(&vid_node->fmt, f, sizeof(struct v4l2_format));
	return ret;
}

static void hailo15_video_node_queue_clean(struct hailo15_video_node *vid_node,
					   enum vb2_buffer_state state)
{
	unsigned long flags;
	struct hailo15_buffer *buf, *nbuf;

	spin_lock_irqsave(&vid_node->qlock, flags);
	list_for_each_entry_safe (buf, nbuf, &vid_node->buf_queue, irqlist) {
		list_del(&buf->irqlist);
		vb2_buffer_done(&buf->vb.vb2_buf, state);
	}
	if (vid_node->prev_buf) {
		vb2_buffer_done(&vid_node->prev_buf->vb.vb2_buf, state);
		vid_node->prev_buf = NULL;
	}
	spin_unlock_irqrestore(&vid_node->qlock, flags);
}

static int
hailo15_video_node_subdev_set_stream(struct hailo15_video_node *vid_node,
				     int enable)
{
	if (WARN_ON(!vid_node))
		return -EINVAL;

	// set the grp_id of subdev to indicate from which pad the call came
	vid_node->direct_sd->grp_id = vid_node->path;

	return hailo15_subdev_call(vid_node, video, s_stream, enable);
}

static int hailo15_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	int ret;

	if (WARN_ON(!vid_node)) {
		pr_err("%s - failed to get vid_node from device video_drvdata\n",
		       __func__);
		return -EINVAL;
	}

	mutex_lock(&vid_node->ioctl_mutex);
	if (!vid_node->streaming) {
		ret = vb2_ioctl_streamon(file, priv, i);
		if (ret) {
			mutex_unlock(&vid_node->ioctl_mutex);
			pr_err("%s - ERROR from vb2_ioctl_streamon: %d\n",
			       __func__, ret);
			return -EINVAL;
		}
		mutex_unlock(&vid_node->ioctl_mutex);
		return 0;

		ret = hailo15_video_node_subdev_set_stream(vid_node, 1);
		if (!ret) {
			vid_node->streaming = 1;
		}
		mutex_unlock(&vid_node->ioctl_mutex);
		return ret;
	}

	return 0;
}
static int hailo15_streamoff(struct file *file, void *priv,
			     enum v4l2_buf_type i)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	int ret = 0;

	if (WARN_ON(!vid_node)) {
		pr_err("%s - failed to get vid_node from device video_drvdata\n",
		       __func__);
		return -EINVAL;
	}

	mutex_lock(&vid_node->ioctl_mutex);
	if (!vid_node->streaming) {
		mutex_unlock(&vid_node->ioctl_mutex);
		return 0;
	}

	ret = vb2_ioctl_streamoff(file, priv, i);
	mutex_unlock(&vid_node->ioctl_mutex);
	return ret;

	ret = hailo15_video_node_subdev_set_stream(vid_node, 0);
	if (!ret) {
		vid_node->streaming = 0;
	}

	mutex_unlock(&vid_node->ioctl_mutex);
	return ret;
}

static int
hailo15_vidioc_subscribe_event(struct v4l2_fh *fh,
			       const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subscribe_event(fh, sub);
	case HAILO15_DEAMON_VIDEO_EVENT:
		return v4l2_event_subscribe(
			fh, sub, HAILO15_DAEMON_VIDEO_EVENT_MAX, NULL);
	default:
		pr_debug("%s - got bad video event type: %u\n", __func__,
			 sub->type);
		return -EINVAL;
	}
}

static int hailo15_s_parm(struct file *file, void *fh,
			  struct v4l2_streamparm *a)
{
	struct v4l2_subdev *sensor_sd;
	struct v4l2_subdev_frame_interval fi;
	struct hailo15_video_node *vid_node = video_drvdata(file);
	int ret = 0;

	fi.interval = a->parm.capture.timeperframe;
	fi.pad = 0;
	sensor_sd = hailo15_get_sensor_subdev(vid_node->mdev);
	if (!sensor_sd) {
		pr_warn("%s - failed to get sensor subdev\n", __func__);
		return -EINVAL;
	}

	ret = v4l2_subdev_call(sensor_sd, video, s_frame_interval, &fi);
	if (ret) {
		pr_warn("%s - failed to set frame rate on the sensor, got %d\n",
			__func__, ret);
		ret = v4l2_subdev_call(sensor_sd, video, g_frame_interval, &fi);
	}
	a->parm.capture.timeperframe = fi.interval;

	return ret;
}

static int hailo15_g_parm(struct file *file, void *fh,
			  struct v4l2_streamparm *a)
{
	struct v4l2_subdev *sensor_sd;
	struct v4l2_subdev_frame_interval fi = { 0 };
	struct hailo15_video_node *vid_node = video_drvdata(file);
	int ret = 0;

	sensor_sd = hailo15_get_sensor_subdev(vid_node->mdev);
	if (!sensor_sd) {
		pr_warn("%s - failed to get sensor subdev\n", __func__);
		return -EINVAL;
	}

	ret = v4l2_subdev_call(sensor_sd, video, g_frame_interval, &fi);
	if (ret) {
		pr_warn("%s - failed to get frame rate from the sensor, got %d\n",
			__func__, ret);
	}
	a->parm.capture.timeperframe = fi.interval;
	a->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;

	return ret;
}

int hailo15_reqbufs(struct file *file, void *priv,
		    struct v4l2_requestbuffers *p)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_reqbufs req;
	int ret = 0;

	ret = vb2_ioctl_reqbufs(file, priv, p);
	if (ret) {
		pr_debug("%s - vb2_ioctl_reqbufs failed\n", __func__);
		return ret;
	}

	pad = media_entity_remote_pad(&vid_node->pad);
	req.pad = pad->index;
	req.num_buffers = p->count;

	if (vid_node->path != VID_GRP_P2A)
		hailo15_subdev_call(vid_node, core, ioctl, ISPIOC_V4L2_REQBUFS,
				    &req);
	return ret;
}

static int hailo15_videoc_queryctrl(struct file *file, void *fh,
				    struct v4l2_queryctrl *a)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_pad_queryctrl pad_query_ctrl;
	int ret;

	pad = media_entity_remote_pad(&vid_node->pad);
	pad_query_ctrl.pad = pad->index;
	pad_query_ctrl.query_ctrl = a;
	ret = v4l2_subdev_call(vid_node->direct_sd, core, ioctl,
			       HAILO15_PAD_QUERYCTRL, &pad_query_ctrl);

	return ret;
}

static int hailo15_videoc_query_ext_ctrl(struct file *file, void *fh,
					 struct v4l2_query_ext_ctrl *a)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_pad_query_ext_ctrl pad_query_ext_ctrl;
	int ret;

	pad = media_entity_remote_pad(&vid_node->pad);
	pad_query_ext_ctrl.pad = pad->index;
	pad_query_ext_ctrl.query_ext_ctrl = a;
	ret = v4l2_subdev_call(vid_node->direct_sd, core, ioctl,
			       HAILO15_PAD_QUERY_EXT_CTRL, &pad_query_ext_ctrl);

	return ret;
}

static int hailo15_vidioc_g_ctrl(struct file *file, void *fh,
				 struct v4l2_control *a)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_pad_control pad_control;
	int ret;

	pad = media_entity_remote_pad(&vid_node->pad);
	pad_control.pad = pad->index;
	pad_control.control = a;
	ret = v4l2_subdev_call(vid_node->direct_sd, core, ioctl,
			       HAILO15_PAD_G_CTRL, &pad_control);

	return ret;
}

static int hailo15_vidioc_s_ctrl(struct file *file, void *fh,
				 struct v4l2_control *a)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_pad_control pad_control;
	int ret;

	pad = media_entity_remote_pad(&vid_node->pad);
	pad_control.pad = pad->index;
	pad_control.control = a;
	ret = v4l2_subdev_call(vid_node->direct_sd, core, ioctl,
			       HAILO15_PAD_S_CTRL, &pad_control);

	return ret;
}

static int hailo15_vidioc_g_ext_ctrls(struct file *file, void *fh,
				      struct v4l2_ext_controls *a)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_pad_ext_controls pad_ext_controls;
	int ret;

	pad = media_entity_remote_pad(&vid_node->pad);
	pad_ext_controls.pad = pad->index;
	pad_ext_controls.ext_controls = a;
	ret = v4l2_subdev_call(vid_node->direct_sd, core, ioctl,
			       HAILO15_PAD_G_EXT_CTRLS, &pad_ext_controls);

	return ret;
}

static int hailo15_vidioc_s_ext_ctrls(struct file *file, void *fh,
				      struct v4l2_ext_controls *a)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_pad_ext_controls pad_ext_controls;
	int ret;

	pad = media_entity_remote_pad(&vid_node->pad);
	pad_ext_controls.pad = pad->index;
	pad_ext_controls.ext_controls = a;
	ret = v4l2_subdev_call(vid_node->direct_sd, core, ioctl,
			       HAILO15_PAD_S_EXT_CTRLS, &pad_ext_controls);

	return ret;
}

static int hailo15_vidioc_try_ext_ctrls(struct file *file, void *fh,
					struct v4l2_ext_controls *a)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_pad_ext_controls pad_ext_controls;
	int ret;

	pad = media_entity_remote_pad(&vid_node->pad);
	pad_ext_controls.pad = pad->index;
	pad_ext_controls.ext_controls = a;
	ret = v4l2_subdev_call(vid_node->direct_sd, core, ioctl,
			       HAILO15_PAD_TRY_EXT_CTRLS, &pad_ext_controls);

	return ret;
}

static int hailo15_vidioc_querymenu(struct file *file, void *fh,
				    struct v4l2_querymenu *a)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_pad_querymenu pad_querymenu;
	int ret;

	pad = media_entity_remote_pad(&vid_node->pad);
	pad_querymenu.pad = pad->index;
	pad_querymenu.querymenu = a;
	ret = v4l2_subdev_call(vid_node->direct_sd, core, ioctl,
			       HAILO15_PAD_QUERYMENU, &pad_querymenu);

	return ret;
}

static const struct v4l2_ioctl_ops video_ioctl_ops = {
	.vidioc_querycap = hailo15_querycap,
	.vidioc_enum_fmt_vid_cap = hailo15_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap_mplane = hailo15_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap_mplane = hailo15_s_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap_mplane = hailo15_try_fmt_vid_cap,
	.vidioc_streamon = hailo15_streamon,
	.vidioc_streamoff = hailo15_streamoff,
	.vidioc_s_parm = hailo15_s_parm,
	.vidioc_g_parm = hailo15_g_parm,
	.vidioc_reqbufs = hailo15_reqbufs,
	.vidioc_queryctrl = hailo15_videoc_queryctrl,
	.vidioc_query_ext_ctrl = hailo15_videoc_query_ext_ctrl,
	.vidioc_g_ctrl = hailo15_vidioc_g_ctrl,
	.vidioc_s_ctrl = hailo15_vidioc_s_ctrl,
	.vidioc_g_ext_ctrls = hailo15_vidioc_g_ext_ctrls,
	.vidioc_s_ext_ctrls = hailo15_vidioc_s_ext_ctrls,
	.vidioc_try_ext_ctrls = hailo15_vidioc_try_ext_ctrls,
	.vidioc_querymenu = hailo15_vidioc_querymenu,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_subscribe_event = hailo15_vidioc_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static long hailo15_video_node_unlocked_ioctl(struct file *file,
					      unsigned int cmd,
					      unsigned long arg)
{
	long ret;
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct hailo15_dma_ctx *ctx;
	uint64_t fc;
	struct hailo15_vsm vsm;
	struct hailo15_get_vsm_params vsm_params;
	fc = 0;

	if (WARN_ON(!vid_node)) {
		return -EINVAL;
	}

	switch (cmd) {
	case VIDEO_FPS_MONITOR_SUBDEV_IOC:
		fc = 0;
		mutex_lock(&vid_node->ioctl_mutex);
		ctx = v4l2_get_subdevdata(vid_node->direct_sd);
		ret = hailo15_video_node_get_frame_count(ctx, &fc);
		if (ret) {
			mutex_unlock(&vid_node->ioctl_mutex);
			break;
		}
		ret = copy_to_user((void *)arg, &fc, sizeof(fc));
		mutex_unlock(&vid_node->ioctl_mutex);
		break;
	case VIDEO_GET_VSM_IOC:
		ret = copy_from_user(&vsm_params, (void *)arg,
				     sizeof(struct hailo15_get_vsm_params));
		if (ret) {
			ret = -EINVAL;
			break;
		}
		mutex_lock(&vid_node->ioctl_mutex);
		ctx = v4l2_get_subdevdata(vid_node->direct_sd);
		ret = hailo15_video_node_get_vsm(ctx, vid_node->path,
						 vsm_params.index, &vsm);
		if (ret) {
			mutex_unlock(&vid_node->ioctl_mutex);
			break;
		}
		ret = copy_to_user(
			&(((struct hailo15_get_vsm_params *)arg)->vsm), &vsm,
			sizeof(struct hailo15_vsm));
		mutex_unlock(&vid_node->ioctl_mutex);
		break;
	default:
		/* video ioctls locks are handled by v4l2 framework */
		ret = video_ioctl2(file, cmd, arg);
		break;
	}

	return ret;
}

static int hailo15_video_node_stream_cancel(struct hailo15_video_node *vid_node)
{
	int ret;

	if (!vid_node->streaming)
		return -EINVAL;

	ret = hailo15_video_node_subdev_set_stream(vid_node, STREAM_OFF);
	if (ret) {
		dev_err(vid_node->dev,
			"can't stop stream on node %d with error %d\n",
			vid_node->id, ret);
	}

	hailo15_video_node_queue_clean(vid_node, VB2_BUF_STATE_ERROR);
	vid_node->streaming = 0;

	if (vid_node->path != VID_GRP_P2A) {
		ret = hailo15_video_post_event_release_pipeline(vid_node);
		if (ret) {
			pr_err("%s - post event release pipeline failed\n",
			       __func__);
		}
		vid_node->pipeline_init = 0;
	}
	vid_node->sequence = 0;
	return ret;
}

int hailo15_video_node_open(struct file *file)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	int ret;

	ret = v4l2_fh_open(file);
	if (ret) {
		pr_err("%s - failed to open vid_node %d\n", __func__,
		       vid_node->id);
		return ret;
	}

	return 0;
}

int hailo15_video_node_release(struct file *file)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	int ret;

	if (vid_node->streaming &&
	    file->private_data == vid_node->queue.owner) {
		ret = hailo15_video_node_stream_cancel(vid_node);
		if (ret) {
			pr_err("%s - stream_cancel failed returning: %d\n",
			       __func__, ret);
			return ret;
		}
	}

	if (file->private_data == vid_node->queue.owner) {
		vb2_fop_release(file);
	}

	return 0;
}

static int hailo15_video_node_mmap(struct file *file,
				   struct vm_area_struct *vma)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	struct hailo15_rmem rmem;
	struct hailo15_video_event_resource resource;
	unsigned long video_event_pfn;
	unsigned long sd_event_pfn;
	unsigned long size;
	struct v4l2_framebuffer fb;
	struct hailo15_dma_ctx *ctx;
	int ret;
	int sd_event_valid = 0;

	memset(&rmem, 0, sizeof(rmem));
	memset(&fb, 0, sizeof(fb));

	ctx = v4l2_get_subdevdata(vid_node->direct_sd);

	ret = hailo15_video_node_get_rmem(ctx, &rmem);
	if (!ret) {
		if (rmem.addr && vma->vm_pgoff == (rmem.addr >> PAGE_SHIFT)) {
			if (vma->vm_end - vma->vm_start > rmem.size) {
				return -ENOMEM;
			}
			return remap_pfn_range(
				vma, vma->vm_start, vma->vm_pgoff,
				vma->vm_end - vma->vm_start,
				pgprot_noncached(vma->vm_page_prot));
		}
	}

	video_event_pfn = vid_node->event_resource.phy_addr >> PAGE_SHIFT;
	ret = hailo15_video_node_get_event_resource(ctx, &resource);
	sd_event_pfn = !ret ? resource.phy_addr >> PAGE_SHIFT : 0;
	if (!ret) {
		sd_event_valid = 1;
	}

	pr_debug(
		"%s - vma->vm_pgoff: 0x%lx, video_event_pfn: 0x%lx, sd_event_pfn: 0x%lx\n",
		__func__, vma->vm_pgoff, video_event_pfn, sd_event_pfn);

	if (vma->vm_pgoff == video_event_pfn || (vma->vm_pgoff == sd_event_pfn && sd_event_valid)) {
		size = vma->vm_end - vma->vm_start;

		if (size > HAILO15_EVENT_RESOURCE_SIZE) {
			pr_err("%s - not enough memory for 0x%lx size\n",
			       __func__, size);
			return -ENOMEM;
		}

		return remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
				       vma->vm_page_prot);
	}

	return vb2_fop_mmap(file, vma);
}

static unsigned int hailo15_video_node_poll(struct file *file,
					    struct poll_table_struct *wait)
{
	struct hailo15_video_node *vid_node = video_drvdata(file);
	if (vid_node->streaming) {
		return vb2_poll(&vid_node->queue, file, wait);
	}
	return v4l2_ctrl_poll(file, wait);
}
static struct v4l2_file_operations video_ops = {
	.owner = THIS_MODULE,
	.read = vb2_fop_read,
	.mmap = hailo15_video_node_mmap,
	.poll = hailo15_video_node_poll,
	.open = hailo15_video_node_open,
	.release = hailo15_video_node_release,
	.unlocked_ioctl = hailo15_video_node_unlocked_ioctl,
};

static int hailo15_queue_setup(struct vb2_queue *q, unsigned int *nbuffers,
			       unsigned int *nplanes, unsigned int sizes[],
			       struct device *alloc_devs[])
{
	struct hailo15_video_node *vid_node = queue_to_node(q);
	const struct hailo15_video_fmt *format;
	int plane, line_length, height, bytesperline;
	format = NULL;
	if (WARN_ON(!vid_node)) {
		pr_err("%s - returning -EINVAL\n", __func__);
		return -EINVAL;
	}

	if (q->num_buffers >= MAX_NUM_FRAMES) {
		dev_info(vid_node->dev, "already allocated maximum buffers");
		return -EINVAL;
	}

	if (*nbuffers > MAX_NUM_FRAMES - q->num_buffers)
		*nbuffers = MAX_NUM_FRAMES - q->num_buffers;

	format =
		hailo15_fourcc_get_format(vid_node->fmt.fmt.pix_mp.pixelformat);
	if (!format) {
		pr_err("%s - failed to get fourcc format\n", __func__);
		return -EINVAL;
	}

	line_length = ALIGN_UP(vid_node->fmt.fmt.pix_mp.width, STRIDE_ALIGN);
	height = vid_node->fmt.fmt.pix_mp.height;

	if(*nplanes){
		if(*nplanes != format->num_planes)
			return -EINVAL;
		for (plane = 0; plane < format->num_planes; ++plane) {
			bytesperline = hailo15_plane_get_bytesperline(
				format, line_length, plane);
			if (sizes[plane] !=
			    hailo15_plane_get_sizeimage(format, height,
							bytesperline, plane)) {
				return -EINVAL;
			}
		}
	}
	else {
		*nplanes = format->num_planes;
		for (plane = 0; plane < format->num_planes; ++plane) {
			bytesperline = hailo15_plane_get_bytesperline(
				format, line_length, plane);
			sizes[plane] = hailo15_plane_get_sizeimage(
				format, height, bytesperline, plane);
		}
	}
	return 0;
}

static int hailo15_video_node_buffer_init(struct vb2_buffer *vb)
{
	struct hailo15_video_node *vid_node = queue_to_node(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf =
		container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	struct hailo15_buffer *buf =
		container_of(vbuf, struct hailo15_buffer, vb);
	struct media_pad *pad;
	int plane;

	memset(buf->dma, 0, sizeof(buf->dma));
	if (vb->num_planes >= FMT_MAX_PLANES) {
		pr_info("%s - num of planes too big: %u\n", __func__,
			vb->num_planes);
		return -EINVAL;
	}

	for (plane = 0; plane < vb->num_planes; ++plane) {
		buf->dma[plane] =
			vb2_dma_contig_plane_dma_addr(vb, plane);
	}

	pad = media_entity_remote_pad(&vid_node->pad);

	if (!pad) {
		dev_err(vid_node->dev,
			"can't queue buffer, got null pad for node %d\n",
			vid_node->id);
		return -EINVAL;
	}

	buf->pad = pad;

	buf->sd = media_entity_to_v4l2_subdev(pad->entity);

	return 0;
}

static int hailo15_video_device_process_vb2_buffer(struct vb2_buffer *vb)
{
	struct hailo15_video_node *vid_node = queue_to_node(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf =
		container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	struct hailo15_buffer *buf =
		container_of(vbuf, struct hailo15_buffer, vb);
	struct hailo15_dma_ctx *ctx;

	if (WARN_ON(!vid_node) || WARN_ON(!vid_node->direct_sd)) {
		pr_err("%s - returning -EINVAL\n", __func__);
		return -EINVAL;
	}

	buf->grp_id = vid_node->path;
	ctx = v4l2_get_subdevdata(vid_node->direct_sd);
	return hailo15_video_node_buffer_process(ctx, buf);
}

static void hailo15_buffer_queue(struct vb2_buffer *vb)
{
	struct hailo15_video_node *vid_node = queue_to_node(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf =
		container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	struct hailo15_buffer *buf =
		container_of(vbuf, struct hailo15_buffer, vb);
	unsigned long flags;

	if (WARN_ON(!vb) || WARN_ON(!vb->vb2_queue)) {
		pr_err("%s - WARN_ON(!vb) || WARN_ON(!vb->vb2_queue), returning\n",
		       __func__);
		return;
	}
	/* if the list is empty, it will be called twice. */
	if (hailo15_video_node_buffer_init(vb)) {
		pr_err("%s - failed hailo15_video_node_buffer_init, returning\n",
		       __func__);
		return;
	}

	spin_lock_irqsave(&vid_node->qlock, flags);
	if (list_empty(&vid_node->buf_queue)) {
		if (hailo15_video_device_process_vb2_buffer(vb)) {
			spin_unlock_irqrestore(&vid_node->qlock, flags);
			return;
		}
	}
	list_add_tail(&buf->irqlist, &vid_node->buf_queue);
	spin_unlock_irqrestore(&vid_node->qlock, flags);
}

static int hailo15_video_device_buffer_done(struct hailo15_dma_ctx *ctx,
					    struct hailo15_buffer *buf,
					    int grp_id)
{
	struct hailo15_video_node *vid_node;
	unsigned long flags;
	struct hailo15_buffer *next_buffer;
	int ret;

	vid_node = NULL;
	if (grp_id < 0 || grp_id >= VID_GRP_MAX)
		return -EINVAL;

	ret = hailo15_video_node_get_private_data(ctx, grp_id,
						  (void **)&vid_node);
	if (ret)
		return ret;

	if (WARN_ON(!vid_node)) {
		pr_err("%s - WARN_ON(!vid_node), returning\n", __func__);
		return -EINVAL;
	}

	if (vid_node->prev_buf) {
		vid_node->prev_buf->vb.sequence = vid_node->sequence++;
		vid_node->prev_buf->vb.vb2_buf.timestamp = ktime_get_ns();
		vb2_buffer_done(&vid_node->prev_buf->vb.vb2_buf,
				VB2_BUF_STATE_DONE);
	}

	if (buf) {
		spin_lock_irqsave(&vid_node->qlock, flags);
		list_del(&buf->irqlist);
		next_buffer = list_first_entry_or_null(
			&vid_node->buf_queue, struct hailo15_buffer, irqlist);
		spin_unlock_irqrestore(&vid_node->qlock, flags);

		if (next_buffer) {
			/* This might cost us some calculations.       */
			/* Consider moving the queue into the context  */
			/* for more ops efficient method               */
			hailo15_video_device_process_vb2_buffer(
				&next_buffer->vb.vb2_buf);
		} else {
			hailo15_video_node_queue_empty(ctx, vid_node->path);
		}
	}

	vid_node->prev_buf = buf;
	return 0;
}

static int hailo15_buffer_prepare(struct vb2_buffer *vb)
{
	struct hailo15_video_node *vid_node = queue_to_node(vb->vb2_queue);
	int plane, line_length, bytesperline;
	struct v4l2_pix_format_mplane *mfmt = &vid_node->fmt.fmt.pix_mp;
	const struct hailo15_video_fmt *format;

	format = hailo15_fourcc_get_format(mfmt->pixelformat);

	if (format == NULL) {
		pr_err("%s - returning -EINVAL\n", __func__);
		return -EINVAL;
	}

	/* maybe should validate addr alignment? */
	for (plane = 0; plane < mfmt->num_planes; ++plane) {
		line_length = ALIGN_UP(mfmt->width, STRIDE_ALIGN);
		bytesperline = hailo15_plane_get_bytesperline(
			format, line_length, plane);
		vb2_set_plane_payload(
			vb, plane,
			hailo15_plane_get_sizeimage(format, mfmt->height,
						    bytesperline, plane));
	}

	return 0;
}

int hailo15_video_node_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct hailo15_video_node *vid_node = queue_to_node(q);
	int ret = 0;

	if (WARN_ON(!vid_node))
		return -EINVAL;

	if (!vid_node->streaming) {
		ret = hailo15_video_node_subdev_set_stream(vid_node, STREAM_ON);
		if (ret) {
			dev_err(vid_node->dev,
				"can't start stream on node %d with error %d\n",
				vid_node->id, ret);
			goto err;
		}

		vid_node->streaming = 1;
	}
	goto out;
err:
	hailo15_video_node_queue_clean(vid_node, VB2_BUF_STATE_QUEUED);
out:
	return ret;
}

void hailo15_video_node_stop_streaming(struct vb2_queue *q)
{
	struct hailo15_video_node *vid_node = queue_to_node(q);

	if (WARN_ON(!vid_node))
		return;

	hailo15_video_node_stream_cancel(vid_node);
}

static struct vb2_ops hailo15_buffer_ops = {
	.start_streaming = hailo15_video_node_start_streaming,
	.stop_streaming = hailo15_video_node_stop_streaming,
	.queue_setup = hailo15_queue_setup,
	.buf_prepare = hailo15_buffer_prepare,
	.buf_queue = hailo15_buffer_queue,
};

static void
hailo15_video_node_video_device_unregister(struct hailo15_video_node *vid_node)
{
	video_unregister_device(vid_node->video_dev);
}

static void
hailo15_video_node_queue_destroy(struct hailo15_video_node *vid_node)
{
	hailo15_video_node_queue_clean(vid_node, VB2_BUF_STATE_ERROR);
}

static void
hailo15_video_node_destroy_media_entity(struct hailo15_video_node *vid_node)
{
	media_entity_cleanup(&vid_node->video_dev->entity);
}

static void
hailo15_video_node_video_device_destroy(struct hailo15_video_node *vid_node)
{
	hailo15_video_node_destroy_media_entity(vid_node);
	mutex_destroy(&vid_node->ioctl_mutex);
	video_device_release(vid_node->video_dev);
}

static void
hailo15_video_node_destroy_v4l2_device(struct hailo15_video_node *vid_node)
{
	v4l2_device_unregister(vid_node->v4l2_dev);
	kfree(vid_node->v4l2_dev);
}

static void
hailo15_video_node_destroy_events(struct hailo15_video_node *vid_node)
{
	if (vid_node->event_resource.virt_addr)
		kfree(vid_node->event_resource.virt_addr);
}

static void hailo15_video_node_destroy(struct hailo15_video_node *vid_node)
{
	hailo15_video_node_video_device_unregister(vid_node);
	hailo15_video_node_destroy_v4l2_device(vid_node);
	hailo15_video_node_queue_destroy(vid_node);
	hailo15_video_node_video_device_destroy(vid_node);
	hailo15_video_node_destroy_events(vid_node);
}

static int hailo15_video_device_destroy(struct hailo15_vid_cap_device *vid_dev)
{
	struct hailo15_subdev_list *tmp;
	struct list_head *pos;
	struct list_head *npos;
	int video_id;
	for (video_id = 0; video_id < MAX_VIDEO_NODE_NUM; ++video_id) {
		if (vid_dev->vid_nodes[video_id]) {
			hailo15_video_node_destroy(
				vid_dev->vid_nodes[video_id]);
			kfree(vid_dev->vid_nodes[video_id]);
		}
	}

	media_device_unregister(&vid_dev->mdev);
	media_device_cleanup(&vid_dev->mdev);
	v4l2_async_notifier_unregister(&vid_dev->subdev_notifier);
	v4l2_async_notifier_cleanup(&vid_dev->subdev_notifier);
	list_for_each_safe (pos, npos, &vid_dev->subdevs) {
		tmp = list_entry(pos, struct hailo15_subdev_list, subdev_list);
		list_del(pos);
		kfree(tmp);
	}
	return 0;
}

static int hailo15_video_node_bind(struct hailo15_video_node *vid_node,
				   struct fwnode_handle *vid_ep,
				   struct v4l2_subdev *sd)
{
	struct fwnode_handle *sd_ep = NULL;
	struct fwnode_handle *vid_remote = NULL;
	struct fwnode_endpoint vid_remote_ep = { 0 };
	struct v4l2_fwnode_link link;
	struct media_entity *source, *sink;
	unsigned int source_pad, sink_pad;
	struct hailo15_dma_ctx *ctx;
	int ret;

	vid_remote = fwnode_graph_get_remote_endpoint(vid_ep);
	if (!vid_remote) {
		pr_err("Couldn't get remote endpoint for vid_node: %d endpoint: %s\n",
		       vid_node->id, fwnode_get_name(vid_ep));
		ret = -EINVAL;
		goto out;
	}

	fwnode_graph_parse_endpoint(vid_remote, &vid_remote_ep);

	sd_ep = fwnode_graph_get_endpoint_by_id(sd->fwnode, vid_remote_ep.port,
						0,
						FWNODE_GRAPH_DEVICE_DISABLED);
	if (!sd_ep) {
		pr_err("Couldn't get remote endpoint for sd: %s\n",
		       fwnode_get_name(sd->fwnode));
		ret = -EINVAL;
		goto err_sd_ep;
	}
	fwnode_handle_put(vid_remote);

	ret = v4l2_fwnode_parse_link(sd_ep, &link);
	if (ret < 0) {
		pr_err("failed to parse link for %pOF: %d\n", to_of_node(sd_ep),
		       ret);
		ret = -EINVAL;
		goto out;
	}
	fwnode_handle_put(sd_ep);

	source = &sd->entity;
	source_pad = link.local_port;
	sink = &vid_node->video_dev->entity;
	sink_pad = 0;
	v4l2_fwnode_put_link(&link);
	ret = media_create_pad_link(source, source_pad, sink, sink_pad,
				    MEDIA_LNK_FL_ENABLED);
	if (ret) {
		pr_err("failed to create %s:%u -> %s:%u link\n", source->name,
		       source_pad, sink->name, sink_pad);
		ret = -EINVAL;
		goto out;
	}

	pr_debug("created %s:%u -> %s:%u link\n", source->name, source_pad,
		 sink->name, sink_pad);
	vid_node->direct_sd = sd;

	ctx = v4l2_get_subdevdata(sd);
	if (!ctx) {
		pr_err("failed to get ctx from subdevdata of sd: %s\n",
		       sd->name);
		goto out;
	}
	ctx->buf_ctx.ops->buffer_done = hailo15_video_device_buffer_done;
	hailo15_video_node_set_private_data(ctx, vid_node->path,
					    (void *)vid_node);
	goto out;

err_sd_ep:
	fwnode_handle_put(vid_remote);
out:
	return ret;
}

static int
hailo15_video_node_notifier_bound(struct v4l2_async_notifier *notifier,
				  struct v4l2_subdev *source_subdev,
				  struct v4l2_async_subdev *asd)
{
	struct hailo15_vid_cap_device *vid_dev = container_of(
		notifier, struct hailo15_vid_cap_device, subdev_notifier);
	struct hailo15_subdev_list *new_entry =
		kzalloc(sizeof(struct hailo15_subdev_list), GFP_KERNEL);
	new_entry->subdev = source_subdev;
	list_add(&new_entry->subdev_list, &vid_dev->subdevs);
	return 0;
}

static void
hailo15_video_node_notifier_unbind(struct v4l2_async_notifier *notifier,
				   struct v4l2_subdev *sd,
				   struct v4l2_async_subdev *asd)
{
	struct hailo15_vid_cap_device *vid_dev = container_of(
		notifier, struct hailo15_vid_cap_device, subdev_notifier);
	int i;
	for (i = 0; i < MAX_VIDEO_NODE_NUM; ++i) {
		if (vid_dev->vid_nodes[i]) {
			media_entity_remove_links(
				&vid_dev->vid_nodes[i]->video_dev->entity);
		}
	}
}

static int hailo15_bind_sd2remote(struct media_entity *sd_entity,
				  struct media_entity *remote_entity,
				  struct fwnode_handle *remote_ep)
{
	struct media_entity *source, *sink;
	struct v4l2_fwnode_link link;
	unsigned int source_pad, sink_pad;
	int ret;

	ret = v4l2_fwnode_parse_link(remote_ep, &link);
	if (ret < 0) {
		pr_err("failed to parse link for remote_ep %s\n",
		       fwnode_get_name(remote_ep));
		ret = -EINVAL;
		goto out;
	}

	// check which entity is the sink and which is the source for the link
	if ((sd_entity->pads[link.local_port].flags & MEDIA_PAD_FL_SOURCE)) {
		source = sd_entity;
		source_pad = link.local_port;
		sink = remote_entity;
		sink_pad = link.remote_port;
	} else {
		source = remote_entity;
		source_pad = link.remote_port;
		sink = sd_entity;
		sink_pad = link.local_port;
	}

	if (media_entity_find_link(&source->pads[source_pad],
				   &sink->pads[sink_pad])) {
		pr_info("%s - link already exists, return successfuly\n",
			__func__);
		goto out;
	}

	ret = media_create_pad_link(source, source_pad, sink, sink_pad,
				    MEDIA_LNK_FL_ENABLED);
	if (ret) {
		pr_err("failed to create %s:%u -> %s:%u link\n", source->name,
		       source_pad, sink->name, sink_pad);
		ret = -EINVAL;
	}

	pr_debug("created %s:%u -> %s:%u link\n", source->name, source_pad,
		 sink->name, sink_pad);

out:
	return ret;
}

static int hailo15_find_remote_in_subdev_list(struct fwnode_handle *ep,
					      struct list_head *subdevs,
					      struct v4l2_subdev **o_subdev)
{
	struct fwnode_handle *remote_fwnode;
	struct hailo15_subdev_list *remote_list;

	remote_fwnode = fwnode_graph_get_remote_port_parent(ep);
	if (!remote_fwnode) {
		pr_err("failed to get remote port parent of %s\n",
		       fwnode_get_name(ep));
		return -1;
	}

	// look for the remote node of the video node in the subdevs list to create a link between them
	list_for_each_entry (remote_list, subdevs, subdev_list) {
		if (remote_list->subdev->fwnode == remote_fwnode) {
			*o_subdev = remote_list->subdev;
			fwnode_handle_put(remote_fwnode);
			return 0;
		}
	}
	fwnode_handle_put(remote_fwnode);
	return -1;
}

static int
hailo15_video_node_notifier_complete(struct v4l2_async_notifier *notifier)
{
	struct hailo15_vid_cap_device *vid_dev = container_of(
		notifier, struct hailo15_vid_cap_device, subdev_notifier);
	struct hailo15_video_node *vid_node;
	struct fwnode_handle *ep;
	struct hailo15_subdev_list *remote_list;
	struct v4l2_subdev *found_sd;
	int i;
	int found;
	int ret;

	// bind video nodes to their subdevs
	for (i = 0; i < MAX_VIDEO_NODE_NUM; ++i) {
		remote_list = NULL;
		ep = NULL;
		if (vid_dev->vid_nodes[i]) {
			vid_node = vid_dev->vid_nodes[i];

			ep = fwnode_graph_get_endpoint_by_id(
				dev_fwnode(vid_node->dev), vid_node->id, 0,
				FWNODE_GRAPH_DEVICE_DISABLED);
			if (!ep) {
				pr_err("failed to get endpoint 0 of node %d, skipping...\n",
				       vid_node->id);
				continue;
			}

			found_sd = NULL;
			found = hailo15_find_remote_in_subdev_list(
				ep, &vid_dev->subdevs, &found_sd);
			if (found == -1)
				continue;

			ret = hailo15_video_node_bind(vid_node, ep, found_sd);
			if (ret) {
				pr_err("failed to bind node %d, skipping...\n",
				       vid_node->id);
				goto err_bind;
			}
		}
	}

	// in case some of the subdevs need to get bound to their own subdevs - create those links
	list_for_each_entry (remote_list, &vid_dev->subdevs, subdev_list) {
		// go over all endpoints of the subdev and look for their remote endpoint in the video device's subdevs list
		while ((ep = fwnode_graph_get_next_endpoint(
				remote_list->subdev->fwnode, ep))) {
			found_sd = NULL;
			found = hailo15_find_remote_in_subdev_list(
				ep, &vid_dev->subdevs, &found_sd);
			if (found == -1)
				continue;

			ret = hailo15_bind_sd2remote(
				&remote_list->subdev->entity, &found_sd->entity,
				ep);
			if (ret) {
				pr_err("failed to bind subdev %s to subdev %s\n",
				       remote_list->subdev->name,
				       found_sd->name);
				goto err_bind;
			}
		}
	}

	ret = media_device_register(&vid_dev->mdev);
	if (ret) {
		pr_err("failed to init media device err=%d\n", ret);
		goto err_media_device_register;
	}

	goto out;

err_media_device_register:
err_bind:
	hailo15_video_device_destroy(vid_dev);
	kfree(vid_dev);
out:
	fwnode_handle_put(ep);
	return v4l2_device_register_subdev_nodes(vid_node->v4l2_dev);
}

static const struct v4l2_async_notifier_operations sd_async_notifier_ops = {
	.bound = hailo15_video_node_notifier_bound,
	.unbind = hailo15_video_node_notifier_unbind,
	.complete = hailo15_video_node_notifier_complete,
};

static const struct media_entity_operations hailo15_video_media_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int hailo15_video_node_queue_init(struct hailo15_video_node *vid_node)
{
	int ret;

	spin_lock_init(&vid_node->qlock);
	INIT_LIST_HEAD(&vid_node->buf_queue);
	mutex_init(&vid_node->buffer_mutex);

	vid_node->queue.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	vid_node->queue.drv_priv = vid_node;
	vid_node->queue.ops = &hailo15_buffer_ops;
	vid_node->queue.io_modes = VB2_MMAP | VB2_DMABUF;
	vid_node->queue.mem_ops = &vb2_dma_contig_memops;
	vid_node->queue.buf_struct_size = sizeof(struct hailo15_buffer);
	vid_node->queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	vid_node->queue.dev = vid_node->dev;
	vid_node->queue.lock = &vid_node->buffer_mutex;
	vid_node->queue.min_buffers_needed = MIN_BUFFERS_NEEDED;
	ret = vb2_queue_init(&vid_node->queue);
	if (ret) {
		return ret;
	}

	vid_node->video_dev->queue = &vid_node->queue;

	return 0;
}

static int
hailo15_video_node_init_media_entity(struct hailo15_video_node *vid_node)
{
	if (WARN_ON(!vid_node->video_dev)) {
		return -EINVAL;
	}

	vid_node->video_dev->entity.name = vid_node->video_dev->name;
	vid_node->video_dev->entity.obj_type = MEDIA_ENTITY_TYPE_VIDEO_DEVICE;
	vid_node->video_dev->entity.function = MEDIA_ENT_F_IO_V4L;
	vid_node->video_dev->entity.ops = &hailo15_video_media_ops;

	vid_node->pad.flags = MEDIA_PAD_FL_SINK | MEDIA_PAD_FL_MUST_CONNECT;
	return media_entity_pads_init(&vid_node->video_dev->entity, 1,
				      &vid_node->pad);
}

static int
hailo15_video_node_video_device_init(struct hailo15_video_node *vid_node)
{
	int ret;
	struct device *dev;

	if (WARN_ON(!vid_node->mdev)) {
		return -EINVAL;
	}

	ret = 0;
	dev = vid_node->mdev->dev;

	vid_node->video_dev = video_device_alloc();

	if (!vid_node->video_dev) {
		dev_err(dev, "can't allocate video device for node %d\n",
			vid_node->id);
		return -ENOMEM;
	}

	mutex_init(&vid_node->ioctl_mutex);

	sprintf(vid_node->video_dev->name, "hailo-vid-cap-%d", vid_node->id);
	/*initialize video device*/
	vid_node->video_dev->release =
		video_device_release_empty; /* We will release the video device on our own */
	vid_node->video_dev->fops = &video_ops;
	vid_node->video_dev->ioctl_ops = &video_ioctl_ops;
	vid_node->video_dev->minor = vid_node->id;
	vid_node->video_dev->vfl_type = VFL_TYPE_VIDEO;
	vid_node->video_dev->vfl_dir = VFL_DIR_RX;
	vid_node->video_dev->device_caps =
		V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING | V4L2_CAP_EXT_PIX_FORMAT;

	video_set_drvdata(vid_node->video_dev, vid_node);

	ret = hailo15_video_node_init_media_entity(vid_node);
	if (ret) {
		dev_err(vid_node->dev, "cant init media entity for node %d\n",
			vid_node->id);
		goto err_media_entity;
	}

	goto out;

err_media_entity:
	mutex_destroy(&vid_node->ioctl_mutex);
	video_device_release(vid_node->video_dev);
out:
	return ret;
}

static int
hailo15_video_node_video_device_register(struct hailo15_video_node *vid_node)
{
	if (WARN_ON(!vid_node))
		return -EINVAL;
	return video_register_device(vid_node->video_dev, VFL_TYPE_VIDEO,
				     vid_node->id);
}

static int
hailo15_video_node_init_v4l2_device(struct hailo15_video_node *vid_node)
{
	int ret;

	ret = 0;
	if (WARN_ON(!vid_node->video_dev)) {
		return -EINVAL;
	}

	vid_node->v4l2_dev = kzalloc(sizeof(struct v4l2_device), GFP_KERNEL);
	if (WARN_ON(!vid_node->v4l2_dev)) {
		return -ENOMEM;
	}

	ret = v4l2_device_register(vid_node->dev, vid_node->v4l2_dev);
	if (WARN_ON(ret)) {
		goto err_v4l2_device_register;
	}

	ret = v4l2_device_register_subdev_nodes(vid_node->v4l2_dev);
	if (ret) {
		pr_err("%sFailed registering the subdevs with code %d \n",
		       __func__, ret);
		goto err_register_subdev_nodes;
	}

	vid_node->video_dev->v4l2_dev = vid_node->v4l2_dev;
	vid_node->v4l2_dev->mdev = vid_node->mdev;

	goto out;

err_register_subdev_nodes:
	v4l2_device_unregister(vid_node->v4l2_dev);
err_v4l2_device_register:
	kfree(vid_node->v4l2_dev);
out:
	return ret;
}

static int hailo15_video_node_init_fmt(struct hailo15_video_node *vid_node)
{
	vid_node->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vid_node->fmt.fmt.pix.field = V4L2_FIELD_NONE;
	vid_node->fmt.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;

	return 0;
}

static int hailo15_video_node_init_events(struct hailo15_video_node *vid_node)
{
	mutex_init(&vid_node->event_resource.event_lock);
	vid_node->event_resource.virt_addr =
		kmalloc(HAILO15_EVENT_RESOURCE_SIZE, GFP_KERNEL);
	if (!vid_node->event_resource.virt_addr)
		return -ENOMEM;

	vid_node->event_resource.phy_addr =
		virt_to_phys(vid_node->event_resource.virt_addr);
	vid_node->event_resource.size = HAILO15_EVENT_RESOURCE_SIZE;
	return 0;
}

static int hailo15_video_node_init(struct hailo15_video_node *vid_node)
{
	int ret;

	ret = hailo15_video_node_video_device_init(vid_node);
	if (ret) {
		dev_err(vid_node->dev, "can't init video device for node %d\n",
			vid_node->id);
		goto out;
	}

	ret = hailo15_video_node_queue_init(vid_node);
	if (ret) {
		dev_err(vid_node->dev,
			"can't init video node queue for node %d\n",
			vid_node->id);
		goto err_queue_init;
	}

	ret = hailo15_video_node_init_fmt(vid_node);
	if (ret) {
		dev_err(vid_node->dev, "can't init fmt for node %d\n",
			vid_node->id);
		goto err_fmt;
	}

	ret = hailo15_video_node_init_v4l2_device(vid_node);
	if (ret) {
		dev_err(vid_node->dev, "can't init v4l2 device for node %d\n",
			vid_node->id);
		goto err_v4l2_device;
	}

	ret = hailo15_video_node_video_device_register(vid_node);
	if (ret) {
		dev_err(vid_node->dev,
			"can't register video device for node %d\n",
			vid_node->id);
		goto err_video_register;
	}

	ret = hailo15_video_node_init_events(vid_node);
	if (ret) {
		dev_err(vid_node->dev, "can't init events for node %d\n",
			vid_node->id);
		goto err_video_register;
	}
	goto out;

/* TODO: MSW-2716: This code section is unused */
/*
err_async_notifier:
	hailo15_video_node_video_device_unregister(vid_node);
*/
err_video_register:
	hailo15_video_node_destroy_v4l2_device(vid_node);
err_v4l2_device:
err_fmt:
	hailo15_video_node_queue_destroy(vid_node);
err_queue_init:
	hailo15_video_node_video_device_destroy(vid_node);
out:
	return ret;
}

static int hailo15_video_init_vid_nodes(struct hailo15_vid_cap_device *vid_dev)
{
	struct hailo15_video_node *vid_node = NULL;
	struct fwnode_handle *ep = NULL;
	struct fwnode_handle *remote_ep;
	struct v4l2_async_subdev *asd;
	int i = 0;
	uint32_t path;
	int ret;
	struct fwnode_endpoint fwnode_ep;

	while (i++ < MAX_VIDEO_NODE_NUM) {
		ep = fwnode_graph_get_next_endpoint(dev_fwnode(vid_dev->dev),
						    ep);
		if (!ep)
			break;

		// parse local and remote fwnodes of ep
		memset(&fwnode_ep, 0, sizeof(struct fwnode_endpoint));

		ret = fwnode_graph_parse_endpoint(ep, &fwnode_ep);
		if (ret) {
			pr_err("failed to parse endpoint %d, skipping...",
			       i - 1);
			fwnode_handle_put(ep);
			continue;
		}

		VIDEO_INDEX_VALIDATE(fwnode_ep.port, ret = -EINVAL;
				     goto err_invalid_port);

		pr_info("%s - parsed video endpoint with port: %d, id: %d\n",
			__func__, fwnode_ep.port, fwnode_ep.id);

		remote_ep = fwnode_graph_get_remote_endpoint(ep);
		if (!remote_ep) {
			pr_err("failed to get remote endpoint for endpoint %d, skipping...\n",
			       i - 1);
			fwnode_handle_put(remote_ep);
			continue;
		}
		fwnode_handle_put(remote_ep);

		// local and remote ep exist - add remote subdev to notifier asd list
		asd = v4l2_async_notifier_add_fwnode_remote_subdev(
			&vid_dev->subdev_notifier, ep,
			struct v4l2_async_subdev);
		if (IS_ERR(asd)) {
			if (PTR_ERR(asd) == -EEXIST) {
				pr_debug(
					"async subdev for remote of %s - already added to a notifier, continue...",
					fwnode_get_name(ep));
			} else {
				pr_err("failed to add remote subdev to notifier! %ld\n",
				       PTR_ERR(asd));
				ret = PTR_ERR(asd);
				goto err_notifier_add;
			}
		}

		// initialize video node
		vid_node =
			kzalloc(sizeof(struct hailo15_video_node), GFP_KERNEL);
		if (!vid_node) {
			ret = -ENOMEM;
			goto err_node_alloc;
		}

		vid_node->asd = asd;
		vid_node->dev = vid_dev->dev;
		vid_node->mdev = &vid_dev->mdev;

		// node id taken from port property of the endpoint (assumes one endpoint per port)
		vid_node->id = fwnode_ep.port;

		// read path property so s_stream knows from where it was called
		ret = fwnode_property_read_u32(ep, "path", &path);
		if (ret || path >= VID_GRP_MAX) {
			pr_err("failed to read path property from video node %d, skipping...\n",
			       fwnode_ep.port);
			kfree(vid_node);
			continue;
		}

		vid_node->path = path;
		ret = hailo15_video_node_init(vid_node);
		if (ret) {
			pr_err("failed to init video node %d, skipping...\n",
			       fwnode_ep.port);
			kfree(vid_node);
			continue;
		}

		vid_dev->vid_nodes[vid_node->id] = vid_node;
		pr_info("vid_node %d initialized successfully\n", vid_node->id);
	}

	// find the first node that exists and use its v4l2_device to register the notifier
	for (i = 0; i < MAX_VIDEO_NODE_NUM; ++i) {
		if (vid_dev->vid_nodes[i]) {
			ret = v4l2_async_notifier_register(
				vid_node->v4l2_dev, &vid_dev->subdev_notifier);
			if (ret) {
				pr_err("failed to register notifier\n");
				goto err_register_notifier;
			}
			break;
		}
	}

	if (i == MAX_VIDEO_NODE_NUM) {
		pr_err("no video node initialized\n");
		ret = -EINTR;
		goto err_register_notifier;
	}
	goto out;

err_register_notifier:
err_node_alloc:
	for (i = 0; i < MAX_VIDEO_NODE_NUM; ++i) {
		if (vid_dev->vid_nodes[i]) {
			hailo15_video_node_destroy(vid_dev->vid_nodes[i]);
			kfree(vid_dev->vid_nodes[i]);
		}
	}
err_invalid_port:
err_notifier_add:
	fwnode_handle_put(ep);
out:
	return ret;
}

static int hailo15_video_probe(struct platform_device *pdev)
{
	int ret;
	struct hailo15_vid_cap_device *vid_dev;

	vid_dev = kzalloc(sizeof(struct hailo15_vid_cap_device), GFP_KERNEL);
	if (!vid_dev) {
		pr_err("failed to allocate vid cap device\n");
		return -ENOMEM;
	}

	vid_dev->dev = &pdev->dev;

	platform_set_drvdata(pdev, vid_dev);

	strscpy(vid_dev->mdev.model, "hailo_media",
		sizeof(vid_dev->mdev.model));
	vid_dev->mdev.dev = &pdev->dev;
	media_device_init(&vid_dev->mdev);

	INIT_LIST_HEAD(&vid_dev->subdevs);
	v4l2_async_notifier_init(&vid_dev->subdev_notifier);
	vid_dev->subdev_notifier.ops = &sd_async_notifier_ops;

	ret = hailo15_video_init_vid_nodes(vid_dev);
	if (ret) {
		pr_err("failed to init video nodes");
		goto err_init_nodes;
	}

	mutex_init(&sd_mutex);

	goto out;
	pr_debug("video device probe success\n");
err_init_nodes:
	media_device_cleanup(&vid_dev->mdev);
	v4l2_async_notifier_unregister(&vid_dev->subdev_notifier);
	v4l2_async_notifier_cleanup(&vid_dev->subdev_notifier);
	kfree(vid_dev);
out:
	return ret;
}

static int hailo15_video_remove(struct platform_device *pdev)
{
	struct hailo15_vid_cap_device *vid_dev;

	mutex_destroy(&sd_mutex);
	vid_dev = platform_get_drvdata(pdev);
	hailo15_video_device_destroy(vid_dev);
	kfree(vid_dev);
	return 0;
}

static const struct of_device_id hailo15_vid_cap_of_match[] = {
	{
		.compatible = "hailo,vid-cap",
	},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, hailo15_vid_cap_of_match);

static struct platform_driver hailo_video_driver = {
	.probe = hailo15_video_probe,
	.remove = hailo15_video_remove,
	.driver = {
		   .name = HAILO_VID_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = hailo15_vid_cap_of_match,
		   },
};

module_platform_driver(hailo_video_driver);

MODULE_DESCRIPTION("Hailo V4L2 video driver");
MODULE_AUTHOR("Hailo SOC SW Team");
MODULE_LICENSE("GPL v2");
