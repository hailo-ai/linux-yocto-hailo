// SPDX-License-Identifier: GPL-2.0
/*
 * dwc-i2s-hailo15l.c - Hailo specific Glue layer for Hailo15l SoC.
 *
 * Copyright (c) 2019-2025 Hailo Technologies Ltd. All rights reserved.
 */

#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/soc/hailo/scmi_hailo_ops.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <sound/designware_i2s.h>

static const struct scmi_hailo_ops *hailo_ops;

#define DW_I2S_HAILO_DRIVER_NAME "designware-i2s-hailo"

#define MAX_NUMBER_OF_I2S_CTRLS 2

struct dw_i2s_hailo {
	void __iomem *cfg_base_addr;
	struct clk *clk_rate[MAX_NUMBER_OF_I2S_CTRLS];
	int source_clk_id[MAX_NUMBER_OF_I2S_CTRLS];
};

struct dw_i2s_hailo *data;

static inline u32 dw_i2s_hailo_readl(struct dw_i2s_hailo *data, u32 offset)
{
	return readl(data->cfg_base_addr + offset);
}

static inline void dw_i2s_hailo_writel(struct dw_i2s_hailo *data, u32 offset, u32 value)
{
	writel(value, data->cfg_base_addr + offset);
}

int dw_i2s_hailo_clk_cfg(int ctrl_id, struct i2s_clk_config_data *config)
{
	int ret, div = 8;
	u32 rate = config->sample_rate * config->data_width * 2 * div;

	pr_debug("Configuring I2S-%d: sample_rate=%d, data_width=%d, rate=%d Hz\n",
		ctrl_id, config->sample_rate, config->data_width, rate);

	ret = clk_set_rate(data->clk_rate[ctrl_id], rate);
	if (ret) {
		pr_err("Can't set I2S %d clock rate: %pe\n", ctrl_id, ERR_PTR(ret));
		return ret;
	}

	return 0;
}

int dw_i2s_0_hailo_clk_cfg(struct i2s_clk_config_data *config)
{
	return dw_i2s_hailo_clk_cfg(0, config);
}

int dw_i2s_1_hailo_clk_cfg(struct i2s_clk_config_data *config)
{
	return dw_i2s_hailo_clk_cfg(1, config);
}

struct i2s_platform_data dw_i2s_hailo_0_platform_data = {
	.quirks = DW_I2S_QUIRK_CLK_CFG_OVERRIDE_ONLY,
	.i2s_clk_cfg = dw_i2s_0_hailo_clk_cfg,
};

struct i2s_platform_data dw_i2s_hailo_1_platform_data = {
	.quirks = DW_I2S_QUIRK_CLK_CFG_OVERRIDE_ONLY,
	.i2s_clk_cfg = dw_i2s_1_hailo_clk_cfg,
};

static struct of_dev_auxdata auxdata_lookup[] = {
    [0] = (struct of_dev_auxdata) {
		.phys_addr = 0x0, // Placeholder, will be set in probe
        .compatible = "hailo,hailo-designware-i2s-scu-dma",   // Match child node compatible
        .platform_data = &dw_i2s_hailo_0_platform_data,
    },
    [1] = (struct of_dev_auxdata) {
		.phys_addr = 0x0, // Placeholder, will be set in probe
        .compatible = "snps,designware-i2s",   // Match child node compatible
        .platform_data = &dw_i2s_hailo_1_platform_data,
    },
    {} // Null-terminate
};

static int dw_i2s_hailo_probe_ext_clk(struct device *dev, struct device_node *child)
{
	struct clk *ext_clk;
	u32 ext_clk_freq;
	int ret;

	if (!dev || !child)
		return -ENODEV;

	ret = of_property_read_u32(child, "source-clock-frequency", &ext_clk_freq);
	if (ret) {
		return ret;
	}

	ext_clk = devm_get_clk_from_child(dev, child, "i2s-ext-mclk");
	if (IS_ERR(ext_clk)) {
		dev_err(dev, "No valid external clock i2s-ext-mclk/sclk found for %s\n", child->name);
		return PTR_ERR(ext_clk);
	}

	ret = clk_set_rate(ext_clk, ext_clk_freq);
	if (ret) {
		dev_err(dev, "Failed to set i2s-ext-mclk rate: %pe\n", ERR_PTR(ret));
	}

	return ret;
}


