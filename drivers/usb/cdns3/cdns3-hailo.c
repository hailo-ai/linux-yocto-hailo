// SPDX-License-Identifier: GPL-2.0
/*
 * cdns3-hailo.c - Hailo specific Glue layer for Cadence USB Controller
 *
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.
 */

#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include "core.h"
#include "drd.h"
#include "cdnsp-gadget.h"


#define DR_MODE_DEVICE "peripheral"
#define DR_MODE_HOST "host"
#define DR_MODE_OTG "otg"

/* usb wrapper config registers */
#define CDNS_HAILO_DRIVER_NAME "cdns3-hailo"
#define USB_CONFIG_REG 0
#define MODE_STRAP_MASK 0x3
#define MODE_STRAP_HOST 0x1
#define MODE_STRAP_DEVICE 0x2
#define USB_ITB_INTR_MASK_REG 0xC
#define ITB_IRQ_MASK_DEVICE 0x1
#define USB_INFO_INTR_MASK 0x1C
#define USB_INFO_INTR_STATUS_REG 0x20
#define IRQ_MASK_HOST 0x1
#define IRQ_MASK_DEVICE 0x1
#define IRQ_MASK_OTG 0x2
#define USB2_PHY_CONFIG_REG 0x4C
#define ISO_IP2SOC_MASK 0x40
#define VBUS_SELECT_MASK 0x08  //vbus select 1 for device mode
#define NUM_SUB_INTERRUPTS 3

#define PORT_OVERRIDE_SLEEPM_SFR BIT(27)
#define PORT_OVERRIDE_SLEEPM_SEL BIT(26)
#define PORT_OVERRIDE_SUSPEND_SFR BIT(25)
#define PORT_OVERRIDE_SUSPEND_SEL BIT(24)
#define PORT_OVERRIDE_XCVSEL_SFR GENMASK(23, 22)
#define PORT_OVERRIDE_XCVSEL_SEL BIT(21)
#define PORT_OVERRIDE_TXBITSTUFF_SFR BIT(20)
#define PORT_OVERRIDE_TXBITSTUFF_SEL BIT(19)
#define PORT_OVERRIDE_OPMODE_SFR GENMASK(18, 17)
#define PORT_OVERRIDE_OPMODE_SEL BIT(16)
#define PORT_OVERRIDE_OVERCURRENT_SFR BIT(13)
#define PORT_OVERRIDE_OVERCURRENT_SEL BIT(12)
#define PORT_OVERRIDE_SESS_VLD_SFR BIT(11)
#define PORT_OVERRIDE_SESS_VLD_SEL BIT(10)
#define PORT_OVERRIDE_IDDIG_SFR BIT(9)
#define PORT_OVERRIDE_IDDIG_SEL BIT(8)
#define PORT_OVERRIDE_DRIVE_VBUS_SFR BIT(6)
#define PORT_OVERRIDE_DRIVE_VBUS_SEL BIT(5)
#define PORT_OVERRIDE_FORCE_OPMODE01 BIT(4)
#define PORT_OVERRIDE_BC_DMPULLDOWN BIT(3)
#define PORT_OVERRIDE_BC_DPPULLDOWN BIT(2)
#define PORT_OVERRIDE_BC_PULLDOWNCTRL BIT(1)
#define PORT_OVERRIDE_LDPULLUP BIT(0)

/* usb3 controller xhci registers */
#define XEC_PRE_REG_250NS 0x21e8
#define XEC_PRE_REG_1US 0x21ec
#define XEC_PRE_REG_10US 0x21f0
#define XEC_PRE_REG_100US 0x21f4
#define XEC_PRE_REG_125US 0x21f8
#define XEC_PRE_REG_1MS 0x21fc
#define XEC_PRE_REG_10MS 0x2200
#define XEC_PRE_REG_100MS 0x2204
#define XEC_LPM_PRE_REG_250NS 0x2208
#define XEC_LPM_PRE_REG_1US 0x220c
#define XEC_LPM_PRE_REG_10US 0x2210
#define XEC_LPM_PRE_REG_100US 0x2214
#define XEC_LPM_PRE_REG_125US 0x2218
#define XEC_LPM_PRE_REG_1MS 0x221c
#define XEC_LPM_PRE_REG_10MS 0x2220
#define XEC_LPM_PRE_REG_100MS 0x2224

