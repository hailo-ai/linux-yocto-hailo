// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Cadence MIPI-CSI2 RX Controller v1.3
 *
 * Copyright (C) 2017 Cadence Design Systems Inc.
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
#include <linux/of_platform.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

#define RES_MIN

#define CSI2RX_DEVICE_CFG_REG 0x000

#define CSI2RX_SOFT_RESET_REG 0x004
#define CSI2RX_SOFT_RESET_PROTOCOL BIT(1)
#define CSI2RX_SOFT_RESET_FRONT BIT(0)

#define CSI2RX_STATIC_CFG_REG 0x008
#define CSI2RX_STATIC_CFG_DLANE_MAP(llane, plane) ((plane) << (16 + (llane)*4))
#define CSI2RX_STATIC_CFG_LANES_MASK GENMASK(11, 8)
#define CSI2RX_STATIC_CFG_EXTENDED_VC_EN BIT(4)

#define CSI2RX_DPHY_LANE_CTRL_REG 0x40
#define CSI2RX_DPHY_LANE_CTRL_RESET_LANES 0x1f
#define CSI2RX_DPHY_LANE_CTRL_ENABLE_LANES 0x1f01f

#define CSI2RX_STREAM_BASE(n) (((n) + 1) * 0x100)

#define CSI2RX_STREAM_CTRL_REG(n) (CSI2RX_STREAM_BASE(n) + 0x000)
#define CSI2RX_STREAM_CTRL_START BIT(0)
#define CSI2RX_STREAM_CTRL_STOP BIT(1)
#define CSI2RX_STREAM_CTRL_SOFT_RESET BIT(4)

#define CSI2RX_STREAM_STATUS_REG(n) (CSI2RX_STREAM_BASE(n) + 0x004)
#define CSI2RX_STREAM_STATUS_RUNNING BIT(31)
#define CSI2RX_STREAM_STATUS_MAX_RETRIES 50 // 1 sec = MAX_RETRIES * SLEEP_MSECS
#define CSI2RX_STREAM_STATUS_SLEEP_MSECS 20

#define CSI2RX_STREAM_DATA_CFG_REG(n) (CSI2RX_STREAM_BASE(n) + 0x008)
#define CSI2RX_STREAM_DATA_CFG_EN_VC_SELECT BIT(31)
#define CSI2RX_STREAM_DATA_CFG_VC_SELECT(n) BIT((n) + 16)
#define CSI2RX_STREAM_DATA_CFG_DT0_RAW10 0x2b
#define CSI2RX_STREAM_DATA_CFG_DT0_RAW12 0x2c
#define CSI2RX_STREAM_DATA_CFG_DT0_PROCESS_ENABLE BIT(7)

#define CSI2RX_STREAM_CFG_REG(n) (CSI2RX_STREAM_BASE(n) + 0x00c)
#define CSI2RX_STREAM_CFG_FIFO_MODE_LARGE_BUF (1 << 8)
#define CSI2RX_STREAM_CFG_FIFO_FILL_LEVEL (2880 << 16)
#define CSI2RX_STREAM_CFG_2_PPC (1<<4)

#define CSI2RX_STREAM_MONITOR_CTRL_REG(n) (CSI2RX_STREAM_BASE(n) + 0x010)

#define CSI2RX_LANES_MAX 4
#define CSI2RX_STREAMS_MAX 4

#define CSI2RX_DPHY_LANE_CONTROL_REG_ADDR 0x40
#define CSI2RX_DPHY_LANE_CONTROL_REG_LANES_ENABLE 0x1f01f
#define CSI2RX_DPHY_LANE_CONTROL_REG_LANES_RESET 0x0001f

#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT0_ADDR_OFFSET (0xb00)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_ADDR_OFFSET (0xb08)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_VAL (0xaaaaaaaa)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_ADDR_OFFSET (0xb0c)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_VAL (0x2aa)
#define CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_ADDR_OFFSET (0x020)
#define CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_VAL (0x429)
#define CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_L__SHIFT 0x00000000
#define CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_R__SHIFT 0x00000005

#define CSI2RX_CID_MODE_SEL (V4L2_CID_USER_BASE + 0x2000)

static int csi2rx_set_ctrl(struct v4l2_ctrl *ctrl);

