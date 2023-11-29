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
#include "hailo15-pixel-mux.h"

#define HAILO15_BUFFER_READY_AP_INT_MASK_OFFSET 0x60
#define HAILO15_BUFFER_READY_AP_INT_STATUS_OFFSET 0x64
#define HAILO15_BUFFER_READY_AP_INT_W1C_OFFSET 0x68
#define HAILO15_BUFFER_READY_AP_INT_W1S_OFFSET 0x6c

enum pixel_mux_pads {
	PIXEL_MUX_PAD_SINK_0,
	PIXEL_MUX_PAD_SINK_1,
	PIXEL_MUX_PAD_SOURCE_0,
	PIXEL_MUX_PAD_SOURCE_1,
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
	struct clk *sys_clk;
	struct clk *clk;
	struct clk *csi_rx0_xtal_clk;
	struct clk *csi_rx1_xtal_clk;
	int irq;

	u8 num_lanes;
	u8 max_lanes;
	u8 max_streams;

	struct v4l2_subdev subdev;
	struct v4l2_async_notifier notifier;
	struct media_pad pads[PIXEL_MUX_PAD_MAX];
	struct hailo15_buf_ctx *buf_ctx;

	/* Remote source */
	struct v4l2_subdev *source_subdev;
	int source_pad;
};

static const struct hailo15_mux_cfg isp_cfg = {
	.pixel_mux_cfg =
		P2A0_2_SW_DBG_P2A1_2_CSIRX0_ISP0_2_CSIRX0_ISP1_2_SW_DBG,
	.isp0_stream0 = ENABLE_VC_0_DT_RAW_12,
	.isp0_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp0_stream2 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream0 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream2 = DISABLE_VC_4_DT_DISABLE,
	.vision_buffer_ready_ap_int_mask = 0x0
};

static const struct hailo15_mux_cfg p2a_cfg = {
	.pixel_mux_cfg =
		P2A0_2_CSIRX0_P2A1_2_CSIRX1_ISP0_2_SW_DBG_ISP1_2_SW_DBG,
	.isp0_stream0 = DISABLE_VC_4_DT_DISABLE,
	.isp0_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp0_stream2 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream0 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream1 = DISABLE_VC_4_DT_DISABLE,
	.isp1_stream2 = DISABLE_VC_4_DT_DISABLE,
	.vision_buffer_ready_ap_int_mask = 0xfff
};

