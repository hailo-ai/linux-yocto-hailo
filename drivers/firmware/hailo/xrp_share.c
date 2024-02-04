// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * XRP: Linux device driver for Xtensa Remote Processing
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include "xrp_share.h"

#include "xrp_cma_alloc.h"
#include "xrp_kernel_defs.h"
#include "xrp_fs.h"
#include "xrp_hw.h"

#include <linux/dma-direct.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mmap_lock.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/dma-buf.h>

bool user_sync_buffer  = false;
module_param(user_sync_buffer , bool, 0644);
MODULE_PARM_DESC(user_sync_buffer,
    "If enabled, the driver won't sync the buffers before and after DSP access."
    "Userspace processes must explicitely request the driver to do the sync, "
    "if needed. "
    "If disabled, the driver will always sync the buffers "
    "before and after DSP access");

#ifndef __io_virt
#define __io_virt(a) ((void __force *)(a))
#endif

static uint32_t lut_mappings[] = {
    [LUT_MAPPING_COMM] = 0xC8000000,
    [LUT_MAPPING_LOG] = 0xd0000000,
    [LUT_MAPPING_IN_DATA] = 0xd8000000,
    [LUT_MAPPING_OUT_DATA] = 0xe0000000,
    [LUT_MAPPING_BUFFER_METADATA] = 0xe8000000,
};

#define DMA_LUT_SIZE (1 << 30)
#define DMA_LUT_MASK (DMA_LUT_SIZE - 1)

// each axi lookup table entry lets us access 128 MB of memory
#define LOOKUP_SIZE (1 << 27)
#define LOOKUP_MASK (LOOKUP_SIZE - 1)

#define KERNEL_TO_DSP_ADDR(addr, mapping) \
    (((addr) & LOOKUP_MASK) | lut_mappings[mapping])

static DEFINE_MUTEX(xrp_shared_allocations_lock);
static DEFINE_HASHTABLE(xrp_shared_allocations, 10);

static bool is_single_dma_lookup_buffer(uintptr_t buffer, size_t size)
{
    return (buffer & DMA_LUT_MASK) + size <= DMA_LUT_MASK;
}

static bool is_single_lookup_buffer(uintptr_t buffer, size_t size)
{
    return (buffer & LOOKUP_MASK) + size <= LOOKUP_SIZE;
}

static long copy_from_x(void *dst, const void *src, size_t sz, bool user)
{
    if (user) {
        return copy_from_user(dst, (void __user *)src, sz);
    } else {
        memcpy(dst, src, sz);
        return 0;
    }
}

static long copy_to_x(void *dst, const void *src, size_t sz, bool user)
{
    if (user) {
        return copy_to_user((void __user *)dst, src, sz);
    } else {
        memcpy(dst, src, sz);
        return 0;
    }
}

static long _xrp_copy_user_phys(
    struct xvp *xvp, uintptr_t vaddr, unsigned long size, phys_addr_t paddr,
    enum ioctl_buffer_flags flags, bool to_phys, bool user)
{
    if (pfn_valid(__phys_to_pfn(paddr))) {
        struct page *page = pfn_to_page(__phys_to_pfn(paddr));
        size_t page_offs = paddr & ~PAGE_MASK;
        size_t offs;

        if (!to_phys)
            xrp_dma_sync_for_cpu(xvp, paddr, size, flags);
        for (offs = 0; offs < size; ++page) {
            void *p = kmap(page);
            size_t sz = PAGE_SIZE - page_offs;
            size_t copy_sz = sz;
            unsigned long rc;

            if (!p)
                return -ENOMEM;

            if (size - offs < copy_sz)
                copy_sz = size - offs;

            if (to_phys)
                rc = copy_from_x(p + page_offs,
                         (void *)(vaddr + offs),
                         copy_sz, user);
            else
                rc = copy_to_x((void *)(vaddr + offs),
                           p + page_offs, copy_sz, user);

            page_offs = 0;
            offs += copy_sz;

            kunmap(page);
            if (rc)
                return -EFAULT;
        }
        if (to_phys)
            xrp_dma_sync_for_device(xvp, paddr, size, flags);
    } else {
        void __iomem *p = ioremap(paddr, size);
        unsigned long rc;

        if (!p) {
            dev_err(xvp->dev,
                "couldn't ioremap %pap x 0x%08x\n",
                &paddr, (u32)size);
            return -EINVAL;
        }
        if (to_phys)
            rc = copy_from_x(__io_virt(p),
                     (void *)vaddr, size, user);
        else
            rc = copy_to_x((void *)vaddr,
                       __io_virt(p), size, user);
        iounmap(p);
        if (rc)
            return -EFAULT;
    }
    return 0;
}

