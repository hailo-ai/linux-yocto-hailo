// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Hailo pixel mux
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved. 
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#define RES_MIN

#include "common.h"
#include "hailo15-media.h"
#include "hailo15-pixel-mux.h"

struct pm_config {
    uint16_t hailo15_buffer_ready_ap_int_mask_offset;
    uint16_t hailo15_buffer_ready_ap_int_status_offset;
    uint16_t hailo15_buffer_ready_ap_int_w1c_offset;
    uint16_t hailo15_buffer_ready_ap_int_w1s_offset;

    uint16_t pixel_mux_cfg_offset;
    uint16_t isp0_stream0_offset;
    uint16_t isp0_stream1_offset;
    uint16_t isp0_stream2_offset;
    uint16_t isp1_stream0_offset;
    uint16_t isp1_stream1_offset;
    uint16_t isp1_stream2_offset;
    uint16_t vision_clock_conf_offset;
    uint16_t vision_subsys_asf_int_mask_offset;
    uint16_t vision_asf_int_fatal_mask_offset;
    uint16_t vision_asf_int_nonfatal_mask_offset;
    uint16_t vision_subsys_err_int_mask_offset;
    uint16_t vision_subsys_err_int_agg_mask_offset;
    uint16_t vision_buffer_ready_ap_int_mask_offset;
    uint16_t hailo15_pixel_mux_vsync_mask_offset;

    uint8_t vc_width;
};

static const struct pm_config hailo15_pm_config = {
    .hailo15_buffer_ready_ap_int_mask_offset = 0x60,
    .hailo15_buffer_ready_ap_int_status_offset = 0x64,
    .hailo15_buffer_ready_ap_int_w1c_offset = 0x68,
    .hailo15_buffer_ready_ap_int_w1s_offset = 0x6c,
    .pixel_mux_cfg_offset = 0x2c,
    .isp0_stream0_offset = 0x30,
    .isp0_stream1_offset = 0x34,
    .isp0_stream2_offset = 0x38,
    .isp1_stream0_offset = 0x3c,
    .isp1_stream1_offset = 0x40,
    .isp1_stream2_offset = 0x44,
    .vision_clock_conf_offset = 0x4c,
    .vision_subsys_asf_int_mask_offset = 0x50,
    .vision_asf_int_fatal_mask_offset = 0x80,
    .vision_asf_int_nonfatal_mask_offset = 0x88,
    .vision_subsys_err_int_mask_offset = 0x90,
    .vision_subsys_err_int_agg_mask_offset = 0xc4,
    .vision_buffer_ready_ap_int_mask_offset = 0x60,
    .hailo15_pixel_mux_vsync_mask_offset = 0x48,
    .vc_width = 2,
};

static const struct pm_config hailo15l_pm_config = {
    .hailo15_buffer_ready_ap_int_mask_offset = 0x58,
    .hailo15_buffer_ready_ap_int_status_offset = 0x5C,
    .hailo15_buffer_ready_ap_int_w1c_offset = 0x60,
    .hailo15_buffer_ready_ap_int_w1s_offset = 0x64,
    .pixel_mux_cfg_offset = 0x24,
    .isp0_stream0_offset = 0x28,
    .isp0_stream1_offset = 0x2c,
    .isp0_stream2_offset = 0x30,
    .isp1_stream0_offset = 0x34,
    .isp1_stream1_offset = 0x38,
    .isp1_stream2_offset = 0x3c,
    .vision_clock_conf_offset = 0x44,
    .vision_subsys_asf_int_mask_offset = 0x48,
    .vision_asf_int_fatal_mask_offset = 0x78,
    .vision_asf_int_nonfatal_mask_offset = 0x80,
    .vision_subsys_err_int_mask_offset = 0x88,
    .vision_subsys_err_int_agg_mask_offset = 0xc0,
    .vision_buffer_ready_ap_int_mask_offset = 0x58,
    .hailo15_pixel_mux_vsync_mask_offset = 0x40,
    .vc_width = 4,
};

static const struct of_device_id hailo_pixel_mux_of_table[] = {
	{ .compatible = "hailo,hailo15-pixel-mux", .data = &hailo15_pm_config },
	{ .compatible = "hailo,hailo15l-pixel-mux", .data = &hailo15l_pm_config },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, hailo_pixel_mux_of_table);

enum pixel_mux_pads {
	PIXEL_MUX_SINK_PAD_0,
	PIXEL_MUX_SINK_PAD_1,
	PIXEL_MUX_SINK_PAD_MAX,
	PIXEL_MUX_SOURCE_PAD_0 = PIXEL_MUX_SINK_PAD_MAX,
	PIXEL_MUX_SOURCE_PAD_1,
	PIXEL_MUX_PAD_MAX,
};