struct cdns_hailo {
	void __iomem *usb_config;
	struct clk_bulk_data *core_clks;
	int num_core_clks;
	struct clk *pclk;
	struct reset_control *usb_rst;
	struct reset_control *usb_apb_rst;
	bool disconnected_overcurrent;
	bool no_usb2_phy_avdd_core_power;
	struct irq_domain *irq_domain;
	int dr_mode;
};

static inline u32 cdns_hailo_readl(struct cdns_hailo *data, u32 offset)
{
	return readl(data->usb_config + offset);
}

static inline void cdns_hailo_writel(struct cdns_hailo *data, u32 offset, u32 value)
{
	writel(value, data->usb_config + offset);
}

static const struct clk_bulk_data hailo_cdns3_core_clks[] = {
	{ .id = "usb_lpm_clk" },
	{ .id = "usb2_refclk" },
	{ .id = "usb_aclk" },
	{ .id = "usb_sof_clk" },
};

static int cdns_hailo_irq_domain_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hwirq)
{
    irq_set_chip_and_handler(irq, &dummy_irq_chip, handle_level_irq);
    return 0;
}

static const struct irq_domain_ops cdns_hailo_irq_domain_ops = {
    .map = cdns_hailo_irq_domain_map,
    .xlate = irq_domain_xlate_onecell,
};

static irqreturn_t cdns_hailo_irq_handler(int irq, void *dev_id)
{
    struct cdns_hailo *data = dev_id;
    uint32_t status;
	int virq;

    // Read the interrupt status register
    status = cdns_hailo_readl(data,  USB_INFO_INTR_STATUS_REG);
    // Dispatch to sub-interrupts using mapped IRQs
    if (status & IRQ_MASK_HOST) {
        virq = irq_find_mapping(data->irq_domain, 0);
        generic_handle_irq(virq);
    }

    if (status & IRQ_MASK_DEVICE) {
        virq = irq_find_mapping(data->irq_domain, 1);
        generic_handle_irq(virq);
    }

    if (status & IRQ_MASK_OTG) {
        virq = irq_find_mapping(data->irq_domain, 2);
        generic_handle_irq(virq);
    }

    // Acknowledge the interrupt
    return IRQ_HANDLED;
}

void cdns_hailo_init(struct cdns_hailo *data)
{
	u32 usb_config, interrupt_mask, usb_mode_strap, phy_config;

    /*  USB config Default mode to be activated after power on reset
			BIT 0-1 mode strap.
            0-neither host nor device (used for otg).
            1-host.
            2-device (default)
			BIT 2 itp_pulse_count_en ,default 0. when set - itp packet counter is enabled (debug feature)
			since these are the only option no need to read the register*/
	usb_mode_strap = ~MODE_STRAP_MASK;

	/* interrupt mask control pin the  usb device interrupt mask
	BIT 0. Info interrupt request used on all modes: 0 -masked ,1-enabled
    BIT 1. otgirq ,Dual mode control interrupt request: 0 -masked ,1-enabled
    BIT 2. host_system_error ,A sideband signaling that is active when catastrophic system error occurs: 0 -masked ,1-enabled
    BIT 3. itp indicates that an ITP packet has been received used for Device and OTG modes. 0 -masked ,1-enabled
	Note for itp to be enabled itb should be enabled also at USB_ITB_INTR_MASK_REG
	*/
	interrupt_mask = cdns_hailo_readl(data, USB_INFO_INTR_MASK);

	usb_config = cdns_hailo_readl(data, USB_CONFIG_REG);

	/* Isolation control pin for all PHY output pins (USB 2.0 only)
	* - 0: For isolating IP outputs (default).
	* - 1: For normal operation.
	*/
	if (!data->no_usb2_phy_avdd_core_power) {
		phy_config = cdns_hailo_readl(data, USB2_PHY_CONFIG_REG);
		phy_config |= (ISO_IP2SOC_MASK | VBUS_SELECT_MASK);
	}

	//the device mode interrupt mask is includes also the host mode
	interrupt_mask &= ~IRQ_MASK_DEVICE;
	//OTG is 0x0
	if (data->dr_mode == USB_DR_MODE_PERIPHERAL) {
		usb_mode_strap |= MODE_STRAP_DEVICE;
		interrupt_mask |= IRQ_MASK_DEVICE;
	} else if (data->dr_mode == USB_DR_MODE_HOST) {
		usb_mode_strap |= MODE_STRAP_HOST;
		interrupt_mask |= IRQ_MASK_HOST;
	} else {
		//otg configuration
		printk(KERN_INFO "OTG mode current not supported\n");
		return;
	}

	cdns_hailo_writel(data, USB_CONFIG_REG, usb_mode_strap);
	cdns_hailo_writel(data, USB_INFO_INTR_MASK, interrupt_mask);
	if (!data->no_usb2_phy_avdd_core_power) {
		cdns_hailo_writel(data, USB2_PHY_CONFIG_REG, phy_config);
	}
}