static long xrp_copy_user_to_phys(
    struct xvp *xvp, uintptr_t vaddr, unsigned long size, phys_addr_t paddr,
    enum ioctl_buffer_flags flags, bool user)
{
    return _xrp_copy_user_phys(xvp, vaddr, size, paddr, flags, true, user);
}

static long xrp_copy_user_from_phys(
    struct xvp *xvp, uintptr_t vaddr, unsigned long size, phys_addr_t paddr,
    enum ioctl_buffer_flags flags, bool user)
{
    return _xrp_copy_user_phys(xvp, vaddr, size, paddr, flags, false, user);
}

static long xvp_copy_virt_to_phys(
    struct xvp *xvp, enum ioctl_buffer_flags flags, uintptr_t vaddr,
    unsigned long size, phys_addr_t *paddr, struct xrp_alien_mapping *mapping,
    bool user)
{
    phys_addr_t phys;
    uintptr_t align = clamp(vaddr & -vaddr, 16ul, PAGE_SIZE);
    uintptr_t offset = vaddr & (align - 1);
    struct xrp_allocation *allocation;
    struct xrp_shared_allocation *shared_allocation;
    long rc;

    xrp_lock_shared_allocations();
    shared_allocation = xrp_get_shared_allocation(flags, (void *)vaddr, size);
    if (shared_allocation) {
        pr_debug("%s: sharing bounce buffer for va: 0x%08lx x 0x%08lx",
             __func__, vaddr, size);
        allocation = shared_allocation->allocation;
        xrp_allocation_get(allocation);
        phys = (allocation->start & -align) | offset;
        if (phys < allocation->start)
            phys += align;
    } else {
        rc = xrp_allocate(xvp->pool,
                  size + align, align, &allocation);
        if (rc < 0) {
            xrp_unlock_shared_allocations();
            return rc;
        }

        phys = (allocation->start & -align) | offset;
        if (phys < allocation->start)
            phys += align;

        if (flags & XRP_FLAG_READ) {
            if (xrp_copy_user_to_phys(xvp,
                          vaddr, size, phys,
                          flags, user)) {
                xrp_allocation_put(allocation);
                xrp_unlock_shared_allocations();
                return -EFAULT;
            }
        }
        shared_allocation = xrp_add_shared_allocation(flags, (void *)vaddr,
            size, allocation);
        if (!shared_allocation) {
            xrp_allocation_put(allocation);
            xrp_unlock_shared_allocations();
            return -ENOMEM;
        }
    }
    xrp_unlock_shared_allocations();

    *paddr = phys;
    *mapping = (struct xrp_alien_mapping){
        .vaddr = vaddr,
        .size = size,
        .paddr = *paddr,
        .allocation = allocation,
        .shared_allocation = shared_allocation,
        .type = ALIEN_COPY,
    };
    pr_debug("%s: copying to pa: %pap\n", __func__, paddr);

    return 0;
}

static int xrp_dma_direction(enum ioctl_buffer_flags flags)
{
    static const enum dma_data_direction xrp_dma_direction[] = {
        [0] = DMA_NONE,
        [XRP_FLAG_READ] = DMA_TO_DEVICE,
        [XRP_FLAG_WRITE] = DMA_FROM_DEVICE,
        [XRP_FLAG_READ_WRITE] = DMA_BIDIRECTIONAL,
    };
    return xrp_dma_direction[flags & XRP_FLAG_READ_WRITE];
}

static bool vma_needs_cache_ops(struct vm_area_struct *vma)
{
    pgprot_t prot = vma->vm_page_prot;

    return pgprot_val(prot) != pgprot_val(pgprot_noncached(prot)) &&
        pgprot_val(prot) != pgprot_val(pgprot_writecombine(prot));
}

static long xrp_writeback_alien_mapping(
    struct xvp *xvp, struct xrp_alien_mapping *alien_mapping,
    enum ioctl_buffer_flags flags, bool user)
{
    struct page *page;
    size_t nr_pages;
    size_t i;
    long ret = 0;