enum csi2rx_pads {
	CSI2RX_PAD_SINK,
	CSI2RX_PAD_SOURCE_STREAM0,
	CSI2RX_PAD_SOURCE_STREAM1,
	CSI2RX_PAD_SOURCE_STREAM2,
	CSI2RX_PAD_SOURCE_STREAM3,
	CSI2RX_PAD_MAX,
};
struct csi2rx_fmt {
	u32				code;
	u8				bpp;
};

enum csi2rx_mode {
	CSI2RX_MODE_SDR,
	CSI2RX_MODE_HDR,
	CSI2RX_MODE_MAX,
};

/* V4l2 subdevice control ops*/
static const struct v4l2_ctrl_ops csi2rx_ctrl_ops = {
	.s_ctrl = csi2rx_set_ctrl,
};

const struct v4l2_ctrl_config csi2rx_mode_sel_ctrl_cfg = {
	.ops = &csi2rx_ctrl_ops,
	.id = CSI2RX_CID_MODE_SEL,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_UPDATE,
	.name = "mode_sel",
	.step = 1,
	.min = 0,
	.max = CSI2RX_MODE_MAX - 1,
	.def = 0,
};

struct csi2rx_priv {
	struct device *dev;
	unsigned int count;

	/*
	 * Used to prevent race conditions between multiple,
	 * concurrent calls to start and stop.
	 */
	struct mutex lock;

	void __iomem *base;
	void __iomem *internal_dphy_base;
	struct clk *sys_clk;
	struct clk *p_clk;
	struct clk *pixel_clk[CSI2RX_STREAMS_MAX];
	struct phy *dphy;
	struct device_node *internal_dphy;

	u8 lanes[CSI2RX_LANES_MAX];
	u8 num_lanes;
	u8 max_lanes;
	u8 max_streams;
	bool has_internal_dphy;

	struct v4l2_subdev subdev;
	struct v4l2_async_notifier notifier;
	struct media_pad pads[CSI2RX_PAD_MAX];
	struct v4l2_mbus_framefmt pad_fmts[CSI2RX_PAD_MAX];
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *mode_sel_ctrl;

	/* Remote source */
	struct v4l2_subdev *source_subdev;
	int source_pad;

	// link frequency in which to configure the internal dphy, if exists
	u64 link_freq;

	enum csi2rx_mode cur_mode;
};

static const struct csi2rx_fmt csi2rx_formats[] = {
	{
		.code	= MEDIA_BUS_FMT_SRGGB12_1X12,
		.bpp	= 2,
	},
	{
		.code	= MEDIA_BUS_FMT_SRGGB12_1X32,
		.bpp	= 4,
	},
};

static const struct v4l2_mbus_framefmt fmt_default = {
	.width		= 3840,
	.height		= 2160,
	.code		= MEDIA_BUS_FMT_SRGGB12_1X12,
	.field		= V4L2_FIELD_NONE,
	.colorspace	= V4L2_COLORSPACE_DEFAULT,
};

/**
 * csi2rx_set_ctrl() - Set subdevice control
 * @ctrl: pointer to v4l2_ctrl structure
 *
 * Supported controls:
 * - CSI2RX_CID_MODE_SEL
 *
 * Return: 0 if successful, error code otherwise.
 */
static int csi2rx_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct csi2rx_priv *csi2rx =
		container_of(ctrl->handler, struct csi2rx_priv, ctrl_handler);
	int ret = 0;

	switch (ctrl->id) {
	case CSI2RX_CID_MODE_SEL:
		csi2rx->cur_mode = ctrl->val;
		break;
	default:
		dev_err(csi2rx->dev, "Invalid control %d", ctrl->id);
		ret = -EINVAL;
	}

	return ret;
}

static inline struct csi2rx_priv *
v4l2_subdev_to_csi2rx(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct csi2rx_priv, subdev);
}

static const struct csi2rx_fmt *csi2rx_get_fmt_by_code(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(csi2rx_formats); i++)
		if (csi2rx_formats[i].code == code)
			return &csi2rx_formats[i];

	return NULL;
}

