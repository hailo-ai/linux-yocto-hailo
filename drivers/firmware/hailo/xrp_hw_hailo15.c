// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 - 2026 Hailo Technologies Ltd. All rights reserved.
 */

#include "xrp_hw.h"

/* DSP memories */
#define DSP_IRAM_ADDRESS (0x60180000)
#define DSP_IRAM_SIZE (0x4000)
#define DSP_DRAM0_ADDRESS (0x60100000)
#define DSP_DRAM0_SIZE (0x60000)
#define DSP_DRAM1_ADDRESS (0x60200000)
#define DSP_DRAM1_SIZE (0x60000)

/* Register offsets (dsp config) */
#define DSP_CONFIG__DSP_CFG (0x4)
#define DSP_CONFIG__AltResetVec (0x8)
#define DSP_CONFIG__DSP_PARITY_ERR_MASK (0x194)
#define DSP_CONFIG__DSP_IP_FAULT_MASK (0x1a4)
#define DSP_CONFIG__DSP_INT_FATAL_MASK (0x1b4)
#define DSP_CONFIG__DSP_INT_NONFATAL_MASK (0x1bc)
#define DSP_CONFIG__DSP_ERR_IRQ_MASK (0x1c4)
#define DSP_CONFIG__RUNSTALL (0x1d4)
#define DSP_CONFIG__DSP_DEBUG_CFG (0x274)
#define DSP_CONFIG__DSP_FAULT_MASK(x) (0x1e0 + 4 * (x))
#define DSP_CONFIG__DSP_AXI_MASTER(x) (0x1e8 + 4 * (x))

/* DSP to physical address translations */
#define DSP_CONFIG__PHYSICAL_ADDRESS_BITS(addr) (((uint64_t)(addr) >> 27) & 0x1ff)
#define DSP_CONFIG__DSP_ADDRESS_BITS(addr) (((uint32_t)(addr) >> 27) & 0xf)

/* DSP_CFG bit offsets */
#define DSP_CONFIG__WWDT_EXT_COUNTER_DIS__OFFSET (18)
#define DSP_CONFIG__DSP_DEBUG_CFG__DBGEN__OFFSET (4)
#define DSP_CONFIG__DSP_DEBUG_CFG__SPIDEN__OFFSET (6)

#define DSP_CONFIG_BIT(name) (1 << DSP_CONFIG__##name##__OFFSET)

static void map_dsp_to_physical_address_hailo15(struct xvp *xvp, uint32_t dsp_address, phys_addr_t physical_address)
{
    int offset = DSP_CONFIG__DSP_ADDRESS_BITS(dsp_address);
    int phys_offset = DSP_CONFIG__PHYSICAL_ADDRESS_BITS(physical_address);
    dev_dbg(xvp->dev, "Mapping dsp lut %d to address %x\n", offset, phys_offset);
    dsp_config_writel(xvp, DSP_AXI_MASTER(offset), phys_offset);
}

static void enable_jtag_hailo15(struct xvp *xvp)
{
	uint32_t dsp_debug_cfg_value = dsp_config_readl(xvp, DSP_DEBUG_CFG);
    dsp_config_writel(
        xvp, DSP_DEBUG_CFG,
        dsp_debug_cfg_value | DSP_CONFIG_BIT(DSP_DEBUG_CFG__DBGEN) | DSP_CONFIG_BIT(DSP_DEBUG_CFG__SPIDEN));
}

static void disable_wwdt_hailo15(struct xvp *xvp)
{
    uint32_t dsp_cfg_value;
    dsp_cfg_value = dsp_config_readl(xvp, DSP_CFG);
    dsp_config_writel(xvp, DSP_CFG, dsp_cfg_value | DSP_CONFIG_BIT(WWDT_EXT_COUNTER_DIS));
}

static void open_interrupts_hailo15(struct xvp *xvp)
{
    dsp_config_writel(xvp, DSP_INT_FATAL_MASK, 0xFFFFFFFF);
    dsp_config_writel(xvp, DSP_INT_NONFATAL_MASK, 0xFFFFFFFF);
    dsp_config_writel(xvp, DSP_IP_FAULT_MASK, 0xFFFFFFF7); /* mask wwdt heartbeat (bit 3) */
    dsp_config_writel(xvp, DSP_ERR_IRQ_MASK, 0xFFFFFFFF);
    dsp_config_writel(xvp, DSP_FAULT_MASK(0), 0xFFFFFFFF);
    dsp_config_writel(xvp, DSP_FAULT_MASK(1), 0xFFFFFFFF);
    dsp_config_writel(xvp, DSP_PARITY_ERR_MASK, 0xFFFFFFFF);
}

static void init_mem_ranges_hailo15(struct xvp *xvp)
{
    xvp->mem_ranges.local[LOCAL_MEM_RANGES_IRAM].start = DSP_IRAM_ADDRESS;
    xvp->mem_ranges.local[LOCAL_MEM_RANGES_IRAM].end = DSP_IRAM_ADDRESS + DSP_IRAM_SIZE;
    xvp->mem_ranges.local[LOCAL_MEM_RANGES_DRAM0].start = DSP_DRAM0_ADDRESS;
    xvp->mem_ranges.local[LOCAL_MEM_RANGES_DRAM0].end = DSP_DRAM0_ADDRESS + DSP_DRAM0_SIZE;
    xvp->mem_ranges.local[LOCAL_MEM_RANGES_DRAM1].start = DSP_DRAM1_ADDRESS;
    xvp->mem_ranges.local[LOCAL_MEM_RANGES_DRAM1].end = DSP_DRAM1_ADDRESS + DSP_DRAM1_SIZE;
}

static void configure_reset_vector_hailo15(struct xvp *xvp)
{
    dsp_config_writel(xvp, AltResetVec, xvp->reset_vector_address);
}

static void xrp_halt_dsp_hailo15(struct xvp *xvp)
{
    dsp_config_writel(xvp, RUNSTALL, 1);
}

static void xrp_release_dsp_hailo15(struct xvp *xvp)
{
    dsp_config_writel(xvp, RUNSTALL, 0);
}

static const struct xrp_hw_ops hw_ops = {
    .map_dsp_to_physical_address = map_dsp_to_physical_address_hailo15,
    .init_mem_ranges = init_mem_ranges_hailo15,
    .enable_jtag = enable_jtag_hailo15,
    .disable_wwdt = disable_wwdt_hailo15,
    .open_interrupts = open_interrupts_hailo15,
    .configure_reset_vector = configure_reset_vector_hailo15,  
    .xrp_halt_dsp = xrp_halt_dsp_hailo15,
    .xrp_release_dsp = xrp_release_dsp_hailo15,
};

int xrp_init_hw_hailo15(struct platform_device *pdev, struct xvp *xvp)
{
    dev_dbg(&pdev->dev, "%s\n", __func__);
    xvp->hw_ops = &hw_ops;

    return xrp_init_hw_common(pdev, xvp);
}