// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Linux module for GP DMA Driver
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2019 - 2024 Hailo Technologies Ltd. All rights reserved.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/dmaengine.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-direct.h>
#include <linux/cdev.h>

#include "hailo15_gp_memcopy.h"
#include "hailo15_gp_vdma.h"


/* Handle a callback and indicate the DMA transfer is completed
 * 
 */
void hailo15_gp_memcopy_tarnsfer_completed(void *param)
{
	struct completion *cmp = (struct completion *)param;
	complete(cmp);
}
static int gp_dma_open(struct inode *device_file, struct file *file)
{
	return 0;
}

static int gp_dma_close(struct inode *device_file, struct file *file)
{
	return 0;
}

static void dma_memcpy_put_pages(phys_addr_t phys, unsigned long n_pages)
{
	struct page *page;
	unsigned long i;
	page = pfn_to_page(__phys_to_pfn(phys));
	for (i = 0; i < n_pages; ++i)
		put_page(page + i);
}

static void dma_memcopy_mapping_destroy(struct memcopy_mapping *mapping)
{
    switch (mapping->type) {
    case MEMCOPY_GUP:
        dma_memcpy_put_pages(mapping->phy_addr, mapping->n_pages);
        break;
    default:
        break;
    }
}

bool dma_memcpy_cacheable(unsigned long pfn, unsigned long n_pages)
{
	unsigned long i;
	for (i = 0; i < n_pages; ++i)
		if (!pfn_valid(pfn + i))
			return false;
	return true;
}
static bool dma_memcpy_vma_needs_cache_ops(struct vm_area_struct *vma)
{
	pgprot_t prot = vma->vm_page_prot;
	return pgprot_val(prot) != pgprot_val(pgprot_noncached(prot)) &&
	       pgprot_val(prot) != pgprot_val(pgprot_writecombine(prot));
}

static long dma_memcpy_pfn_virt_to_phys(struct vm_area_struct *vma,
					unsigned long vaddr, unsigned long size,
					phys_addr_t *paddr)
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
			pr_debug("%s: non-contiguous physical memory\n",
				 __func__);
			return -EINVAL;
		}
		next_phys = __pfn_to_phys(next_pfn);
		pfn = next_pfn;
	}
	pr_debug("%s: success, paddr: %pap\n", __func__, paddr);
	return 0;
}
static long dma_memcpy_gup_virt_to_phys(unsigned long vaddr, unsigned long size,
					phys_addr_t *paddr)
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
		pr_err("%s: asked for %d pages, but got only %d\n", __func__,
		       nr_pages, ret);
		nr_pages = ret;
		ret = -EINVAL;
		goto out_put;
	}
	for (i = 1; i < nr_pages; ++i) {
		if (page[i] != page[i - 1] + 1) {
			pr_err("%s: non-contiguous physical memory\n",
			       __func__);
			ret = -EINVAL;
			goto out_put;
		}
	}
	*paddr = __pfn_to_phys(page_to_pfn(page[0])) + (vaddr & ~PAGE_MASK);
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

static long dma_memcpy_virt_to_phys(struct device *dev, unsigned long size,
				    struct memcopy_mapping *mapping)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	long rc = -EINVAL;
	phys_addr_t phys = ~0ul;
	unsigned long n_pages =0;
	unsigned long virt = mapping->virt_addr;

	vma = find_vma(mm, virt);
	if (!vma) {
		pr_err("%s: no vma for vaddr/size = 0x%08lx/0x%08lx\n",
		       __func__, virt, size);
		return -EINVAL;
	}
	
	n_pages = PFN_UP(virt + size) - PFN_DOWN(virt);
	/*
    * A range can only be mapped directly if it is either
    * uncached or HW-specific cache operations can handle it.
    */
	if (vma && vma->vm_flags & (VM_IO | VM_PFNMAP)) {
		mapping->type = MEMCOPY_PFN_MAP;
		rc = dma_memcpy_pfn_virt_to_phys(vma, virt, size, &phys);
		if (rc == 0 && dma_memcpy_vma_needs_cache_ops(vma) &&
		    !dma_memcpy_cacheable(PFN_DOWN(phys), n_pages)) {
			pr_debug("%s: needs unsupported cache mgmt\n",
				 __func__);
			rc = -EINVAL;
		}
	} else {
		mapping->type = MEMCOPY_GUP;
		mmap_read_unlock(mm);
		rc = dma_memcpy_gup_virt_to_phys(virt, size, &phys);
		if (rc == 0 && (!vma || dma_memcpy_vma_needs_cache_ops(vma)) &&
		    !dma_memcpy_cacheable(PFN_DOWN(phys), n_pages)) {
			pr_debug("%s: needs unsupported cache mgmt\n",
				 __func__);
			dma_memcpy_put_pages(phys, n_pages);
			rc = -EINVAL;
		}
		mmap_read_lock(mm);
	}
	/* We couldn't share it. Fail the request. */
	if (rc < 0) {
		pr_err("%s: couldn't map virt to phys\n", __func__);
		return rc;
	}
	mapping->dma_addr = phys_to_dma(dev, phys);
	if (mapping->dma_addr == 0) {
		pr_err("%s: phys_to_dma fail\n", __func__);
		return rc;
	}
	mapping->n_pages = n_pages;
	mapping->phy_addr = phys;
	return 0;
}
static int translate_addr(struct device *dev, unsigned long length,
			   struct memcopy_mapping *mapping_src,
			   struct memcopy_mapping *mapping_dst)
{
	int ret = 0;
	if ((mapping_dst->virt_addr == 0) || (mapping_src->virt_addr == 0)){
		printk("Invalid Virt Address src: %02lx  dest: %02lx \n", mapping_src->virt_addr, mapping_dst->virt_addr);
		return -1;
	}