static int cdns_dphy_rx_band_control_select(u64 data_rate)
{
	unsigned int i;
	u64 data_rate_mbps = data_rate / 1000000;
	static const int data_rates_mbps[] = { 80,   100,  120,	 160,  200,
					       240,  280,  320,	 360,  400,
					       480,  560,  640,	 720,  800,
					       880,  1040, 1200, 1350, 1500,
					       1750, 2000, 2250, 2500 };

	static const int data_rates_mbps_num_elements =
		sizeof(data_rates_mbps) / sizeof(data_rates_mbps[0]);
	for (i = 0; i < data_rates_mbps_num_elements - 2; i++)
		if (data_rate_mbps >= data_rates_mbps[i] &&
		    data_rate_mbps < data_rates_mbps[i + 1])
			return i;
	return 0;
}

static void cdns_dphy_rx_init(struct csi2rx_priv *csi2rx)
{
	void __iomem *internal_dphy_base = csi2rx->internal_dphy_base;
	u64 data_rate = csi2rx->link_freq;
	u32 phy_band_control = 0;
	u32 clock_selection = 0;

	writel(CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_VAL,
	       internal_dphy_base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_ADDR_OFFSET);
	writel(CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_VAL,
	       internal_dphy_base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_ADDR_OFFSET);
	writel(CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_VAL,
	       internal_dphy_base +
		       CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_ADDR_OFFSET);

	clock_selection = cdns_dphy_rx_band_control_select(data_rate);
	phy_band_control =
		(clock_selection
		 << CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_R__SHIFT) |
		(clock_selection
		 << CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_L__SHIFT);

	if(csi2rx->cur_mode == CSI2RX_MODE_SDR){
		pr_debug("%s - mode sdr set dphy rate from DTS\n", __func__);
		writel(clock_selection,
			internal_dphy_base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT0_ADDR_OFFSET);
	}
	else {
		pr_debug("%s - mode hdr set dphy rate to 0x1ad\n", __func__);
		writel(0x1ad,
			internal_dphy_base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT0_ADDR_OFFSET);
	}

	dev_dbg(csi2rx->dev, "finished cdns_dphy_rx_init");
}

static void csi2rx_reset(struct csi2rx_priv *csi2rx)
{
	writel(CSI2RX_SOFT_RESET_PROTOCOL | CSI2RX_SOFT_RESET_FRONT,
	       csi2rx->base + CSI2RX_SOFT_RESET_REG);

	udelay(10);

	writel(0, csi2rx->base + CSI2RX_SOFT_RESET_REG);
}