struct pixel_mux_priv {
	struct device *dev;
	unsigned int count;

	/*
	 * Used to prevent race conditions between multiple,
	 * concurrent calls to start and stop.
	 */
	struct mutex lock;

	void __iomem *base;
	struct clk *vision_clk;
	struct clk *vision_hclk;
	int irq;

	u8 num_lanes;
	u8 max_lanes;
	u8 max_streams;

	struct v4l2_subdev subdev;
	struct v4l2_async_notifier subdev_notifier;
	struct v4l2_async_notifier video_notifier;
	struct media_pad pads[PIXEL_MUX_PAD_MAX];
	struct v4l2_mbus_framefmt pad_fmts[PIXEL_MUX_PAD_MAX];
	int num_exposures;

	bool enabled;

	/* Remote source */
	struct {
		struct v4l2_subdev *subdev;
		int pad;
		int configured;
	} remote_sources[PIXEL_MUX_SINK_PAD_MAX];

	const struct pm_config *pm_cfg;
};

struct indexed_v4l2_async_subdev {
	struct v4l2_async_subdev asd;
	int index;
};

static const struct v4l2_mbus_framefmt fmt_default = {
	.width		= 3840,
	.height		= 2160,
	.code		= MEDIA_BUS_FMT_SRGGB12_1X12,
	.field		= V4L2_FIELD_NONE,
	.colorspace	= V4L2_COLORSPACE_DEFAULT,
};

static const struct hailo15_mux_cfg isp_cfg = {
	.pixel_mux_cfg =
		P2A0_DIS_P2A1_DIS_ISP0_2_CSIRX0_ISP1_2_CSIRX1,
	.isp0_stream0 = ENABLE_VC_0_DT_RAW_12,
	.isp0_stream1 = ENABLE_VC_1_DT_RAW_12,
	.isp0_stream2 = ENABLE_VC_2_DT_RAW_12,
	.isp1_stream0 = ENABLE_VC_0_DT_RAW_12,
	.isp1_stream1 = ENABLE_VC_1_DT_RAW_12,
	.isp1_stream2 = ENABLE_VC_2_DT_RAW_12,
	.vision_buffer_ready_ap_int_mask = 0x0
};

static const struct hailo15_mux_cfg p2a_cfg_3dol = {
	.pixel_mux_cfg =
		P2A0_2_CSIRX0_P2A1_2_CSIRX1_ISP0_2_SW_DBG_ISP1_2_SW_DBG,
	.isp0_stream0 = DISABLE_VC_4_DT_DISABLE,
	.isp0_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp0_stream2 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream0 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream2 = DISABLE_VC_4_DT_DISABLE,
	.vision_buffer_ready_ap_int_mask = 0xf44
};

static const struct hailo15_mux_cfg p2a_cfg_2dol = {
	.pixel_mux_cfg =
		P2A0_2_CSIRX0_P2A1_2_CSIRX1_ISP0_2_SW_DBG_ISP1_2_SW_DBG,
	.isp0_stream0 = DISABLE_VC_4_DT_DISABLE,
	.isp0_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp0_stream2 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream0 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream2 = DISABLE_VC_4_DT_DISABLE,
	.vision_buffer_ready_ap_int_mask = 0xf22
};

static const struct hailo15_mux_cfg p2a_cfg_sdr = {
	.pixel_mux_cfg =
		P2A0_2_CSIRX0_P2A1_2_CSIRX1_ISP0_2_SW_DBG_ISP1_2_SW_DBG,
	.isp0_stream0 = DISABLE_VC_4_DT_DISABLE,
	.isp0_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp0_stream2 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream0 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream2 = DISABLE_VC_4_DT_DISABLE,
	.vision_buffer_ready_ap_int_mask = 0xf00	// bit CSI-RX-1[7:4], CSI-RX-0[3:0] are enabled/disabled by rxwrapper.
};

static const struct hailo15_mux_interrupt_cfg int_cfg = {
	.pixel_mux_vsync_mask = 0xffff,
	.vision_subsys_asf_int_mask = 0x1f,
	.vision_asf_int_fatal_mask = 0xff,
	.vision_asf_int_nonfatal_mask = 0xff,
	.vision_subsys_err_int_mask = 0x7ff,
	.vision_subsys_err_int_agg_mask = 0x1f
};