static const struct hailo15_mux_interrupt_cfg int_cfg = {
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
	int ret;

	switch (cmd) {
	case VIDEO_GET_P2A_REGS:
		if (!arg)
			ret = -EINVAL;
		((struct hailo15_p2a_buffer_regs_addr *)arg)
			->buffer_ready_ap_int_mask_addr =
			pixel_mux->base +
			HAILO15_BUFFER_READY_AP_INT_MASK_OFFSET;
		((struct hailo15_p2a_buffer_regs_addr *)arg)
			->buffer_ready_ap_int_status_addr =
			pixel_mux->base +
			HAILO15_BUFFER_READY_AP_INT_STATUS_OFFSET;
		((struct hailo15_p2a_buffer_regs_addr *)arg)
			->buffer_ready_ap_int_w1c_addr =
			pixel_mux->base +
			HAILO15_BUFFER_READY_AP_INT_W1C_OFFSET;
		((struct hailo15_p2a_buffer_regs_addr *)arg)
			->buffer_ready_ap_int_w1s_addr =
			pixel_mux->base +
			HAILO15_BUFFER_READY_AP_INT_W1S_OFFSET;
		break;
	default:
		pr_debug("pixel_mux: got unsupported ioctl 0x%x\n", cmd);
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

static int hailo_pixel_mux_async_bound(struct v4l2_async_notifier *notifier,
				       struct v4l2_subdev *s_subdev,
				       struct v4l2_async_subdev *asd)
{
	struct v4l2_subdev *subdev = notifier->sd;
	struct pixel_mux_priv *pixel_mux = v4l2_subdev_to_pixel_mux(subdev);

	pr_info("%s: enter\n", __func__);
	pixel_mux->source_pad = media_entity_get_fwnode_pad(
		&s_subdev->entity, s_subdev->fwnode, MEDIA_PAD_FL_SOURCE);
	dev_info(pixel_mux->dev, "%s source pad %d\n", __func__,
		 pixel_mux->source_pad);
	if (pixel_mux->source_pad < 0) {
		dev_err(pixel_mux->dev,
			"Couldn't find output pad for subdev %s\n",
			s_subdev->name);
		return pixel_mux->source_pad;
	}
	pixel_mux->source_subdev = s_subdev;

	dev_dbg(pixel_mux->dev, "Bound %s pad: %d\n", s_subdev->name,
		pixel_mux->source_pad);

	return media_create_pad_link(
		&pixel_mux->source_subdev->entity, pixel_mux->source_pad,
		&pixel_mux->subdev.entity, 0,
		MEDIA_LNK_FL_ENABLED | MEDIA_LNK_FL_IMMUTABLE);
}

static void
hailo_pixel_mux_configure_dest(const struct pixel_mux_priv *pixel_mux,
			       const struct hailo15_mux_cfg *mux_cfg)
{
	pr_debug("%s enter\n", __func__);
	writel(mux_cfg->pixel_mux_cfg, pixel_mux->base + PIXEL_MUX_CFG_OFFSET);
	writel(mux_cfg->isp0_stream0, pixel_mux->base + ISP0_STREAM0_OFFSET);
	writel(mux_cfg->isp0_stream1, pixel_mux->base + ISP0_STREAM1_OFFSET);
	writel(mux_cfg->isp0_stream2, pixel_mux->base + ISP0_STREAM2_OFFSET);
	writel(mux_cfg->isp1_stream0, pixel_mux->base + ISP1_STREAM0_OFFSET);
	writel(mux_cfg->isp1_stream1, pixel_mux->base + ISP1_STREAM1_OFFSET);
	writel(mux_cfg->isp1_stream2, pixel_mux->base + ISP1_STREAM2_OFFSET);
	writel(mux_cfg->vision_buffer_ready_ap_int_mask,
	       pixel_mux->base + VISION_BUFFER_READY_AP_INT_MASK_OFFSET);
	writel(int_cfg.vision_subsys_asf_int_mask,
	       pixel_mux->base + VISION_SUBSYS_ASF_INT_MASK_OFFSET);
	writel(int_cfg.vision_asf_int_fatal_mask,
	       pixel_mux->base + VISION_ASF_INT_FATAL_MASK_OFFSET);
	writel(int_cfg.vision_asf_int_nonfatal_mask,
	       pixel_mux->base + VISION_ASF_INT_NONFATAL_MASK_OFFSET);
	writel(int_cfg.vision_subsys_err_int_mask,
	       pixel_mux->base + VISION_SUBSYS_ERR_INT_MASK_OFFSET);
	writel(int_cfg.vision_subsys_err_int_agg_mask,
	       pixel_mux->base + VISION_SUBSYS_ERR_INT_AGG_MASK_OFFSET);
}

static int pixel_mux_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct pixel_mux_priv *pixel_mux = v4l2_subdev_to_pixel_mux(sd);
	struct v4l2_subdev *subdev;
	struct media_pad *pad;
	int ret;

	if (!pixel_mux)
		return -EINVAL;

	if (enable) {
		// TODO - remove this when we have the ability to make clocks that are not in the same heirarchy depend on each other (MSW-2254)
		dev_dbg(pixel_mux->dev, "%s enabling csi_rx0_xtal_clk\n",
			__func__);
		ret = clk_prepare_enable(pixel_mux->csi_rx0_xtal_clk);
		if (ret) {
			pr_err("%s - failed enabling csi_rx0_xtal_clk\n",
			       __func__);
			return -EAGAIN;
		}

		if (sd->grp_id == VID_GRP_ISP_MP ||
		    sd->grp_id == VID_GRP_ISP_SP) {
			hailo_pixel_mux_configure_dest(pixel_mux, &isp_cfg);
		} else if (sd->grp_id == VID_GRP_P2A) {
			hailo_pixel_mux_configure_dest(pixel_mux, &p2a_cfg);
		} else {
			ret = -EINVAL;
			goto err_bad_src_grp;
		}
	}

	pad = &pixel_mux->pads[0];
	if (pad)
		pad = media_entity_remote_pad(pad);

	if (pad && is_media_entity_v4l2_subdev(pad->entity)) {
		subdev = media_entity_to_v4l2_subdev(pad->entity);
		v4l2_subdev_call(subdev, video, s_stream, enable);
	}
	ret = 0;
	goto finish;

err_bad_src_grp:
	dev_err(pixel_mux->dev, "%s: bad group id of source subdev\n",
		__func__);
finish:
	return ret;
}

static struct v4l2_subdev_core_ops pixel_mux_core_ops = {
	.ioctl = pixel_mux_priv_ioctl,
};

static const struct v4l2_async_notifier_operations pixel_mux_notifier_ops = {
	.bound = hailo_pixel_mux_async_bound,
};

static const struct media_entity_operations pixel_mux_sd_media_ops = {
	.link_setup = pixel_mux_link_setup,
};

static struct v4l2_subdev_video_ops pixel_mux_subdev_video_ops = {
	.s_stream = pixel_mux_s_stream,
};

static struct v4l2_subdev_ops pixel_mux_subdev_ops = {
	.core = &pixel_mux_core_ops,
	.video = &pixel_mux_subdev_video_ops,
};

static int pixel_mux_get_ep(struct pixel_mux_priv *pixel_mux)
{
	struct v4l2_fwnode_endpoint v4l2_ep = { .bus_type = 0 };
	struct v4l2_async_subdev *asd;
	struct fwnode_handle *fwh;
	struct device_node *ep;
	int ret;

	dev_info(pixel_mux->dev, "%s enter\n", __func__);
	ep = of_graph_get_endpoint_by_regs(pixel_mux->dev->of_node, 0, 0);
	if (!ep) {
		dev_err(pixel_mux->dev, "no endpoint found\n");
		return -EINVAL;
	}

	fwh = of_fwnode_handle(ep);
	ret = v4l2_fwnode_endpoint_parse(fwh, &v4l2_ep);
	if (ret) {
		dev_err(pixel_mux->dev, "Could not parse v4l2 endpoint\n");
		of_node_put(ep);
		return ret;
	}

	v4l2_async_notifier_init(&pixel_mux->notifier);

	asd = v4l2_async_notifier_add_fwnode_remote_subdev(
		&pixel_mux->notifier, fwh, struct v4l2_async_subdev);
	of_node_put(ep);
	if (IS_ERR(asd)) {
		dev_err(pixel_mux->dev, "%s error asd\n", __func__);
		return PTR_ERR(asd);
	}

	pixel_mux->notifier.ops = &pixel_mux_notifier_ops;

	ret = v4l2_async_subdev_notifier_register(&pixel_mux->subdev,
						  &pixel_mux->notifier);
	if (ret) {
		dev_err(pixel_mux->dev,
			"%s failed to register subdev notifier\n", __func__);
		v4l2_async_notifier_cleanup(&pixel_mux->notifier);
	}

	return ret;
}

/* Initialize the dma context.                                                  */
/* The dma context holds the required information for proper buffer management. */
static int hailo15_init_dma_ctx(struct hailo15_dma_ctx *ctx,
				struct pixel_mux_priv *pixel_mux)
{
	ctx->dev = (void *)pixel_mux;
	spin_lock_init(&ctx->buf_ctx.irqlock);
	INIT_LIST_HEAD(&ctx->buf_ctx.dmaqueue);
	ctx->buf_ctx.ops = NULL;
	pixel_mux->buf_ctx = &ctx->buf_ctx;
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

	dev_info(&pdev->dev, "%s enter\n", __func__);

	pixel_mux = kzalloc(sizeof(*pixel_mux), GFP_KERNEL);
	if (!pixel_mux)
		return -ENOMEM;
	platform_set_drvdata(pdev, pixel_mux);

	pixel_mux->dev = &pdev->dev;
	ret = pixel_mux_get_ep(pixel_mux);
	if (ret) {
		dev_err(&pdev->dev, "%s getting endpoint failed, ret=%d\n",
			__func__, ret);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_info("cant find resources\n");
		return -EINVAL;
	}
	pixel_mux->base = devm_ioremap_resource(&pdev->dev, res);

	// TODO - remove this when we have the ability to make clocks that are not in the same heirarchy depend on each other (MSW-2254)
	pixel_mux->csi_rx0_xtal_clk =
		devm_clk_get(&pdev->dev, "csi_rx0_xtal_clk");
	if (IS_ERR(pixel_mux->csi_rx0_xtal_clk)) {
		dev_err(&pdev->dev,
			"Couldn't get pixel_mux->csi_rx0_xtal_clk clock\n");
		return PTR_ERR(pixel_mux->csi_rx0_xtal_clk);
	}

	subdev = &pixel_mux->subdev;
	subdev->owner = THIS_MODULE;
	subdev->dev = &pdev->dev;
	v4l2_subdev_init(subdev, &pixel_mux_subdev_ops);

	// v4l2_set_subdevdata(&pixel_mux->subdev, &pdev->dev);
	snprintf(subdev->name, V4L2_SUBDEV_NAME_SIZE, "%s.%s", KBUILD_MODNAME,
		 dev_name(&pdev->dev));
	subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	subdev->entity.function = MEDIA_ENT_F_VID_MUX;
	pixel_mux->pads[PIXEL_MUX_PAD_SOURCE_0].flags = MEDIA_PAD_FL_SOURCE;
	pixel_mux->pads[PIXEL_MUX_PAD_SOURCE_1].flags = MEDIA_PAD_FL_SOURCE;
	pixel_mux->pads[PIXEL_MUX_PAD_SINK_0].flags = MEDIA_PAD_FL_SINK;
	pixel_mux->pads[PIXEL_MUX_PAD_SINK_1].flags = MEDIA_PAD_FL_SINK;

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

	ret = v4l2_async_register_subdev(subdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s Async register failed, ret=%d\n",
			__func__, ret);
		goto probe_err_entity_cleanup;
	}

	mutex_init(&pixel_mux->lock);

	dev_info(&pdev->dev, "%s hailo pixel mux probed succesfully\n",
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
	kfree(pixel_mux);

	return 0;
}

static const struct of_device_id hailo_pixel_mux_of_table[] = {
	{ .compatible = "hailo,hailo15-pixel-mux" },
	{},
};
MODULE_DEVICE_TABLE(of, hailo_pixel_mux_of_table);

static struct platform_driver pixel_mux_driver = {
	.probe	= pixel_mux_probe,
	.remove	= pixel_mux_remove,

	.driver	= {
		.name		= "hailo-pixel-mux",
		.of_match_table	= hailo_pixel_mux_of_table,
	},
};

module_platform_driver(pixel_mux_driver);
MODULE_AUTHOR("Tanya Vasilevitsky <tatyanav@hailo.ai>");
MODULE_DESCRIPTION("Hailo pixel mux");
MODULE_LICENSE("GPL v2");