    switch (alien_mapping->type) {
    case ALIEN_GUP:
        xrp_dma_sync_for_cpu(
            xvp, alien_mapping->paddr, alien_mapping->size, flags);
        pr_debug("%s: dirtying alien GUP @va = %p, pa = %pap\n",
             __func__, (void __user *)alien_mapping->vaddr,
             &alien_mapping->paddr);
        page = pfn_to_page(__phys_to_pfn(alien_mapping->paddr));
        nr_pages = PFN_UP(alien_mapping->vaddr + alien_mapping->size) -
            PFN_DOWN(alien_mapping->vaddr);
        for (i = 0; i < nr_pages; ++i)
            SetPageDirty(page + i);
        break;

    case ALIEN_COPY:
        pr_debug("%s: synchronizing alien copy @pa = %pap back to %p\n",
             __func__, &alien_mapping->paddr,
             (void __user *)alien_mapping->vaddr);
        if (xrp_copy_user_from_phys(xvp,
                        alien_mapping->vaddr,
                        alien_mapping->size,
                        alien_mapping->paddr,
                        flags, user))
            ret = -EINVAL;
        break;

    default:
        break;
    }
    return ret;
}

static unsigned xvp_get_region_vma_count(
    uintptr_t virt, unsigned long size, struct vm_area_struct *vma)
{
    unsigned i;
    struct mm_struct *mm = current->mm;

    if (virt + size < virt)
        return 0;
    if (vma->vm_start > virt)
        return 0;
    if (vma->vm_start <= virt &&
        virt + size <= vma->vm_end)
        return 1;
    for (i = 2; ; ++i) {
        struct vm_area_struct *next_vma = find_vma(mm, vma->vm_end);

        if (!next_vma)
            return 0;
        if (next_vma->vm_start != vma->vm_end)
            return 0;
        vma = next_vma;
        if (virt + size <= vma->vm_end)
            return i;
    }
    return 0;
}


static void xrp_put_pages(phys_addr_t phys, unsigned long n_pages)
{
    struct page *page;
    unsigned long i;

    page = pfn_to_page(__phys_to_pfn(phys));
    for (i = 0; i < n_pages; ++i)
        put_page(page + i);
}

static void xrp_alien_mapping_destroy(struct xrp_alien_mapping *alien_mapping)
{
    switch (alien_mapping->type) {
    case ALIEN_GUP:
        xrp_put_pages(alien_mapping->paddr,
                  PFN_UP(alien_mapping->vaddr +
                     alien_mapping->size) -
                  PFN_DOWN(alien_mapping->vaddr));
        break;
    case ALIEN_COPY:
        xrp_lock_shared_allocations();
        if (xrp_allocation_put(alien_mapping->allocation))
            xrp_remove_shared_allocation(alien_mapping->shared_allocation);
        xrp_unlock_shared_allocations();
        break;
    default:
        break;
    }
}

static long xvp_pfn_virt_to_phys(
    struct xvp *xvp, struct vm_area_struct *vma, uintptr_t vaddr,
    unsigned long size, phys_addr_t *paddr, struct xrp_alien_mapping *mapping)
{
    int ret;
    unsigned long i;
    unsigned long nr_pages = PFN_UP(vaddr + size) - PFN_DOWN(vaddr);
    unsigned long pfn;

    ret = follow_pfn(vma, vaddr, &pfn);
    if (ret)
        return ret;

    *paddr = __pfn_to_phys(pfn) + (vaddr & ~PAGE_MASK);
 
    for (i = 1; i < nr_pages; ++i) {
        unsigned long next_pfn;
        phys_addr_t next_phys;

        ret = follow_pfn(vma, vaddr + (i << PAGE_SHIFT), &next_pfn);
        if (ret)
            return ret;
        if (next_pfn != pfn + 1) {
            pr_debug("%s: non-contiguous physical memory\n", __func__);
            return -EINVAL;
        }
        next_phys = __pfn_to_phys(next_pfn);
        pfn = next_pfn;
    }

    if (!is_single_dma_lookup_buffer(*paddr, size)) {
        pr_debug("%s: memory crossed 1GiB window\n", __func__);
        return -EINVAL;
    }

    *mapping = (struct xrp_alien_mapping){
        .vaddr = vaddr,
        .size = size,
        .paddr = *paddr,
        .type = ALIEN_PFN_MAP,
    };
    pr_debug("%s: success, paddr: %pap\n", __func__, paddr);
    return 0;
}