static long pixel_mux_priv_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
				 void *arg)
{
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct pixel_mux_priv *pixel_mux = ctx->dev;
    struct hailo15_p2a_buffer_regs_addr *p2a_buffer_regs = arg;
	int ret = 0;

	dev_dbg(pixel_mux->dev, "%s called with cmd: %d, arg: %p\n", __func__, cmd, arg);

	switch (cmd) {
	case VIDEO_GET_P2A_REGS:
		if (!arg) {
			ret = -EINVAL;
			break;
		}
		p2a_buffer_regs->buffer_ready_ap_int_mask_addr = pixel_mux->base + 
			pixel_mux->pm_cfg->hailo15_buffer_ready_ap_int_mask_offset;
		p2a_buffer_regs->buffer_ready_ap_int_status_addr = pixel_mux->base + 
			pixel_mux->pm_cfg->hailo15_buffer_ready_ap_int_status_offset;
		p2a_buffer_regs->buffer_ready_ap_int_w1c_addr = pixel_mux->base + 
			pixel_mux->pm_cfg->hailo15_buffer_ready_ap_int_w1c_offset;
		p2a_buffer_regs->buffer_ready_ap_int_w1s_addr = pixel_mux->base + 
			pixel_mux->pm_cfg->hailo15_buffer_ready_ap_int_w1s_offset;
		break;

	case VIDIOC_QUERYCAP:
		pr_debug("pixel_mux does't supports querycap\n");
		ret = -ENOENT;
		break;

	default:
		pr_debug("pixel_mux: got unsupported ioctl 0x%x, Context(process: %s, PID: %d)\n", cmd, current->comm, current->pid);
		ret = -ENOENT;
		break;
	}
	return ret;
}

static int pixel_mux_link_setup(struct media_entity *entity,
				const struct media_pad *local,
				const struct media_pad *remote, u32 flags)
{
	return 0;
}

static inline struct pixel_mux_priv *
v4l2_subdev_to_pixel_mux(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct pixel_mux_priv, subdev);
}

static int hailo15_pixel_mux_async_bound(struct v4l2_async_notifier *subdev_notifier,
				       struct v4l2_subdev *s_subdev,
				       struct v4l2_async_subdev *asd)
{
	struct v4l2_subdev *subdev = subdev_notifier->sd;
	struct pixel_mux_priv *pixel_mux = v4l2_subdev_to_pixel_mux(subdev);
	struct indexed_v4l2_async_subdev *iasd = container_of(asd,
							       struct indexed_v4l2_async_subdev,
							       asd);
	int index = iasd->index;
	int result;

	pr_debug("%s: subdev %s bounded\n", __func__, s_subdev->name);

	pixel_mux->remote_sources[index].pad = media_entity_get_fwnode_pad(
		&s_subdev->entity, s_subdev->fwnode, MEDIA_PAD_FL_SOURCE);
	dev_info(pixel_mux->dev, "%s source pad %d\n", __func__,
		 pixel_mux->remote_sources[index].pad);
	if (pixel_mux->remote_sources[index].pad < 0) {
		dev_err(pixel_mux->dev,
			"Couldn't find output pad for subdev %s\n",
			s_subdev->name);
		return pixel_mux->remote_sources[index].pad;
	}
	pixel_mux->remote_sources[index].subdev = s_subdev;

	dev_dbg(pixel_mux->dev, "Bound %s pad: %d\n", s_subdev->name,
		pixel_mux->remote_sources[index].pad);

	result = media_create_pad_link(
			&pixel_mux->remote_sources[index].subdev->entity,
			pixel_mux->remote_sources[index].pad,
			&pixel_mux->subdev.entity,
			iasd->index,
			MEDIA_LNK_FL_ENABLED | MEDIA_LNK_FL_IMMUTABLE);

	pr_debug("%s: media_create_pad_link result %d\n", __func__, result);
	return result;
}

unsigned int hailo15_mux_isp_stream_cfg_to_reg(
	const struct pixel_mux_priv *pixel_mux,
	const struct hailo15_mux_isp_stream_cfg *cfg)
{
	const unsigned int enable_bits = 1;
	unsigned int vc_bits = pixel_mux->pm_cfg->vc_width;
	return (cfg->enable | 
		cfg->vc << enable_bits | 
		cfg->dt << (enable_bits + vc_bits));
}

