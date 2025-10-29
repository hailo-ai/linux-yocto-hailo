// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_HW_H
#define XRP_HW_H

#include "xrp_common.h"

#include <linux/platform_device.h>
#include <linux/types.h>

struct xrp_hw_ops {
    void (*map_dsp_to_physical_address)(struct xvp *xvp, uint32_t dsp_address, phys_addr_t physical_address);
    void (*init_mem_ranges)(struct xvp *xvp);
    void (*enable_jtag)(struct xvp *xvp);
    void (*disable_wwdt)(struct xvp *xvp);
    void (*open_interrupts)(struct xvp *xvp);
    void (*configure_reset_vector)(struct xvp *xvp);
    void (*xrp_halt_dsp)(struct xvp *xvp);
    void (*xrp_release_dsp)(struct xvp *xvp);
};

void map_dsp_to_physical_address(struct xvp *xvp, uint32_t dsp_address, phys_addr_t physical_address);

int xrp_enable_dsp(struct xvp *xvp);

void xrp_disable_dsp(struct xvp *xvp);

void xrp_halt_dsp(struct xvp *xvp);

void xrp_release_dsp(struct xvp *xvp);

void xrp_send_device_irq(struct xvp *xvp);

void xrp_memcpy_tohw(void __iomem *dst, const void *src, size_t sz);

void xrp_memset_hw(void __iomem *to, int c, size_t sz);

bool xrp_is_cmd_complete(struct xvp *xvp, struct xrp_comm *xrp_comm);

long xrp_init_hw_common(struct platform_device *pdev, struct xvp *xvp);

long xrp_init_hw_hailo15(struct platform_device *pdev, struct xvp *xvp);

long xrp_init_hw_hailo15l(struct platform_device *pdev, struct xvp *xvp);

bool is_valid_fw_addr(struct xvp *xvp, phys_addr_t addr);

#define dsp_config_writel(xvp, reg, value) _dsp_config_writel((xvp), DSP_CONFIG__##reg, (value))
#define dsp_config_readl(xvp, reg) _dsp_config_readl((xvp), DSP_CONFIG__##reg)

void _dsp_config_writel(struct xvp *xvp, size_t offset, u32 value);
u32 _dsp_config_readl(struct xvp *xvp, size_t offset);

#endif