static int dw_i2s_hailo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct device_node *child;
	struct resource *res;
	int ret;

	if (!node)
		return -ENODEV;

	hailo_ops = scmi_hailo_get_ops();
	if (IS_ERR(hailo_ops)) {
		return PTR_ERR(hailo_ops);
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	platform_set_drvdata(pdev, data);

	data->cfg_base_addr = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(data->cfg_base_addr))
		return PTR_ERR(data->cfg_base_addr);

	for_each_child_of_node(node, child) {
		struct scmi_hailo_set_i2s_source_clock_a2p msg;
		int ctrl_id = MAX_NUMBER_OF_I2S_CTRLS;
		struct resource res;
		bool scanned[MAX_NUMBER_OF_I2S_CTRLS] = { false, false };

		if (!of_device_is_available(child)) {
			dev_dbg(dev, "Skipping unavailable child node %s\n", child->name);
			continue;
		}

		ret = of_property_read_u32(child, "ctrl-id", &ctrl_id);
		if (ret) {
			dev_err(dev, "Failed to read %s ctrl-id property, err %pe\n", child->name, ERR_PTR(ret));
			continue;
		}

		if (ctrl_id >= MAX_NUMBER_OF_I2S_CTRLS) {
			dev_err(dev, "%s ctrl-id %d > %d, skipping...\n", child->name, ctrl_id, MAX_NUMBER_OF_I2S_CTRLS);
			continue;
		}

		if (scanned[ctrl_id] == true) {
			dev_err(dev, "%s ctrl-id %d already scanned, skipping...\n", child->name, ctrl_id);
			continue;
		}
		scanned[ctrl_id] = true;

		if(!of_device_is_compatible(child, auxdata_lookup[ctrl_id].compatible)) {
			dev_err(dev, "Unsupported I2S controller %s, skipping...\n", child->name);
			continue;
		}

		// Find out child node resource address to match auxdata_lookup
		if (of_address_to_resource(child, 0, &res)) {
			dev_err(dev, "Failed to read resource address for %s, err %pe\n", child->name, ERR_PTR(ret));
			continue;
		}
		auxdata_lookup[ctrl_id].phys_addr = res.start;

		// Read source-clock-id property id
		ret = of_property_read_u32(child, "source-clock-id", &data->source_clk_id[ctrl_id]);
		if (ret) {
			dev_err(dev, "Failed to read source-clock-id for %s, err %pe\n", child->name, ERR_PTR(ret));
			continue;
		}

		// Set the source clock id for the I2S controller
		dev_info(dev, "set %s source clock id to #%d\n", child->name, data->source_clk_id[ctrl_id]);
		msg.i2s_ctrl = ctrl_id;
		msg.source_clock = data->source_clk_id[ctrl_id];
		hailo_ops->set_i2s_source_clk(&msg);

		// Get clk pointer to Hailo's I2S controller wrapper i2sclk-rate clock (derived by bclkfrom.
		data->clk_rate[ctrl_id] = devm_get_clk_from_child(dev, child, "i2sclk-rate");
		if (IS_ERR(data->clk_rate[ctrl_id])) {
			// no aditional platfor data is needed
			auxdata_lookup[ctrl_id].platform_data = NULL;
			// check if external mclk is provided.
			ret = dw_i2s_hailo_probe_ext_clk(dev, child);
			if (ret) {
				continue;
			}
		}
	}

	ret = of_platform_populate(node, NULL, auxdata_lookup, dev);
	if (ret) {
		dev_err(dev, "failed to populate I2S controller nodes, err %pe\n", ERR_PTR(ret));
		return ret;
	}

	return 0;
}

static int dw_i2s_hailo_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	of_platform_depopulate(dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id dw_i2s_hailo_of_match[] = {
	{ .compatible = "hailo,designware-i2s"},
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, dw_i2s_hailo_of_match);

static struct platform_driver dw_i2s_hailo_driver = {
	.probe		= dw_i2s_hailo_probe,
	.remove		= dw_i2s_hailo_remove,
	.driver		= {
		.name	= DW_I2S_HAILO_DRIVER_NAME,
		.of_match_table	= dw_i2s_hailo_of_match,
		//.pm	= &dw_i2s_hailo_pm_ops,
	},
};
module_platform_driver(dw_i2s_hailo_driver);

MODULE_DESCRIPTION("DESIGNWARE I2S Hailo Glue Layer");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:designware_i2s-hailo");