static void
hailo_pixel_mux_configure_dest(const struct pixel_mux_priv *pixel_mux,
			       const struct hailo15_mux_cfg *mux_cfg)
{
	const struct pm_config *pm_cfg = pixel_mux->pm_cfg;
	u32 mask;
	pr_debug("%s enter\n", __func__);

	writel(mux_cfg->pixel_mux_cfg, pixel_mux->base + pm_cfg->pixel_mux_cfg_offset);
	writel(hailo15_mux_isp_stream_cfg_to_reg(pixel_mux, &mux_cfg->isp0_stream0),
		pixel_mux->base + pm_cfg->isp0_stream0_offset);
	writel(hailo15_mux_isp_stream_cfg_to_reg(pixel_mux, &mux_cfg->isp0_stream1),
		pixel_mux->base + pm_cfg->isp0_stream1_offset);
	writel(hailo15_mux_isp_stream_cfg_to_reg(pixel_mux, &mux_cfg->isp0_stream2),
		pixel_mux->base + pm_cfg->isp0_stream2_offset);
	writel(hailo15_mux_isp_stream_cfg_to_reg(pixel_mux, &mux_cfg->isp1_stream0),
		pixel_mux->base + pm_cfg->isp1_stream0_offset);
	writel(hailo15_mux_isp_stream_cfg_to_reg(pixel_mux, &mux_cfg->isp1_stream1),
		pixel_mux->base + pm_cfg->isp1_stream1_offset);
	writel(hailo15_mux_isp_stream_cfg_to_reg(pixel_mux, &mux_cfg->isp1_stream2),
		pixel_mux->base + pm_cfg->isp1_stream2_offset);


	mask = readl(pixel_mux->base + pm_cfg->vision_buffer_ready_ap_int_mask_offset);
	writel((mask | mux_cfg->vision_buffer_ready_ap_int_mask),
		pixel_mux->base + pm_cfg->vision_buffer_ready_ap_int_mask_offset);

	writel(int_cfg.pixel_mux_vsync_mask,
		pixel_mux->base + pm_cfg->hailo15_pixel_mux_vsync_mask_offset);
	writel(int_cfg.vision_subsys_asf_int_mask,
		pixel_mux->base + pm_cfg->vision_subsys_asf_int_mask_offset);
	writel(int_cfg.vision_asf_int_fatal_mask,
		pixel_mux->base + pm_cfg->vision_asf_int_fatal_mask_offset);
	writel(int_cfg.vision_asf_int_nonfatal_mask,
		pixel_mux->base + pm_cfg->vision_asf_int_nonfatal_mask_offset);
	writel(int_cfg.vision_subsys_err_int_mask,
		pixel_mux->base + pm_cfg->vision_subsys_err_int_mask_offset);
	writel(int_cfg.vision_subsys_err_int_agg_mask,
		pixel_mux->base + pm_cfg->vision_subsys_err_int_agg_mask_offset);
}

static int pixel_mux_grp_id_to_pad_index(int grp_id)
{
	switch (grp_id) {
		case HAILO15_VID_GRP_SX_CSI0_ISP_MP:
		case HAILO15_VID_GRP_SX_CSI0_ISP_SP:
		case HAILO15_VID_GRP_SX_CSI0_P2A:
		case HAILO15_VID_GRP_S0_CSI0_P2A:
		case HAILO15_VID_GRP_S1_CSI0_P2A:
		case HAILO15_VID_GRP_S2_CSI0_P2A:
		case HAILO15_VID_GRP_S3_CSI0_P2A:
			return PIXEL_MUX_SINK_PAD_0;
		case HAILO15_VID_GRP_SX_CSI1_ISP_MP:
		case HAILO15_VID_GRP_SX_CSI1_ISP_SP:
		case HAILO15_VID_GRP_SX_CSI1_P2A:
		case HAILO15_VID_GRP_S0_CSI1_P2A:
		case HAILO15_VID_GRP_S1_CSI1_P2A:
		case HAILO15_VID_GRP_S2_CSI1_P2A:
		case HAILO15_VID_GRP_S3_CSI1_P2A:
			return PIXEL_MUX_SINK_PAD_1;
		default:
			return -EINVAL;
	}
}

