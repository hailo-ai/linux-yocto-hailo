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
#include <linux/dma-buf.h>
#include "hailo15_gp_memcopy.h"
#include "hailo15_gp_vdma.h"


static void dma_memcpy_put_pages(phys_addr_t phys, unsigned long n_pages)
{
	struct page *page;
	unsigned long i;
	page = pfn_to_page(__phys_to_pfn(phys));
	for (i = 0; i < n_pages; ++i)
		put_page(page + i);
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


static void dma_memcopy_mapping_destroy(struct memcopy_mapping *mapping)
{
    struct dma_buf *dmabuf = mapping->dmabuf;
    struct dma_buf_attachment *attach = mapping->attachment;
    struct sg_table *sgt = mapping->sgt;
    
	switch (mapping->type) {
		case MEMCOPY_GUP:
        	dma_memcpy_put_pages(mapping->phy_addr, mapping->n_pages);
        	break;
		case MEMCOPY_DMA_BUF:
			dma_buf_unmap_attachment(attach, sgt, mapping->direction);
    		dma_buf_detach(dmabuf, attach);
    		dma_buf_put(dmabuf);
			break;
    	default:
        	break;
    }
	return;
}

static long dma_memcpy_mapping(struct device *dev, unsigned long size,
				    struct memcopy_mapping *mapping)
{
	long ret = 0;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attach = NULL;
    struct sg_table *sgt = NULL;
    struct scatterlist *sg = NULL;
    int i;

	dmabuf = dma_buf_get(mapping->fd);
    if (IS_ERR(dmabuf)) {
        pr_err("%s: dma_buf_get failed, err=%ld\n", __func__, PTR_ERR(dmabuf));
        ret = -EINVAL;
        goto l_exit;
    }

	attach = dma_buf_attach(dmabuf, dev);
    if (IS_ERR(attach)) {
        pr_err("%s: dma_buf_attach failed, err=%ld\n", __func__, PTR_ERR(attach));
        ret = -EINVAL;
        goto l_buf_get;
    }

	sgt = dma_buf_map_attachment(attach, mapping->direction);
    if (IS_ERR(sgt)) {
        pr_err("%s: dma_buf_map_attachment failed, err=%ld\n", __func__, PTR_ERR(sgt));
        ret = -EINVAL;
        goto l_buf_attach;
    }

	 // print out the scatterlist for debug purposes
    for_each_sgtable_dma_sg(sgt, sg, i) {
        dma_addr_t sg_dma_addr = sg_dma_address(sg);
        pr_debug("%s: sg[%d] = %pad, len = %u, offset = %u\n",
            __func__, i, &sg_dma_addr, sg_dma_len(sg), sg->offset); 
    }
    if (sgt->nents > 1) {
        pr_err("%s: buffer is not contiguous\n", __func__);
        ret = -EINVAL;
        goto l_buf_map;
    }

	sg = sgt->sgl;
    if (sg_dma_len(sg) < size) {
        pr_err("%s: Dma mapped length (%u) is smaller than requested size (%lu)\n", __func__, sg_dma_len(sg), size);
        ret = -EINVAL;
        goto l_buf_map;
    }
    mapping->dma_addr = sg_dma_address(sg);
    mapping->attachment = attach;
    mapping->dmabuf = dmabuf;
    mapping->sgt = sgt;
    
	goto l_exit;

l_buf_map:
    dma_buf_unmap_attachment(attach, sgt, mapping->direction);
l_buf_attach:
    dma_buf_detach(dmabuf, attach);
l_buf_get:
    dma_buf_put(dmabuf);
l_exit:
    return ret;

}


static int dma_memcopy_mapping_create(struct device *dev, unsigned long length, struct memcopy_mapping *mapping)
{
	int ret = 0;

	if (mapping->type == MEMCOPY_DMA_BUF) {
		ret = dma_memcpy_mapping(dev, length, mapping);
		if (ret < 0)
			return -1;
	} else {
		if (mapping->virt_addr == 0) {
			pr_err("Invalid Virt Address: %02lx \n", mapping->virt_addr);
			return -1;
		}
		ret = dma_memcpy_virt_to_phys(dev, length, mapping);
		if (ret < 0)
			return -1;
	}

	if (mapping->dma_addr == 0) {
		pr_err("Translate mapping Failed\n");
		return -1;
	}
	
	return ret;
	
}

static void get_mapping_info(struct dma_copy_info copy_info,
			    struct memcopy_mapping *mapping_dst,
			    struct memcopy_mapping *mapping_src)
{
	if (copy_info.is_dma_buff) {
		mapping_dst->type = MEMCOPY_DMA_BUF;
		mapping_dst->fd = copy_info.dst_fd;
		mapping_dst->direction = DMA_FROM_DEVICE;
		mapping_src->type = MEMCOPY_DMA_BUF;
		mapping_src->fd = copy_info.src_fd;
		mapping_src->direction = DMA_TO_DEVICE;
	} else {
		mapping_dst->virt_addr = copy_info.virt_dst_addr;
		mapping_src->virt_addr = copy_info.virt_src_addr;
	}
}

static int transfer_memcpy(struct dma_copy_info copy_info)
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
		pr_err("dma_request_channel Failed\n");
		status = -ENODEV;
		goto no_channel;
	}
	hailo_chan = to_hailo15_gp_vdma_chan(chan);
	
	get_mapping_info(copy_info, &mapping_dst, &mapping_src);
	status = dma_memcopy_mapping_create(hailo_chan->dev, copy_info.length, &mapping_src);
	if( status < 0)
		goto free;
	
	status = dma_memcopy_mapping_create(hailo_chan->dev, copy_info.length, &mapping_dst);
	if( status < 0)
		goto destroy_mapping_src;

	chan_desc = dmaengine_prep_dma_memcpy(chan, mapping_dst.dma_addr, mapping_src.dma_addr,
					      copy_info.length, DMA_MEM_TO_MEM);
	if (!chan_desc) {
		pr_err("dmaengine_prep_dma_memcpy Failed\n");
		status = -1;
		goto destroy_mapping;
	}

	init_completion(&cmp);

	chan_desc->callback = hailo15_gp_memcopy_tarnsfer_completed;
	chan_desc->callback_param = &cmp;

	cookie = dmaengine_submit(chan_desc);

	/* Fire the DMA transfer */
	dma_async_issue_pending(chan);

	if (wait_for_completion_timeout(&cmp, msecs_to_jiffies(3000)) <= 0) {
		pr_err("Memcopy Timeout!\n");
		status = -1;
	}

	status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);
	if (status == DMA_COMPLETE) {
		pr_debug("DMA transfer has completed!\n");
	} else {
		pr_err("Error on DMA transfer\n");
	}

	dmaengine_terminate_all(chan);