static int csi2rx_start(struct csi2rx_priv *csi2rx)
{
	unsigned int i;
	unsigned long lanes_used = 0;
	u32 reg;
	int ret;

	// see if the mode changed before the stream
	ret = __v4l2_ctrl_handler_setup(csi2rx->subdev.ctrl_handler);
	if (ret) {
		dev_err(csi2rx->dev, "fail to setup handler");
		return ret;
	}

	ret = clk_prepare_enable(csi2rx->p_clk);
	if (ret)
		return ret;

	csi2rx_reset(csi2rx);

	if (csi2rx->has_internal_dphy) {
		writel(CSI2RX_DPHY_LANE_CTRL_RESET_LANES,
		       csi2rx->base + CSI2RX_DPHY_LANE_CTRL_REG);
		cdns_dphy_rx_init(csi2rx);
		writel(CSI2RX_DPHY_LANE_CTRL_ENABLE_LANES,
		       csi2rx->base + CSI2RX_DPHY_LANE_CTRL_REG);
	}
	reg = csi2rx->num_lanes << 8;
	for (i = 0; i < csi2rx->num_lanes; i++) {
		reg |= CSI2RX_STATIC_CFG_DLANE_MAP(i, csi2rx->lanes[i]);
		set_bit(csi2rx->lanes[i], &lanes_used);
	}

	/*
	 * Even the unused lanes need to be mapped. In order to avoid
	 * to map twice to the same physical lane, keep the lanes used
	 * in the previous loop, and only map unused physical lanes to
	 * the rest of our logical lanes.
	 */
	for (i = csi2rx->num_lanes; i < csi2rx->max_lanes; i++) {
		unsigned int idx =
			find_first_zero_bit(&lanes_used, csi2rx->max_lanes);
		set_bit(idx, &lanes_used);
		reg |= CSI2RX_STATIC_CFG_DLANE_MAP(i, i + 1);
	}
	reg |= CSI2RX_STATIC_CFG_EXTENDED_VC_EN;

	writel(reg, csi2rx->base + CSI2RX_STATIC_CFG_REG);
	ret = v4l2_subdev_call(csi2rx->source_subdev, video, s_stream, true);
	if (ret)
		goto err_disable_pclk;

	/*
	 * Create a static mapping between the CSI virtual channels
	 * and the output stream.
	 *
	 * This should be enhanced, but v4l2 lacks the support for
	 * changing that mapping dynamically.
	 *
	 * We also cannot enable and disable independent streams here,
	 * hence the reference counting.
	 */
	for (i = 0; i < csi2rx->max_streams; i++) {
		struct v4l2_mbus_framefmt *mfmt;
		const struct csi2rx_fmt *fmt;
		ret = clk_prepare_enable(csi2rx->pixel_clk[i]);
		if (ret)
			goto err_disable_pixclk;

		mfmt = &csi2rx->pad_fmts[i];
		fmt = csi2rx_get_fmt_by_code(mfmt->code);
		if (!fmt)
			goto err_disable_pixclk;

		writel(CSI2RX_STREAM_CTRL_SOFT_RESET,
		       csi2rx->base + CSI2RX_STREAM_CTRL_REG(i));
		if(csi2rx->cur_mode == CSI2RX_MODE_SDR){
			u32 csi2rx_stream_cfg_flags = CSI2RX_STREAM_CFG_FIFO_MODE_LARGE_BUF;
			csi2rx_stream_cfg_flags |= fmt->bpp == 2 ? CSI2RX_STREAM_CFG_2_PPC : 0;
			pr_debug("%s - mode sdr set fill level to 0\n", __func__);
			writel(csi2rx_stream_cfg_flags,
				csi2rx->base + CSI2RX_STREAM_CFG_REG(i));
		}
		else {
			pr_debug("%s - mode hdr set fill level to 0x%x\n", 
			__func__, CSI2RX_STREAM_CFG_FIFO_FILL_LEVEL);
			writel(CSI2RX_STREAM_CFG_FIFO_FILL_LEVEL | CSI2RX_STREAM_CFG_FIFO_MODE_LARGE_BUF,
				csi2rx->base + CSI2RX_STREAM_CFG_REG(i));
		}

		writel(CSI2RX_STREAM_DATA_CFG_DT0_PROCESS_ENABLE |
			       CSI2RX_STREAM_DATA_CFG_DT0_RAW12,
		       csi2rx->base + CSI2RX_STREAM_DATA_CFG_REG(i));
		writel(CSI2RX_STREAM_CTRL_START,
		       csi2rx->base + CSI2RX_STREAM_CTRL_REG(i));
	}

	ret = clk_prepare_enable(csi2rx->sys_clk);
	if (ret)
		goto err_disable_pixclk;

	clk_disable_unprepare(csi2rx->p_clk);
	return 0;

err_disable_pixclk:
	for (; i > 0; i--)
		clk_disable_unprepare(csi2rx->pixel_clk[i - 1]);

err_disable_pclk:
	clk_disable_unprepare(csi2rx->p_clk);

	return ret;
}

static void csi2rx_stop(struct csi2rx_priv *csi2rx)
{
	unsigned int i;
	unsigned int status_reg_val;
	unsigned int ctrl_reg_val;
	int retry;

	clk_prepare_enable(csi2rx->p_clk);
	clk_disable_unprepare(csi2rx->sys_clk);

	for (i = 0; i < csi2rx->max_streams; i++) {
		ctrl_reg_val = readl(csi2rx->base + CSI2RX_STREAM_CTRL_REG(i));
		writel(ctrl_reg_val | CSI2RX_STREAM_CTRL_STOP,
		       csi2rx->base + CSI2RX_STREAM_CTRL_REG(i));

		retry = 0;
		status_reg_val =
			readl(csi2rx->base + CSI2RX_STREAM_STATUS_REG(i));
		while ((status_reg_val & CSI2RX_STREAM_STATUS_RUNNING) > 0 &&
		       retry++ < CSI2RX_STREAM_STATUS_MAX_RETRIES) {
			msleep(CSI2RX_STREAM_STATUS_SLEEP_MSECS);
			status_reg_val = readl(csi2rx->base +
					       CSI2RX_STREAM_STATUS_REG(i));
		}

		if (retry > CSI2RX_STREAM_STATUS_MAX_RETRIES) {
			dev_err(csi2rx->dev,
				"Stream stop exceeded number of retries\n");
		}

		clk_disable_unprepare(csi2rx->pixel_clk[i]);
	}

	clk_disable_unprepare(csi2rx->p_clk);

	if (v4l2_subdev_call(csi2rx->source_subdev, video, s_stream, false))
		dev_warn(csi2rx->dev, "Couldn't disable our subdev\n");
}

