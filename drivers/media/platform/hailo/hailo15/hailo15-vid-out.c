#include "hailo15-vid-out.h"

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>
#include "hailo15-events.h"
#include "hailo15-media.h"

#define HAILO_VID_NAME "hailo_video_out"

#define CREATE_TRACE_POINTS
#include "hailo15-vid-out-traces.h"


#define MIN_BUFFERS_NEEDED 0
#define MAX_NUM_FRAMES HAILO15_MAX_BUFFERS
#define NUM_PLANES                                                             \
	1 /*working on packed mode, this should be determined from the format*/

#define STREAM_OFF 0
#define STREAM_ON 1

/* Threshold for logging slow QBUF processing */
#define HAILO15_QBUF_SLOW_THRESHOLD_MS (10)

/* Threshold for logging slow frame processing in buffer_done.
 * One frame at 30fps.
 */
#define HAILO15_FRAME_SLOW_THRESHOLD_MS (33)

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

static struct mutex sd_mutex;

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
	struct hailo15_video_out_node *vid_node = video_drvdata(file);
	if (WARN_ON(!vid_node))
		return -EINVAL;

	strcpy(cap->driver, "hailo_video_out");
	strcpy(cap->card, "HAILO");
	cap->bus_info[0] = 0;
	snprintf((char *)cap->bus_info, sizeof(cap->bus_info),
		 "platform:hailo%d", vid_node->id);

	return 0;
}

static int hailo15_enum_fmt_vid_out(struct file *file, void *priv,
				    struct v4l2_fmtdesc *f)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);

	if (WARN_ON(!vid_node))
		return -EINVAL;

	if (f->index < hailo15_get_out_formats_count()) {
		f->pixelformat = hailo15_get_out_formats()[f->index].fourcc;
		return 0;
	}

	return -EINVAL;
}

static int hailo15_g_fmt_vid_out(struct file *file, void *priv,
				 struct v4l2_format *f)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);

	if (WARN_ON(!vid_node))
		return -EINVAL;

	if (f->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	/* is it really a user-space struct ?*/
	memcpy(f, &vid_node->fmt, sizeof(struct v4l2_format));
	return 0;
}

static int _hailo15_try_fmt_vid_out(struct file *file, void *priv,
				    struct v4l2_format *f, int active)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	const struct hailo15_video_fmt *format = NULL;
	struct v4l2_subdev_format fmt;
	struct v4l2_subdev_state try_fmt;
	struct v4l2_subdev_pad_config pad_cfg;
	char fourcc_str[5];
	int ret;

	pr_debug("%s - requested format: %dx%d fourcc: 0x%X (%s), num_planes: %u\n",
	       __func__, pix_mp->width, pix_mp->height, pix_mp->pixelformat,
	       hailo15_fourcc_to_string(pix_mp->pixelformat, fourcc_str),
	       pix_mp->num_planes);

	memset(&fmt, 0, sizeof(fmt));
	memset(&try_fmt, 0, sizeof(try_fmt));
	memset(&pad_cfg, 0, sizeof(pad_cfg));

	if (WARN_ON(!vid_node)) {
		pr_err("%s - vid_node is NULL, returning -EINVAL\n", __func__);
		return -EINVAL;
    }

	VALIDATE_STREAM_OFF(vid_node, return -EBUSY);

	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type)){
		pr_err("%s - f->type %d is not multiplanar, returning -EINVAL\n", __func__, f->type);
		return -EINVAL;
	}
	format = hailo15_fourcc_get_out_format(pix_mp->pixelformat, pix_mp->num_planes);

	if (format == NULL) {
		pr_err("%s - format is NULL, returning -EINVAL\n", __func__);
		return -EINVAL;
	}

	if (pix_mp->width % format->width_modulus){
		pr_err("%s - pix_mp->width %d is not divisible by format->width_modulus %d, returning -EINVAL\n",
            __func__, pix_mp->width, format->width_modulus);
		return -EINVAL;
	}

	ret = hailo15_fill_planes_fmt(format, pix_mp);
	if (ret) {
		pr_err("%s - fill_planes_fmt failed with: %d\n", __func__, ret);
		return ret;
	}

	pix_mp->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	pix_mp->colorspace = V4L2_COLORSPACE_DEFAULT;

	return ret;
}

