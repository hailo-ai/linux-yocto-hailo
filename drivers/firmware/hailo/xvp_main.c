// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * XRP: Linux device driver for Xtensa Remote Processing
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/acpi.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-direct.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/mmap_lock.h>

#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#include "xrp_cma_alloc.h"
#include "xrp_firmware.h"
#include "xrp_hw.h"
#include "xrp_common.h"
#include "xrp_kernel_defs.h"
#include "xrp_kernel_dsp_interface.h"
#include "xrp_private_alloc.h"
#include "xrp_log.h"
#include "xrp_share.h"
#include "xrp_exec.h"
#include "xrp_fs.h"
#include "xrp_io.h"
#include "xrp_boot.h"

#define DRIVER_NAME "xrp"

static DEFINE_IDA(xvp_nodeid);

static inline void xvp_file_lock(struct xvp *xvp)
{
    spin_lock(&xvp->busy_list_lock);
}

static inline void xvp_file_unlock(struct xvp *xvp)
{
    spin_unlock(&xvp->busy_list_lock);
}

static void xrp_allocation_queue(struct xvp *xvp,
                 struct xrp_allocation *xrp_allocation)
{
    xvp_file_lock(xvp);

    xrp_allocation->next = xvp->busy_list;
    xvp->busy_list = xrp_allocation;

    xvp_file_unlock(xvp);
}

static struct xrp_allocation *
xrp_allocation_dequeue(struct xvp *xvp, phys_addr_t paddr, size_t size)
{
    struct xrp_allocation **pcur;
    struct xrp_allocation *cur;

    xvp_file_lock(xvp);

    for (pcur = &xvp->busy_list; (cur = *pcur);
         pcur = &((*pcur)->next)) {
        if (paddr >= cur->start &&
            paddr + size - cur->start <= cur->size) {
            *pcur = cur->next;
            break;
        }
    }

    xvp_file_unlock(xvp);
    return cur;
}

static long xrp_ioctl_alloc(struct file *filp, struct xrp_ioctl_alloc __user *p)
{
    struct xvp *xvp = filp->private_data;
    struct xrp_allocation *xrp_allocation;
    unsigned long vaddr;
    struct xrp_ioctl_alloc xrp_ioctl_alloc;
    long err;

    if (copy_from_user(&xrp_ioctl_alloc, p, sizeof(*p)))
        return -EFAULT;

    err = xrp_allocate(xvp->pool, xrp_ioctl_alloc.size,
                xrp_ioctl_alloc.align, &xrp_allocation);
    if (err)
        return err;

    xrp_allocation_queue(xvp, xrp_allocation);

    vaddr = vm_mmap(filp, 0, xrp_allocation->size,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            xrp_allocation_addr(xrp_allocation));

    if (IS_ERR((void *)vaddr)) {
        return PTR_ERR((void *)vaddr);
    }

    xrp_ioctl_alloc.addr = vaddr;

    if (copy_to_user(p, &xrp_ioctl_alloc, sizeof(*p))) {
        vm_munmap(vaddr, xrp_ioctl_alloc.size);
        return -EFAULT;
    }
    return 0;
}

static long xrp_ioctl_free(struct file *filp, struct xrp_ioctl_alloc __user *p)
{
    struct xvp *xvp = filp->private_data;
    struct mm_struct *mm = current->mm;
    struct xrp_ioctl_alloc xrp_ioctl_alloc;
    struct vm_area_struct *vma;
    unsigned long start;
    size_t size;

    if (copy_from_user(&xrp_ioctl_alloc, p, sizeof(*p)))
        return -EFAULT;

    start = xrp_ioctl_alloc.addr;
    dev_dbg(
        xvp->dev, "%s: Request for freeing vaddr: 0x%08lx\n", __func__, start);

    mmap_read_lock(mm);
    vma = find_vma(mm, start);

    if (vma && vma->vm_file == filp && vma->vm_start <= start &&
        start < vma->vm_end) {
        start = vma->vm_start;
        size = vma->vm_end - vma->vm_start;
        mmap_read_unlock(mm);
        dev_dbg(
            xvp->dev, "%s: Unmapping vma: vaddr: 0x%08lx, size: %zu\n",
            __func__, start, size);
        return vm_munmap(start, size);
    }
    dev_dbg(
        xvp->dev, "%s: no vma/bad vma for vaddr: 0x%08lx\n", __func__, start);
    mmap_read_unlock(mm);

    return -EINVAL;
    
}

