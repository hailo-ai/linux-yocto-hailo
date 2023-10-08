// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include "xrp_io.h"

#include "xrp_kernel_dsp_interface.h"

#include <linux/slab.h>
#include <linux/io.h>

inline void xrp_comm_write32(volatile void __iomem *addr, u32 v)
{
    __raw_writel(v, addr);
}

inline u32 xrp_comm_read32(volatile void __iomem *addr)
{
    return __raw_readl(addr);
}

inline void xrp_comm_write(volatile void __iomem *addr, void *p, size_t sz)
{
    size_t sz32 = sz & ~3;
    u32 v;

    while (sz32) {
        memcpy(&v, p, sizeof(v));
        __raw_writel(v, addr);
        p += 4;
        addr += 4;
        sz32 -= 4;
    }
    sz &= 3;
    if (sz) {
        v = 0;
        memcpy(&v, p, sz);
        __raw_writel(v, addr);
    }
}

inline void xrp_comm_read(volatile void __iomem *addr, void *p, size_t sz)
{
    size_t sz32 = sz & ~3;
    u32 v;

    while (sz32) {
        v = __raw_readl(addr);
        memcpy(p, &v, sizeof(v));
        p += 4;
        addr += 4;
        sz32 -= 4;
    }
    sz &= 3;
    if (sz) {
        v = __raw_readl(addr);
        memcpy(p, &v, sz);
    }
}

inline void __iomem *xrp_comm_put_tlv(
    void __iomem **addr, uint32_t type, uint32_t length)
{
    struct xrp_dsp_tlv __iomem *tlv = *addr;

    xrp_comm_write32(&tlv->type, type);
    xrp_comm_write32(&tlv->length, length);
    *addr = tlv->value + ((length + 3) / 4);
    return tlv->value;
}

inline void __iomem *xrp_comm_get_tlv(
    void __iomem **addr, uint32_t *type, uint32_t *length)
{
    struct xrp_dsp_tlv __iomem *tlv = *addr;

    *type = xrp_comm_read32(&tlv->type);
    *length = xrp_comm_read32(&tlv->length);
    *addr = tlv->value + ((*length + 3) / 4);
    return tlv->value;
}