static int hailo15_try_fmt_vid_out(struct file *file, void *priv,
				   struct v4l2_format *f)
{
	return _hailo15_try_fmt_vid_out(file, priv, f, 0);
}

static int hailo15_s_fmt_vid_out(struct file *file, void *priv,
				 struct v4l2_format *f)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);
	int ret = 0;

	if (WARN_ON(!vid_node))
		return -EINVAL;
	VALIDATE_STREAM_OFF(vid_node, return -EBUSY);
	ret = _hailo15_try_fmt_vid_out(file, priv, f, 1);
	if (ret) {
		pr_err("%s - try set fmt failed with: %d\n", __func__, ret);
		return ret;
	}

	memcpy(&vid_node->fmt, f, sizeof(struct v4l2_format));
	return ret;
}

static int
hailo15_video_out_node_subdev_set_stream(struct hailo15_video_out_node *vid_node,
				     int enable)
{
	int ret;
	if (WARN_ON(!vid_node))
		return -EINVAL;

	// set the grp_id of subdev to indicate from which pad the call came
	vid_node->direct_sd->grp_id = vid_node->path;
	ret = hailo15_subdev_call(vid_node, video, s_stream, enable);
	if (ret) {
		pr_warn("%s - s_stream %s to subdev %s failed, err = (%pe)\n",
			__func__, enable ? "on" : "off", vid_node->direct_sd->name, ERR_PTR(ret));
	}
	return ret;
}

static int hailo15_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);
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
			return ret;
		}
		mutex_unlock(&vid_node->ioctl_mutex);
		return 0;

	}

	mutex_unlock(&vid_node->ioctl_mutex);

	return 0;
}
static int hailo15_streamoff(struct file *file, void *priv,
			     enum v4l2_buf_type i)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);
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
}

static int hailo15_reqbufs(struct file *file, void *priv,
		    struct v4l2_requestbuffers *p)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);
	struct media_pad *pad;
	struct hailo15_reqbufs req;
	int ret = 0;
	req.num_buffers = p->count;
	vid_node->reqbufs = 0;

	ret = vb2_ioctl_reqbufs(file, priv, p);
	if (ret) {
		pr_debug("%s - vb2_ioctl_reqbufs failed\n", __func__);
		return ret;
	}

	pad = media_entity_remote_pad(&vid_node->pad);
	req.pad = pad->index;

	ret = hailo15_subdev_call(vid_node, core, ioctl, ISPIOC_V4L2_REQBUFS, &req);
	if (ret) {
		pr_err("%s - reqbufs from subdev %s failed, err = (%pe)\n",
				__func__, vid_node->direct_sd->name, ERR_PTR(ret));
		return ret;
	}

	vid_node->reqbufs = req.num_buffers;
	return ret;
}


static const struct v4l2_ioctl_ops video_ioctl_ops = {
	.vidioc_querycap = hailo15_querycap,
	.vidioc_enum_fmt_vid_out = hailo15_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out_mplane = hailo15_g_fmt_vid_out,
	.vidioc_s_fmt_vid_out_mplane = hailo15_s_fmt_vid_out,
	.vidioc_try_fmt_vid_out_mplane = hailo15_try_fmt_vid_out,
	.vidioc_streamon = hailo15_streamon,
	.vidioc_streamoff = hailo15_streamoff,
	.vidioc_reqbufs = hailo15_reqbufs,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
};

static int hailo15_video_out_node_stream_cancel(struct hailo15_video_out_node *vid_node)
{
	int ret;

	if (!vid_node->streaming)
		return -EINVAL;

	vid_node->streaming = STREAM_OFF;
	ret = hailo15_video_out_node_subdev_set_stream(vid_node, STREAM_OFF);
	if (ret) {
		dev_err(vid_node->dev,
			"can't stop stream on node %d with error %d\n",
			vid_node->id, ret);
	}

	vid_node->sequence = 0;
	return ret;
}

static int hailo15_video_out_node_open(struct file *file)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);
	int ret;
	ret = v4l2_fh_open(file);
	if (ret) {
		pr_err("%s - failed to open vid_node %d\n", __func__,
		       vid_node->id);
		return ret;
	}

	return 0;
}

