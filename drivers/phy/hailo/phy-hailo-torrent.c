// SPDX-License-Identifier: GPL-2.0
/**
 * Wrapper driver for Cadence Torrent Multi-Protocol PHY used in Hailo-15 SoC for Pcie and USB3
 *
 * Copyright (c) 2019-2026 Hailo Technologies Ltd. All rights reserved.
 */

#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <dt-bindings/phy/phy-hailo-torrent.h>

#define NO_USB_LANE (-1)

#define PCI_PHY_LINK_LANES_CFG_MAX PCI_PHY_LINK_LANES_CFG__4x1__MASTER_LANES_LN0_LN1_LN2_LN4
#define PCI_PMA_PLL_FULL_RATE_CLK_DIVIDER_MAX PCI_PMA_PLL_FULL_RATE_CLK_DIVIDER_8


#define PCIE_PHY_LANE_MODE__PCIE 0
#define PCIE_PHY_LANE_MODE__USB 1

/* pcie config registers::pcie config */
#define  PHY_MODE_LN_0_SHIFT 8
#define  PHY_MODE_LN_0_MASK 0x300
#define  PHY_MODE_LN_1_SHIFT 10
#define  PHY_MODE_LN_1_MASK 0xC00
// TODO Not relevant to 15L
#define  PHY_MODE_LN_2_SHIFT 12
#define  PHY_MODE_LN_2_MASK 0x3000
#define  PHY_MODE_LN_3_SHIFT 14
#define  PHY_MODE_LN_3_MASK 0xC000

/* pcie config registers::phy_constant_zero_value_for_debug_0 */
#define  PMA_FULLRT_DIV_LN_0_MASK 0xC00
#define  PMA_FULLRT_DIV_LN_0_SHIFT 10
#define  PHY_LINK_CFG_LN_1_SHIFT 23
#define  PHY_LINK_CFG_LN_1_MASK 0x800000
#define  PMA_FULLRT_DIV_LN_1_MASK 0x3000000
#define  PMA_FULLRT_DIV_LN_1_SHIFT 24

/* pcie config registers::phy_constant_zero_value_for_debug_1 */
#define  PHY_LINK_CFG_LN_2_SHIFT 5
#define  PHY_LINK_CFG_LN_2_MASK 0x20
#define  PMA_FULLRT_DIV_LN_2_MASK 0xC0
#define  PMA_FULLRT_DIV_LN_2_SHIFT 6
#define  PHY_LINK_CFG_LN_3_SHIFT 19
#define  PHY_LINK_CFG_LN_3_MASK 0x80000
#define  PMA_FULLRT_DIV_LN_3_MASK 0x300000
#define  PMA_FULLRT_DIV_LN_3_SHIFT 20

/* pcie config registers::pcie_config */
#define  PHY_RESET_N_EN_SHIFT 10
#define  PHY_RESET_N_EN_MASK 0x400
#define  PHY_RESET_N_VAL_SHIFT 11
#define  PHY_RESET_N_VAL_MASK 0x800

/* usb config registers */
#define  USB_PIPE_DMUX_SEL_SHIFT 0
#define  USB_PIPE_DMUX_SEL_MASK 1
#define  USB_RX_PIPE_MUX_SEL_SHIFT 1
#define  USB_RX_PIPE_MUX_SEL_MASK 2
#define  PHY_PIPE_LANES01_MUX_SEL_SHIFT 4
#define  PHY_PIPE_LANES01_MUX_SEL_MASK 0x10
#define  PHY_PIPE_LANES23_MUX_SEL_SHIFT 5
#define  PHY_PIPE_LANES23_MUX_SEL_MASK 0x20


struct usb_config_regs_offsets {
	u32 usb_pcie_pipe_mux_cfg;
};
struct pcie_config_regs_offsets {
	u32 pcie_cfg;
	u32 phy_constant_zero_value_for_debug_0;
	u32 phy_constant_zero_value_for_debug_1;
	u32 pcie_cfg_bypass;
};

