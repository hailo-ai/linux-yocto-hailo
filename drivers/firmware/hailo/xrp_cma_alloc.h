// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2017 Cadence Design Systems Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_CMA_ALLOC_H
#define XRP_CMA_ALLOC_H

#include "xrp_alloc.h"

struct device;

#ifdef CONFIG_CMA
long xrp_init_cma_pool(struct xrp_allocation_pool **pool, struct device *dev);
#else
static inline long xrp_init_cma_pool(struct xrp_allocation_pool **pool,
                     struct device *dev)
{
    return -ENXIO;
}
#endif

#endif
