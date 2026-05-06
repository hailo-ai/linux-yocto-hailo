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
#include <linux/of_address.h>
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
#include <linux/phy/phy.h>
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

#define SUB_INTERRUPT_HOST 0
#define SUB_INTERRUPT_DEVICE 1
#define SUB_INTERRUPT_OTG 2
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

#define DECLARE_REG_U32(reg_name, offset, shift, width) \
	enum { reg_name##__OFFSET = (offset) }; \
	enum { reg_name##__SHIFT = (shift) }; \
	enum { reg_name##__MASK = GENMASK((shift) + (width) - 1, (shift)) }

/* xhci/device Timers regs relative offsets */
DECLARE_REG_U32(XEC_PRE_REG_250NS, 0x21e8, 0, 24);
DECLARE_REG_U32(XEC_PRE_REG_1US, 0x21ec, 0, 24);
DECLARE_REG_U32(XEC_PRE_REG_10US, 0x21f0, 0, 24);
DECLARE_REG_U32(XEC_PRE_REG_100US, 0x21f4, 0, 24);
DECLARE_REG_U32(XEC_PRE_REG_125US, 0x21f8, 0, 24);
DECLARE_REG_U32(XEC_PRE_REG_1MS, 0x21fc, 0, 24);
DECLARE_REG_U32(XEC_PRE_REG_10MS, 0x2200, 0, 24);
DECLARE_REG_U32(XEC_PRE_REG_100MS, 0x2204, 0, 24);
DECLARE_REG_U32(XEC_LPM_PRE_REG_250NS, 0x2208, 0, 24);
DECLARE_REG_U32(XEC_LPM_PRE_REG_1US, 0x220c, 0, 24);
DECLARE_REG_U32(XEC_LPM_PRE_REG_10US, 0x2210, 0, 24);
DECLARE_REG_U32(XEC_LPM_PRE_REG_100US, 0x2214, 0, 24);
DECLARE_REG_U32(XEC_LPM_PRE_REG_125US, 0x2218, 0, 24);
DECLARE_REG_U32(XEC_LPM_PRE_REG_1MS, 0x221c, 0, 24);
DECLARE_REG_U32(XEC_LPM_PRE_REG_10MS, 0x2220, 0, 24);
DECLARE_REG_U32(XEC_LPM_PRE_REG_100MS, 0x2224, 0, 24);

DECLARE_REG_U32(XEC_CFG_3XPORT_LTSSM_1_CFG_LTSSM_TIMER_POLL_LFPS_VAL, 0x2044, 16, 9);
DECLARE_REG_U32(D_XEC_USBSSP_CHICKEN_BITS, 0x2228, 19, 1);

/* Devcie Rx buferr configuration regs relative offsets */
DECLARE_REG_U32(D_XEC_XBUF_RX_STAT, 0x2258, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_RX_STAT_MASK, 0x226C, 0, 9);
DECLARE_REG_U32(D_XEC_XBUF_RX_TAG_0, 0x225C, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_RX_TAG_MASK_0, 0x2270, 0, 9);
DECLARE_REG_U32(D_XEC_XBUF_RX_DATA_0, 0x2260, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_RX_DATA_MASK_0, 0x2274, 0, 9);
DECLARE_REG_U32(D_XEC_XBUF_RX_TAG_1, 0x2264, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_RX_TAG_MASK_1, 0x2278, 0, 9);
DECLARE_REG_U32(D_XEC_XBUF_RX_DATA_1, 0x2268, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_RX_DATA_MASK_1, 0x227C, 0, 9);

/* Devcie Tx buferr configuration regs relative offsets */
DECLARE_REG_U32(D_XEC_XBUF_TX_CMD, 0x2280, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_TX_CMD_MASK, 0x22C4, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_0, 0x2284, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_1, 0x228C, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_2, 0x2294, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_3, 0x229C, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_4, 0x22A4, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_5, 0x22AC, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_6, 0x22B4, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_7, 0x22BC, 0, 7);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_0, 0x2288, 0, 11);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_1, 0x2290, 0, 11);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_2, 0x2298, 0, 11);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_3, 0x22A0, 0, 11);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_4, 0x22A8, 0, 11);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_5, 0x22B0, 0, 11);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_6, 0x22B8, 0, 11);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_7, 0x22C0, 0, 11);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_MASK_0, 0x22C8, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_MASK_1, 0x22D0, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_MASK_2, 0x22D8, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_MASK_3, 0x22E0, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_MASK_4, 0x22E8, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_MASK_5, 0x22F0, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_MASK_6, 0x22F8, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_TAG_MASK_7, 0x2300, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_MASK_0, 0x22CC, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_MASK_1, 0x22D4, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_MASK_2, 0x22DC, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_MASK_3, 0x22E4, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_MASK_4, 0x22EC, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_MASK_5, 0x22F4, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_MASK_6, 0x22FC, 0, 10);
DECLARE_REG_U32(D_XEC_XBUF_TX_DATA_MASK_7, 0x2304, 0, 10);