struct hailo_torrent {
	void __iomem *usb_config;
	void __iomem *pcie_config;
	struct clk *usb_pclk;
	struct clk *pcie_pclk;
	struct clk *pcie_aclk;
	struct reset_control *pcie_rst;
	struct reset_control *pcie_apb_rst;
	int usb_lane;
	u32 lanes_cfg;
	u32 usb_lane_pma_pll_full_rate_divider;
	struct pcie_config_regs_offsets *pcie_config_regs_offsets;
	struct usb_config_regs_offsets *usb_config_regs_offsets;
	void (*usb_pcie_pipe_mux_cfg)(struct hailo_torrent *params);
};

struct hailo_matched_data {
	int max_usb_lanes;
	struct usb_config_regs_offsets usb_config_regs_offsets;
	struct pcie_config_regs_offsets pcie_config_regs_offsets;
	void (*usb_pcie_pipe_mux_cfg)(struct hailo_torrent *params);
};

static inline u32 hailo_torrent_pcie_config_readl(struct hailo_torrent *data, u32 offset)
{
	return readl(data->pcie_config + offset);
}

static inline void hailo_torrent_pcie_config_writel(struct hailo_torrent *data, u32 offset, u32 value)
{
	writel(value, data->pcie_config + offset);
}

static inline u32 hailo_torrent_usb_config_readl(struct hailo_torrent *data, u32 offset)
{
	return readl(data->usb_config + offset);
}

static inline void hailo_torrent_usb_config_writel(struct hailo_torrent *data, u32 offset, u32 value)
{
	writel(value, data->usb_config + offset);
}

static int pcie_pma_lane_full_rate_clk_divider_cfg(struct hailo_torrent *data, int lane, u32 divider)
{
	u32 offset, mask, shift, value;

	switch(lane) {
	case 0:
		offset = data->pcie_config_regs_offsets->phy_constant_zero_value_for_debug_0;
		mask = PMA_FULLRT_DIV_LN_0_MASK;
		shift = PMA_FULLRT_DIV_LN_0_SHIFT;
		break;
	case 1:
		offset = data->pcie_config_regs_offsets->phy_constant_zero_value_for_debug_0;
		mask = PMA_FULLRT_DIV_LN_1_MASK;
		shift = PMA_FULLRT_DIV_LN_1_SHIFT;
		break;
	case 2:
		offset = data->pcie_config_regs_offsets->phy_constant_zero_value_for_debug_1;
		mask = PMA_FULLRT_DIV_LN_2_MASK;
		shift = PMA_FULLRT_DIV_LN_2_SHIFT;
		break;
	case 3:
		offset = data->pcie_config_regs_offsets->phy_constant_zero_value_for_debug_1;
		mask = PMA_FULLRT_DIV_LN_3_MASK;
		shift = PMA_FULLRT_DIV_LN_3_SHIFT;
		break;
	default:
		pr_err("phy-hailo-torrent: invalid pma lane %d\n", lane);
		return -EINVAL;
	}

	value = hailo_torrent_pcie_config_readl(data, offset);
	value &= ~mask;
	value |= divider << shift;
	hailo_torrent_pcie_config_writel(data, offset, value);

	return 0;
}

static void pcie_phy_link_lanes_cfg(struct hailo_torrent *data)
{
	u32 value;
	u32 phy_constant_zero_value_for_debug_0_offset = data->pcie_config_regs_offsets->phy_constant_zero_value_for_debug_0;
	u32 phy_constant_zero_value_for_debug_1_offset = data->pcie_config_regs_offsets->phy_constant_zero_value_for_debug_1;
	u8 cfg_ln_1, cfg_ln_2, cfg_ln_3;

	cfg_ln_1 = (data->lanes_cfg >> 0) & 1;
	cfg_ln_2 = (data->lanes_cfg >> 1) & 1;
	cfg_ln_3 = (data->lanes_cfg >> 2) & 1;

	value = hailo_torrent_pcie_config_readl(data, phy_constant_zero_value_for_debug_0_offset);
	value &= ~PHY_LINK_CFG_LN_1_MASK;
	value |= cfg_ln_1 << PHY_LINK_CFG_LN_1_SHIFT;
	hailo_torrent_pcie_config_writel(data, phy_constant_zero_value_for_debug_0_offset, value);

	value = hailo_torrent_pcie_config_readl(data, phy_constant_zero_value_for_debug_1_offset);
	value &= ~PHY_LINK_CFG_LN_2_MASK;
	value |= cfg_ln_2 << PHY_LINK_CFG_LN_2_SHIFT;
	hailo_torrent_pcie_config_writel(data, phy_constant_zero_value_for_debug_1_offset, value);

	value = hailo_torrent_pcie_config_readl(data, phy_constant_zero_value_for_debug_1_offset);
	value &= ~PHY_LINK_CFG_LN_3_MASK;
	value |= cfg_ln_3 << PHY_LINK_CFG_LN_3_SHIFT;
	hailo_torrent_pcie_config_writel(data, phy_constant_zero_value_for_debug_1_offset, value);
}

