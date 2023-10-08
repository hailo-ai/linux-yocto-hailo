// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2016 - 2017 Cadence Design Systems Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_ALLOC_H
#define XRP_ALLOC_H

#ifndef __KERNEL__

#include <stdint.h>
#include <xrp_atomic.h>
#include <xrp_thread_impl.h>

typedef uint32_t u32;
typedef uint32_t phys_addr_t;
typedef _Atomic uint32_t atomic_t;

struct mutex {
    xrp_mutex o;
};

static inline void atomic_inc(atomic_t *v)
{
    ++*(volatile atomic_t *)v;
}

static inline int atomic_dec_and_test(atomic_t *v)
{
    return --*(volatile atomic_t *)v == 0;
}

#endif

struct xrp_allocation_pool;
struct xrp_allocation;

struct xrp_allocation_ops {
    long (*alloc)(struct xrp_allocation_pool *allocation_pool,
              u32 size, u32 align, struct xrp_allocation **alloc);
    void (*free)(struct xrp_allocation *allocation);
    void (*free_pool)(struct xrp_allocation_pool *allocation_pool);
    phys_addr_t (*offset)(const struct xrp_allocation *allocation);
};

struct xrp_allocation_pool {
    const struct xrp_allocation_ops *ops;
};

struct xrp_allocation {
    struct xrp_allocation_pool *pool;
    struct xrp_allocation *next;
    phys_addr_t start;
    u32 size;
    atomic_t ref;
};

static inline void xrp_free_pool(struct xrp_allocation_pool *allocation_pool)
{
    allocation_pool->ops->free_pool(allocation_pool);
}

static inline void xrp_free(struct xrp_allocation *allocation)
{
    return allocation->pool->ops->free(allocation);
}

static inline long xrp_allocate(struct xrp_allocation_pool *allocation_pool,
                u32 size, u32 align,
                struct xrp_allocation **alloc)
{
    return allocation_pool->ops->alloc(allocation_pool,
                       size, align, alloc);
}

static inline void xrp_allocation_get(struct xrp_allocation *xrp_allocation)
{
    atomic_inc(&xrp_allocation->ref);
}

static inline int xrp_allocation_put(struct xrp_allocation *xrp_allocation)
{
    if (atomic_dec_and_test(&xrp_allocation->ref)) {
        xrp_allocation->pool->ops->free(xrp_allocation);
        return 1;
    }
    return 0;
}

static inline phys_addr_t xrp_allocation_offset(const struct xrp_allocation *allocation)
{
    return allocation->pool->ops->offset(allocation);
}

#endif