static long xvp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval;
    struct xvp *xvp = filp->private_data;

    switch (cmd) {
    case XRP_IOCTL_ALLOC:
        dev_dbg(xvp->dev, "%s: XRP_IOCTL_ALLOC\n", __func__);
        retval = xrp_ioctl_alloc(filp, (struct xrp_ioctl_alloc __user *)arg);
        break;

    case XRP_IOCTL_FREE:
        dev_dbg(xvp->dev, "%s: XRP_IOCTL_FREE\n", __func__);
        retval = xrp_ioctl_free(filp, (struct xrp_ioctl_alloc __user *)arg);
        break;

    case XRP_IOCTL_QUEUE:
    case XRP_IOCTL_QUEUE_NS:
        dev_dbg(xvp->dev, "%s: XRP_IOCTL_QUEUE\n", __func__);
        retval = xrp_ioctl_submit_sync(
            filp, (struct xrp_ioctl_queue __user *)arg);
        break;

    case XRP_IOCTL_DMA_SYNC:
        dev_dbg(xvp->dev, "%s: XRP_IOCTL_SYNC\n", __func__);
        retval = xrp_ioctl_sync_dma(
            filp, (struct xrp_ioctl_sync_buffer __user *)arg);
        break;

    default:
        dev_err(xvp->dev, "%s: Unknown command (%d)\n", __func__, cmd);
        retval = -EINVAL;
        break;
    }
    return retval;
}

static void xvp_vm_open(struct vm_area_struct *vma)
{
    xrp_allocation_get(vma->vm_private_data);
}

static void xvp_vm_close(struct vm_area_struct *vma)
{
    xrp_allocation_put(vma->vm_private_data);
}

static const struct vm_operations_struct xvp_vm_ops = {
    .open = xvp_vm_open,
    .close = xvp_vm_close,
};

static int xvp_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int err;
    struct xvp *xvp = filp->private_data;
    unsigned long pfn = vma->vm_pgoff;
    struct xrp_allocation *xrp_allocation;
    phys_addr_t paddr = pfn << PAGE_SHIFT;
    size_t size = vma->vm_end - vma->vm_start;

    xrp_allocation = xrp_allocation_dequeue(xvp, paddr, size);
    if (xrp_allocation) {
        pgprot_t prot = vma->vm_page_prot;

        if (!xrp_cacheable(xvp, pfn, PFN_DOWN(size))) {
            prot = pgprot_writecombine(prot);
            vma->vm_page_prot = prot;
        }

        err = remap_pfn_range(vma, vma->vm_start, pfn, size, prot);
        dev_dbg(
            xvp->dev, "%s: Mapped paddr: %pap to vaddr: 0x%08lx, size: %zu\n",
            __func__, &paddr, vma->vm_start, size);

        vma->vm_private_data = xrp_allocation;
        vma->vm_ops = &xvp_vm_ops;
    } else {
        err = -EINVAL;
    }

    return err;
}

static int xvp_open(struct inode *inode, struct file *filp)
{
    struct xvp *xvp = container_of(filp->private_data, struct xvp, miscdev);
    int rc;

    filp->private_data = xvp;

    rc = xrp_boot(xvp);
    if (rc < 0)
        goto exit;

    rc = 0;

exit:
    xrp_add_known_file(filp);
    return rc;
}