static void pcie_phy_bypass_reset_setup(struct hailo_torrent *data)
{
	u32 value;
	u32 pcie_cfg_bypass_offset = data->pcie_config_regs_offsets->pcie_cfg_bypass;

	/*
	 *                              __
	 *                             |  \
	 *   PERSTN (0)   ----- AND----|(0)|     PCIE PHY
	 *   MPERST_MASK  -----/       |MUX|---[phy_reset_n]
	 *   BYPASS_VAL   -------------|(1)|
	 *                             |__/
	 *                              |
	 *                          BYPASS_EN
	 */
	value = hailo_torrent_pcie_config_readl(data, pcie_cfg_bypass_offset);
	value &= ~PHY_RESET_N_VAL_MASK;
	value |= PHY_RESET_N_EN_MASK;
	hailo_torrent_pcie_config_writel(data, pcie_cfg_bypass_offset, value);
}

// usb_lane may be -1 to indicate no USB lane
static void pcie_phy_lanes_mode_cfg(struct hailo_torrent *data)
{
	u32 value;
	u32 pcie_cfg_offset = data->pcie_config_regs_offsets->pcie_cfg;
	u8 lane_0_mode = PCIE_PHY_LANE_MODE__PCIE;
	u8 lane_1_mode = PCIE_PHY_LANE_MODE__PCIE;
	u8 lane_2_mode = PCIE_PHY_LANE_MODE__PCIE;
	u8 lane_3_mode = PCIE_PHY_LANE_MODE__PCIE;

	switch(data->usb_lane) {
	case 0:
		lane_0_mode = PCIE_PHY_LANE_MODE__USB;
		break;
	case 1:
		lane_1_mode = PCIE_PHY_LANE_MODE__USB;
		break;
	case 2:
		lane_2_mode = PCIE_PHY_LANE_MODE__USB;
		break;
	case 3:
		lane_3_mode = PCIE_PHY_LANE_MODE__USB;
		break;
	}

	value = hailo_torrent_pcie_config_readl(data, pcie_cfg_offset);
	value &= ~PHY_MODE_LN_0_MASK;
	value |= lane_0_mode << PHY_MODE_LN_0_SHIFT;
	value &= ~PHY_MODE_LN_1_MASK;
	value |= lane_1_mode << PHY_MODE_LN_1_SHIFT;
	value &= ~PHY_MODE_LN_2_MASK;
	value |= lane_2_mode << PHY_MODE_LN_2_SHIFT;
	value &= ~PHY_MODE_LN_3_MASK;
	value |= lane_3_mode << PHY_MODE_LN_3_SHIFT;
	hailo_torrent_pcie_config_writel(data, pcie_cfg_offset, value);
}