static long xvp_gup_virt_to_phys(
    struct xvp *xvp, uintptr_t vaddr, unsigned long size,
    phys_addr_t *paddr, struct xrp_alien_mapping *mapping)
{
    int ret;
    int i;
    int nr_pages;
    struct page **page;

    if (PFN_UP(vaddr + size) - PFN_DOWN(vaddr) > INT_MAX)
        return -EINVAL;

    nr_pages = PFN_UP(vaddr + size) - PFN_DOWN(vaddr);
    page = kmalloc(nr_pages * sizeof(void *), GFP_KERNEL);
    if (!page)
        return -ENOMEM;

    ret = get_user_pages_fast(vaddr, nr_pages, 1, page);
    if (ret < 0)
        goto out;

    if (ret < nr_pages) {
        pr_debug("%s: asked for %d pages, but got only %d\n",
             __func__, nr_pages, ret);
        nr_pages = ret;
        ret = -EINVAL;
        goto out_put;
    }

    for (i = 1; i < nr_pages; ++i) {
        if (page[i] != page[i - 1] + 1) {
            pr_debug("%s: non-contiguous physical memory\n",
                 __func__);
            ret = -EINVAL;
            goto out_put;
        }
    }

    *paddr = __pfn_to_phys(page_to_pfn(page[0])) + (vaddr & ~PAGE_MASK);
    if (!is_single_dma_lookup_buffer(*paddr, size)) {
        pr_debug("%s: memory crossed 1GiB window\n", __func__);
        ret = -EINVAL;
        goto out_put;
    }

    *mapping = (struct xrp_alien_mapping){
        .vaddr = vaddr,
        .size = size,
        .paddr = *paddr,
        .type = ALIEN_GUP,
    };
    ret = 0;
    pr_debug("%s: success, paddr: %pap\n", __func__, paddr);

out_put:
    if (ret < 0)
        for (i = 0; i < nr_pages; ++i)
            put_page(page[i]);
out:
    kfree(page);
    return ret;
}

void xrp_lock_shared_allocations(void)
{
    mutex_lock(&xrp_shared_allocations_lock);
}

void xrp_unlock_shared_allocations(void)
{
    mutex_unlock(&xrp_shared_allocations_lock);
}

struct xrp_shared_allocation *xrp_get_shared_allocation(
    enum ioctl_buffer_flags flags, void *virt, unsigned long size)
{
    uintptr_t vaddr = (uintptr_t)virt;
    unsigned long mm = (unsigned long)current->mm;
    struct xrp_shared_allocation *p;

    hash_for_each_possible(xrp_shared_allocations, p, node, vaddr ^ size ^ mm) {
        if (p->vaddr == vaddr && p->size == size && p->mm == mm) {
            return p;
        }
    }
    return NULL;
}

struct xrp_shared_allocation *xrp_add_shared_allocation(
    enum ioctl_buffer_flags flags, void *virt, unsigned long size,
    struct xrp_allocation *allocation)
{
    uintptr_t vaddr = (uintptr_t)virt;
    unsigned long mm = (unsigned long)current->mm;
    struct xrp_shared_allocation *p = kmalloc(sizeof(*p), GFP_KERNEL);

    if (!p)
        return NULL;

    p->flags = flags;
    p->vaddr = vaddr;
    p->size = size;
    p->mm = mm;
    p->allocation = allocation;

    hash_add(xrp_shared_allocations, &p->node, vaddr ^ size ^ mm);
    return p;
}

void xrp_remove_shared_allocation(struct xrp_shared_allocation *p)
{
    WARN_ON(!p);
    if (p) {
        hash_del(&p->node);
        kfree(p);
    }
}

bool xrp_cacheable(struct xvp *xvp, unsigned long pfn, unsigned long n_pages)
{
    unsigned long i;

    for (i = 0; i < n_pages; ++i)
        if (!pfn_valid(pfn + i))
            return false;
    return true;
}

void xrp_dma_sync_for_device(
    struct xvp *xvp, phys_addr_t phys, unsigned long size,
    enum ioctl_buffer_flags flags)
{
    dma_sync_single_for_device(
        xvp->dev, phys_to_dma(xvp->dev, phys), size, xrp_dma_direction(flags));
}