#define write_u32_reg(regs_base, reg_name, value) \
	do { \
		u32 reg_val = readl((regs_base) + (reg_name##__OFFSET)); \
		reg_val = ((reg_val) & ~(reg_name##__MASK)) | (((value) << (reg_name##__SHIFT)) & (reg_name##__MASK)); \
		writel(reg_val, (regs_base) + (reg_name##__OFFSET)); \
	} while (0)

#define read_u32_reg(regs_base, reg_name) \
	(((readl((regs_base) + (reg_name##__OFFSET))) & (reg_name##__MASK)) >> (reg_name##__SHIFT))

struct cdns_hailo {
	void __iomem *usb_config;
	void __iomem *cdns_dev_regs;
	struct clk_bulk_data *core_clks;
	int num_core_clks;
	struct clk *pclk;
	struct reset_control *usb_rst;
	struct reset_control *usb_apb_rst;
	bool disconnected_overcurrent;
	bool no_usb2_phy_avdd_core_power;
	bool usb2_is_inactive;
	struct irq_domain *irq_domain;
	int dr_mode;
	raw_spinlock_t irq_lock;
	u32 sw_irq_mask;
	u32 sof_timers_reset_base_clock_rate;
	u32 lpm_timers_reset_base_clock_rate;
	u32 sof_timers_base_clock_rate;
	u32 lpm_timers_base_clock_rate;
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
	{ .id = "usb_sof_clk" },
	{ .id = "usb_aclk" },
};

static const struct clk_bulk_data hailo_cdns3_core_clks__no_usb2_refclk[] = {
	{ .id = "usb_lpm_clk" },
	{ .id = "usb_sof_clk" },
	{ .id = "usb_aclk" },
};


static u32 sw_irq_to_hw_irq_mask(u32 sw_irq_mask)
{
	u32 hw_mask = 0;

	if (sw_irq_mask & BIT(SUB_INTERRUPT_HOST))
		hw_mask |= IRQ_MASK_HOST;
	if (sw_irq_mask & BIT(SUB_INTERRUPT_DEVICE))
		hw_mask |= IRQ_MASK_DEVICE;
	if (sw_irq_mask & BIT(SUB_INTERRUPT_OTG))
		hw_mask |= IRQ_MASK_OTG;

	return hw_mask;
}

/* interrupt mask control pin the  usb device interrupt mask
	BIT 0. Info interrupt request used on all modes: 0 -masked ,1-enabled
	BIT 1. otgirq ,Dual mode control interrupt request: 0 -masked ,1-enabled
	BIT 2. host_system_error ,A sideband signaling that is active when catastrophic system error occurs: 0 -masked ,1-enabled
	BIT 3. itp indicates that an ITP packet has been received used for Device and OTG modes. 0 -masked ,1-enabled
	Note for itp to be enabled itb should be enabled also at USB_ITB_INTR_MASK_REG
*/
static void cdns3_hailo_irq_mask(struct irq_data *d)
{
	struct cdns_hailo *data = irq_data_get_irq_chip_data(d);
	unsigned long flags;

	raw_spin_lock_irqsave(&data->irq_lock, flags);
	data->sw_irq_mask &= ~BIT(d->hwirq);
	cdns_hailo_writel(data, USB_INFO_INTR_MASK, sw_irq_to_hw_irq_mask(data->sw_irq_mask));
	raw_spin_unlock_irqrestore(&data->irq_lock, flags);
}

static void cdns3_hailo_irq_unmask(struct irq_data *d)
{
	struct cdns_hailo *data = irq_data_get_irq_chip_data(d);
	unsigned long flags;

	raw_spin_lock_irqsave(&data->irq_lock, flags);
	data->sw_irq_mask |= BIT(d->hwirq);
	cdns_hailo_writel(data, USB_INFO_INTR_MASK, sw_irq_to_hw_irq_mask(data->sw_irq_mask));
	raw_spin_unlock_irqrestore(&data->irq_lock, flags);
}


struct irq_chip cdns3_hailo_irq_chip = {
	.name		= "cdns3-hailo-irqchip",
	.irq_mask	= cdns3_hailo_irq_mask,
	.irq_unmask	= cdns3_hailo_irq_unmask,
	.flags		= IRQCHIP_SKIP_SET_WAKE,
};

static int cdns_hailo_irq_domain_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hwirq)
{
    struct cdns_hailo *data = d->host_data;
    irq_set_chip_and_handler(irq, &cdns3_hailo_irq_chip, handle_level_irq);
    irq_set_chip_data(irq, data);
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
        virq = irq_find_mapping(data->irq_domain, SUB_INTERRUPT_HOST);
        generic_handle_irq(virq);
    }

    if (status & IRQ_MASK_DEVICE) {
        virq = irq_find_mapping(data->irq_domain, SUB_INTERRUPT_DEVICE);
        generic_handle_irq(virq);
    }

    if (status & IRQ_MASK_OTG) {
        virq = irq_find_mapping(data->irq_domain, SUB_INTERRUPT_OTG);
        generic_handle_irq(virq);
    }

    // Acknowledge the interrupt
    return IRQ_HANDLED;
}

void cdns_hailo_init(struct cdns_hailo *data)
{
	u32 usb_config, usb_mode_strap, phy_config;

    /*  USB config Default mode to be activated after power on reset
			BIT 0-1 mode strap.
            0-neither host nor device (used for otg).
            1-host.
            2-device (default)
			BIT 2 itp_pulse_count_en ,default 0. when set - itp packet counter is enabled (debug feature)
			since these are the only option no need to read the register*/
	usb_mode_strap = ~MODE_STRAP_MASK;

	usb_config = cdns_hailo_readl(data, USB_CONFIG_REG);

	/* Isolation control pin for all PHY output pins (USB 2.0 only)
	* - 0: For isolating IP outputs (default).
	* - 1: For normal operation.
	*/
	if (!data->no_usb2_phy_avdd_core_power) {
		phy_config = cdns_hailo_readl(data, USB2_PHY_CONFIG_REG);
		phy_config |= (ISO_IP2SOC_MASK | VBUS_SELECT_MASK);
	}

	//OTG is 0x0
	if (data->dr_mode == USB_DR_MODE_PERIPHERAL) {
		usb_mode_strap |= MODE_STRAP_DEVICE;
	} else if (data->dr_mode == USB_DR_MODE_HOST) {
		usb_mode_strap |= MODE_STRAP_HOST;
	} else {
		//otg configuration
		printk(KERN_INFO "OTG mode current not supported\n");
		return;
	}

	cdns_hailo_writel(data, USB_CONFIG_REG, usb_mode_strap);
	// disable all interrupts as a part of initialization
	cdns_hailo_writel(data, USB_INFO_INTR_MASK, 0);
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

    // PRE REG Timers (derived from SOF clock 48 MHz)
    write_u32_reg(hcd->regs, XEC_PRE_REG_250NS, 0xb);
    write_u32_reg(hcd->regs, XEC_PRE_REG_1US, 0x2f);
    write_u32_reg(hcd->regs, XEC_PRE_REG_10US, 0x1df);
    write_u32_reg(hcd->regs, XEC_PRE_REG_100US, 0x12bf);
    write_u32_reg(hcd->regs, XEC_PRE_REG_125US, 0x176f);
    write_u32_reg(hcd->regs, XEC_PRE_REG_1MS, 0xbb7f);
    write_u32_reg(hcd->regs, XEC_PRE_REG_10MS, 0x752ff);
    write_u32_reg(hcd->regs, XEC_PRE_REG_100MS, 0x493dff);
    // PRE LPM REG Timers (derived from LPM clock 25 MHz)
    write_u32_reg(hcd->regs, XEC_LPM_PRE_REG_250NS, 0xb);
    write_u32_reg(hcd->regs, XEC_LPM_PRE_REG_1US, 0x2f);
    write_u32_reg(hcd->regs, XEC_LPM_PRE_REG_10US, 0x1df);
    write_u32_reg(hcd->regs, XEC_LPM_PRE_REG_100US, 0x12bf);
    write_u32_reg(hcd->regs, XEC_LPM_PRE_REG_125US, 0x176f);
    write_u32_reg(hcd->regs, XEC_LPM_PRE_REG_1MS, 0xbb7f);
    write_u32_reg(hcd->regs, XEC_LPM_PRE_REG_10MS, 0x752ff);
    write_u32_reg(hcd->regs, XEC_LPM_PRE_REG_100MS, 0x493dff);

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

#ifdef CDNS_TIMERS_DEBUG
static int cdns_hailo_validate_timer_registers(struct platform_device *pdev)
{
	u32 actual;
	int i, errors = 0;
	struct cdns_hailo *data = (struct cdns_hailo *)dev_get_drvdata(&pdev->dev);
	u32 lpm_clock_rate = data->lpm_timers_base_clock_rate;
	u32 sof_clock_rate = data->sof_timers_base_clock_rate;

	struct {
		u32 offset;
		u32 expected;
		const char *name;
	} timers[] = {
		// SOF clock based timers
		{XEC_PRE_REG_250NS__OFFSET, sof_clock_rate/4000000-1, "XEC_PRE_REG_250NS"},
		{XEC_PRE_REG_1US__OFFSET,   sof_clock_rate/1000000-1, "XEC_PRE_REG_1US"},
		{XEC_PRE_REG_10US__OFFSET,  sof_clock_rate/100000-1, "XEC_PRE_REG_10US"},
		{XEC_PRE_REG_100US__OFFSET, sof_clock_rate/10000-1, "XEC_PRE_REG_100US"},
		{XEC_PRE_REG_125US__OFFSET, sof_clock_rate/8000-1, "XEC_PRE_REG_125US"},
		{XEC_PRE_REG_1MS__OFFSET,   sof_clock_rate/1000-1, "XEC_PRE_REG_1MS"},
		{XEC_PRE_REG_10MS__OFFSET,  sof_clock_rate/100-1, "XEC_PRE_REG_10MS"},
		{XEC_PRE_REG_100MS__OFFSET, sof_clock_rate/10-1, "XEC_PRE_REG_100MS"},
		// LPM clock based timers
		{XEC_LPM_PRE_REG_250NS__OFFSET, lpm_clock_rate/4000000-1, "XEC_LPM_PRE_REG_250NS"},
		{XEC_LPM_PRE_REG_1US__OFFSET,   lpm_clock_rate/1000000-1, "XEC_LPM_PRE_REG_1US"},
		{XEC_LPM_PRE_REG_10US__OFFSET,  lpm_clock_rate/100000-1, "XEC_LPM_PRE_REG_10US"},
		{XEC_LPM_PRE_REG_100US__OFFSET, lpm_clock_rate/10000-1, "XEC_LPM_PRE_REG_100US"},
		{XEC_LPM_PRE_REG_125US__OFFSET, lpm_clock_rate/8000-1, "XEC_LPM_PRE_REG_125US"},
		{XEC_LPM_PRE_REG_1MS__OFFSET,   lpm_clock_rate/1000-1, "XEC_LPM_PRE_REG_1MS"},
		{XEC_LPM_PRE_REG_10MS__OFFSET,  lpm_clock_rate/100-1, "XEC_LPM_PRE_REG_10MS"},
		{XEC_LPM_PRE_REG_100MS__OFFSET, lpm_clock_rate/10-1, "XEC_LPM_PRE_REG_100MS"}
	};

	/* Validate timer registers */
	for (i = 0; i < ARRAY_SIZE(timers); i++) {
		actual = readl(data->cdns_dev_regs + timers[i].offset);
		if (actual != timers[i].expected) {
			dev_err(&pdev->dev, "Timer validation failed: %s expected 0x%x, got 0x%x\n",
			       timers[i].name, timers[i].expected, actual);
			errors++;
		}
	}

	if (errors == 0) {
		dev_dbg(&pdev->dev, "All timer registers validated successfully\n");
		return 0;
	} else {
		dev_err(&pdev->dev, "Timer validation failed with %d errors\n", errors);
		return -EIO;
	}
}
#endif // CDNS_TIMERS_DEBUG

/**
 * cdns_hailo_update_timer_registers - Update XEC timer registers based on clock name
 * @pdev: Platform device pointer
 * @timers_type: Type of timers to update ("sof" or "lpm")
 *
 * This function finds the appropriate clock, gets its rate, validates it, and updates
 * either XEC_PRE_REG timers (SOF-based) or XEC_LPM_PRE_REG timers (LPM-based)
 * with values calculated from the clock rate.
 */
static int cdns_hailo_update_timer_registers(struct platform_device *pdev, const char *timers_type)
{
	struct cdns_hailo *data;
	struct clk *target_clk = NULL;
	u32 clock_rate;
	u32 timers_reset_base_clock_rate;
	const char *clock_name;
	int i;

	if (!pdev || !timers_type) {
		dev_err(&pdev->dev, "Invalid parameters for timer register update\n");
		return -EINVAL;
	}

	data = (struct cdns_hailo *)dev_get_drvdata(&pdev->dev);
	if (!data || !data->cdns_dev_regs) {
		dev_err(&pdev->dev, "Invalid platform data or device registers\n");
		return -EINVAL;
	}

	/* Determine clock name based on timer type */
	if (strcmp(timers_type, "sof") == 0) {
		clock_name = "usb_sof_clk";
		data->sof_timers_reset_base_clock_rate = read_u32_reg(data->cdns_dev_regs, XEC_PRE_REG_100MS) * 10;
		timers_reset_base_clock_rate = data->sof_timers_reset_base_clock_rate;
	} else if (strcmp(timers_type, "lpm") == 0) {
		clock_name = "usb_lpm_clk";
		data->lpm_timers_reset_base_clock_rate = read_u32_reg(data->cdns_dev_regs, XEC_LPM_PRE_REG_100MS) * 10;
		timers_reset_base_clock_rate = data->lpm_timers_reset_base_clock_rate;
	} else {
		dev_err(&pdev->dev, "Unknown timer type: %s. Expected 'sof' or 'lmp'\n", timers_type);
		return -EINVAL;
	}

	/* Find the target clock in the clock array */
	for (i = 0; i < data->num_core_clks; i++) {
		if (strcmp(data->core_clks[i].id, clock_name) == 0) {
			target_clk = data->core_clks[i].clk;
			break;
		}
	}
	if (!target_clk) {
		dev_err(&pdev->dev, "%s not found in clock array\n", clock_name);
		return -EINVAL;
	}

	/* Get and validate clock rate */
	clock_rate = clk_get_rate(target_clk);
	if (clock_rate == 0) {
		dev_err(&pdev->dev, "Gadget %s timers won't be updated since %s clock rate (0 Hz)\n", timers_type, clock_name);
		dev_err(&pdev->dev, "Note: gadget %s timers reset values are based on %s clock rate of %u Hz\n", timers_type, clock_name, timers_reset_base_clock_rate);
		dev_err(&pdev->dev, "      Controller may not function correctly\n");
		return 0;
	}

	if (strcmp(timers_type, "sof") == 0) {
		data->sof_timers_base_clock_rate = clock_rate;
	} else if (strcmp(timers_type, "lpm") == 0) {
		data->lpm_timers_base_clock_rate = clock_rate;
	}

	dev_info(&pdev->dev, "Update gadgets XEC_%s_REG timers according to %s clock rate (%u Hz)\n", 
		 strcmp(timers_type, "sof") == 0 ? "PRE" : "LPM_PRE", clock_name, clock_rate);

	if (strcmp(timers_type, "sof") == 0) {
		/* Update SOF-based XEC_PRE_REG timers */
		write_u32_reg(data->cdns_dev_regs, XEC_PRE_REG_250NS, clock_rate/4000000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_PRE_REG_1US,   clock_rate/1000000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_PRE_REG_10US,  clock_rate/100000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_PRE_REG_100US, clock_rate/10000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_PRE_REG_125US, clock_rate/8000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_PRE_REG_1MS,   clock_rate/1000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_PRE_REG_10MS,  clock_rate/100-1);
		write_u32_reg(data->cdns_dev_regs, XEC_PRE_REG_100MS, clock_rate/10-1);
	} else if (strcmp(timers_type, "lpm") == 0) {
		/* Update LPM-based XEC_LPM_PRE_REG timers */
		write_u32_reg(data->cdns_dev_regs, XEC_LPM_PRE_REG_250NS, clock_rate/4000000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_LPM_PRE_REG_1US,   clock_rate/1000000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_LPM_PRE_REG_10US,  clock_rate/100000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_LPM_PRE_REG_100US, clock_rate/10000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_LPM_PRE_REG_125US, clock_rate/8000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_LPM_PRE_REG_1MS,   clock_rate/1000-1);
		write_u32_reg(data->cdns_dev_regs, XEC_LPM_PRE_REG_10MS,  clock_rate/100-1);
		write_u32_reg(data->cdns_dev_regs, XEC_LPM_PRE_REG_100MS, clock_rate/10-1);
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
		dev_err(pdev->dev, "%s: gadget pointer is NULL\n", __func__);
		return -EINVAL;
	}

	if (!gadget->name || strcmp(gadget->name, "cdnsp-gadget") != 0) {
		dev_dbg(pdev->dev, "%s: not a cdnsp-gadget (name=%s), skipping\n", 
			 __func__, gadget->name ? gadget->name : "NULL");
		return 0;
	}	

	pdev = gadget_to_cdnsp(gadget);
	if (!pdev) {
		dev_err(pdev->dev, "%s: failed to get cdnsp_device from gadget\n", __func__);
		return -ENODEV;
	}

	if (!pdev->dev) {
		dev_err(pdev->dev, "%s: cdnsp_device has no associated device\n", __func__);
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
		dev_err(pdev->dev, "%s: no Hailo platform data available\n", __func__);
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
		reg_val |= PORT_OVERRIDE_SUSPEND_SFR;
		reg_val &= ~PORT_OVERRIDE_SESS_VLD_SFR;
		reg_val &= ~PORT_OVERRIDE_IDDIG_SFR;

		writel(reg_val, &regs->override);

		reg_val |= PORT_OVERRIDE_SLEEPM_SEL;
		reg_val |= PORT_OVERRIDE_SUSPEND_SEL;
		reg_val |= PORT_OVERRIDE_SESS_VLD_SEL;
		reg_val |= PORT_OVERRIDE_IDDIG_SEL;

		// reg_val |= 0x0F000500;
		writel(reg_val, &regs->override);

		dev_info(pdev->dev, "%s: USB2 phy AVVD core power is not conncted, set override reg = 0x%08x\n", __func__, reg_val);
	}

	return 0;
}

static int cdns_gadget_config_phase_quirk(struct platform_device *pdev)
{
	int ret;

	// Setting PRE REG Timers according to sof clock rate
	ret = cdns_hailo_update_timer_registers(pdev, "sof");
	if (ret) {
		return ret;
	}

	// Setting PRE LPM REG Timers according to lpm clock rate
	ret = cdns_hailo_update_timer_registers(pdev, "lpm");
	if (ret) {
		return ret;
	}

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

static bool is_probing_ready(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct device_node *child;
	struct phy *usb3_phy;

	for_each_child_of_node(node, child) {
		if (of_device_is_compatible(child, "cdnsp,usb3")) {
			/* Check if PHY is available for proper probe ordering */
			usb3_phy = of_phy_get(child, "cdns3,usb3-phy");
			if (IS_ERR(usb3_phy)) {
				if (PTR_ERR(usb3_phy) == -EPROBE_DEFER) {
					dev_info(dev, "PHY not ready, deferring probe for proper ordering\n");
					of_node_put(child);
					return false;
				}
				/* PHY not found or other error - proceed anyway */
				dev_info(dev, "PHY not found or error (%ld), proceeding anyway\n", PTR_ERR(usb3_phy));
			} else {
				/* PHY found and ready */
				dev_info(dev, "PHY found and ready, proceeding with probe\n");
				of_phy_put(usb3_phy);
			}
			of_node_put(child);
			return true;
		}
	}
	
	/* No compatible child found, proceed with probing */
	dev_info(dev, "No USB3 child node found, proceeding without PHY check\n");
	return true;
}

static void cdns_usb3_dr_mode_get(struct platform_device *pdev, int *dr_mode)
{
	const char *dr_mode_str;
	struct device_node *child;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	for_each_child_of_node(node, child) {
		if (of_device_is_compatible(child, "cdnsp,usb3")) {
			if (of_property_read_string(child, "dr_mode", &dr_mode_str)) {
				break;
			}

			if (!strcmp(dr_mode_str, DR_MODE_HOST)) {
				*dr_mode = USB_DR_MODE_HOST;
			} else if (!strcmp(dr_mode_str, DR_MODE_DEVICE)) {
				*dr_mode = USB_DR_MODE_PERIPHERAL;
			} else if(!strcmp(dr_mode_str, DR_MODE_OTG)) {
				*dr_mode = USB_DR_MODE_OTG;
			}
			dev_info(&pdev->dev, "node(%s) dr_mode is set to %s(%d)\n", child->name, dr_mode_str, *dr_mode);
			break;
		}
	}
}

static struct resource* cdns_usb3_ioresource_mem_get(struct platform_device *pdev, int dr_mode)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct device_node *child;
	char *resource_name;
	struct resource* dev_res;
	int index, ret;

	switch (dr_mode) {
		case USB_DR_MODE_PERIPHERAL:
			resource_name = "dev";
			break;
		case USB_DR_MODE_HOST:
			resource_name = "xhci";
			break;
		case USB_DR_MODE_OTG:
			resource_name = "otg";
			break;
		default:
			dev_err(&pdev->dev, "Unsupported dr_mode for resource retrieval: %d\n", dr_mode);
			return ERR_PTR(-EINVAL);
	}

	for_each_child_of_node(node, child) {
		if (of_device_is_compatible(child, "cdnsp,usb3")) {
			/* Find the index of the named resource */
			index = of_property_match_string(child, "reg-names", resource_name);
			if (index < 0) {
				dev_err(&pdev->dev, "node(%s) resource '%s' not found in reg-names\n", child->name, resource_name);
				return ERR_PTR(-ENOENT);
			}
			
			/* Allocate resource structure */
			dev_res = kzalloc(sizeof(*dev_res), GFP_KERNEL);
			if (!dev_res)
				return ERR_PTR(-ENOMEM);
			
			/* Get the resource by index */
			ret = of_address_to_resource(child, index, dev_res);
			if (ret) {
				dev_err(&pdev->dev, "node(%s) Failed to get resource '%s' at index %d: %d\n", child->name, resource_name, index, ret);
				kfree(dev_res);
				return ERR_PTR(ret);
			}
			
			dev_info(&pdev->dev, "node(%s) Found %s register resource: start=0x%08x, size=0x%08x\n", child->name, resource_name,
				(u32)dev_res->start, (u32)resource_size(dev_res));
			
			return dev_res;
		}
	}
	return ERR_PTR(-ENOENT);
}

static int cdns_hailo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct cdns_hailo *data;
	struct resource *res, *cdns_dev_res = NULL; //, *cdns_otg_res = NULL;
	int ret, irq, loop_index, virq;


	if (!node)
		return -ENODEV;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (!is_probing_ready(pdev)) {
		return -EPROBE_DEFER;
	}

	raw_spin_lock_init(&data->irq_lock);
	data->sw_irq_mask = 0;

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
		cdns_hailo_pdata.quirks |= CDNS3_VBUS_VALIDATION_CONTROLLED_BY_SFR; // Not controlled by OTG PHY
	}

    if (of_property_read_bool(node, "usbreset-as-system-reboot")) {
        dev_info(&pdev->dev, "USB reset will trigger system reboot\n");
		cdns_hailo_pdata.quirks |= CDNS3_USB_RESET_AS_SYSTEM_REBOOT;
    }

	// Iterate through the child nodes to find the cdns_usb3 node
	cdns_usb3_dr_mode_get(pdev, &data->dr_mode);
	if (data->dr_mode == USB_DR_MODE_UNKNOWN) {
		dev_info(&pdev->dev, "dr_mode property not found setting \"host\" as default mode\n");
		data->dr_mode = USB_DR_MODE_HOST;
	}

	/* Configure clocks based on DR mode */
	data->usb2_is_inactive = of_property_read_bool(node, "usb2-is-inactive");
	if (data->usb2_is_inactive) {
		data->num_core_clks = ARRAY_SIZE(hailo_cdns3_core_clks__no_usb2_refclk);
		data->core_clks = devm_kmemdup(dev, hailo_cdns3_core_clks__no_usb2_refclk,
					sizeof(hailo_cdns3_core_clks__no_usb2_refclk), GFP_KERNEL);
		dev_info(&pdev->dev, "Using device mode clocks (no usb2_refclk)\n");
	} else {
		data->num_core_clks = ARRAY_SIZE(hailo_cdns3_core_clks);
		data->core_clks = devm_kmemdup(dev, hailo_cdns3_core_clks,
					sizeof(hailo_cdns3_core_clks), GFP_KERNEL);
		dev_info(&pdev->dev, "Using host mode clocks (with usb2_refclk)\n");
	}

	if (!data->core_clks)
		return -ENOMEM;

	pm_runtime_get_sync(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	/* Note: reset deassert order matters: 1-usb_apb, 2-usb !!! */
	reset_control_deassert(data->usb_apb_rst);
	ret = clk_prepare_enable(data->pclk);
	if (ret)
		return ret;

	ret = devm_clk_bulk_get(dev, data->num_core_clks, data->core_clks);
	if (ret)
		return ret;

	if (data->dr_mode == USB_DR_MODE_PERIPHERAL) {
		cdns_dev_res = cdns_usb3_ioresource_mem_get(pdev, USB_DR_MODE_PERIPHERAL);
		if (IS_ERR(cdns_dev_res)) {
			return PTR_ERR(cdns_dev_res);
		}
		
		data->cdns_dev_regs = devm_ioremap(&pdev->dev, cdns_dev_res->start, resource_size(cdns_dev_res));
		if (!data->cdns_dev_regs) {
			dev_err(&pdev->dev, "can't map IOMEM resource for peripheral mode\n");
			return -ENOMEM;
		}

		ret = cdns_gadget_config_phase_quirk(pdev);
		if (ret) {
			dev_err(&pdev->dev, "failed to apply gadget quirks: %d\n", ret);
			return ret;
		}
	}
	
	// note: must be called before the core clocks are enabled
	cdns_hailo_init(data);

	reset_control_deassert(data->usb_rst);

	ret = clk_bulk_prepare_enable(data->num_core_clks, data->core_clks);
	if (ret)
		return ret;

	ret = of_platform_populate(node, NULL, cdns_hailo_auxdata, dev);
	if (ret) {
		dev_err(dev, "failed to create children: %d\n", ret);
		goto err;
	}

#ifdef CDNS_TIMERS_DEBUG	
	cdns_hailo_validate_timer_registers(pdev);
#endif

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

	/* Graceful shutdown sequence to prevent controller timeouts */
	dev_info(dev, "Starting graceful USB controller shutdown\n");

	/* Depopulate child devices first - let them clean up properly with IRQs available */
	of_platform_depopulate(dev);
	
	/* Additional delay for gadget unbind to complete */
	msleep(10);

	/* Remove IRQ domain AFTER all children are gone */
	if (data->irq_domain) {
		irq_domain_remove(data->irq_domain);
		data->irq_domain = NULL;
	}
	
	/* Disable clocks in reverse order */
	clk_bulk_disable_unprepare(data->num_core_clks, data->core_clks);
	clk_disable_unprepare(data->pclk);
	
	/* Assert resets in reverse order: usb first, then usb_apb */
	reset_control_assert(data->usb_rst);
	reset_control_assert(data->usb_apb_rst);
	msleep(10); /* Allow resets to take effect */
	
	/* Clean up runtime PM */
	pm_runtime_put_sync(dev);
	pm_runtime_set_suspended(dev);
	pm_runtime_disable(dev);
	platform_set_drvdata(pdev, NULL);

	dev_info(dev, "USB controller shutdown completed\n");
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