static int cdns_hailo_xhci_init_quirk(struct usb_hcd *hcd)
{
	struct device *dev = hcd->self.controller;
	struct cdns *cdns = dev_get_drvdata(dev->parent);
	struct cdns_hailo *data = dev_get_drvdata(dev->parent->parent);
	u32 value;

	if (!hcd->regs || !cdns->otg_cdnsp_regs)
		return 0;

    // PRE REG Timers
    writel(0xb, hcd->regs + XEC_PRE_REG_250NS);
    writel(0x2f, hcd->regs + XEC_PRE_REG_1US);
    writel(0x1df, hcd->regs + XEC_PRE_REG_10US);
    writel(0x12bf, hcd->regs + XEC_PRE_REG_100US);
    writel(0x176f, hcd->regs + XEC_PRE_REG_125US);
    writel(0xbb7f, hcd->regs + XEC_PRE_REG_1MS);
    writel(0x752ff, hcd->regs + XEC_PRE_REG_10MS);
    writel(0x493dff, hcd->regs + XEC_PRE_REG_100MS);
    // PRE LMP REG Timers
    writel(0xb, hcd->regs + XEC_LPM_PRE_REG_250NS);
    writel(0x2f, hcd->regs + XEC_LPM_PRE_REG_1US);
    writel(0x1df, hcd->regs + XEC_LPM_PRE_REG_10US);
    writel(0x12bf, hcd->regs + XEC_LPM_PRE_REG_100US);
    writel(0x176f, hcd->regs + XEC_LPM_PRE_REG_125US);
    writel(0xbb7f, hcd->regs + XEC_LPM_PRE_REG_1MS);
    writel(0x752ff, hcd->regs + XEC_LPM_PRE_REG_10MS);
    writel(0x493dff, hcd->regs + XEC_LPM_PRE_REG_100MS);

	/* if overcurrent wire is disconnected, we have to override the overcurrent_n pin */
	if (data->disconnected_overcurrent) {
		value = readl(&cdns->otg_cdnsp_regs->override);
		/* Overcurrent override select, allows SW driver override overcurrent pin as follows:
		* - 0: overcurrent is controlled from external FAULT detector
		* - 1: overcurrent controlled from SFR
		*/
		value |= OVERRIDE_OVERCURRENT_SEL;
		/* SFR overcurrent_n control.
		* - 0: overcurrent_n = 0
		* - 1: overcurrent_n = 1
		* Note: overcurrent active state is low.
		*/
		value |= OVERRIDE_OVERCURRENT_SFR;
		writel(value, &cdns->otg_cdnsp_regs->override);
	}

	return 0;
}