static int csi2rx_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);
	int ret = 0;

	mutex_lock(&csi2rx->lock);

	if (enable) {
		/*
		 * If we're not the first users, there's no need to
		 * enable the whole controller.
		 */
		if (!csi2rx->count) {
			ret = csi2rx_start(csi2rx);
			if (ret)
				goto out;
		}

		csi2rx->count++;
	} else {
		if (csi2rx->count) {
			csi2rx->count--;
		} else {
			goto out;
		}

		/*
		 * Let the last user turn off the lights.
		 */
		if (!csi2rx->count)
			csi2rx_stop(csi2rx);
	}
out:
	mutex_unlock(&csi2rx->lock);
	return ret;
}

static int csi2rx_get_fmt(struct v4l2_subdev *subdev,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_format *fmt)
{
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);
	struct v4l2_mbus_framefmt *src_format;
	struct v4l2_mbus_framefmt *dst_format;
	if (!csi2rx || !fmt || fmt->pad >= CSI2RX_PAD_MAX)
		return -EINVAL;
	
	src_format = &csi2rx->pad_fmts[fmt->pad];
	dst_format = &fmt->format;
	if (!src_format || !dst_format)
		return -EINVAL;
	
	*dst_format = *src_format;
	return 0;
}

static int csi2rx_set_fmt(struct v4l2_subdev *subdev,
		struct v4l2_subdev_state *state,
		struct v4l2_subdev_format *fmt)
{
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);
	const struct v4l2_mbus_framefmt *src_format = &fmt->format;
	struct v4l2_mbus_framefmt *dst_format;

	if (!csi2rx_get_fmt_by_code(fmt->format.code)) {
		dev_err(csi2rx->dev, "Unsupported media bus format: 0x%x\n",
			fmt->format.code);
		return -EINVAL;
	}
	
	dst_format = &csi2rx->pad_fmts[fmt->pad];
	if (!dst_format)
		return -EINVAL;

	*dst_format = *src_format;
	return 0;
}

static const struct v4l2_subdev_video_ops csi2rx_video_ops = {
	.s_stream = csi2rx_s_stream,
};

static const struct v4l2_subdev_pad_ops csi2rx_pad_ops = {
	.get_fmt               = csi2rx_get_fmt,
	.set_fmt               = csi2rx_set_fmt,
};

static const struct v4l2_subdev_ops csi2rx_subdev_ops = {
	.video = &csi2rx_video_ops,
	.pad = &csi2rx_pad_ops,
};

static int csi2rx_async_bound(struct v4l2_async_notifier *notifier,
			      struct v4l2_subdev *s_subdev,
			      struct v4l2_async_subdev *asd)
{
	struct v4l2_subdev *subdev = notifier->sd;
	struct csi2rx_priv *csi2rx = v4l2_subdev_to_csi2rx(subdev);

	csi2rx->source_pad = media_entity_get_fwnode_pad(
		&s_subdev->entity, s_subdev->fwnode, MEDIA_PAD_FL_SOURCE);
	if (csi2rx->source_pad < 0) {
		dev_err(csi2rx->dev, "Couldn't find output pad for subdev %s\n",
			s_subdev->name);
		return csi2rx->source_pad;
	}

	csi2rx->source_subdev = s_subdev;

	dev_dbg(csi2rx->dev, "Bound %s pad: %d\n", s_subdev->name,
		csi2rx->source_pad);

