// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Hailo D-PHY
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved. 
 */


#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/phy/phy-mipi-dphy.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>

#include "hailo15-pixel-mux.h"
#include "hailo15-rxwrapper.h"

#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT0_ADDR_OFFSET (0xb00)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_ADDR_OFFSET (0xb08)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_VAL (0xaaaaaaaa)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_ADDR_OFFSET (0xb0c)
#define CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_VAL (0x2aa)
#define CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_ADDR_OFFSET (0x020)
#define CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_VAL (0x429)
#define CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_L__SHIFT 0x00000000
#define CDNS_MIPI_DPHY_RX_PCS_TX_DIG_TBIT0__BAND_CTL_REG_R__SHIFT 0x00000005

/* H15L 2-clock-lane bring-up. Single shared DPHY whose rxwrapper
 * IP_CTRL.ipconfig_cmn comes up at the HW reset value of 0
 * (single-link). Reconfigure to dual: hold CMN in reset via the
 * rxwrapper API, set ipconfig_cmn=2, release, poll o_cmn_ready,
 * enable right-side lanes, ask the pixel-mux to route shared V6/V5
 * pads to CSI2RX1. Cross-IP register pokes go through the owning
 * driver's API; this driver only touches DPHY-internal registers.
 */
#define HAILO15L_DPHY_ISO_CMN_CTRL_OFFSET (0xc0c)
#define HAILO15L_DPHY_ISO_CMN_READY_BIT   BIT(5)
#define HAILO15L_DPHY_ISO_ENABLE_OFFSET   (0xc08)

#define HAILO15L_DPHY_ISO_ENABLE_DUAL_VAL (0x33f)

#define HAILO15L_DPHY_NUM_RXWRAPPERS      2

struct dphy_priv {
	struct device *dev;
	void __iomem *base;
	struct phy *phy;
	/* sister-IP handles for cross-IP 2cl bring-up. Resolved at probe
	 * from DT phandles 'hailo,rxwrappers' / 'hailo,pixel-mux'; held
	 * until dphy unbinds and released via devm action.
	 */
	struct device *rxwrapper_devs[HAILO15L_DPHY_NUM_RXWRAPPERS];
	struct device *pmux_dev;
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

static int hailo15_dphy_rx_phy_configure(struct phy *phy,
					 union phy_configure_opts *opts)
{
	struct dphy_priv *dphy = phy_get_drvdata(phy);
	unsigned long data_rate = opts->mipi_dphy.hs_clk_rate;
	u32 phy_band_control;

	if (!dphy || !dphy->base) {
		pr_err("dphy not initialized\n");
		return -ENODEV;
	}

	if (!data_rate) {
		dev_err(dphy->dev, "data rate 0 is invalid\n");
		return -EINVAL;
	}

	writel(CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_VAL,
		dphy->base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT2_ADDR_OFFSET);
	writel(CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_VAL,
		dphy->base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT3_ADDR_OFFSET);
	writel(CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_VAL,
		dphy->base + CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_ADDR_OFFSET);

	phy_band_control = hailo15_dphy_calc_phy_band_control((s64)data_rate);
	dev_dbg(dphy->dev, "set dphy rate to 0x%x\n", phy_band_control);
	writel(phy_band_control,
		dphy->base + CDNS_MIPI_DPHY_RX_TX_DIG_TBIT0_ADDR_OFFSET);