static int xvp_close(struct inode *inode, struct file *filp)
{
    struct xvp *xvp = filp->private_data;

    xrp_remove_known_file(filp);
    xrp_shutdown(xvp);

    return 0;
}

static const struct file_operations xvp_fops = {
    .owner = THIS_MODULE,
    .llseek = no_llseek,
    .unlocked_ioctl = xvp_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = xvp_ioctl,
#endif
    .mmap = xvp_mmap,
    .open = xvp_open,
    .release = xvp_close,
};

static int xvp_log_open(struct inode *inode, struct file *filp)
{	
    struct xvp *xvp = container_of(filp->private_data, struct xvp, misclogdev);

    filp->private_data = xvp;

	return 0;
}

static int xvp_log_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t xvp_log_read(struct file *f, char __user *buffer, size_t size,
            loff_t *offset)
{
    struct xvp *xvp = f->private_data;
    (void)offset;

    return xrp_cyclic_log_read(xvp, buffer, size);
}

static const struct file_operations xvp_log_fops = {
	.owner  = THIS_MODULE,
	.llseek = no_llseek,
	.open = xvp_log_open,
	.release = xvp_log_close,
	.read = xvp_log_read,
};

static int xrp_init_comm_buffer(struct xvp *xvp)
{
    int ret;

    xvp->comm.size = PAGE_SIZE;
    xvp->comm.addr = dma_alloc_attrs(
        xvp->dev, xvp->comm.size, &xvp->comm.dma_addr, GFP_KERNEL, 0);
    if (!xvp->comm.addr) {
        dev_err(xvp->dev, "%s: couldn't allocate comm buffer\n", __func__);
        ret = -ENOMEM;
        goto err;
    }

    ret = xrp_share_dma(
        xvp, xvp->comm.dma_addr, xvp->comm.size, &xvp->comm.paddr,
        &xvp->comm.dsp_paddr, LUT_MAPPING_COMM, false);
    if(ret < 0) {
        dev_err(xvp->dev, "%s: comm buffer couldn't be shared\n", __func__);
        goto err_free;
    }

    pr_debug(
        "%s: comm = %pap/%p\n", __func__, &xvp->comm.paddr, xvp->comm.addr);

    return 0;

err_free:
    dma_free_attrs(
        xvp->dev, xvp->comm.size, xvp->comm.addr, xvp->comm.dma_addr, 0);
err:
    return ret;
}

static int compare_queue_priority(const void *a, const void *b)
{
    const void *const *ppa = a;
    const void *const *ppb = b;
    const struct xrp_comm *pa = *ppa, *pb = *ppb;

    if (pa->priority == pb->priority)
        return 0;
    else
        return pa->priority < pb->priority ? -1 : 1;
}