void xrp_dma_sync_for_cpu(
    struct xvp *xvp, phys_addr_t phys, unsigned long size,
    enum ioctl_buffer_flags flags)
{
    dma_sync_single_for_cpu(
        xvp->dev, phys_to_dma(xvp->dev, phys), size, xrp_dma_direction(flags));
}

long xrp_share_dma(
    struct xvp *xvp, dma_addr_t addr, unsigned long size, phys_addr_t *paddr,
    uint32_t *dsp_paddr, enum lut_mapping lut_mapping, bool config_lut)
{
    phys_addr_t phys = dma_to_phys(xvp->dev, addr);

    dev_dbg(xvp->dev, "%s: sharing dma buffer. Phys: %pap\n", __func__, &phys);
        
    if (lut_mapping != NO_LUT_MAPPING && dsp_paddr) {
        if (!is_single_lookup_buffer(phys, size)) {
            dev_err(xvp->dev,
                "%s: buffer %pap of size 0x%lx requires more than 1 axi lookup",
                __func__, &phys, size);
            return -EINVAL;
        }

        if (config_lut) {
            map_dsp_to_physical_address(
                xvp, KERNEL_TO_DSP_ADDR(phys, lut_mapping), phys);
        }
        *dsp_paddr = KERNEL_TO_DSP_ADDR(phys, lut_mapping);
    }

    *paddr = phys;

    return 0;
}

long xrp_share_kernel(
    struct xvp *xvp, void *virt, unsigned long size,
    enum ioctl_buffer_flags flags, phys_addr_t *paddr, uint32_t *dsp_paddr,
    struct xrp_mapping *mapping, enum lut_mapping lut_mapping, bool config_lut)
{
    phys_addr_t phys = __pa(virt);

    dev_dbg(xvp->dev, "%s: sharing kernel-only buffer: %pap\n",
        __func__, &phys);
        
    if (lut_mapping != NO_LUT_MAPPING && dsp_paddr) {
        if (!is_single_lookup_buffer(phys, size)) {
            dev_err(xvp->dev,
                "%s: buffer %pap of size 0x%lx requires more than 1 axi lookup",
                __func__, &phys, size);
            return -EINVAL;
        }

        if (config_lut) {
            map_dsp_to_physical_address(
                xvp, KERNEL_TO_DSP_ADDR(phys, lut_mapping), phys);
        }
        
        *dsp_paddr = KERNEL_TO_DSP_ADDR(phys, lut_mapping);
    }

    mapping->type = XRP_MAPPING_KERNEL;
    mapping->kernel.size = size;
    mapping->kernel.vaddr = (uintptr_t)virt;
    mapping->kernel.paddr = phys;    
    *paddr = phys;

    xrp_dma_sync_for_device(xvp, phys, size, flags);
    
    dev_dbg(xvp->dev, "%s: mapping = %p, mapping->type = %d\n",
        __func__, mapping, mapping->type);
    return 0;
}

long xrp_share_block(
    struct file *filp, void *virt, unsigned long size,
    enum ioctl_buffer_flags flags, phys_addr_t *paddr, uint32_t *dsp_paddr,
    struct xrp_mapping *mapping, enum lut_mapping lut_mapping, bool config_lut)
{
    uintptr_t vaddr = (uintptr_t)virt;
    phys_addr_t phys = ~0ul;
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vma;
    bool do_cache = true;
    long rc = -EINVAL;
    struct xvp *xvp = filp->private_data;