static int cdns_hailo_gadget_init_quirk(struct usb_gadget *gadget)
{
    struct cdnsp_device *pdev;
    struct cdns *cdns;
    struct cdnsp_otg_regs __iomem *regs;
	struct cdns_hailo *data;
	uint32_t reg_val = 0;

	if (!gadget) {
		pr_err("%s: gadget pointer is NULL\n", __func__);
		return -EINVAL;
	}

	if (!gadget->name || strcmp(gadget->name, "cdnsp-gadget") != 0) {
		pr_debug("%s: not a cdnsp-gadget (name=%s), skipping\n", 
			 __func__, gadget->name ? gadget->name : "NULL");
		return 0;
	}	

	pdev = gadget_to_cdnsp(gadget);
	if (!pdev) {
		pr_err("%s: failed to get cdnsp_device from gadget\n", __func__);
		return -ENODEV;
	}

	if (!pdev->dev) {
		pr_err("%s: cdnsp_device has no associated device\n", __func__);
		return -ENODEV;
	}

	cdns = dev_get_drvdata(pdev->dev);
	if (!cdns) {
		dev_err(pdev->dev, "%s: no CDNS core structure available\n", __func__);
		return -ENODEV;
	}

	if (!pdev->dev->parent) {
		dev_err(pdev->dev, "%s: no parent device available\n", __func__);
		return -ENODEV;
	}

	data = dev_get_drvdata(pdev->dev->parent);
	if (!data) {
		dev_err(pdev->dev->parent, "%s: no Hailo platform data available\n", __func__);
		return -ENODEV;
	}
    
	regs = cdns->otg_cdnsp_regs;
	if (!regs) {
		dev_err(pdev->dev, "%s: CDNSP OTG registers not mapped\n", __func__);
		return -ENODEV;
	}

	/* Check for no-usb2-power device tree property */
	if (data->no_usb2_phy_avdd_core_power) {
		reg_val = readl(&regs->override);

		reg_val |= PORT_OVERRIDE_SLEEPM_SFR;
		reg_val &= ~PORT_OVERRIDE_SUSPEND_SFR;
		reg_val |= FIELD_PREP(PORT_OVERRIDE_XCVSEL_SFR, 0x1);
		reg_val &= ~PORT_OVERRIDE_TXBITSTUFF_SFR;
		reg_val |= FIELD_PREP(PORT_OVERRIDE_OPMODE_SFR, 0x1);
		reg_val &= ~PORT_OVERRIDE_OVERCURRENT_SFR;
		reg_val |= PORT_OVERRIDE_SESS_VLD_SFR;
		reg_val &= ~PORT_OVERRIDE_IDDIG_SFR;
		reg_val &= ~PORT_OVERRIDE_DRIVE_VBUS_SFR;
		reg_val |= PORT_OVERRIDE_FORCE_OPMODE01;

		writel(reg_val, &regs->override);

		reg_val |= PORT_OVERRIDE_SLEEPM_SEL;
		reg_val |= PORT_OVERRIDE_SUSPEND_SEL;
		reg_val |= PORT_OVERRIDE_XCVSEL_SEL;
		reg_val |= PORT_OVERRIDE_TXBITSTUFF_SEL;
		reg_val |= PORT_OVERRIDE_OPMODE_SEL;
		reg_val |= PORT_OVERRIDE_OVERCURRENT_SEL;
		reg_val |= PORT_OVERRIDE_SESS_VLD_SEL;
		reg_val |= PORT_OVERRIDE_IDDIG_SEL;
		reg_val |= PORT_OVERRIDE_DRIVE_VBUS_SEL;

		// reg_val |= 0xD6B1D30U;
		writel(reg_val, &regs->override);

		dev_info(pdev->dev, "%s: USB2 phy AVVD core power is not conncted, set override reg = =0x%08x\n", __func__, reg_val);
	}

	dev_info(pdev->dev, "updating timers\n");
    // PRE REG Timers
    writel(0xb, pdev->regs + XEC_PRE_REG_250NS);
    writel(0x2f, pdev->regs + XEC_PRE_REG_1US);
    writel(0x1df, pdev->regs + XEC_PRE_REG_10US);
    writel(0x12bf, pdev->regs + XEC_PRE_REG_100US);
    writel(0x176f, pdev->regs + XEC_PRE_REG_125US);
    writel(0xbb7f, pdev->regs + XEC_PRE_REG_1MS);
    writel(0x752ff, pdev->regs + XEC_PRE_REG_10MS);
    writel(0x493dff, pdev->regs + XEC_PRE_REG_100MS);
    // PRE LMP REG Timers
    writel(0xb, pdev->regs + XEC_LPM_PRE_REG_250NS);
    writel(0x2f, pdev->regs + XEC_LPM_PRE_REG_1US);
    writel(0x1df, pdev->regs + XEC_LPM_PRE_REG_10US);
    writel(0x12bf, pdev->regs + XEC_LPM_PRE_REG_100US);
    writel(0x176f, pdev->regs + XEC_LPM_PRE_REG_125US);
    writel(0xbb7f, pdev->regs + XEC_LPM_PRE_REG_1MS);
    writel(0x752ff, pdev->regs + XEC_LPM_PRE_REG_10MS);
    writel(0x493dff, pdev->regs + XEC_LPM_PRE_REG_100MS);

	return 0;
}