static void hailo15l_usb_pcie_pipe_mux_cfg(struct hailo_torrent *data)
{
	u32 value;
	u32 usb_pcie_pipe_mux_cfg_offset = data->usb_config_regs_offsets->usb_pcie_pipe_mux_cfg;

	value = hailo_torrent_usb_config_readl(data, usb_pcie_pipe_mux_cfg_offset);
	switch (data->usb_lane) {
	case 1:
		value |= USB_RX_PIPE_MUX_SEL_MASK;
		value |= PHY_PIPE_LANES23_MUX_SEL_MASK;
		break;
	case 0:
		value &= ~USB_RX_PIPE_MUX_SEL_MASK;
		value |= PHY_PIPE_LANES01_MUX_SEL_MASK;
		break;
	}
	hailo_torrent_usb_config_writel(data, usb_pcie_pipe_mux_cfg_offset, value);
}

static void hailo15_usb_pcie_pipe_mux_cfg(struct hailo_torrent *data)
{
	u32 value;
	u32 usb_pcie_pipe_mux_cfg_offset = data->usb_config_regs_offsets->usb_pcie_pipe_mux_cfg;

	value = hailo_torrent_usb_config_readl(data, usb_pcie_pipe_mux_cfg_offset);
	switch (data->usb_lane) {
	case 3:
		value |= USB_RX_PIPE_MUX_SEL_MASK;
		value |= USB_PIPE_DMUX_SEL_MASK;
		value |= PHY_PIPE_LANES23_MUX_SEL_MASK;
		break;
	case 2:
		value |= USB_RX_PIPE_MUX_SEL_MASK;
		value &= ~USB_PIPE_DMUX_SEL_MASK;
		value |= PHY_PIPE_LANES23_MUX_SEL_MASK;
		break;
	case 1:
		value &= ~USB_RX_PIPE_MUX_SEL_MASK;
		value |= USB_PIPE_DMUX_SEL_MASK;
		value &= ~PHY_PIPE_LANES01_MUX_SEL_MASK;
		break;
	case 0:
		value &= ~USB_RX_PIPE_MUX_SEL_MASK;
		value &= ~USB_PIPE_DMUX_SEL_MASK;
		value &= ~PHY_PIPE_LANES01_MUX_SEL_MASK;
		break;
	}
	hailo_torrent_usb_config_writel(data, usb_pcie_pipe_mux_cfg_offset, value);
}



int hailo_torrent_init(struct hailo_torrent *data)
{
	int ret;

	if (data->usb_lane != NO_USB_LANE) {
		ret = pcie_pma_lane_full_rate_clk_divider_cfg(data, data->usb_lane, data->usb_lane_pma_pll_full_rate_divider);
		if (ret)
			return ret;
	}
	pcie_phy_link_lanes_cfg(data);
	pcie_phy_bypass_reset_setup(data);
	pcie_phy_lanes_mode_cfg(data);
	data->usb_pcie_pipe_mux_cfg(data);

	return 0;
}

static const struct of_dev_auxdata hailo_torrent_auxdata[] = {
	{
		.compatible = "cdns,torrent-phy"
	},
	{},
};

