#include "common.h"
#include <linux/of_graph.h>
#include <linux/property.h>

int hailo15_v4l2_notifier_bound(struct v4l2_async_notifier *notifier,
				struct v4l2_subdev *source_subdev,
				struct v4l2_async_subdev *asd,
				struct media_entity *sink_entity)
{
	int ret;
	struct fwnode_handle *ep;
	struct v4l2_fwnode_link link;
	struct media_entity *source, *sink;
	unsigned int source_pad, sink_pad;

	if (WARN_ON(!notifier) || WARN_ON(!source_subdev) || WARN_ON(!asd) ||
	    WARN_ON(!sink_entity)) {
		return -EINVAL;
	}

	ep = fwnode_graph_get_next_endpoint(asd->match.fwnode, NULL);
	if (!ep) {
		return -ENOENT;
	}

	memset(&link, 0, sizeof(link));
	ret = v4l2_fwnode_parse_link(ep, &link);

	fwnode_handle_put(ep);

	if (ret < 0) {
		return ret;
	}
	source = &source_subdev->entity;
	source_pad = link.remote_port;
	sink = sink_entity;
	sink_pad = link.local_port;
	v4l2_fwnode_put_link(&link);
	return media_create_pad_link(source, source_pad, sink, sink_pad,
				     MEDIA_LNK_FL_ENABLED);
}
EXPORT_SYMBOL(hailo15_v4l2_notifier_bound);

int hailo15_plane_get_bytesperline(const struct hailo15_video_fmt *format,
				   int width, int plane)
{
	if (plane >= format->num_planes)
		return 0;
	return (((width * format->planes[plane].bpp) /
		format->planes[plane].hscale_ratio) / BITS_IN_BYTE);
}
EXPORT_SYMBOL(hailo15_plane_get_bytesperline);

int hailo15_plane_get_sizeimage(const struct hailo15_video_fmt *format,
				int height, int bytesperline, int plane)
{
	if (plane >= format->num_planes)
		return 0;
	return ((bytesperline * height) / format->planes[plane].vscale_ratio);
}
EXPORT_SYMBOL(hailo15_plane_get_sizeimage);

int hailo15_fill_planes_fmt(const struct hailo15_video_fmt *format,
			    struct v4l2_pix_format_mplane *mfmt)
{
	int plane;
	struct v4l2_plane_pix_format *plane_fmt;

	if (WARN_ON(!format->num_planes ||
		    format->num_planes > FMT_MAX_PLANES)) {
		return -EINVAL;
	}

	mfmt->num_planes = format->num_planes;

	for (plane = 0; plane < mfmt->num_planes; ++plane) {
		plane_fmt = &mfmt->plane_fmt[plane];
		plane_fmt->bytesperline = hailo15_plane_get_bytesperline(
			format, ALIGN_UP(mfmt->width, STRIDE_ALIGN), plane);
		plane_fmt->sizeimage = hailo15_plane_get_sizeimage(
			format, mfmt->height, plane_fmt->bytesperline, plane);
	}

	return 0;
}
EXPORT_SYMBOL(hailo15_fill_planes_fmt);

const struct hailo15_video_fmt *hailo15_fourcc_get_format(uint32_t fourcc, __u8 num_planes)
{
	const struct hailo15_video_fmt *formats = hailo15_get_formats();
	int format;

	for (format = 0; format < hailo15_get_formats_count(); ++format) {
		if (formats[format].fourcc == fourcc && formats[format].num_planes == num_planes)
			return &formats[format];
	}

	return NULL;
};
EXPORT_SYMBOL(hailo15_fourcc_get_format);

char *hailo15_fourcc_to_string(uint32_t fourcc, char *buf)
{
	buf[0] = (fourcc >> 0) & 0xFF;
 	buf[1] = (fourcc >> 8) & 0xFF;
 	buf[2] = (fourcc >> 16) & 0xFF;
 	buf[3] = (fourcc >> 24) & 0xFF;
 	buf[4] = '\0';
 	return buf;
}
EXPORT_SYMBOL(hailo15_fourcc_to_string);

const struct hailo15_video_fmt *hailo15_fourcc_get_out_format(uint32_t fourcc, __u8 num_planes)
{
	const struct hailo15_video_fmt *formats = hailo15_get_out_formats();
	int format;
	char fourcc_str[5];

	for (format = 0; format < hailo15_get_out_formats_count(); ++format) {
		if (formats[format].fourcc == fourcc && formats[format].num_planes == num_planes)
			return &formats[format];
	}
	pr_err("%s - no matching format for fourcc: 0x%X (%s), num_planes: %u\n", __func__, fourcc, hailo15_fourcc_to_string(fourcc, fourcc_str), num_planes);
	return NULL;
};
EXPORT_SYMBOL(hailo15_fourcc_get_out_format);

const struct hailo15_video_fmt *hailo15_code_get_format(uint32_t code)
{
	const struct hailo15_video_fmt *formats = hailo15_get_formats();
	int format;

	for (format = 0; format < hailo15_get_formats_count(); ++format) {
		if (formats[format].code == code)
			return &formats[format];
	}

	return NULL;
};
EXPORT_SYMBOL(hailo15_code_get_format);

