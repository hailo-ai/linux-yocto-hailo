// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 - 2026 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_IO_H
#define XRP_IO_H

#include "xrp_common.h"
#include "xrp_kernel_dsp_interface.h"

#include <linux/compiler.h>


static inline void xrp_comm_write32(volatile void *addr, u32 v)
{
    WRITE_ONCE(*(u32 *)addr, v);
}

static inline u32 xrp_comm_read32(volatile void *addr)
{
    return READ_ONCE(*(u32 *)addr);
}

static inline void xrp_comm_write(volatile void *addr, void *p, size_t sz)
{
    size_t sz32 = sz & ~3;
    u32 v;

    while (sz32) {
        memcpy(&v, p, sizeof(v));
        WRITE_ONCE(*(u32 *)addr, v);
        p += 4;
        addr += 4;
        sz32 -= 4;
    }
    sz &= 3;
    if (sz) {
        v = 0;
        memcpy(&v, p, sz);
        WRITE_ONCE(*(u32 *)addr, v);
    }
}

static inline void xrp_comm_read(volatile void *addr, void *p, size_t sz)
{
    size_t sz32 = sz & ~3;
    u32 v;

    while (sz32) {
        v = READ_ONCE(*(u32 *)addr);
        memcpy(p, &v, sizeof(v));
        p += 4;
        addr += 4;
        sz32 -= 4;
    }
    sz &= 3;
    if (sz) {
        v = READ_ONCE(*(u32 *)addr);
        memcpy(p, &v, sz);
    }
}

static inline void *xrp_comm_put_tlv(
    void **addr, uint32_t type, uint32_t length)
{
    struct xrp_dsp_tlv *tlv = *addr;

    xrp_comm_write32(&tlv->type, type);
    xrp_comm_write32(&tlv->length, length);
    *addr = tlv->value + ((length + 3) / 4);
    return tlv->value;
}

static inline void *xrp_comm_get_tlv(
    void **addr, uint32_t *type, uint32_t *length)
{
    struct xrp_dsp_tlv *tlv = *addr;

    *type = xrp_comm_read32(&tlv->type);
    *length = xrp_comm_read32(&tlv->length);
    *addr = tlv->value + ((*length + 3) / 4);
    return tlv->value;
}

#endif