destroy_mapping:
	dma_memcopy_mapping_destroy(&mapping_dst);
destroy_mapping_src:
	dma_memcopy_mapping_destroy(&mapping_src);
free:
	dma_release_channel(chan);
no_channel:
	return status;
}

static long gp_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct dma_copy_info copy_info;
	
	switch (cmd) {
	case GP_DMA_XFER:
		/* Perform the DMA transfer on the specified channel blocking til it completes
	 	*/
		if (copy_from_user(&copy_info,
				   (struct dma_copy_info *)arg,
				   sizeof(copy_info))) {
			pr_err("copy_from_user Fail for GP_DMA_XFER\n");
			return -EINVAL;
		}
		copy_info.status = transfer_memcpy(copy_info);
		if (copy_to_user((struct dma_copy_info *)arg,
				 &copy_info, sizeof(copy_info))) {
			pr_err("copy_to_user Fail for GP_DMA_XFER \n");
			return -EINVAL;
		}
		break;

	default:
		pr_err("Invalid IOCTL\n");
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
	pr_debug("hailo15_gp_memcopy_init\n");

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
	pr_debug("hailo15_gp_memcopy_exit\n");
	unregister_chrdev(MYMAJOR, "dma_memcpy");
}

module_init(hailo15_gp_memcopy_init);

module_exit(hailo15_gp_memcopy_exit);

MODULE_AUTHOR("Hailo Technologies Ltd.");
MODULE_DESCRIPTION("GP-DMA Warrper Prototype");
MODULE_LICENSE("GPL v2");