	return media_create_pad_link(
		&csi2rx->source_subdev->entity, csi2rx->source_pad,
		&csi2rx->subdev.entity, 0,
		MEDIA_LNK_FL_ENABLED | MEDIA_LNK_FL_IMMUTABLE);
}

static const struct v4l2_async_notifier_operations csi2rx_notifier_ops = {
	.bound = csi2rx_async_bound,
};

static int csi2rx_get_resources(struct csi2rx_priv *csi2rx,
				struct platform_device *pdev)
{
	struct resource *res;
	struct platform_device *internal_dphy_pdev;
	unsigned char i;
	u32 dev_cfg;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	csi2rx->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(csi2rx->base))
		return PTR_ERR(csi2rx->base);

	csi2rx->sys_clk = devm_clk_get(&pdev->dev, "sys_clk");
	if (IS_ERR(csi2rx->sys_clk)) {
		dev_err(&pdev->dev, "Couldn't get sys clock\n");
		return PTR_ERR(csi2rx->sys_clk);
	}

	csi2rx->p_clk = devm_clk_get(&pdev->dev, "p_clk");
	if (IS_ERR(csi2rx->p_clk)) {
		dev_err(&pdev->dev, "Couldn't get P clock\n");
		return PTR_ERR(csi2rx->p_clk);
	}

	csi2rx->dphy = devm_phy_optional_get(&pdev->dev, "dphy");
	if (IS_ERR(csi2rx->dphy)) {
		dev_err(&pdev->dev, "Couldn't get external D-PHY\n");
		return PTR_ERR(csi2rx->dphy);
	}

	/*
	 * FIXME: Once we'll have external D-PHY support, the check
	 * will need to be removed.
	 */
	if (csi2rx->dphy) {
		dev_err(&pdev->dev, "External D-PHY not supported yet\n");
		return -EINVAL;
	}

	ret = clk_prepare_enable(csi2rx->p_clk);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't prepare and enable P clock\n");
		return ret;
	}

	dev_cfg = readl(csi2rx->base + CSI2RX_DEVICE_CFG_REG);
	clk_disable_unprepare(csi2rx->p_clk);

	csi2rx->max_lanes = dev_cfg & 7;
	if (csi2rx->max_lanes > CSI2RX_LANES_MAX) {
		dev_err(&pdev->dev, "Invalid number of lanes: %u\n",
			csi2rx->max_lanes);
		return -EINVAL;
	}

	csi2rx->max_streams = (dev_cfg >> 4) & 7;
	if (csi2rx->max_streams > CSI2RX_STREAMS_MAX) {
		dev_err(&pdev->dev, "Invalid number of streams: %u\n",
			csi2rx->max_streams);
		return -EINVAL;
	}

	csi2rx->has_internal_dphy = dev_cfg & BIT(3) ? true : false;

	if (csi2rx->has_internal_dphy) {
		csi2rx->internal_dphy =
			of_get_child_by_name(pdev->dev.of_node, "dphy");
		if (csi2rx->internal_dphy) {
			internal_dphy_pdev = of_device_alloc(
				csi2rx->internal_dphy, NULL, &pdev->dev);
			if (!internal_dphy_pdev) {
				dev_err(&pdev->dev, "of_device_alloc failed\n");
				return PTR_ERR(internal_dphy_pdev);
			}
			res = platform_get_resource(internal_dphy_pdev,
						    IORESOURCE_MEM, 0);
			csi2rx->internal_dphy_base = devm_ioremap_resource(
				&internal_dphy_pdev->dev, res);
			if (IS_ERR(csi2rx->internal_dphy_base)) {
				dev_err(&pdev->dev,
					"devm_ioremap_resource failed\n");
				return PTR_ERR(csi2rx->internal_dphy_base);
			}

			of_node_put(csi2rx->internal_dphy);
			kfree(internal_dphy_pdev);
		}
	}

	for (i = 0; i < csi2rx->max_streams; i++) {
		char clk_name[16];

		snprintf(clk_name, sizeof(clk_name), "pixel_if%u_clk", i);
		csi2rx->pixel_clk[i] = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(csi2rx->pixel_clk[i])) {
			dev_err(&pdev->dev, "Couldn't get clock %s\n",
				clk_name);
			return PTR_ERR(csi2rx->pixel_clk[i]);
		}
	}

	return 0;
}

