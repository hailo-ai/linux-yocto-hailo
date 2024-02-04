// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Linux module for GP DMA driver
 *
 * Copyright (c) 2019 - 2024 Hailo Technologies Ltd. All rights reserved.
 */
#ifndef __HAILO15_GP_MEMCOPY_H
#define __HAILO15_GP_MEMCOPY_H
#include <linux/dmaengine.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of_dma.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>

#define DRIVER_NAME 			"hailo15_gp_memcopy"
#define MAX_CHANNEL 16

struct dma_copy_info {
	int src_fd;
    int dst_fd;
	unsigned long length;
    int status;
    bool is_dma_buff;
    unsigned long virt_src_addr;
    unsigned long virt_dst_addr;
};

struct dma_memcpy_info {
	int channel_count;
	struct dma_chan channels[MAX_CHANNEL];
	char **names;
    dev_t dev_node;
    struct device *dev;
	struct cdev cdev;
	struct class *class_p;
};

struct memcopy_mapping {
    int fd;
	dma_addr_t dma_addr;
    struct dma_buf_attachment *attachment;
    struct dma_buf *dmabuf;
    struct sg_table *sgt;
    enum dma_data_direction direction;
    unsigned long n_pages;
    enum { MEMCOPY_GUP,
           MEMCOPY_PFN_MAP,
           MEMCOPY_DMA_BUF,
           MEMCOPY_UNKNOWN,
    } type;
    unsigned long virt_addr;
    phys_addr_t phy_addr;
};
#define GP_DMA_XFER  _IOWR('a', 'a', struct dma_copy_info *)

#endif	/* __HAILO15_GP_MEMCOPY_H */