// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Hailo D-PHY
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved. 
 */


#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include "hailo15-dphy.h"

#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT0_ADDR_OFFSET (0xb00)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_ADDR_OFFSET (0xb08)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_VAL (0xaaaaaaaa)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_ADDR_OFFSET (0xb0c)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_VAL (0x2aa)
#define CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_ADDR_OFFSET (0x020)
#define CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_VAL (0x429)
#define CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_L__SHIFT 0x00000000
#define CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_R__SHIFT 0x00000005

struct dphy_priv {
	struct device *dev;
	void __iomem *base;
	struct phy *phy;
};

static int hailo15_dphy_rx_band_control_select(u64 data_rate)
{
	unsigned int i;
	u64 data_rate_mbps = data_rate / 1000000;
	static const int data_rates_mbps[] = {	80,		100,	120,	160,	200,
											240,	280,	320,	360,	400,
											480,	560,	640,	720,	800,
											880,	1040,	1200,	1350,	1500,
											1750,	2000,	2250,	2500 };

	static const int data_rates_mbps_num_elements = ARRAY_SIZE(data_rates_mbps);

	for (i = 0; i < data_rates_mbps_num_elements - 2; i++)
		if (data_rate_mbps >= data_rates_mbps[i] &&
			data_rate_mbps < data_rates_mbps[i + 1])
			return i;
	return 0;
}

static u32 hailo15_dphy_calc_phy_band_control(s64 data_rate)
{
	u32 clock_selection = hailo15_dphy_rx_band_control_select(data_rate);
	u32 phy_band_control =
		(clock_selection << CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_R__SHIFT) |
		(clock_selection << CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_L__SHIFT);
	return phy_band_control;
}

int hailo15_dphy_rx_init(struct phy *phy, s64 data_rate)
{
	struct dphy_priv *dphy = phy_get_drvdata(phy);
	u32 phy_band_control = 0;

	if (!dphy || !dphy->base) {
		pr_err("dphy not initialized\n");
		return -ENODEV;
	}

	if (data_rate == 0) {
		dev_err(dphy->dev, "data rate 0 is invalid\n");
		return -EINVAL;
	}

	writel(CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_VAL,
		dphy->base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_ADDR_OFFSET);
	writel(CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_VAL,
		dphy->base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_ADDR_OFFSET);
	writel(CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_VAL,
		dphy->base + CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_ADDR_OFFSET);

	phy_band_control = hailo15_dphy_calc_phy_band_control(data_rate);

	dev_dbg(dphy->dev, "%s - set dphy rate from DTS to 0x%x\n", __func__, phy_band_control);
	writel(phy_band_control,
		dphy->base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT0_ADDR_OFFSET);

	dev_dbg(dphy->dev, "finished hailo15_dphy_rx_init");
	return 0;
}
EXPORT_SYMBOL(hailo15_dphy_rx_init);


static int hailo15_dphy_get_resources(struct dphy_priv *dphy,
				struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dphy->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dphy->base))
		return PTR_ERR(dphy->base);

	return 0;
}

// No operations supported for now
static const struct phy_ops hailo15_dphy_ops;

static int hailo15_dphy_probe(struct platform_device *pdev)
{
	struct dphy_priv *dphy;
	struct phy_provider *phy_provider;
	int ret;

	dev_info(&pdev->dev, "D-PHY probe started");

	dphy = kzalloc(sizeof(*dphy), GFP_KERNEL);
	if (!dphy)
		return -ENOMEM;
	platform_set_drvdata(pdev, dphy);
	dphy->dev = &pdev->dev;

	ret = hailo15_dphy_get_resources(dphy, pdev);
	if (ret)
		goto err_free_priv;

	dphy->phy = devm_phy_create(&pdev->dev, NULL, &hailo15_dphy_ops);
	if (IS_ERR(dphy->phy)) {
		ret = PTR_ERR(dphy->phy);
		dev_err(&pdev->dev, "failed to create PHY (%d)\n", ret);
		goto err_free_priv;
	}

	phy_set_drvdata(dphy->phy, dphy);
	phy_provider = devm_of_phy_provider_register(&pdev->dev,
						     of_phy_simple_xlate);

	if (IS_ERR(phy_provider)) {
		ret = PTR_ERR(phy_provider);
		dev_err(&pdev->dev, "failed to register PHY provider (%d)\n", ret);
		goto err_free_priv;
	}

	if (ret < 0)
		goto err_free_priv;

	dev_info(&pdev->dev, "probe finished successfully\n");

	return 0;

err_free_priv:
	kfree(dphy);
	return ret;
}

static int hailo15_dphy_remove(struct platform_device *pdev)
{
	struct dphy_priv *dphy = platform_get_drvdata(pdev);
	kfree(dphy);
	return 0;
}

static const struct of_device_id dphy_of_table[] = {
	{ .compatible = "hailo,hailo15-dphy" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, dphy_of_table);

static struct platform_driver dphy_driver = {
	.probe	= hailo15_dphy_probe,
	.remove	= hailo15_dphy_remove,

	.driver	= {
		.name		= "hailo15-dphy",
		.of_match_table	= dphy_of_table,
	},
};

module_platform_driver(dphy_driver);
MODULE_AUTHOR("Yotam Amir <yotama@hailo.ai>");
MODULE_DESCRIPTION("Hailo D-PHY");
MODULE_LICENSE("GPL");
