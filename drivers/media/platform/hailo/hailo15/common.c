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
	return ((width * format->planes[plane].bpp) /
		format->planes[plane].hscale_ratio);
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
		    format->num_planes >= FMT_MAX_PLANES)) {
		pr_info("hailo15-vid-cap: invalid planarity\n");
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

const struct hailo15_video_fmt *hailo15_fourcc_get_format(uint32_t fourcc)
{
	const struct hailo15_video_fmt *formats = hailo15_get_formats();
	int format;

	for (format = 0; format < hailo15_get_formats_count(); ++format) {
		if (formats[format].fourcc == fourcc)
			return &formats[format];
	}

	return NULL;
};
EXPORT_SYMBOL(hailo15_fourcc_get_format);

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

struct v4l2_subdev *hailo15_get_sensor_subdev(struct media_device *mdev)
{
	struct media_entity *entity;
	struct v4l2_subdev *sensor_sd = NULL;

	if (!mdev)
		return NULL;

	media_device_for_each_entity (entity, mdev) {
		if (entity->function == MEDIA_ENT_F_CAM_SENSOR) {
			sensor_sd = media_entity_to_v4l2_subdev(entity);
			break;
		}
	}

	return sensor_sd;
};
EXPORT_SYMBOL(hailo15_get_sensor_subdev);