static int pixel_mux_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct pixel_mux_priv *pixel_mux = v4l2_subdev_to_pixel_mux(sd);
	struct v4l2_subdev *subdev;
	struct media_pad *pad;
	int ret;

	pr_debug("%s: enable=%d\n", __func__, enable);

	if (!pixel_mux)
		return -EINVAL;

	if (pixel_mux_grp_id_to_pad_index(sd->grp_id) < 0)
		return -EINVAL;

	if (enable && !pixel_mux->enabled) {
		dev_dbg(pixel_mux->dev, "%s enabling vision_hclk\n", __func__);
		ret = clk_prepare_enable(pixel_mux->vision_hclk);
		if (ret) {
			pr_err("%s - failed enabling vision_hclk\n", __func__);
			return -EAGAIN;
		}
		dev_dbg(pixel_mux->dev, "%s enabling vision_clk\n", __func__);
		ret = clk_prepare_enable(pixel_mux->vision_clk);
		if (ret) {
			pr_err("%s - failed enabling vision_clk\n", __func__);
			return -EAGAIN;
		}
		pixel_mux->enabled = 1;
	}

	if (enable && !pixel_mux->remote_sources[sd->grp_id].configured) {
		if (hailo15_is_isp_grp_id(sd->grp_id)) {
			hailo_pixel_mux_configure_dest(pixel_mux, &isp_cfg);
		} else if (hailo15_is_p2a_grp_id(sd->grp_id)) {
			switch (pixel_mux->num_exposures) {
				case 1:
					dev_dbg(pixel_mux->dev, "Configuring P2A for 1 exposure\n");
					hailo_pixel_mux_configure_dest(pixel_mux,
									&p2a_cfg_sdr);
					break;
				case 2:
					dev_dbg(pixel_mux->dev, "Configuring P2A for 2 exposures\n");
					hailo_pixel_mux_configure_dest(pixel_mux,
									&p2a_cfg_2dol);
					break;
				case 3:
					dev_dbg(pixel_mux->dev, "Configuring P2A for 3 exposures\n");
					hailo_pixel_mux_configure_dest(pixel_mux,
									&p2a_cfg_3dol);
					break;
				default:
					dev_dbg(pixel_mux->dev, "Configuring P2A for 1 exposure (default)\n");
					hailo_pixel_mux_configure_dest(pixel_mux,
									&p2a_cfg_sdr);
					break;
			}
		} else {
			ret = -EINVAL;
			goto err_bad_src_grp;
		}
	}

	pixel_mux->remote_sources[sd->grp_id].configured = enable;

	pad = &pixel_mux->pads[pixel_mux_grp_id_to_pad_index(sd->grp_id)];
	if (pad)
		pad = media_entity_remote_pad(pad);

	if (pad && is_media_entity_v4l2_subdev(pad->entity)) {
		subdev = media_entity_to_v4l2_subdev(pad->entity);
		ret = v4l2_subdev_call(subdev, video, s_stream, enable);
		if (ret) {
			dev_err(pixel_mux->dev, "%s: failed to set enable source subdev\n",
				__func__);
			goto finish;
		}
	}
	ret = 0;
	goto finish;

err_bad_src_grp:
	dev_err(pixel_mux->dev, "%s: bad group id of source subdev\n",
		__func__);
finish:
	return ret;
}

static int pixel_mux_get_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_format *fmt)
{
	struct pixel_mux_priv *pixel_mux =
		v4l2_subdev_to_pixel_mux(sd);

	struct v4l2_mbus_framefmt *src_format;
	struct v4l2_mbus_framefmt *dst_format;
	if (!pixel_mux || !fmt || fmt->pad >= PIXEL_MUX_PAD_MAX)
		return -EINVAL;

	src_format = &pixel_mux->pad_fmts[fmt->pad];
	dst_format = &fmt->format;
	if (!src_format || !dst_format)
		return -EINVAL;

	*dst_format = *src_format;
	return 0;
}

static int pixel_mux_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_format *fmt)
{
	struct pixel_mux_priv *pixel_mux = v4l2_subdev_to_pixel_mux(sd);
	struct v4l2_subdev *subdev;
	struct media_pad *pad;
	unsigned int sink_pad_idx = PIXEL_MUX_SOURCE_PAD_0;
	const struct v4l2_mbus_framefmt *src_format = &fmt->format;
	struct v4l2_mbus_framefmt *dst_format;
	struct v4l2_subdev_format csi_fmt = {0};
	int ret = 0;

	dev_dbg(pixel_mux->dev, "%s: enter\n", __func__);

	if (!pixel_mux)
		return -EINVAL;

	/* set format in pixel_mux->pad_fmts */
	dst_format = &pixel_mux->pad_fmts[fmt->pad];
	if (!dst_format)
		return -EINVAL;
	*dst_format = *src_format;

	switch (src_format->code) {
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
		pixel_mux->num_exposures = 1;
		break;
	case MEDIA_BUS_FMT_SRGGB12_2X12:
	case MEDIA_BUS_FMT_SGBRG12_2X12:
		pixel_mux->num_exposures = 2;
		break;
	case MEDIA_BUS_FMT_SRGGB12_3X12:
	case MEDIA_BUS_FMT_SGBRG12_3X12:
		pixel_mux->num_exposures = 3;
		break;
	default:
		pixel_mux->num_exposures = 1;
		break;
	}

	memcpy(&csi_fmt, fmt, sizeof(struct v4l2_subdev_format));

	/* change format to one of two: x16 or x32 */
	if (hailo15_is_isp_grp_id(sd->grp_id)) {
		csi_fmt.format.code = MEDIA_BUS_FMT_SRGGB12_1X32;
	} else if (hailo15_is_p2a_grp_id(sd->grp_id)) {
		csi_fmt.format.code = (src_format->code == MEDIA_BUS_FMT_YVYU8_2X8)
							? MEDIA_BUS_FMT_YVYU8_2X8
							: MEDIA_BUS_FMT_SRGGB12_1X12;
	} else {
		ret = -EINVAL;
		goto err_bad_src_grp;
	}

	/* Propagate fake format to sink */
	sink_pad_idx = pixel_mux_grp_id_to_pad_index(sd->grp_id);
	pad = &pixel_mux->pads[sink_pad_idx];
	if (pad)
		pad = media_entity_remote_pad(pad);

	if (pad && is_media_entity_v4l2_subdev(pad->entity)) {
		subdev = media_entity_to_v4l2_subdev(pad->entity);
		subdev->grp_id = sd->grp_id;
		ret = v4l2_subdev_call(subdev, pad, set_fmt, NULL, &csi_fmt);
		if (ret) {
			dev_err(pixel_mux->dev, "%s - failed to set format %x on sink subdev %s, ret %d\n",
				__func__, csi_fmt.format.code, subdev->name, ret);
		}
	}
	goto finish;

err_bad_src_grp:
	dev_err(pixel_mux->dev, "%s: bad group id of source subdev\n",
		__func__);
finish:
	return ret;
}