static int hailo15_video_out_node_release(struct file *file)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);
	int ret;

	if (vid_node->streaming &&
	    file->private_data == vid_node->queue.owner) {
		ret = hailo15_video_out_node_stream_cancel(vid_node);
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

static __poll_t hailo15_video_out_node_poll(struct file *file,
					    struct poll_table_struct *wait)
{
	struct hailo15_video_out_node *vid_node = video_drvdata(file);
	if (vid_node->streaming) {
		return vb2_poll(&vid_node->queue, file, wait);
	}
	return v4l2_ctrl_poll(file, wait);
}
static struct v4l2_file_operations video_ops = {
	.owner = THIS_MODULE,
	.read = vb2_fop_read,
	.mmap = vb2_fop_mmap,
	.poll = hailo15_video_out_node_poll,
	.open = hailo15_video_out_node_open,
	.release = hailo15_video_out_node_release,
	.unlocked_ioctl = video_ioctl2,
};

static int hailo15_queue_setup(struct vb2_queue *q, unsigned int *nbuffers,
			       unsigned int *nplanes, unsigned int sizes[],
			       struct device *alloc_devs[])
{
	struct hailo15_video_out_node *vid_node = queue_to_node(q);
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
		hailo15_fourcc_get_out_format(vid_node->fmt.fmt.pix_mp.pixelformat, vid_node->fmt.fmt.pix_mp.num_planes);
	if (!format) {
		pr_err("%s - failed to get fourcc format\n", __func__);
		return -EINVAL;
	}

	line_length = ALIGN_UP(vid_node->fmt.fmt.pix_mp.width, STRIDE_ALIGN);
	height = vid_node->fmt.fmt.pix_mp.height;

	if(*nplanes){
		if(*nplanes != format->num_planes){
			return -EINVAL;
		}
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

static int hailo15_video_out_node_buffer_init(struct vb2_buffer *vb)
{
	struct hailo15_video_out_node *vid_node = queue_to_node(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf =
		container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	struct hailo15_buffer *buf =
		container_of(vbuf, struct hailo15_buffer, vb);
	struct media_pad *pad;
	int plane;

	memset(buf->dma, 0, sizeof(buf->dma));
	hailo15_buf_list_init(buf);
	if (vb->num_planes >= FMT_MAX_PLANES) {
		dev_err(vid_node->dev, "%s - num of planes too big: %u\n", __func__,
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
	struct hailo15_video_out_node *vid_node = queue_to_node(vb->vb2_queue);
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
	return hailo15_video_node_buffer_process(ctx, vid_node->path, buf, true);
}

static void hailo15_buffer_queue(struct vb2_buffer *vb)
{
	struct hailo15_video_out_node *vid_node = queue_to_node(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	struct hailo15_buffer *hbuf = container_of(vbuf, struct hailo15_buffer, vb);
	ktime_t qbuf_start;
	s64 elapsed_ms;

	if (WARN_ON(!vb) || WARN_ON(!vb->vb2_queue)) {
		pr_err("%s - WARN_ON(!vb) || WARN_ON(!vb->vb2_queue), returning\n",
		       __func__);
		return;
	}
	/* if the list is empty, it will be called twice. */
	if (hailo15_video_out_node_buffer_init(vb)) {
		pr_err("%s - failed hailo15_video_out_node_buffer_init, returning\n",
		       __func__);
		return;
	}

	memset(&hbuf->timing, 0, sizeof(hbuf->timing));
	qbuf_start = ktime_get();
	hbuf->timing.qbuf_start = qbuf_start;

 	/* TODO call isp buffer_queue (isp will manage the buffer queue)*/
	trace_hailo_buffer_queued(hbuf, vid_node);
	hailo15_video_device_process_vb2_buffer(vb);

	elapsed_ms = ktime_ms_delta(ktime_get(), qbuf_start);
	if (elapsed_ms >= HAILO15_QBUF_SLOW_THRESHOLD_MS) {
		pr_info_ratelimited("%s: QBUF processing slow for path=%d index=%d - elapsed=%lld ms\n",
				__func__, vid_node->path, vb->index, elapsed_ms);
	}
}

static int hailo15_video_device_buffer_done(struct hailo15_dma_ctx *ctx,
					    struct hailo15_buffer *buf,
					    int grp_id)
{
	struct hailo15_video_out_node *vid_node;
	ktime_t now;
	s64 elapsed_ms;
	int ret;

	vid_node = NULL;
	if (grp_id < 0 || grp_id >= HAILO15_VID_GRP_MAX)
		return -EINVAL;

	ret = hailo15_video_node_get_private_data(ctx, grp_id,
						  (void **)&vid_node);
	if (ret)
		return ret;

	if (WARN_ON(!vid_node)) {
		pr_err("%s - WARN_ON(!vid_node), returning\n", __func__);
		return -EINVAL;
	}

	if (buf) {
		now = ktime_get();

		if (buf->timing.fe_switch_start) {
			elapsed_ms = ktime_ms_delta(now, buf->timing.fe_switch_start);
			if (elapsed_ms >= HAILO15_FRAME_SLOW_THRESHOLD_MS) {
				pr_info_ratelimited("%s: slow frame for grp_id=%d index=%d - elapsed=%lld ms."
							"  timestamps: qbuf_start=%lld, fe_switch_start=%lld, fe_switch_end=%lld, "
							"rdma_ready=%lld, frame_end=%lld, now=%lld\n",
							__func__, grp_id, buf->vb.vb2_buf.index, elapsed_ms,
							ktime_to_ns(buf->timing.qbuf_start),
							ktime_to_ns(buf->timing.fe_switch_start),
							ktime_to_ns(buf->timing.fe_switch_end),
							ktime_to_ns(buf->timing.rdma_ready),
							ktime_to_ns(buf->timing.frame_end),
							ktime_to_ns(now));
			}
		}

		buf->queue_sequence = vid_node->sequence;
		buf->vb.sequence = vid_node->sequence % vid_node->reqbufs;
		vid_node->sequence++;
		buf->vb.vb2_buf.timestamp = ktime_get_ns();
		vb2_buffer_done(&buf->vb.vb2_buf,
				vid_node->streaming ? VB2_BUF_STATE_DONE : VB2_BUF_STATE_ERROR);

		trace_hailo_buffer_done(buf, vid_node);
	}

	return 0;
}

static int hailo15_buffer_prepare(struct vb2_buffer *vb)
{
	int plane, line_length, bytesperline;
	struct hailo15_video_out_node *vid_node = queue_to_node(vb->vb2_queue);
	struct v4l2_pix_format_mplane *mfmt = &vid_node->fmt.fmt.pix_mp;
	const struct hailo15_video_fmt *format;

	format = hailo15_fourcc_get_out_format(mfmt->pixelformat, mfmt->num_planes);

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

static int hailo15_video_out_node_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct hailo15_video_out_node *vid_node = queue_to_node(q);
	int ret = 0;

	if (WARN_ON(!vid_node))
		return -EINVAL;
	if (!vid_node->streaming) {
		trace_hailo15_vidout_start_streaming(vid_node);
		ret = hailo15_video_out_node_subdev_set_stream(vid_node, STREAM_ON);
		if (ret) {
			dev_err(vid_node->dev,
				"can't start stream on node %d with error %d\n",
				vid_node->id, ret);
			goto out;
		}

		vid_node->streaming = STREAM_ON;
	}
	goto out;
out:
	trace_hailo15_vidout_start_streaming_error(vid_node);
	return ret;
}

static void hailo15_video_out_node_stop_streaming(struct vb2_queue *q)
{
	struct hailo15_video_out_node *vid_node = queue_to_node(q);
	unsigned int i;

	if (WARN_ON(!vid_node))
		return;

	hailo15_video_out_node_stream_cancel(vid_node);

	/* Return any buffers still owned by the driver.
	 * The ISP's s_stream(OFF) drops cur_buf[grp_id] without calling
	 * vb2_buffer_done(), so we must return all remaining ACTIVE buffers
	 * here to satisfy the vb2 stop_streaming contract.
	 * This is safe because q->lock is held by vb2_core_streamoff,
	 * preventing concurrent buf_queue calls, and the ISP is fully
	 * quiesced after stream_cancel (mi_stopped, workqueues drained). */
	for (i = 0; i < q->num_buffers; ++i) {
		if (q->bufs[i]->state == VB2_BUF_STATE_ACTIVE)
			vb2_buffer_done(q->bufs[i], VB2_BUF_STATE_ERROR);
	}

	trace_hailo15_vidout_stop_streaming(vid_node);
}


static void hailo15_buffer_trace_dequeue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf;
	struct hailo15_buffer *hbuf;
	struct hailo15_video_out_node *vid_node = queue_to_node(vb->vb2_queue);
	if (WARN_ON(!vb) || WARN_ON(!vb->vb2_queue)) {
		pr_err("%s - WARN_ON(!p) || WARN_ON(!p->vb2_queue), returning\n",
		       __func__);
	}
	vbuf = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	hbuf = container_of(vbuf, struct hailo15_buffer, vb);
	trace_hailo_buffer_dequeued(hbuf, vid_node);
}

static struct vb2_ops hailo15_buffer_ops = {
	.start_streaming = hailo15_video_out_node_start_streaming,
	.stop_streaming = hailo15_video_out_node_stop_streaming,
	.queue_setup = hailo15_queue_setup,
	.buf_prepare = hailo15_buffer_prepare,
	.buf_queue = hailo15_buffer_queue,
	.buf_finish = hailo15_buffer_trace_dequeue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
};

static void
hailo15_video_out_node_video_device_unregister(struct hailo15_video_out_node *vid_node)
{
	video_unregister_device(vid_node->video_dev);
}

static void
hailo15_video_out_node_destroy_media_entity(struct hailo15_video_out_node *vid_node)
{
	media_entity_cleanup(&vid_node->video_dev->entity);
}

static void
hailo15_video_out_node_video_device_destroy(struct hailo15_video_out_node *vid_node)
{
	hailo15_video_out_node_destroy_media_entity(vid_node);
	mutex_destroy(&vid_node->ioctl_mutex);
	video_device_release(vid_node->video_dev);
}

static void
hailo15_video_out_node_destroy_v4l2_device(struct hailo15_video_out_node *vid_node)
{
	v4l2_device_unregister(vid_node->v4l2_dev);
	kfree(vid_node->v4l2_dev);
}

static void hailo15_video_out_node_destroy(struct hailo15_video_out_node *vid_node)
{
	hailo15_video_out_node_video_device_unregister(vid_node);
	hailo15_video_out_node_destroy_v4l2_device(vid_node);
	hailo15_video_out_node_video_device_destroy(vid_node);
}

static int hailo15_video_device_destroy(struct hailo15_vid_out_device *vid_dev)
{
	struct hailo15_subdev_list *tmp;
	struct list_head *pos;
	struct list_head *npos;
	int video_id;
	for (video_id = 0; video_id < MAX_VIDEO_NODE_NUM; ++video_id) {
		if (vid_dev->vid_nodes[video_id]) {
			hailo15_video_out_node_destroy(
				vid_dev->vid_nodes[video_id]);
			kfree(vid_dev->vid_nodes[video_id]);
		}
	}

	media_device_unregister(&vid_dev->mdev);
	media_device_cleanup(&vid_dev->mdev);
	list_for_each_safe (pos, npos, &vid_dev->subdevs) {
		tmp = list_entry(pos, struct hailo15_subdev_list, subdev_list);
		list_del(pos);
		kfree(tmp);
	}
	return 0;
}

static const struct media_entity_operations hailo15_video_media_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int hailo15_video_out_node_queue_init(struct hailo15_video_out_node *vid_node)
{
	int ret;

	spin_lock_init(&vid_node->qlock);
	INIT_LIST_HEAD(&vid_node->buf_queue);
	mutex_init(&vid_node->buffer_mutex);

	vid_node->queue.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	vid_node->queue.drv_priv = vid_node;
	vid_node->queue.ops = &hailo15_buffer_ops;
	vid_node->queue.io_modes = VB2_MMAP | VB2_DMABUF;
	vid_node->queue.mem_ops = &vb2_dma_contig_memops;
	vid_node->queue.buf_struct_size = sizeof(struct hailo15_buffer);
	vid_node->queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
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
hailo15_video_out_node_init_media_entity(struct hailo15_video_out_node *vid_node)
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
hailo15_video_out_node_video_device_init(struct hailo15_video_out_node *vid_node)
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

	sprintf(vid_node->video_dev->name, "hailo-vid-out-%s", hailo15_grp_id_to_str(vid_node->path));
	/*initialize video device*/
	vid_node->video_dev->release =
		video_device_release_empty; /* We will release the video device on our own */
	vid_node->video_dev->fops = &video_ops;
	vid_node->video_dev->ioctl_ops = &video_ioctl_ops;
	vid_node->video_dev->minor = 5;
	vid_node->video_dev->vfl_type = VFL_TYPE_VIDEO;
	vid_node->video_dev->vfl_dir = VFL_DIR_TX;
	vid_node->video_dev->device_caps =
		V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING | V4L2_CAP_EXT_PIX_FORMAT;

	video_set_drvdata(vid_node->video_dev, vid_node);

	ret = hailo15_video_out_node_init_media_entity(vid_node);
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
hailo15_video_out_node_video_device_register(struct hailo15_video_out_node *vid_node)
{
	if (WARN_ON(!vid_node))
		return -EINVAL;
	return video_register_device(vid_node->video_dev, VFL_TYPE_VIDEO,
				    	vid_node->path);
}

static int
hailo15_video_out_node_init_v4l2_device(struct hailo15_video_out_node *vid_node)
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

	vid_node->video_dev->v4l2_dev = vid_node->v4l2_dev;
	vid_node->v4l2_dev->mdev = vid_node->mdev;

	ret = hailo15_media_register_v4l2_device(vid_node->v4l2_dev, vid_node->id);
	if(ret){
		pr_err("failed to create media links\n");
		goto err_register_subdev_nodes;
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

static int hailo15_video_out_node_init_fmt(struct hailo15_video_out_node *vid_node)
{
	vid_node->fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	vid_node->fmt.fmt.pix.field = V4L2_FIELD_NONE;
	vid_node->fmt.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;

	return 0;
}

static int hailo15_video_out_node_init(struct hailo15_video_out_node *vid_node)
{
	int ret;

	ret = hailo15_video_out_node_video_device_init(vid_node);
	if (ret) {
		dev_err(vid_node->dev, "can't init video device for node %d\n",
			vid_node->id);
		goto out;
	}

	ret = hailo15_video_out_node_queue_init(vid_node);
	if (ret) {
		dev_err(vid_node->dev,
			"can't init video node queue for node %d\n",
			vid_node->id);
		goto err_queue_init;
	}

	ret = hailo15_video_out_node_init_fmt(vid_node);
	if (ret) {
		dev_err(vid_node->dev, "can't init fmt for node %d\n",
			vid_node->id);
		goto err_fmt;
	}

	ret = hailo15_video_out_node_init_v4l2_device(vid_node);
	if (ret) {
		dev_err(vid_node->dev, "can't init v4l2 device for node %d\n",
			vid_node->id);
		goto err_v4l2_device;
	}

	ret = hailo15_video_out_node_video_device_register(vid_node);
	if (ret) {
		dev_err(vid_node->dev,
			"can't register video device for node %d\n",
			vid_node->id);
		goto err_video_register;
	}

	ret = hailo15_media_create_links(vid_node->dev, &vid_node->video_dev->entity, vid_node->id);
	if(ret){
		dev_err(vid_node->dev, "can't create media links for node %d\n", vid_node->id);
		goto err_create_links;
	}

	goto out;

/* TODO: MSW-2716: This code section is unused */
/*
err_async_notifier:
	hailo15_video_out_node_video_device_unregister(vid_node);
*/
err_create_links:
err_video_register:
	hailo15_video_out_node_destroy_v4l2_device(vid_node);
err_v4l2_device:
err_fmt:
err_queue_init:
	hailo15_video_out_node_video_device_destroy(vid_node);
out:
	return ret;
}

static int hailo15_video_init_vid_nodes(struct hailo15_vid_out_device *vid_dev)
{
	struct hailo15_video_out_node *vid_node = NULL;
	struct fwnode_handle *ep = NULL;
	int i = 0;
	uint32_t path;
	int ret = -EINVAL;
	struct fwnode_endpoint fwnode_ep;
	struct v4l2_subdev* sd;
	struct hailo15_dma_ctx* ctx;


	while (i++ < MAX_VIDEO_NODE_NUM) {
		pr_info("vid out initializing %d\n", i);
		ep = fwnode_graph_get_next_endpoint(dev_fwnode(vid_dev->dev),
						    ep);
		if (!ep)
			break;

		// initialize video node
		vid_node =
			kzalloc(sizeof(struct hailo15_video_out_node), GFP_KERNEL);
		if (!vid_node) {
			ret = -ENOMEM;
			goto err_node_alloc;
		}

		vid_node->dev = vid_dev->dev;
		vid_node->mdev = hailo15_media_get_media_device();

		// node id taken from port property of the endpoint (assumes one endpoint per port)
		vid_node->id = fwnode_ep.port;

		// read path property so s_stream knows from where it was called
		ret = fwnode_property_read_u32(ep, "path", &path);
		if (ret || path >= HAILO15_VID_GRP_MAX) {
			pr_err("failed to read path property from video node %d, skipping...\n",
			       fwnode_ep.port);
			fwnode_handle_put(ep);
			kfree(vid_node);
			continue;
		}

		vid_node->path = path;
		ret = hailo15_video_out_node_init(vid_node);
		if (ret) {
			pr_err("failed to init video node %d, skipping...\n",
			       fwnode_ep.port);
			fwnode_handle_put(ep);
			kfree(vid_node);
			continue;
		}

		ret = hailo15_media_get_subdev(vid_node->dev, vid_node->id, &sd);
		if(ret){
			pr_err("vid_node %d failed to get subdevice\n", vid_node->id);
			goto err_node_alloc;
		}

		vid_node->direct_sd = sd;
		ctx = v4l2_get_subdevdata(sd);
		ctx->buf_ctx[vid_node->path].ops->buffer_done = hailo15_video_device_buffer_done;
		hailo15_video_node_set_private_data(ctx, vid_node->path,
		                                           (void *)vid_node);

		vid_dev->vid_nodes[vid_node->id] = vid_node;
		pr_info("vid_node %d initialized successfully\n", vid_node->id);
	}

	goto out;

err_node_alloc:
	for (i = 0; i < MAX_VIDEO_NODE_NUM; ++i) {
		if (vid_dev->vid_nodes[i]) {
			hailo15_video_out_node_destroy(vid_dev->vid_nodes[i]);
			kfree(vid_dev->vid_nodes[i]);
		}
	}
out:
	return ret;
}

static int hailo15_video_probe(struct platform_device *pdev)
{
	int ret;
	struct hailo15_vid_out_device *vid_dev;

	pr_info("video out probe start!\n");
	ret = hailo15_media_get_sink_endpoints_status(&pdev->dev);

	if(ret){
		pr_info("out endpoints not ready!\n");
		return ret;
	}

	if(!hailo15_media_device_initialized()){
		pr_info("media device is not initialized\n");
		return -EPROBE_DEFER;
	}

	vid_dev = kzalloc(sizeof(struct hailo15_vid_out_device), GFP_KERNEL);
	if (!vid_dev) {
		pr_err("failed to allocate vid out device\n");
		return -ENOMEM;
	}

	vid_dev->dev = &pdev->dev;

	platform_set_drvdata(pdev, vid_dev);

	ret = hailo15_video_init_vid_nodes(vid_dev);
	if (ret) {
		pr_err("failed to init video nodes");
		goto err_init_nodes;
	}

	mutex_init(&sd_mutex);

	pm_runtime_get_sync(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	pr_debug("video device probe success\n");
	goto out;
err_init_nodes:
	kfree(vid_dev);
out:
	return ret;
}

static int hailo15_video_remove(struct platform_device *pdev)
{
	struct hailo15_vid_out_device *vid_dev;

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	mutex_destroy(&sd_mutex);
	vid_dev = platform_get_drvdata(pdev);
	hailo15_video_device_destroy(vid_dev);
	kfree(vid_dev);
	return 0;
}

static const struct of_device_id hailo15_vid_out_of_match[] = {
	{
		.compatible = "hailo,vid-out",
	},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, hailo15_vid_out_of_match);

static struct platform_driver hailo_video_driver = {
	.probe = hailo15_video_probe,
	.remove = hailo15_video_remove,
	.driver = {
		   .name = HAILO_VID_NAME,
		   .of_match_table = hailo15_vid_out_of_match,
		   },
};

module_platform_driver(hailo_video_driver);

MODULE_DESCRIPTION("Hailo V4L2 video driver");
MODULE_AUTHOR("Hailo SOC SW Team");
MODULE_LICENSE("GPL v2");