struct v4l2_subdev *hailo15_get_csi2rx_subdev(struct media_device *mdev, int grp_id)
{
	struct media_entity *entity, *csi_entity;
	struct media_pad *csi_pad;
	struct v4l2_subdev *pixel_mux_sd = NULL;
	struct v4l2_subdev *csi_sd = NULL;
	int pad;

	if (!mdev) {
		pr_err("%s: media device is NULL\n", __func__);
		return NULL;
	}

	media_device_for_each_entity (entity, mdev) {
		if (entity->function == MEDIA_ENT_F_VID_MUX) {
			pixel_mux_sd = media_entity_to_v4l2_subdev(entity);
			break;
		}
	}

	if (!pixel_mux_sd) {
		pr_err("%s: no pixel mux subdev found\n", __func__);
		return NULL;
	}

	pad = pixel_mux_grp_id_to_sink_pad_index(grp_id);
	csi_pad = media_entity_remote_pad(&pixel_mux_sd->entity.pads[pad]);
	if (!csi_pad) {
		pr_err("%s: no remote pad found for group id %d (pad %d)\n", __func__, grp_id, pad);
		return NULL;
	}

	csi_entity = csi_pad->entity;
	if (!is_media_entity_v4l2_subdev(csi_entity)){
		pr_err("%s: remote pad %d is not a v4l2 subdev\n", __func__, pad);
		return NULL;
	}

	csi_sd = media_entity_to_v4l2_subdev(csi_entity);
	if (!csi_sd) {
		pr_err("%s: no csi subdev found for group id %d (pad %d)\n", __func__, grp_id, pad);
		return NULL;
	}

	return csi_sd;
}
EXPORT_SYMBOL(hailo15_get_csi2rx_subdev);

struct v4l2_subdev *hailo15_get_sensor_subdev(struct media_device *mdev, int grp_id)
{
	struct media_entity *sensor_entity;
	struct media_pad *sensor_pad;
	struct v4l2_subdev *csi_sd = NULL;
	struct v4l2_subdev *sensor_sd = NULL;
	int pad;
	u32 i;

	if (!mdev) {
		pr_err("%s: media device is NULL\n", __func__);
		return NULL;
	}

	pad = pixel_mux_grp_id_to_sink_pad_index(grp_id);
	csi_sd = hailo15_get_csi2rx_subdev(mdev, grp_id);
	if (!csi_sd) {
		return NULL;
	}

	/* Iterate over the pads of the CSI subdev to find the sensor subdev */
	for (i = 0; i < csi_sd->entity.num_pads; i++) {
		sensor_pad = media_entity_remote_pad(&csi_sd->entity.pads[i]);
		if (!sensor_pad)
			continue;

		sensor_entity = sensor_pad->entity;
		if (!is_media_entity_v4l2_subdev(sensor_entity))
			continue;

		/* Check if this entity is marked as a camera sensor */
		if (sensor_entity->function != MEDIA_ENT_F_CAM_SENSOR)
			continue;

		sensor_sd = media_entity_to_v4l2_subdev(sensor_entity);
		if (!sensor_sd) {
			pr_err("%s: no sensor subdev found for group id %d (pad %d)\n", __func__, grp_id, pad);
			return NULL;
		}
		break;
	}

	if (!sensor_sd) {
		pr_err("%s: no sensor subdev found for group id %d (pad %d)\n", __func__, grp_id, pad);
		return NULL;
	}

	return sensor_sd;
};
EXPORT_SYMBOL(hailo15_get_sensor_subdev);

void hailo15_print_irq_error_message(struct err_status_reg *err_status_reg, u32 errors, int irq)
{
	int i;
	struct error_message error_message;

	if (!err_status_reg) {
		pr_err("err_status_reg is null\n");
		return;
	}

	for (i = 0; i < err_status_reg->num_errors; i++) {
		error_message = err_status_reg->errors[i];
		if (errors & error_message.mask) {
			pr_err("IRQ %d - %s: %s 0x%x\n", irq, err_status_reg->name, error_message.message, errors & error_message.mask);
		}
	}
}
EXPORT_SYMBOL(hailo15_print_irq_error_message);

const struct hailo15_video_fmt *hailo15_get_out_formats(void)
{
	return __hailo15_out_formats;
}
EXPORT_SYMBOL(hailo15_get_out_formats);

const struct hailo15_video_fmt *hailo15_get_formats(void)
{
    return __hailo15_formats;
}
EXPORT_SYMBOL(hailo15_get_formats);

unsigned int hailo15_get_out_formats_count(void)
{
    return ARRAY_SIZE(__hailo15_out_formats);
}
EXPORT_SYMBOL(hailo15_get_out_formats_count);

unsigned int hailo15_get_formats_count(void)
{
    return ARRAY_SIZE(__hailo15_formats);
}
EXPORT_SYMBOL(hailo15_get_formats_count);
