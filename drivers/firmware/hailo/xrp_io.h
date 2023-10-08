// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_IO_H
#define XRP_IO_H

#include "xrp_common.h"

#include "linux/types.h"

inline void xrp_comm_write32(volatile void __iomem *addr, u32 v);

inline u32 xrp_comm_read32(volatile void __iomem *addr);

inline void xrp_comm_write(volatile void __iomem *addr, void *p, size_t sz);

inline void xrp_comm_read(volatile void __iomem *addr, void *p, size_t sz);

inline void __iomem *xrp_comm_put_tlv(
    void __iomem **addr, uint32_t type, uint32_t length);

inline void __iomem *xrp_comm_get_tlv(
    void __iomem **addr, uint32_t *type, uint32_t *length);

#endif