static long xrp_init_common(struct platform_device *pdev, struct xvp *xvp)
{
    long ret;
    char nodename[sizeof("xvp") + 3 * sizeof(int)];
    char log_nodename[sizeof("xvp_log") + 3 * sizeof(int)];
    int nodeid;
    unsigned i;
    struct xrp_mapping *mapping;

    if (of_reserved_mem_device_init_by_name(
            xvp->dev, xvp->dev->of_node, "cma") < 0) {
        ret = -ENODEV;
        goto err;
    }

    atomic_set(&xvp->open_counter, 0);

    spin_lock_init(&xvp->busy_list_lock);

    xvp->dev = &pdev->dev;
    platform_set_drvdata(pdev, xvp);

    mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
    if (!mapping) {
        dev_err(
            &pdev->dev, "%s: failed to allocate log buffer mapping\n",
            __func__);
        ret = -ENOMEM;
        goto err;
    }

    ret = xrp_share_kernel(
        xvp, xvp->cyclic_log.addr, xvp->cyclic_log.size, XRP_FLAG_READ_WRITE,
        &xvp->cyclic_log.paddr, &xvp->cyclic_log.dsp_paddr, mapping,
        LUT_MAPPING_LOG, false);
    if(ret < 0) {
        dev_err(&pdev->dev, "%s: log buffer couldn't be shared\n", __func__);
        goto err_free;
    }

    xvp->cyclic_log.mapping = mapping;

    ret = xrp_init_comm_buffer(xvp);
    if (ret < 0)
        goto err_unshare;

    ret = xrp_init_cma_pool(&xvp->pool, xvp->dev);
    if (ret < 0)
        goto err_free_comm;

    ret = device_property_read_u32_array(xvp->dev, "queue-priority", NULL,
                         0);
    if (ret > 0) {
        xvp->n_queues = ret;
        xvp->queue_priority =
            devm_kmalloc(&pdev->dev, ret * sizeof(u32), GFP_KERNEL);
        if (xvp->queue_priority == NULL)
            goto err_free_pool;
        ret = device_property_read_u32_array(xvp->dev, "queue-priority",
                             xvp->queue_priority,
                             xvp->n_queues);
        if (ret < 0)
            goto err_free_pool;
        dev_dbg(xvp->dev,
            "multiqueue (%d) configuration, queue priorities:\n",
            xvp->n_queues);
        for (i = 0; i < xvp->n_queues; ++i)
            dev_dbg(xvp->dev, "  %d\n", xvp->queue_priority[i]);
    } else {
        xvp->n_queues = 1;
    }
    xvp->queue = devm_kmalloc(
        &pdev->dev, xvp->n_queues * sizeof(*xvp->queue), GFP_KERNEL);
    xvp->queue_ordered =
        devm_kmalloc(&pdev->dev,
                 xvp->n_queues * sizeof(*xvp->queue_ordered),
                 GFP_KERNEL);
    if (xvp->queue == NULL || xvp->queue_ordered == NULL)
        goto err_free_pool;

    for (i = 0; i < xvp->n_queues; ++i) {
        mutex_init(&xvp->queue[i].lock);
        xvp->queue[i].comm = xvp->comm.addr + XRP_DSP_CMD_STRIDE * i;
        init_completion(&xvp->queue[i].completion);
        if (xvp->queue_priority)
            xvp->queue[i].priority = xvp->queue_priority[i];
        xvp->queue_ordered[i] = xvp->queue + i;
    }
    sort(xvp->queue_ordered, xvp->n_queues, sizeof(*xvp->queue_ordered),
         compare_queue_priority, NULL);
    if (xvp->n_queues > 1) {
        dev_dbg(xvp->dev, "SW -> HW queue priority mapping:\n");
        for (i = 0; i < xvp->n_queues; ++i) {
            dev_dbg(xvp->dev, "  %d -> %d\n", i,
                xvp->queue_ordered[i]->priority);
        }
    }

    ret = device_property_read_string(xvp->dev, "firmware-name",
                      &xvp->firmware_name);
    if (ret == -EINVAL || ret == -ENODATA) {
        dev_dbg(xvp->dev,
            "no firmware-name property, not loading firmware");
    } else if (ret < 0) {
        dev_err(xvp->dev, "invalid firmware name (%ld)", ret);
        goto err_free_pool;
    }

    nodeid = ida_simple_get(&xvp_nodeid, 0, 0, GFP_KERNEL);
    if (nodeid < 0) {
        ret = nodeid;
        goto err_free_pool;
    }
    xvp->nodeid = nodeid;
    sprintf(nodename, "xvp%u", nodeid);

    xvp->miscdev = (struct miscdevice){
        .minor = MISC_DYNAMIC_MINOR,
        .name = devm_kstrdup(&pdev->dev, nodename, GFP_KERNEL),
        .nodename = devm_kstrdup(&pdev->dev, nodename, GFP_KERNEL),
        .fops = &xvp_fops,
    };

    ret = misc_register(&xvp->miscdev);
    if (ret < 0)
        goto err_free_id;

    sprintf(log_nodename, "xvp_log%u", nodeid);
	xvp->misclogdev = (struct miscdevice){
		.minor = MISC_DYNAMIC_MINOR,
		.name = devm_kstrdup(&pdev->dev, log_nodename, GFP_KERNEL),
		.nodename = devm_kstrdup(&pdev->dev, log_nodename, GFP_KERNEL),
		.fops = &xvp_log_fops,
	};

	ret = misc_register(&xvp->misclogdev);
	if (ret < 0)
		goto err_free_dev;

    xvp->state = DSP_STATE_CLOSED;
    
    return PTR_ERR(xvp);

err_free_dev:
    misc_deregister(&xvp->miscdev);
err_free_id:
    ida_simple_remove(&xvp_nodeid, nodeid);
err_free_pool:
    xrp_free_pool(xvp->pool);
err_free_comm:
    dma_free_attrs(
        xvp->dev, xvp->comm.size, xvp->comm.addr, xvp->comm.dma_addr, 0);
err_unshare:
    (void)xrp_unshare_kernel(xvp, mapping, XRP_FLAG_READ_WRITE);
err_free:
    kfree(mapping);
err:
    dev_err(&pdev->dev, "%s: ret = %ld\n", __func__, ret);
    return ret;
}