    vma = find_vma(mm, vaddr);
    if (!vma) {
        dev_err(
            xvp->dev, "%s: no vma for vaddr/size = 0x%08lx/0x%08lx\n",
            __func__, vaddr, size);
        return -EINVAL;
    }
    /*
     * Region requested for sharing should be within single VMA.
     * That's true for the majority of cases, but sometimes (e.g.
     * sharing buffer in the beginning of .bss which shares a
     * file-mapped page with .data, followed by anonymous page)
     * region will cross multiple VMAs. Support it in the simplest
     * way possible: start with get_user_pages and use shadow copy
     * if that fails.
     */
    switch (xvp_get_region_vma_count(vaddr, size, vma)) {
    case 0:
        dev_err(
            xvp->dev, "%s: bad vma for vaddr/size = 0x%08lx/0x%08lx\n",
            __func__, vaddr, size);
        dev_err(
            xvp->dev, "%s: vma->vm_start = 0x%08lx, vma->vm_end = 0x%08lx\n",
            __func__, vma->vm_start, vma->vm_end);
        return -EINVAL;
    case 1:
        break;
    default:
        dev_dbg(
            xvp->dev, "%s: multiple vmas cover vaddr/size = 0x%08lx/0x%08lx\n",
            __func__, vaddr, size);
        vma = NULL;
        break;
    }
    /*
     * And it need to be allocated from the same file descriptor
     */
    if (vma && (vma->vm_file == filp || xrp_is_known_file(vma->vm_file))) {
        struct xrp_allocation *xrp_allocation = vma->vm_private_data;

        phys = PFN_PHYS(vma->vm_pgoff) + vaddr - vma->vm_start;
        dev_dbg(
            xvp->dev, "%s: XRP allocation at vaddr: 0x%08lx, paddr: %pap\n",
            __func__, vaddr, &phys);
        
        mapping->type = XRP_MAPPING_NATIVE;
        mapping->native.xrp_allocation = xrp_allocation;
        mapping->native.vaddr = vaddr;
        xrp_allocation_get(xrp_allocation);
        do_cache = !user_sync_buffer && vma_needs_cache_ops(vma);
        rc = 0;
    }
    if (rc < 0) {
        struct xrp_alien_mapping *alien_mapping =
            &mapping->alien_mapping;
        unsigned long n_pages = PFN_UP(vaddr + size) - PFN_DOWN(vaddr);

        /* Otherwise this is alien allocation. */
        dev_dbg(
            xvp->dev, "%s: non-XVP allocation at vaddr: 0x%08lx\n",
            __func__, vaddr);

        /*
         * A range can only be mapped directly if it is either
         * uncached or HW-specific cache operations can handle it.
         */
        if (vma && vma->vm_flags & (VM_IO | VM_PFNMAP)) {
            rc = xvp_pfn_virt_to_phys(
                    xvp, vma, vaddr, size, &phys, alien_mapping);
            if (rc == 0 && vma_needs_cache_ops(vma) &&
                !xrp_cacheable(xvp, PFN_DOWN(phys), n_pages)) {
                dev_err(
                    xvp->dev, "%s: needs unsupported cache mgmt\n", __func__);
                rc = -EINVAL;
            }
        } else {
            mmap_read_unlock(mm);
            rc = xvp_gup_virt_to_phys(xvp, vaddr, size, &phys, alien_mapping);
            if (rc == 0 &&
                (!vma || vma_needs_cache_ops(vma)) &&
                !xrp_cacheable(xvp, PFN_DOWN(phys), n_pages)) {
                dev_err(
                    xvp->dev, "%s: needs unsupported cache mgmt\n", __func__);
                xrp_put_pages(phys, n_pages);
                rc = -EINVAL;
            }
            mmap_read_lock(mm);
        }
        if (rc == 0 && vma && !vma_needs_cache_ops(vma))
            do_cache = false;

        /*
         * If we couldn't share try to make a shadow copy.
         */
        if (rc < 0) {
            alien_mapping = &mapping->alien_mapping;
            rc = xvp_copy_virt_to_phys(xvp, flags, vaddr, size, &phys, 
                alien_mapping, true);
            do_cache = false;
        }

        /* We couldn't share it. Fail the request. */
        if (rc < 0) {
            dev_err(xvp->dev, "%s: couldn't map virt to phys\n", __func__);
            return -EINVAL;
        }

        phys = alien_mapping->paddr + vaddr - alien_mapping->vaddr;
        mapping->type = XRP_MAPPING_ALIEN;
    }

    if (lut_mapping != NO_LUT_MAPPING && dsp_paddr) {
        if (!is_single_lookup_buffer(phys, size)) {
            dev_err(
                xvp->dev,
                "%s: buffer %pap of size 0x%lx requires more than 1 axi lookup",
                __func__, &phys, size);
            return -EINVAL;
        }

        if (config_lut) {
            map_dsp_to_physical_address(
                xvp, KERNEL_TO_DSP_ADDR(phys, lut_mapping), phys);
        }
            
        *dsp_paddr = KERNEL_TO_DSP_ADDR(phys, lut_mapping);
    }

    *paddr = phys; 

    dev_dbg(
        xvp->dev, "%s: mapping = %p, mapping->type = %d\n",
        __func__, mapping, mapping->type);

    if (do_cache)
        xrp_dma_sync_for_device(xvp, phys, size, flags);