	ret = dma_memcpy_virt_to_phys(dev, length, mapping_src);
	if (ret < 0)
		return -1;

	ret = dma_memcpy_virt_to_phys(dev, length, mapping_dst);
	if (ret < 0)
		return -1;

	if ((mapping_dst->dma_addr == 0) || (mapping_src ->dma_addr == 0)) {
		printk("Translate mapping Failed\n");
		return -1;
	}

	return ret;
}

static int transfer_memcpy(unsigned long virt_src_addr,
			   unsigned long virt_dst_addr, unsigned long length)
{
	dma_cap_mask_t mask;
	struct dma_chan *chan;
	struct dma_async_tx_descriptor *chan_desc;
	dma_cookie_t cookie;
	struct completion cmp;
	int status = 0;
	struct memcopy_mapping mapping_dst = {0};
	struct memcopy_mapping mapping_src = {0};
	struct hailo15_gp_vdma_chan *hailo_chan;

	pr_debug("Start_transfer\n");
	
	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY, mask);
	chan = dma_request_channel(mask, NULL, NULL);
	if (!chan) {
		printk("dma_request_channel Failed\n");
		status = -ENODEV;
		goto no_channel;
	}
	hailo_chan = to_hailo15_gp_vdma_chan(chan);
	mapping_dst.virt_addr = virt_dst_addr;
	mapping_src.virt_addr = virt_src_addr;
	status = translate_addr(hailo_chan->dev, length, &mapping_src, &mapping_dst);
	if( status < 0) {
		goto free;
	}

	chan_desc = dmaengine_prep_dma_memcpy(chan, mapping_dst.dma_addr, mapping_src.dma_addr,
					      length, DMA_MEM_TO_MEM);
	if (!chan_desc) {
		printk("dmaengine_prep_dma_memcpy Failed\n");
		status = -1;
		goto free;
	}

	init_completion(&cmp);

	chan_desc->callback = hailo15_gp_memcopy_tarnsfer_completed;
	chan_desc->callback_param = &cmp;

	cookie = dmaengine_submit(chan_desc);

	/* Fire the DMA transfer */
	dma_async_issue_pending(chan);

	if (wait_for_completion_timeout(&cmp, msecs_to_jiffies(3000)) <= 0) {
		printk("Memcopy Timeout!\n");
		status = -1;
	}

	status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);
	if (status == DMA_COMPLETE) {
		pr_debug("DMA transfer has completed!\n");
	} else {
		printk("Error on DMA transfer\n");
	}

	dmaengine_terminate_all(chan);

	dma_memcopy_mapping_destroy(&mapping_src);
	dma_memcopy_mapping_destroy(&mapping_dst);
free:
	dma_release_channel(chan);
no_channel:
	return status;
}

static long gp_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct channel_buffer_info buffer_info;

	switch (cmd) {
	case GP_DMA_XFER:
		/* Perform the DMA transfer on the specified channel blocking til it completes
	 	*/
		if (copy_from_user(&buffer_info,
				   (struct channel_buffer_info *)arg,
				   sizeof(buffer_info))) {
			printk("copy_from_user Fail for GP_DMA_XFER\n");
			return -EINVAL;
		}
		buffer_info.status = transfer_memcpy(buffer_info.virt_src_addr,
						     buffer_info.virt_dst_addr, buffer_info.length);
		if (copy_to_user((struct channel_buffer_info *)arg,
				 &buffer_info, sizeof(buffer_info))) {
			printk("copy_to_user Fail for GP_DMA_XFER \n");
			return -EINVAL;
		}
		break;

	default:
		printk("Invalid IOCTL\n");
		return -EINVAL;
	}

	return 0;
}

static struct file_operations gp_dma_fops = { .owner = THIS_MODULE,
					      .open = gp_dma_open,
					      .release = gp_dma_close,
					      .unlocked_ioctl = gp_dma_ioctl };

#define MYMAJOR 64
static int __init hailo15_gp_memcopy_init(void)
{
	int retval;
	printk("hailo15_gp_memcopy_init\n");

	retval = register_chrdev(MYMAJOR, "dma_memcpy", &gp_dma_fops);
	if (retval == 0) {
		pr_debug(" GP DMA memcopy ioctl registered Device number Major: %d, Minor: %d\n",
		       MYMAJOR, 0);
	} else if (retval > 0) {
		pr_info("GP DMA memcopy - registered Device number Major: %d, Minor: %d\n",
		       retval >> 20, retval & 0xfffff);
	} else {
		pr_err("GP DMA memcopy Could not register device number!\n");
		return -1;
	}
	return 0;
}

static void __exit hailo15_gp_memcopy_exit(void)
{
	printk("hailo15_gp_memcopy_exit\n");
	unregister_chrdev(MYMAJOR, "dma_memcpy");
}

module_init(hailo15_gp_memcopy_init);

module_exit(hailo15_gp_memcopy_exit);

MODULE_AUTHOR("Hailo Technologies Ltd.");
MODULE_DESCRIPTION("GP-DMA Warrper Prototype");
MODULE_LICENSE("GPL v2");