static bool hailo_pixel_mux_async_check_subdev_notifier_completion(struct v4l2_async_notifier *video_notifier)
{
	// The pixel mux subdevice notifier is the child of the video notifier
	struct v4l2_async_notifier *subdev_notifier = hailo15_media_find_child_notifier(video_notifier);
	bool result;

	pr_debug("hailo15_pixel_mux: checking subdev notifier completion\n");

	if (!subdev_notifier) {
		// If the subdevice notifier could not be found, we cannot complete the registration
		dev_info(video_notifier->v4l2_dev->dev, "Could not find child notifier in the list");
		return false;
	}

	result = hailo15_media_check_completion(subdev_notifier);
	pr_debug("hailo15_pixel_mux: subdev notifier completion is %d\n", result);

	return result;
}

static int hailo15_pixel_mux_async_complete(struct v4l2_async_notifier *video_notifier)
{
	struct v4l2_device *video_dev = video_notifier->v4l2_dev;
	int ret = 0;

	pr_debug("hailo15_pixel_mux: complete function invoked\n");

	/* Since Linux invokes the complete function on the root notifier (which doesnt hold the subdevice),
	 * we need to check if the subdevice notifier is ready */
	if (!hailo_pixel_mux_async_check_subdev_notifier_completion(video_notifier))
		return 0;

	if (!video_dev) {
		dev_err(video_dev->dev, "Complete function was invoked, but the notifier does not hold a v4l2 video device!");
		return -EINVAL;
	}

	ret = hailo15_media_register_video_subdev_nodes(video_dev);
	if (ret) {
		dev_err(video_dev->dev, "Failed registering the subdevs with code %d", ret);
		return ret;
	}

	pr_debug("hailo15_pixel_mux: complete function finished\n");

	return 0;
}

static const struct v4l2_async_notifier_operations
hailo15_pixel_mux_subdev_notifier_ops = {
	.bound = hailo15_pixel_mux_async_bound,
},
hailo15_pixel_mux_video_notifier_ops = {
	.complete = hailo15_pixel_mux_async_complete,
};