	return 0;
}


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

static int hailo15_dphy_2cl_init(struct dphy_priv *dphy)
{
	struct device *rxw0 = dphy->rxwrapper_devs[0];
	struct device *rxw1 = dphy->rxwrapper_devs[1];
	u32 val;
	int ret;

	if (!rxw0 || !rxw1 || !dphy->pmux_dev) {
		dev_err(dphy->dev,
			"2-clock-lane requested but cross-IP phandles missing in DT\n");
		return -ENODEV;
	}

	/* already in dual-link mode (driver reload): skip — re-asserting
	 * lane_rstb_cmn would tear down any live stream.
	 */
	if (hailo15_rxwrapper_is_dual_link_active(rxw0) &&
	    (readl(dphy->base + HAILO15L_DPHY_ISO_CMN_CTRL_OFFSET)
		    & HAILO15L_DPHY_ISO_CMN_READY_BIT)) {
		dev_info(dphy->dev,
			 "2-clock-lane already configured by previous probe, skipping bring-up\n");
		return 0;
	}

	/* disable CMN SSM (TBIT2 bit 0 = 0) */
	writel(CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_VAL & ~BIT(0),
	       dphy->base + CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_ADDR_OFFSET);

	/* assert lane_rstb_cmn=0 + ipconfig_cmn=2 on both rxwrappers
	 * (must precede CMN release per Cadence DPHY Rx UG)
	 */
	ret = hailo15_rxwrapper_dual_link_assert_cmn_reset(rxw0);
	if (ret)
		return ret;
	ret = hailo15_rxwrapper_dual_link_assert_cmn_reset(rxw1);
	if (ret)
		return ret;

	udelay(10);

	/* re-enable CMN SSM */
	writel(CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_VAL,
	       dphy->base + CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_ADDR_OFFSET);

	/* release lane_rstb_cmn */
	hailo15_rxwrapper_dual_link_release_cmn_reset(rxw0);
	hailo15_rxwrapper_dual_link_release_cmn_reset(rxw1);

	/* poll o_cmn_ready (ISO_CMN_CTRL bit 5), 100ms */
	ret = readl_poll_timeout(dphy->base + HAILO15L_DPHY_ISO_CMN_CTRL_OFFSET,
				 val,
				 val & HAILO15L_DPHY_ISO_CMN_READY_BIT,
				 100, 100000);
	if (ret) {
		dev_err(dphy->dev,
			"o_cmn_ready timed out, ISO_CMN_CTRL=0x%08x — rolling back\n",
			val);
		/* rollback so single-link csi2rx0 keeps working */
		hailo15_rxwrapper_restore_single_link(rxw0);
		hailo15_rxwrapper_restore_single_link(rxw1);
		writel(CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_VAL,
		       dphy->base + CDNS_MIPI_DPHY_RX_CMN_DIG_TBIT2_ADDR_OFFSET);
		return ret;
	}

	/* enable right-side lanes in ISO_PHY_ISO_ENABLE */
	writel(HAILO15L_DPHY_ISO_ENABLE_DUAL_VAL,
	       dphy->base + HAILO15L_DPHY_ISO_ENABLE_OFFSET);

	/* route shared V6/V5 pads to CSI2RX1 via the pixel-mux driver */
	ret = hailo15_pixel_mux_route_right_lane(dphy->pmux_dev, 1);
	if (ret) {
		dev_err(dphy->dev,
			"pixel-mux right-lane routing failed (%d)\n", ret);
		return ret;
	}

	dev_info(dphy->dev, "2-clock-lane dual-link mode initialized\n");
	return 0;
}

static const struct phy_ops hailo15_dphy_ops = {
	.configure = hailo15_dphy_rx_phy_configure,
	.owner     = THIS_MODULE,
};

/* Resolve a DT phandle ('hailo,rxwrappers'/'hailo,pixel-mux') to the
 * already-probed sister device. Returns -EPROBE_DEFER when the target
 * node exists but its driver hasn't bound yet. The success path
 * transfers the of_find_device_by_node refcount to the caller, who
 * must put_device() to release it.
 */
static int hailo15_dphy_resolve_phandle(struct device *dev,
					const char *prop, int index,
					struct device **out)
{
	struct device_node *np;
	struct platform_device *target;

	np = of_parse_phandle(dev->of_node, prop, index);
	if (!np)
		return -ENOENT;

	target = of_find_device_by_node(np);
	of_node_put(np);
	if (!target)
		return -EPROBE_DEFER;
	if (!target->dev.driver) {
		put_device(&target->dev);
		return -EPROBE_DEFER;
	}