    return 0;
}


long xrp_share_dmabuf(struct file *filp, int fd, unsigned long size, enum ioctl_buffer_flags flags, dma_addr_t *paddr, struct xrp_mapping *mapping)
{
    long ret = 0;
    struct xvp *xvp = filp->private_data;
    struct dma_buf *dmabuf = NULL;
    struct dma_buf_attachment *attach = NULL;
    struct sg_table *sgt = NULL;
    struct scatterlist *sg = NULL;
    int i;

    dev_dbg(xvp->dev, "%s: fd = %d, size = %lu, flags = %d\n", __func__, fd, size, flags);

    dmabuf = dma_buf_get(fd);
    if (IS_ERR(dmabuf)) {
        dev_err(xvp->dev, "%s: dma_buf_get failed, err=%ld\n", __func__, PTR_ERR(dmabuf));
        ret = -EINVAL;
        goto l_exit;
    }

    attach = dma_buf_attach(dmabuf, xvp->dev);
    if (IS_ERR(attach)) {
        dev_err(xvp->dev, "%s: dma_buf_attach failed, err=%ld\n", __func__, PTR_ERR(attach));
        ret = -EINVAL;
        goto l_buf_get;
    }

    sgt = dma_buf_map_attachment(attach, xrp_dma_direction(flags));
    if (IS_ERR(sgt)) {
        dev_err(xvp->dev, "%s: dma_buf_map_attachment failed, err=%ld\n", __func__, PTR_ERR(sgt));
        ret = -EINVAL;
        goto l_buf_attach;
    }

    // print out the scatterlist for debug purposes
    for_each_sgtable_dma_sg(sgt, sg, i) {
        dma_addr_t sg_dma_addr = sg_dma_address(sg);

        dev_dbg(xvp->dev, "%s: sg[%d] = %pad, len = %u, offset = %u\n",
            __func__, i, &sg_dma_addr, sg_dma_len(sg), sg->offset); 
    }

    if (sgt->nents > 1) {
        dev_err(xvp->dev, "%s: buffer is not contiguous\n", __func__);
        ret = -EINVAL;
        goto l_buf_map;
    }

    sg = sgt->sgl;

    if (sg_dma_len(sg) < size) {
        dev_err(xvp->dev, "%s: Dma mapped length (%u) is smaller than requested size (%lu)\n", __func__, sg_dma_len(sg), size);
        ret = -EINVAL;
        goto l_buf_map;
    }

    *paddr = sg_dma_address(sg);

    mapping->type = XRP_MAPPING_DMABUF;
    mapping->dmabuf.attachment = attach;
    mapping->dmabuf.dmabuf = dmabuf;
    mapping->dmabuf.sgt = sgt;

    goto l_exit;

l_buf_map:
    dma_buf_unmap_attachment(attach, sgt, xrp_dma_direction(flags));
l_buf_attach:
    dma_buf_detach(dmabuf, attach);
l_buf_get:
    dma_buf_put(dmabuf);
l_exit:
    return ret;
}

static long xrp_unshare_kernel(
    struct xvp *xvp, struct xrp_mapping *mapping, enum ioctl_buffer_flags flags)
{
    dev_dbg(xvp->dev, "%s: mapping = %p, mapping->type = %d\n",
        __func__, mapping, mapping->type);

    if (flags & XRP_FLAG_WRITE) {
        xrp_dma_sync_for_cpu(xvp,
            mapping->kernel.paddr, mapping->kernel.size, flags);
    }

    return 0;
}

static long xrp_unshare_alien(
    struct xvp *xvp, struct xrp_mapping *mapping, enum ioctl_buffer_flags flags)
{
    long ret = 0;

    dev_dbg(xvp->dev, "%s: mapping = %p, mapping->type = %d\n",
        __func__, mapping, mapping->type);
    
    if (flags & XRP_FLAG_WRITE) {
        ret = xrp_writeback_alien_mapping(xvp,
                            &mapping->alien_mapping,
                            flags,
                            true);
    }
    xrp_alien_mapping_destroy(&mapping->alien_mapping);
    
    return ret;
}

static long xrp_unshare_native(
    struct xvp *xvp, struct xrp_mapping *mapping, enum ioctl_buffer_flags flags)
{
    dev_dbg(xvp->dev, "%s: mapping = %p, mapping->type = %d\n",
        __func__, mapping, mapping->type);

