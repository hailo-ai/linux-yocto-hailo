// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * XRP: Linux device driver for Xtensa Remote Processing
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_SHARE_H
#define XRP_SHARE_H

#include "xrp_common.h"
#include "xrp_kernel_defs.h"

#include <linux/types.h>

struct xrp_shared_allocation {
    enum ioctl_buffer_flags flags;
    uintptr_t vaddr;
    unsigned long size;
    unsigned long mm;
    struct xrp_allocation *allocation;
    struct hlist_node node;
};

struct xrp_alien_mapping {
    uintptr_t vaddr;
    unsigned long size;
    phys_addr_t paddr;
    struct xrp_allocation *allocation;
    struct xrp_shared_allocation *shared_allocation;
    enum { ALIEN_GUP,
           ALIEN_PFN_MAP,
           ALIEN_COPY,
    } type;
};

struct xrp_mapping {
    enum { XRP_MAPPING_NONE,
           XRP_MAPPING_NATIVE,
           XRP_MAPPING_ALIEN,
           XRP_MAPPING_KERNEL = 0x4,
    } type;
    union {
        struct {
            struct xrp_allocation *xrp_allocation;
            uintptr_t vaddr;
        } native;
        struct xrp_alien_mapping alien_mapping;
        struct {
            unsigned long size;
            uintptr_t vaddr;
            phys_addr_t paddr;
        } kernel;
    };
};

enum lut_mapping {
    LUT_MAPPING_LOG,
    LUT_MAPPING_IN_DATA,
    LUT_MAPPING_OUT_DATA,
    LUT_MAPPING_BUFFER_METADATA,
    LUT_MAPPING_COMM,

    NO_LUT_MAPPING,
};

bool xrp_cacheable(struct xvp *xvp, unsigned long pfn, unsigned long n_pages);

void xrp_dma_sync_for_device(
    struct xvp *xvp, phys_addr_t phys, unsigned long size,
    enum ioctl_buffer_flags flags);

void xrp_dma_sync_for_cpu(
    struct xvp *xvp, phys_addr_t phys, unsigned long size,
    enum ioctl_buffer_flags flags);

long xrp_share_kernel(
    struct xvp *xvp, void *virt, unsigned long size,
    enum ioctl_buffer_flags flags, phys_addr_t *paddr, uint32_t *dsp_paddr,
    struct xrp_mapping *mapping, enum lut_mapping lut_mapping, bool config_lut);

long xrp_share_dma(
    struct xvp *xvp, dma_addr_t addr, unsigned long size, phys_addr_t *paddr,
    uint32_t *dsp_paddr, enum lut_mapping lut_mapping, bool config_lut);

/* Share blocks of memory, from host to IVP or back.
 *
 * When sharing to IVP return physical addresses in paddr.
 * Areas allocated from the driver can always be shared in both directions.
 * Contiguous 3rd party allocations need to be shared to IVP before they can
 * be shared back.
 */
long xrp_share_block(
    struct xvp *xvp, void *virt, unsigned long size,
    enum ioctl_buffer_flags flags, phys_addr_t *paddr, uint32_t *dsp_paddr,
    struct xrp_mapping *mapping, enum lut_mapping lut_mapping, bool config_lut);

long xrp_unshare_kernel(
    struct xvp *xvp, struct xrp_mapping *mapping,
    enum ioctl_buffer_flags flags);

long xrp_unshare_block(
    struct xvp *xvp, struct xrp_mapping *mapping,
    enum ioctl_buffer_flags flags);

void xrp_lock_shared_allocations(void);

void xrp_unlock_shared_allocations(void);

struct xrp_shared_allocation *xrp_get_shared_allocation(
    enum ioctl_buffer_flags flags, void *virt, unsigned long size);

struct xrp_shared_allocation *xrp_add_shared_allocation(
    enum ioctl_buffer_flags flags, void *virt, unsigned long size,
    struct xrp_allocation *allocation);

void xrp_remove_shared_allocation(struct xrp_shared_allocation *p);

#endif