	*out = &target->dev;
	return 0;
}

/* Resolve all 2cl-related phandles. Refs cached in priv and released
 * via devm action when dphy unbinds.
 */
static int hailo15_dphy_resolve_2cl_phandles(struct dphy_priv *dphy)
{
	int i, ret;

	for (i = 0; i < HAILO15L_DPHY_NUM_RXWRAPPERS; i++) {
		ret = hailo15_dphy_resolve_phandle(dphy->dev,
			"hailo,rxwrappers", i, &dphy->rxwrapper_devs[i]);
		if (ret) {
			dev_err(dphy->dev,
				"hailo,rxwrappers[%d] resolve failed (%d)\n",
				i, ret);
			return ret;
		}
	}

	ret = hailo15_dphy_resolve_phandle(dphy->dev,
		"hailo,pixel-mux", 0, &dphy->pmux_dev);
	if (ret) {
		dev_err(dphy->dev,
			"hailo,pixel-mux resolve failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

static void hailo15_dphy_release_sister_devs(void *data)
{
	struct dphy_priv *dphy = data;
	int i;

	for (i = 0; i < HAILO15L_DPHY_NUM_RXWRAPPERS; i++) {
		if (dphy->rxwrapper_devs[i]) {
			put_device(dphy->rxwrapper_devs[i]);
			dphy->rxwrapper_devs[i] = NULL;
		}
	}
	if (dphy->pmux_dev) {
		put_device(dphy->pmux_dev);
		dphy->pmux_dev = NULL;
	}
}

static int hailo15_dphy_probe(struct platform_device *pdev)
{
	struct dphy_priv *dphy;
	struct phy_provider *phy_provider;
	int ret;

	dev_info(&pdev->dev, "D-PHY probe started\n");

	dphy = devm_kzalloc(&pdev->dev, sizeof(*dphy), GFP_KERNEL);
	if (!dphy)
		return -ENOMEM;
	platform_set_drvdata(pdev, dphy);
	dphy->dev = &pdev->dev;

	ret = hailo15_dphy_get_resources(dphy, pdev);
	if (ret)
		return ret;

	dphy->phy = devm_phy_create(&pdev->dev, NULL, &hailo15_dphy_ops);
	if (IS_ERR(dphy->phy)) {
		ret = PTR_ERR(dphy->phy);
		dev_err(&pdev->dev, "failed to create PHY (%d)\n", ret);
		return ret;
	}

	phy_set_drvdata(dphy->phy, dphy);
	phy_provider = devm_of_phy_provider_register(&pdev->dev,
						     of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		ret = PTR_ERR(phy_provider);
		dev_err(&pdev->dev, "failed to register PHY provider (%d)\n", ret);
		return ret;
	}

	/* Work mode is bound to the DT overlay state at boot: the
	 * 'hailo,enable-2-clock-lane' property is present iff the
	 * dual-sensor overlay is applied. Configure the rxwrappers and
	 * pixel-mux for the chosen mode here, once, so dphy mode is
	 * invariant for the rest of the dphy lifetime. Relies on
	 * rxwrapper / pixel-mux / isp being bound before dphy probes
	 * (guaranteed by their =y kernel config); they ungate their own
	 * clocks at probe, so the rxwrapper register window is already
	 * accessible from here.
	 */
	if (of_property_read_bool(pdev->dev.of_node, "hailo,enable-2-clock-lane")) {
		ret = devm_add_action_or_reset(&pdev->dev,
			hailo15_dphy_release_sister_devs, dphy);
		if (ret)
			return ret;

		ret = hailo15_dphy_resolve_2cl_phandles(dphy);
		if (ret)
			return ret;

		ret = hailo15_dphy_2cl_init(dphy);
		if (ret) {
			dev_err(&pdev->dev,
				"2-clock-lane init failed at probe (%d)\n", ret);
			return ret;
		}
	}

	dev_info(&pdev->dev, "probe finished successfully\n");

	return 0;
}

static const struct of_device_id dphy_of_table[] = {
	{ .compatible = "hailo,hailo15-dphy" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, dphy_of_table);

static struct platform_driver dphy_driver = {
	.probe	= hailo15_dphy_probe,
	.driver	= {
		.name		= "hailo15-dphy",
		.of_match_table	= dphy_of_table,
	},
};

module_platform_driver(dphy_driver);
MODULE_AUTHOR("Yotam Amir <yotama@hailo.ai>");
MODULE_DESCRIPTION("Hailo D-PHY");
MODULE_LICENSE("GPL");
