// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2017 Cadence Design Systems Inc.
 */

#ifndef XRP_PRIVATE_ALLOC_H
#define XRP_PRIVATE_ALLOC_H

#include "xrp_alloc.h"

long xrp_init_private_pool(struct xrp_allocation_pool **pool,
               phys_addr_t start, u32 size);

#endif