static int
hailo15_pixel_mux_parse_dt(struct pixel_mux_priv *hailo15_pixel_mux)
{
	struct indexed_v4l2_async_subdev *asd;
	struct fwnode_handle *fwh;
	struct device_node *ep;
	int ret;
	int i = 0;
	bool valid_ep_found = false;

	dev_dbg(hailo15_pixel_mux->dev, "Parsing DT\n");
	v4l2_async_notifier_init(&hailo15_pixel_mux->subdev_notifier);
	v4l2_async_notifier_init(&hailo15_pixel_mux->video_notifier);

	hailo15_pixel_mux->video_notifier.ops = &hailo15_pixel_mux_video_notifier_ops;
	v4l2_async_notifier_register(hailo15_pixel_mux->subdev.v4l2_dev, &hailo15_pixel_mux->video_notifier);

	hailo15_pixel_mux->subdev_notifier.parent = &hailo15_pixel_mux->video_notifier;

	/* Iterate over sink ports (0-1) */
	/* NOTE: pad number matches port number */
	for (i = 0; i < PIXEL_MUX_SINK_PAD_MAX; ++i) {
		ep = of_graph_get_endpoint_by_regs(hailo15_pixel_mux->dev->of_node, i, -1);
		if (!ep) {
			dev_dbg(hailo15_pixel_mux->dev, "No endpoint found for port #%d\n", i);
			continue;
		}

		fwh = of_fwnode_handle(ep);

		ret = fwnode_device_is_available(fwnode_graph_get_remote_port_parent(fwh));
		if (!ret) {
			dev_dbg(hailo15_pixel_mux->dev, "The device of port #%d is disabled in the device tree (fwnode_device_is_available returned %d)", i, ret);
			continue;
		}

		asd = v4l2_async_notifier_add_fwnode_remote_subdev(
			&hailo15_pixel_mux->subdev_notifier, fwh, struct indexed_v4l2_async_subdev);
		of_node_put(ep);
		if (IS_ERR(asd)) {
			dev_err(hailo15_pixel_mux->dev, "Failed to add port #%d remote subdev notifier\n", i);
			return PTR_ERR(asd);
		}

		asd->index = i;

		valid_ep_found = true;
	}

	if (!valid_ep_found) {
		dev_err(hailo15_pixel_mux->dev, "No valid sink endpoints in DT\n");
		return -ENODEV;
	}

	hailo15_pixel_mux->subdev_notifier.ops = &hailo15_pixel_mux_subdev_notifier_ops;
	hailo15_pixel_mux->subdev_notifier.sd = &hailo15_pixel_mux->subdev;
	ret = v4l2_async_subdev_notifier_register(&hailo15_pixel_mux->subdev,
						  &hailo15_pixel_mux->subdev_notifier);
	if (ret) {
		dev_err(hailo15_pixel_mux->dev, "Failed to register subdev notifier\n");
		v4l2_async_notifier_cleanup(&hailo15_pixel_mux->subdev_notifier);
	}

	return ret;
}

static int hailo15_pixel_mux_registered(struct v4l2_subdev* sd)
{
	struct pixel_mux_priv *hailo15_pixel_mux_priv =
		v4l2_subdev_to_pixel_mux(sd);
	return hailo15_pixel_mux_parse_dt(hailo15_pixel_mux_priv);
}

static struct v4l2_subdev_internal_ops hailo15_pixel_mux_internal_ops = {
	.registered = hailo15_pixel_mux_registered,
};

static struct v4l2_subdev_core_ops pixel_mux_core_ops = {
	.ioctl = pixel_mux_priv_ioctl,
};

static const struct media_entity_operations pixel_mux_sd_media_ops = {
	.link_setup = pixel_mux_link_setup,
};

static struct v4l2_subdev_video_ops pixel_mux_subdev_video_ops = {
	.s_stream = pixel_mux_s_stream,
};

static const struct v4l2_subdev_pad_ops pixel_mux_pad_ops = {
	.get_fmt               = pixel_mux_get_fmt,
	.set_fmt               = pixel_mux_set_fmt,
};

static struct v4l2_subdev_ops pixel_mux_subdev_ops = {
	.core = &pixel_mux_core_ops,
	.video = &pixel_mux_subdev_video_ops,
	.pad = &pixel_mux_pad_ops,
};



/* Initialize the dma context.                                                  */
/* The dma context holds the required information for proper buffer management. */
static int hailo15_init_dma_ctx(struct hailo15_dma_ctx *ctx,
				struct pixel_mux_priv *pixel_mux)
{
	ctx->dev = (void *)pixel_mux;
	v4l2_set_subdevdata(&pixel_mux->subdev, ctx);
	return 0;
}