static void csi2rx_parse_v4l2_remote_ep(struct csi2rx_priv *csi2rx)
{
	struct v4l2_fwnode_endpoint v4l2_ep = { .bus_type = 0 };
	struct fwnode_handle *fwh;
	struct device_node *ep;
	struct fwnode_handle *remote_ep;
	int ret;

	ep = of_graph_get_endpoint_by_regs(csi2rx->dev->of_node, 0, 0);
	if (!ep)
		return;
	fwh = of_fwnode_handle(ep);
	remote_ep = fwnode_graph_get_remote_endpoint(fwh);
	ret = v4l2_fwnode_endpoint_alloc_parse(remote_ep, &v4l2_ep);
	if (ret) {
		dev_info(csi2rx->dev, "Could not parse v4l2 remote endpoint\n");
		of_node_put(ep);
		v4l2_fwnode_endpoint_free(&v4l2_ep);
		return;
	}
	if (v4l2_ep.nr_of_link_frequencies > 1) {
		dev_err(csi2rx->dev,
			"No support for multiple link frequencies yet");
		return;
	}
	csi2rx->link_freq = v4l2_ep.link_frequencies[0];
	of_node_put(ep);
	v4l2_fwnode_endpoint_free(&v4l2_ep);
}

static int csi2rx_parse_dt(struct csi2rx_priv *csi2rx)
{
	struct v4l2_fwnode_endpoint v4l2_ep = { .bus_type = 0 };
	struct v4l2_async_subdev *asd;
	struct fwnode_handle *fwh;
	struct device_node *ep;
	int ret;

	ep = of_graph_get_endpoint_by_regs(csi2rx->dev->of_node, 0, 0);
	if (!ep)
		return -EINVAL;

	fwh = of_fwnode_handle(ep);
	ret = v4l2_fwnode_endpoint_parse(fwh, &v4l2_ep);
	if (ret) {
		dev_err(csi2rx->dev, "Could not parse v4l2 endpoint\n");
		of_node_put(ep);
		return ret;
	}

	if (v4l2_ep.bus_type != V4L2_MBUS_CSI2_DPHY) {
		dev_err(csi2rx->dev, "Unsupported media bus type: 0x%x\n",
			v4l2_ep.bus_type);
		of_node_put(ep);
		return -EINVAL;
	}

	memcpy(csi2rx->lanes, v4l2_ep.bus.mipi_csi2.data_lanes,
	       sizeof(csi2rx->lanes));
	csi2rx->num_lanes = v4l2_ep.bus.mipi_csi2.num_data_lanes;
	if (csi2rx->num_lanes > csi2rx->max_lanes) {
		dev_err(csi2rx->dev, "Unsupported number of data-lanes: %d\n",
			csi2rx->num_lanes);
		of_node_put(ep);
		return -EINVAL;
	}

	csi2rx_parse_v4l2_remote_ep(csi2rx);

	v4l2_async_notifier_init(&csi2rx->notifier);

	asd = v4l2_async_notifier_add_fwnode_remote_subdev(
		&csi2rx->notifier, fwh, struct v4l2_async_subdev);
	of_node_put(ep);
	if (IS_ERR(asd))
		return PTR_ERR(asd);

	csi2rx->notifier.ops = &csi2rx_notifier_ops;

	ret = v4l2_async_subdev_notifier_register(&csi2rx->subdev,
						  &csi2rx->notifier);
	if (ret)
		v4l2_async_notifier_cleanup(&csi2rx->notifier);

	return ret;
}

/**
 * csi2rx_init_controls() - Initialize sensor subdevice controls
 * @csi2rx: pointer to csi2rx device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int csi2rx_init_controls(struct csi2rx_priv *csi2rx)
{
	struct v4l2_ctrl_handler *ctrl_hdlr = &csi2rx->ctrl_handler;
	int ret;

	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 1);
	if (ret)
		return ret;

	/* Serialize controls with sensor device */
	ctrl_hdlr->lock = &csi2rx->lock;
	
	csi2rx->mode_sel_ctrl = 
		v4l2_ctrl_new_custom(ctrl_hdlr, &csi2rx_mode_sel_ctrl_cfg, NULL);

	if (ctrl_hdlr->error) {
		dev_err(csi2rx->dev, "control init failed: %d",
			ctrl_hdlr->error);
		v4l2_ctrl_handler_free(ctrl_hdlr);
		return ctrl_hdlr->error;
	}

	csi2rx->subdev.ctrl_handler = ctrl_hdlr;

	return 0;
}