static int hailo_torrent_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct hailo_torrent *data;
	struct resource *usb_res, *pcie_res;
	struct hailo_matched_data *match;
	int ret;

	if (!node)
		return -ENODEV;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);

	/* Must executed after dev_set_drvdata(...) */
	match = (struct hailo_matched_data*)of_device_get_match_data(&pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "unexpected device type\n");
		return -ENODEV;
	}
	data->pcie_config_regs_offsets = &match->pcie_config_regs_offsets;
	data->usb_config_regs_offsets = &match->usb_config_regs_offsets;
	data->usb_pcie_pipe_mux_cfg = match->usb_pcie_pipe_mux_cfg;
	usb_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "usb-config");
	if (!usb_res) {
		dev_err(dev, "can't get IOMEM usb config resource\n");
		return -ENXIO;
	}
	/* the usb config is shared with the torrent PHY wrapper driver, so therefore
	   we can't use devm_platform_ioremap_resource() */
	data->usb_config = devm_ioremap(&pdev->dev, usb_res->start, resource_size(usb_res));
	if (!data->usb_config) {
		dev_err(dev, "can't map IOMEM usb config resource\n");
		return -ENOMEM;
	}

	pcie_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie-config");
	if (!pcie_res) {
		dev_err(dev, "can't get IOMEM pcie config resource\n");
		return -ENXIO;
	}
	/* the pcie config is shared with the pcie endpoint driver, so therefore
	   we can't use devm_platform_ioremap_resource() */
	data->pcie_config = devm_ioremap(&pdev->dev, pcie_res->start, resource_size(pcie_res));
	if (!data->pcie_config) {
		dev_err(dev, "can't map IOMEM pcie config resource\n");
		return -ENOMEM;
	}

	if (of_property_read_u32(pdev->dev.of_node, "lanes-config", &data->lanes_cfg)) {
		dev_err(dev, "lanes-config property not found\n");
		return -EINVAL;
	}
	if (data->lanes_cfg > PCI_PHY_LINK_LANES_CFG_MAX) {
		dev_err(dev, "invalid lanes-config %u", data->lanes_cfg);
		return -EINVAL;
	}

	if (of_property_read_u32(pdev->dev.of_node, "usb-lane-pma-pll-full-rate-divider", &data->usb_lane_pma_pll_full_rate_divider)) {
		dev_err(dev, "usb-lane-pma-pll-full-rate-divider property not found\n");
		return -EINVAL;
	}
	if (data->usb_lane_pma_pll_full_rate_divider > PCI_PMA_PLL_FULL_RATE_CLK_DIVIDER_MAX) {
		dev_err(dev, "invalid usb-lane-pma-pll-full-rate-divider %u", data->usb_lane_pma_pll_full_rate_divider);
		return -EINVAL;
	}

	if (of_property_read_u32(pdev->dev.of_node, "usb-lane", (u32 *)&data->usb_lane)) {
		/* By default assume no usb lane */
		data->usb_lane = NO_USB_LANE;
	}
	if (data->usb_lane >= match->max_usb_lanes) {
		dev_err(dev, "invalid usb lane %u", data->usb_lane);
		return -EINVAL;
	}

	data->pcie_rst = devm_reset_control_get(&pdev->dev, "pcie");
	if (IS_ERR(data->pcie_rst)) {
		dev_err(&pdev->dev, "Failed to get reset control on pcie\n");
		return PTR_ERR(data->pcie_rst);
	}

	data->pcie_apb_rst = devm_reset_control_get(&pdev->dev, "pcie_apb");
	if (IS_ERR(data->pcie_apb_rst)) {
		dev_err(&pdev->dev, "Failed to get reset control on pcie_apb\n");
		return PTR_ERR(data->pcie_apb_rst);
	}

	/* Note: reset deassert order matters: 1-pcie, 2-pcie_apb !!! */
	reset_control_deassert(data->pcie_rst);
	reset_control_deassert(data->pcie_apb_rst);

	data->usb_pclk = devm_clk_get(dev, "usb_pclk");
	if (IS_ERR(data->usb_pclk))
		return PTR_ERR(data->usb_pclk);

	data->pcie_aclk = devm_clk_get(dev, "pcie_aclk");
	if (IS_ERR(data->pcie_aclk))
		return PTR_ERR(data->pcie_aclk);

	data->pcie_pclk = devm_clk_get(dev, "pcie_pclk");
	if (IS_ERR(data->pcie_pclk))
		return PTR_ERR(data->pcie_pclk);

	pm_runtime_get_sync(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	ret = clk_prepare_enable(data->usb_pclk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(data->pcie_aclk);
	if (ret)
		goto err_disable_usb_pclk;

	ret = clk_prepare_enable(data->pcie_pclk);
	if (ret)
		goto err_disable_pcie_aclk_usb_pclk;

	ret = hailo_torrent_init(data);
	if (ret) {
		dev_err(dev, "failed to initialize torrent phy: %d\n", ret);
		goto err_disable_pcie_clks_and_usb;
	}

	clk_disable_unprepare(data->usb_pclk);

	dev_info(dev, "Torrent phy wrapper initialized successfully (usb in lane %d, lanes config %d, divider %d)",
		data->usb_lane,
		data->lanes_cfg,
		data->usb_lane_pma_pll_full_rate_divider);

	ret = of_platform_populate(node, NULL, hailo_torrent_auxdata, dev);
	if (ret) {
		dev_err(dev, "failed to create children: %d\n", ret);
		goto err_disable_pcie_clks;
	}

	return ret;

err_disable_pcie_clks:
	clk_disable_unprepare(data->pcie_pclk);
	clk_disable_unprepare(data->pcie_aclk);
	return ret;
err_disable_pcie_clks_and_usb:
	clk_disable_unprepare(data->pcie_pclk);
err_disable_pcie_aclk_usb_pclk:
	clk_disable_unprepare(data->pcie_aclk);
err_disable_usb_pclk:
	clk_disable_unprepare(data->usb_pclk);
	return ret;
}

static int hailo_torrent_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hailo_torrent *data = dev_get_drvdata(dev);

	of_platform_depopulate(dev);
	clk_disable_unprepare(data->pcie_pclk);
	clk_disable_unprepare(data->pcie_aclk);
	reset_control_assert(data->pcie_apb_rst);
	reset_control_assert(data->pcie_rst);
	pm_runtime_put_sync(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_disable(dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int hailo_torrent_resume(struct device *dev)
{
	int ret;
	struct hailo_torrent *data = dev_get_drvdata(dev);

	ret = clk_prepare_enable(data->pcie_pclk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(data->pcie_aclk);
	if (ret)
		clk_disable_unprepare(data->pcie_pclk);

	return ret;
}

static int hailo_torrent_suspend(struct device *dev)
{
	struct hailo_torrent *data = dev_get_drvdata(dev);

	clk_disable_unprepare(data->pcie_pclk);
	clk_disable_unprepare(data->pcie_aclk);

	return 0;
}
#endif

static const struct dev_pm_ops hailo_torrent_pm_ops = {
	SET_RUNTIME_PM_OPS(hailo_torrent_suspend, hailo_torrent_resume, NULL)
};

static struct hailo_matched_data hailo15_matched_data = {
	.max_usb_lanes = 4,
	.pcie_config_regs_offsets = {
		.pcie_cfg = 0x9DC,
		.phy_constant_zero_value_for_debug_0 = 0x91C,
		.phy_constant_zero_value_for_debug_1 = 0x920,
		.pcie_cfg_bypass = 0x950,
	},
	.usb_config_regs_offsets = {
		.usb_pcie_pipe_mux_cfg = 0x5C,
	},
	.usb_pcie_pipe_mux_cfg = hailo15_usb_pcie_pipe_mux_cfg,
};

static struct hailo_matched_data hailo15l_matched_data = {
	.max_usb_lanes = 2,
	.pcie_config_regs_offsets = {
		.pcie_cfg = 0x9D4,
		.phy_constant_zero_value_for_debug_0 = 0x914,
		.phy_constant_zero_value_for_debug_1 = 0x918,
		.pcie_cfg_bypass = 0x948,
	},
	.usb_config_regs_offsets = {
		.usb_pcie_pipe_mux_cfg = 0x5C,
	},
	.usb_pcie_pipe_mux_cfg = hailo15l_usb_pcie_pipe_mux_cfg,
};

static const struct of_device_id hailo_torrent_of_match[] = {
	{ .compatible = "hailo,torrent-phy", .data = (void*)&hailo15_matched_data},
	{ .compatible = "hailo15,torrent-phy", .data = (void*)&hailo15_matched_data},
	{ .compatible = "hailo15l,torrent-phy", .data = (void*)&hailo15l_matched_data},
	{},
};
MODULE_DEVICE_TABLE(of, hailo_torrent_of_match);

static struct platform_driver hailo_torrent_driver = {
	.probe		= hailo_torrent_probe,
	.remove		= hailo_torrent_remove,
	.driver		= {
		.name	= "phy-hailo-torrent",
		.of_match_table	= of_match_ptr(hailo_torrent_of_match),
		.pm	= &hailo_torrent_pm_ops,
	},
};
module_platform_driver(hailo_torrent_driver);

MODULE_ALIAS("platform:phy-hailo-torrent");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Cadence Torrent Multi-Protocol PHY wrapper driver for Hailo-15 SoC");
