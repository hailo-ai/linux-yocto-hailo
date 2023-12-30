// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2017 Cadence Design Systems Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
#include <linux/dma-mapping.h>
#else
#include <linux/dma-direct.h>
#endif
#include <linux/kernel.h>
#include <linux/slab.h>
#include "xrp_cma_alloc.h"

struct xrp_cma_allocation {
    struct xrp_allocation allocation;
    void *kvaddr;
};

struct xrp_cma_pool {
    struct xrp_allocation_pool pool;
    struct device *dev;
};

static long xrp_cma_alloc(struct xrp_allocation_pool *allocation_pool,
              u32 size, u32 align, struct xrp_allocation **alloc)
{
    struct xrp_cma_pool *pool = container_of(allocation_pool,
                         struct xrp_cma_pool, pool);
    struct xrp_cma_allocation *new_cma;
    struct xrp_allocation *new;
    dma_addr_t dma_addr;
    phys_addr_t phys_addr;
    void *kvaddr;

    size = ALIGN(size, PAGE_SIZE);

    new_cma = kzalloc(sizeof(struct xrp_cma_allocation), GFP_KERNEL);
    if (!new_cma)
        return -ENOMEM;

    new = &new_cma->allocation;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)
    {
        DEFINE_DMA_ATTRS(attrs);

        dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
        kvaddr = dma_alloc_attrs(pool->dev, size, &dma_addr,
                     GFP_KERNEL, &attrs);
    }
#else
    kvaddr = dma_alloc_attrs(pool->dev, size, &dma_addr, GFP_KERNEL,
                 DMA_ATTR_NO_KERNEL_MAPPING);
#endif
    if (!kvaddr) {
        kfree(new_cma);
        return -ENOMEM;
    }
    phys_addr = dma_to_phys(pool->dev, dma_addr);
    new->pool = allocation_pool;
    new->start = phys_addr;
    new->size = size;
    atomic_set(&new->ref, 0);
    xrp_allocation_get(new);
    new_cma->kvaddr = kvaddr;
    *alloc = new;

    dev_dbg(pool->dev, "Allocated dma buffer: kvaddr: %p, paddr: %pap, size: %d", kvaddr, &phys_addr, size);

    return 0;
}

static void xrp_cma_free(struct xrp_allocation *xrp_allocation)
{
    struct xrp_cma_pool *pool = container_of(xrp_allocation->pool,
                         struct xrp_cma_pool, pool);
    struct xrp_cma_allocation *a = container_of(xrp_allocation,
                            struct xrp_cma_allocation,
                            allocation);

    dev_dbg(pool->dev, "Freeing dma buffer: kvaddr: %p, paddr: %pap, size: %d", a->kvaddr, &xrp_allocation->start, xrp_allocation->size);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)
    DEFINE_DMA_ATTRS(attrs);

    dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
    dma_free_attrs(pool->dev, xrp_allocation->size,
               a->kvaddr,
               phys_to_dma(pool->dev, xrp_allocation->start),
               &attrs);
#else
    dma_free_attrs(pool->dev, xrp_allocation->size,
               a->kvaddr,
               phys_to_dma(pool->dev, xrp_allocation->start),
               DMA_ATTR_NO_KERNEL_MAPPING);
#endif
    kfree(a);
}

static void xrp_cma_free_pool(struct xrp_allocation_pool *allocation_pool)
{
    struct xrp_cma_pool *pool = container_of(allocation_pool,
                         struct xrp_cma_pool, pool);

    kfree(pool);
}

static phys_addr_t xrp_cma_addr(const struct xrp_allocation *allocation)
{
    return allocation->start;
}

static const struct xrp_allocation_ops xrp_cma_pool_ops = {
    .alloc = xrp_cma_alloc,
    .free = xrp_cma_free,
    .free_pool = xrp_cma_free_pool,
    .addr = xrp_cma_addr,
};

long xrp_init_cma_pool(struct xrp_allocation_pool **ppool, struct device *dev)
{
    struct xrp_cma_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);

    if (!pool)
        return -ENOMEM;

    pool->pool.ops = &xrp_cma_pool_ops;
    pool->dev = dev;
    *ppool = &pool->pool;
    return 0;
}