typedef long xrp_init_function(struct platform_device *pdev, struct xvp *xvp);

static long xrp_init_cma(struct platform_device *pdev, struct xvp *xvp)
{
    long ret;

    ret = xrp_init_hw(pdev, xvp);
    if (ret < 0)
        return ret;

    return xrp_init_common(pdev, xvp);
}

static int xrp_deinit(struct platform_device *pdev)
{
    struct xvp *xvp = platform_get_drvdata(pdev);

    if (!xvp) {
        return 0;
    }

    misc_deregister(&xvp->misclogdev);
    misc_deregister(&xvp->miscdev);
    xrp_release_firmware(xvp);
    xrp_free_pool(xvp->pool);
    dma_free_attrs(
        xvp->dev, xvp->comm.size, xvp->comm.addr, xvp->comm.dma_addr, 0);
    ida_simple_remove(&xvp_nodeid, xvp->nodeid);
    (void)xrp_unshare_kernel(xvp, xvp->cyclic_log.mapping, XRP_FLAG_READ_WRITE);
    kfree(xvp->cyclic_log.mapping);    
    devm_kfree(&pdev->dev, xvp);
    return 0;
}

static const struct of_device_id xrp_of_match[] = {
    {
        .compatible = "cdns,xrp-hailo,cma",
        .data = xrp_init_cma,
    },
    {},
};
MODULE_DEVICE_TABLE(of, xrp_of_match);

static int xrp_probe(struct platform_device *pdev)
{
    long ret;
    xrp_init_function *init;
    const struct of_device_id *match;
    struct xvp *xvp;

    match = of_match_device(xrp_of_match, &pdev->dev);
    if (!match) {
        dev_err(&pdev->dev, "%s: no OF device match found\n", __func__);
        return -ENODEV;
    }

    xvp = devm_kzalloc(&pdev->dev, sizeof(*xvp), GFP_KERNEL);
    if (!xvp) {
        return -ENOMEM;
    }

    init = match->data;
    ret = init(pdev, xvp);
    if (IS_ERR_VALUE(ret)) {
        devm_kfree(&pdev->dev, xvp);
        return ret;
    }

    return 0;
}

static int xrp_remove(struct platform_device *pdev)
{
    return xrp_deinit(pdev);
}

static struct platform_driver xrp_driver = {
    .probe   = xrp_probe,
    .remove  = xrp_remove,
    .driver  = {
        .name = DRIVER_NAME,
        .of_match_table = of_match_ptr(xrp_of_match),
    },
};

module_platform_driver(xrp_driver);

MODULE_AUTHOR("Takayuki Sugawara");
MODULE_AUTHOR("Max Filippov");
MODULE_DESCRIPTION("XRP: Linux device driver for Xtensa Remote Processing");
MODULE_LICENSE("Dual MIT/GPL");