static int pixel_mux_probe(struct platform_device *pdev)
{
	struct pixel_mux_priv *pixel_mux;
	struct v4l2_subdev *subdev;
	struct hailo15_dma_ctx *dma_ctx;
	int ret;
	struct resource *res;
	unsigned int i;

	dev_info(&pdev->dev, "probe started");

	pixel_mux = devm_kzalloc(&pdev->dev, sizeof(*pixel_mux), GFP_KERNEL);
	if (!pixel_mux)
		return -ENOMEM;

	pixel_mux->pm_cfg  = (const struct pm_config *)of_device_get_match_data(&pdev->dev);
	if (!pixel_mux->pm_cfg) {
		dev_err(&pdev->dev, "No pm_config match found\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, pixel_mux);

	pixel_mux->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("cant find resources\n");
		return -EINVAL;
	}
	dev_dbg(&pdev->dev, "using %pR\n", res);	

	pixel_mux->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(pixel_mux->base)) {
        dev_err(&pdev->dev, "Failed to remap IO memory %pR, err = (%pe)\n", res, pixel_mux->base);
        return PTR_ERR(pixel_mux->base);
	}

	pixel_mux->vision_clk = devm_clk_get(&pdev->dev, "vision_clk");
	if (IS_ERR(pixel_mux->vision_clk)) {
		dev_err(&pdev->dev,
			"Couldn't get pixel_mux->vision_clk clock\n");
		return PTR_ERR(pixel_mux->vision_clk);
	}

	pixel_mux->vision_hclk = devm_clk_get(&pdev->dev, "vision_hclk");
	if (IS_ERR(pixel_mux->vision_hclk)) {
		dev_err(&pdev->dev,
			"Couldn't get pixel_mux->vision_hclk clock\n");
		return PTR_ERR(pixel_mux->vision_hclk);
	}

	subdev = &pixel_mux->subdev;
	subdev->owner = THIS_MODULE;
	subdev->dev = &pdev->dev;
	v4l2_subdev_init(subdev, &pixel_mux_subdev_ops);

	// v4l2_set_subdevdata(&pixel_mux->subdev, &pdev->dev);
	snprintf(subdev->name, V4L2_SUBDEV_NAME_SIZE, "%s.%s", KBUILD_MODNAME,
		 dev_name(&pdev->dev));
	subdev->internal_ops = &hailo15_pixel_mux_internal_ops;
	subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	subdev->entity.function = MEDIA_ENT_F_VID_MUX;
	pixel_mux->pads[PIXEL_MUX_SOURCE_PAD_0].flags = MEDIA_PAD_FL_SOURCE;
	pixel_mux->pads[PIXEL_MUX_SOURCE_PAD_1].flags = MEDIA_PAD_FL_SOURCE;
	pixel_mux->pads[PIXEL_MUX_SINK_PAD_0].flags = MEDIA_PAD_FL_SINK;
	pixel_mux->pads[PIXEL_MUX_SINK_PAD_1].flags = MEDIA_PAD_FL_SINK;

	for (i = PIXEL_MUX_SOURCE_PAD_0; i < PIXEL_MUX_PAD_MAX; i++)
		pixel_mux->pad_fmts[i] = fmt_default;

	/*create media pads*/
	ret = media_entity_pads_init(&subdev->entity, PIXEL_MUX_PAD_MAX,
				     pixel_mux->pads);
	subdev->entity.ops = &pixel_mux_sd_media_ops;
	if (ret < 0) {
		dev_err(&pdev->dev,
			"%s Failed to init media pads with ret=%d\n", __func__,
			ret);
		return ret;
	}

	dma_ctx = devm_kzalloc(&pdev->dev, sizeof(struct hailo15_dma_ctx),
			       GFP_KERNEL);

	if (!dma_ctx) {
		ret = -ENOMEM;
		goto err_alloc_dma_ctx;
	}

	ret = hailo15_init_dma_ctx(dma_ctx, pixel_mux);
	if (ret) {
		pr_err("can't init dma context\n");
		goto err_init_dma_ctx;
	}

	pm_runtime_get_sync(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ret = v4l2_async_register_subdev(subdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s Async register failed, ret=%d\n",
			__func__, ret);
		goto probe_err_entity_cleanup;
	}

	mutex_init(&pixel_mux->lock);

	ret = hailo15_media_create_connections(&pdev->dev, subdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s Failed to create connections\n", __func__);
		goto probe_err_entity_cleanup;
	}

	dev_info(&pdev->dev, "%s hailo pixel mux probed successfully\n",
		 __func__);
	return 0;

probe_err_entity_cleanup:
err_init_dma_ctx:
err_alloc_dma_ctx:
	media_entity_cleanup(&subdev->entity);
	return ret;
}

static int pixel_mux_remove(struct platform_device *pdev)
{
	struct pixel_mux_priv *pixel_mux = platform_get_drvdata(pdev);
	dev_info(&pdev->dev, "%s enter\n", __func__);

	media_entity_cleanup(&pixel_mux->subdev.entity);
	v4l2_async_unregister_subdev(&pixel_mux->subdev);

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	kfree(pixel_mux);

	return 0;
}

static struct platform_driver pixel_mux_driver = {
	.probe	= pixel_mux_probe,
	.remove	= pixel_mux_remove,
	.driver	= {
		.name = "hailo-pixel-mux",
		.of_match_table	= hailo_pixel_mux_of_table,
	},
};

module_platform_driver(pixel_mux_driver);
MODULE_AUTHOR("Tanya Vasilevitsky <tatyanav@hailo.ai>");
MODULE_DESCRIPTION("Hailo pixel mux");
MODULE_LICENSE("GPL v2");