    if ((flags & XRP_FLAG_WRITE) && !user_sync_buffer) {
        xrp_dma_sync_for_cpu(xvp,
            mapping->native.xrp_allocation->start,
            mapping->native.xrp_allocation->size, flags);
    }

    xrp_allocation_put(mapping->native.xrp_allocation);

    return 0;
}

static long xrp_unshare_dmabuf(struct xvp *xvp, struct xrp_mapping *mapping, enum ioctl_buffer_flags flags)
{
    struct dma_buf *dmabuf = mapping->dmabuf.dmabuf;
    struct dma_buf_attachment *attach = mapping->dmabuf.attachment;
    struct sg_table *sgt = mapping->dmabuf.sgt;
    int direction = xrp_dma_direction(flags);

    dev_dbg(xvp->dev, "%s: dmabuf = %p, attachment = %p, sgt = %p, flags = %d\n", __func__, dmabuf, attach, sgt, flags);
    
    if (direction == DMA_NONE) {
        // DMA_NONE is illegal direction for dma_buf_unmap_attachment
        direction = DMA_TO_DEVICE;
    }
    dma_buf_unmap_attachment(attach, sgt, direction);
    dma_buf_detach(dmabuf, attach);
    dma_buf_put(dmabuf);

    return 0;
}

long xrp_unshare(struct xvp *xvp, struct xrp_mapping *mapping, enum ioctl_buffer_flags flags)
{
    long ret = 0;

    switch (mapping->type) {
    case XRP_MAPPING_KERNEL:
        ret = xrp_unshare_kernel(xvp, mapping, flags);
        break;

    case XRP_MAPPING_NATIVE:
        ret = xrp_unshare_native(xvp, mapping, flags);
        break;

    case XRP_MAPPING_ALIEN:
        ret = xrp_unshare_alien(xvp, mapping, flags);
        break;

    case XRP_MAPPING_DMABUF:
        ret = xrp_unshare_dmabuf(xvp, mapping, flags);
        break;

    case XRP_MAPPING_NONE:
        break;

    default:
        dev_err(xvp->dev, "%s: unknown mapping type: %d\n", __func__, mapping->type);
        ret = -EINVAL;
        break;
    }

    mapping->type = XRP_MAPPING_NONE;

    return ret;
}

long xrp_ioctl_sync_dma(struct file *filp, struct xrp_ioctl_sync_buffer __user *p)
{
    uintptr_t vaddr;
    phys_addr_t phys;
    struct vm_area_struct *vma;
    struct xrp_ioctl_sync_buffer sync_buffer;
    struct xvp *xvp = filp->private_data;

    if (!user_sync_buffer) {
        dev_warn_once(
            xvp->dev,
            "%s: dma sync is permitted only when user_sync_buffer "
            "option is turned on\n", __func__);
        return 0;
    }

    if (copy_from_user(&sync_buffer, p, sizeof(*p))) {
        dev_err(xvp->dev, "%s: copy from user failed\n", __func__);
        return -EFAULT;
    }

    vaddr = (uintptr_t)sync_buffer.addr;

    mmap_read_lock(current->mm);
    vma = find_vma(current->mm, vaddr);
    mmap_read_unlock(current->mm);

    if (!vma) {
        dev_err(xvp->dev, "%s: no vma for vaddr = 0x%08lx\n", __func__, vaddr);
        return -EINVAL;
    }

    if (!(vma && (vma->vm_file == filp || xrp_is_known_file(vma->vm_file)))) {
        dev_err(
            xvp->dev,"%s: non-XVP allocation at vaddr: 0x%08lx\n",
            __func__, vaddr);
        return -EINVAL;
    }

    phys = PFN_PHYS(vma->vm_pgoff) + vaddr - vma->vm_start;
    pr_debug("%s: XRP allocation at vaddr: 0x%08lx, paddr: %pap\n",
                __func__, vaddr, &phys);

    switch (sync_buffer.access_time) {
    case XRP_FLAG_BUFFER_SYNC_START:
        xrp_dma_sync_for_cpu(
            xvp, phys, sync_buffer.size, sync_buffer.direction);
        break;

    case XRP_FLAG_BUFFER_SYNC_END:
        xrp_dma_sync_for_device(
            xvp, phys, sync_buffer.size, sync_buffer.direction);
        break;
    }

    return 0;
}