static struct cdns3_platform_data cdns_hailo_pdata = {
	.xhci_init_quirk = cdns_hailo_xhci_init_quirk,
	.gadget_init_quirk = cdns_hailo_gadget_init_quirk,
	.quirks = 0,
};

static const struct of_dev_auxdata cdns_hailo_auxdata[] = {
	{
		.compatible = "cdnsp,usb3",
		.platform_data = &cdns_hailo_pdata,
	},
	{},
};


static int cdns_hailo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct cdns_hailo *data;
	struct device_node *child;
	const char *dr_mode_str;
	struct resource *res;
	int ret, irq, loop_index,virq;

	if (!node)
		return -ENODEV;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "can't get IOMEM resource\n");
		return -ENXIO;
	}

	//register the irq domain
	data->irq_domain = irq_domain_add_linear(node, NUM_SUB_INTERRUPTS, &cdns_hailo_irq_domain_ops, data);
    if (!data->irq_domain) {
        dev_err(&pdev->dev, "Failed to add IRQ domain\n");
        return -ENOMEM;
    }

    irq = platform_get_irq_byname(pdev, "usb_info_intr");
    if (irq < 0) {
        return irq;
	}

	for (loop_index = 0; loop_index < NUM_SUB_INTERRUPTS; loop_index++) {
    	virq = irq_create_mapping(data->irq_domain, loop_index);
		if (!virq) {
			dev_err(dev, "Failed to map sub-interrupt %d\n", loop_index);
		} else {
			dev_dbg(dev, "Mapped sub-interrupt %d to virtual IRQ %d\n", loop_index, virq);
		}
    }
	ret = devm_request_irq(&pdev->dev, irq, cdns_hailo_irq_handler, 0, CDNS_HAILO_DRIVER_NAME, data);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request info IRQ: %d\n", ret);
        irq_domain_remove(data->irq_domain);
        return ret;
    }
	/* The usb config is shared with the torrent PHY wrapper driver, so therefore
	   we can't use devm_platform_ioremap_resource() */
	data->usb_config = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!data->usb_config) {
		dev_err(dev, "can't map IOMEM resource\n");
		return -ENOMEM;
	}

	data->num_core_clks = ARRAY_SIZE(hailo_cdns3_core_clks);
	data->core_clks = devm_kmemdup(dev, hailo_cdns3_core_clks,
				sizeof(hailo_cdns3_core_clks), GFP_KERNEL);

	if (!data->core_clks)
		return -ENOMEM;
	
	/* deassert USB out of reset */
	data->usb_apb_rst = devm_reset_control_get(&pdev->dev, "usb_apb");
	if (IS_ERR(data->usb_apb_rst)) {
		dev_err(&pdev->dev, "Failed to get reset control on usb_apb\n");
		return PTR_ERR(data->usb_apb_rst);
	}
	data->usb_rst = devm_reset_control_get(&pdev->dev, "usb");
	if (IS_ERR(data->usb_rst)) {
		dev_err(&pdev->dev, "Failed to get reset control on usb\n");
		return PTR_ERR(data->usb_rst);
	}

	data->pclk = devm_clk_get(dev, "usb_pclk");
	if (IS_ERR(data->pclk))
		return PTR_ERR(data->pclk);

	data->disconnected_overcurrent = of_property_read_bool(node, "disconnected-overcurrent");
	data->no_usb2_phy_avdd_core_power = of_property_read_bool(node, "no-usb2-phy-avdd-core-power");
	if (data->no_usb2_phy_avdd_core_power) {
		dev_info(&pdev->dev, "Overriding VBUS validation since USB2 phy AVDD core power is not connected\n");
		cdns_hailo_pdata.quirks |= CDNS3_DONT_CLEAR_OVERRIDE_SESS_VLD;
	}
	
	// Iterate through the child nodes to find the cdns_usb3 node
	data->dr_mode = USB_DR_MODE_UNKNOWN;
    for_each_child_of_node(node, child) {
        if (of_device_is_compatible(child, "cdnsp,usb3")) {
            // Parse the dr_mode property from the cdns_usb3 node
            if (of_property_read_string(child, "dr_mode", &dr_mode_str)) {
                dev_err(&pdev->dev, "Failed to get dr_mode property\n");
                return -EINVAL;
            }

            if (!strcmp(dr_mode_str, DR_MODE_HOST)) {
                data->dr_mode = USB_DR_MODE_HOST;
            } else if (!strcmp(dr_mode_str, DR_MODE_DEVICE)) {
                data->dr_mode = USB_DR_MODE_PERIPHERAL;
            } else if(!strcmp(dr_mode_str, DR_MODE_OTG)) {
                dev_err(&pdev->dev, "Invalid dr_mode property: %s\n", dr_mode_str);
                return -EINVAL;
            }

            // Now you can use usb3->dr_mode in your driver
            dev_info(&pdev->dev, "dr_mode is set to %s\n", dr_mode_str);
            break;
        }
	}
	if (data->dr_mode == USB_DR_MODE_UNKNOWN) {
		dev_info(&pdev->dev, "dr_mode property not found setting \"host\" as default mode\n");
		data->dr_mode = USB_DR_MODE_HOST;
	}
	pm_runtime_get_sync(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	/* Note: reset deassert order matters: 1-usb_apb, 2-usb !!! */
	reset_control_deassert(data->usb_apb_rst);
	ret = clk_prepare_enable(data->pclk);
	if (ret)
		return ret;
	reset_control_deassert(data->usb_rst);

	// note: must be called before the core clocks are enabled
	cdns_hailo_init(data);

	ret = devm_clk_bulk_get(dev, data->num_core_clks, data->core_clks);
	if (ret)
		return ret;

	ret = clk_bulk_prepare_enable(data->num_core_clks, data->core_clks);
	if (ret)
		return ret;

	ret = of_platform_populate(node, NULL, cdns_hailo_auxdata, dev);
	if (ret) {
		dev_err(dev, "failed to create children: %d\n", ret);
		goto err;
	}
	return ret;
err:
	clk_bulk_disable_unprepare(data->num_core_clks, data->core_clks);
	clk_disable_unprepare(data->pclk);
	return ret;
}

