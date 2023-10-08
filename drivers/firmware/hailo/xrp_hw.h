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

int xrp_enable_dsp(struct xvp *xvp);

int xrp_disable_dsp(struct xvp *xvp);

void xrp_halt_dsp(struct xvp *xvp);

void xrp_release_dsp(struct xvp *xvp);

void xrp_send_device_irq(struct xvp *xvp);

void xrp_memcpy_tohw(void __iomem *dst, const void *src, size_t sz);

void xrp_memset_hw(void __iomem *to, int c, size_t sz);

bool xrp_is_cmd_complete(struct xvp *xvp, struct xrp_comm *xrp_comm);

long xrp_init_hw(struct platform_device *pdev, struct xvp *xvp);

bool is_valid_fw_addr(struct xvp *xvp, phys_addr_t addr);

void map_dsp_to_physical_address(struct xvp *xvp, uint32_t dsp_address, phys_addr_t physical_address);

#endif