static int csi2rx_probe(struct platform_device *pdev)
{
	struct csi2rx_priv *csi2rx;
	unsigned int i;
	int ret;

	csi2rx = kzalloc(sizeof(*csi2rx), GFP_KERNEL);
	if (!csi2rx)
		return -ENOMEM;
	platform_set_drvdata(pdev, csi2rx);
	csi2rx->dev = &pdev->dev;
	mutex_init(&csi2rx->lock);

	ret = csi2rx_get_resources(csi2rx, pdev);
	if (ret)
		goto err_free_priv;

	ret = csi2rx_parse_dt(csi2rx);
	if (ret)
		goto err_free_priv;

	csi2rx->subdev.owner = THIS_MODULE;
	csi2rx->subdev.dev = &pdev->dev;
	v4l2_subdev_init(&csi2rx->subdev, &csi2rx_subdev_ops);
	v4l2_set_subdevdata(&csi2rx->subdev, &pdev->dev);
	snprintf(csi2rx->subdev.name, V4L2_SUBDEV_NAME_SIZE, "%s.%s",
		 KBUILD_MODNAME, dev_name(&pdev->dev));

	/* Create our media pads */
	csi2rx->subdev.entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	csi2rx->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	csi2rx->pads[CSI2RX_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	for (i = CSI2RX_PAD_SOURCE_STREAM0; i < CSI2RX_PAD_MAX; i++)
		csi2rx->pads[i].flags = MEDIA_PAD_FL_SOURCE;
	
	for (i = CSI2RX_PAD_SOURCE_STREAM0; i < CSI2RX_PAD_MAX; i++)
		csi2rx->pad_fmts[i] = fmt_default;

	ret = media_entity_pads_init(&csi2rx->subdev.entity, CSI2RX_PAD_MAX,
				     csi2rx->pads);
	if (ret)
		goto err_cleanup;

	csi2rx->cur_mode = CSI2RX_MODE_SDR;
	ret = csi2rx_init_controls(csi2rx);
	if (ret) {
		dev_err(csi2rx->dev, "failed to init controls: %d", ret);
		goto err_cleanup;
	}

	ret = v4l2_async_register_subdev(&csi2rx->subdev);
	if (ret < 0)
		goto err_cleanup;

	dev_info(
		&pdev->dev,
		"Probed CSI2RX with %u/%u lanes, %u streams, %s D-PHY with link frequency of %llu bps\n",
		csi2rx->num_lanes, csi2rx->max_lanes, csi2rx->max_streams,
		csi2rx->has_internal_dphy ? "internal" : "no",
		csi2rx->link_freq);

	return 0;

err_cleanup:
	v4l2_async_notifier_cleanup(&csi2rx->notifier);
err_free_priv:
	kfree(csi2rx);
	return ret;
}

static int csi2rx_remove(struct platform_device *pdev)
{
	struct csi2rx_priv *csi2rx = platform_get_drvdata(pdev);

	v4l2_async_unregister_subdev(&csi2rx->subdev);
	kfree(csi2rx);

	return 0;
}

static const struct of_device_id csi2rx_of_table[] = {
	{ .compatible = "cdns,csi2rx" },
	{},
};
MODULE_DEVICE_TABLE(of, csi2rx_of_table);

static struct platform_driver csi2rx_driver = {
	.probe	= csi2rx_probe,
	.remove	= csi2rx_remove,

	.driver	= {
		.name		= "cdns-csi2rx",
		.of_match_table	= csi2rx_of_table,
	},
};
module_platform_driver(csi2rx_driver);
MODULE_AUTHOR("Maxime Ripard <maxime.ripard@bootlin.com>");
MODULE_DESCRIPTION("Cadence CSI2-RX controller");
MODULE_LICENSE("GPL");