static int cdns_hailo_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cdns_hailo *data = dev_get_drvdata(dev);

	irq_domain_remove(data->irq_domain);
	of_platform_depopulate(dev);
	clk_bulk_disable_unprepare(data->num_core_clks, data->core_clks);
	clk_disable_unprepare(data->pclk);
	reset_control_assert(data->usb_rst);
	reset_control_assert(data->usb_apb_rst);
	pm_runtime_put_sync(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_disable(dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int cdns_hailo_resume(struct device *dev)
{
	int ret;
	struct cdns_hailo *data = dev_get_drvdata(dev);

	ret = clk_prepare_enable(data->pclk);
	if (ret)
		return ret;

	return clk_bulk_prepare_enable(data->num_core_clks, data->core_clks);
}

static int cdns_hailo_suspend(struct device *dev)
{
	struct cdns_hailo *data = dev_get_drvdata(dev);

	clk_bulk_disable_unprepare(data->num_core_clks, data->core_clks);
	clk_disable_unprepare(data->pclk);

	return 0;
}
#endif

static const struct dev_pm_ops cdns_hailo_pm_ops = {
	SET_RUNTIME_PM_OPS(cdns_hailo_suspend, cdns_hailo_resume, NULL)
};

static const struct of_device_id cdns_hailo_of_match[] = {
	{ .compatible = "hailo,usb3", },
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, cdns_hailo_of_match);

static struct platform_driver cdns_hailo_driver = {
	.probe		= cdns_hailo_probe,
	.remove		= cdns_hailo_remove,
	.driver		= {
		.name	= CDNS_HAILO_DRIVER_NAME,
		.of_match_table	= cdns_hailo_of_match,
		// .pm	= &cdns_hailo_pm_ops, TODO: enable power management
	},
};
module_platform_driver(cdns_hailo_driver);

MODULE_ALIAS("platform:cdns3-hailo");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Cadence USB3 Hailo Glue Layer");
