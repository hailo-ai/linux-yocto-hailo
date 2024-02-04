// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Linux device driver for GP DMA Controller
 *
 * Copyright (c) 2019 - 2024 Hailo Technologies Ltd. All rights reserved.
 */
#ifndef __DMA_HAILODMA_H
#define __DMA_HAILODMA_H

#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/dmaengine.h>
#include <linux/io.h>
#include "hailo15_gp_dma_ip_config_macros.h"
#include "hailo15_gp_dma_sw_engine_config_macros.h"

#define MAX_CHANNELS_PER_DEVICE 16
#define VDMA_DESCRIPTOR_LIST_ALIGN  (1 << 16)
#define VDMA_PAGE_ALIGNMENT 12
#define VDMA_MAX_PAGE_SIZE (1 << VDMA_PAGE_ALIGNMENT)
#define DESCRIPTOR_PAGE_SIZE_SHIFT (8)
#define DESCRIPTOR_DESC_CONTROL (0x2)
#define DESCRIPTOR_ADDR_L_MASK (0xFFFFFFC0)


#define DESC_STATUS_REQ                       (1 << 0)
#define DESC_STATUS_REQ_ERR                   (1 << 1)
#define DESC_REQUREST_IRQ_PROCESSED           (1 << 2)
#define DESC_REQUREST_IRQ_ERR                 (1 << 3)
#define DESC_IRQ_AXI_0						  (1 << 4)
#define DESC_IRQ_AXI_1						  (1 << 5)

#define DESCRIPTOR_SET_IRQ (0x24) // an IRQ is issued when this Descriptor has been processed. 
								// an IRQ must be issued on AXI Domain 1

enum gp_dma_interrupts {
	GP_DMA_AP_INT_0_IRQ_ID = 0,
	GP_DMA_AP_INT_1_IRQ_ID,
	GP_DMA_AP_INT_2_IRQ_ID,
	GP_DMA_AP_INT_3_IRQ_ID
};

#define to_hailo15_gp_vdma_chan(chan) container_of(chan, struct hailo15_gp_vdma_chan, common)

struct hailo15_gp_vdma_descriptor {
    uint32_t    PageSize_DescControl;
    uint32_t    AddrL_rsvd_DataID;
    uint32_t    AddrH;
    uint32_t    RemainingPageSize_Status;
};

// Continous buffer that holds a descriptor list.
struct hailo_descriptors_list_buffer {
    void                               *kernel_address;
    dma_addr_t                         dma_address;
    uint32_t                           buffer_size;
	struct hailo15_gp_vdma_descriptor *desc_list;
    uint32_t                      desc_count;  // Must be power of 2 if is_circular is set.
    bool                          is_circular;
};

struct hailo15_gp_vdma_chan_regs {
	volatile uint32_t channel_src_cfg0;
    volatile uint32_t channel_src_cfg1;                                                                                          /* offset: 0x324 ; repeat: [1]        */
	volatile uint32_t channel_src_cfg2;                                                                                          /* offset: 0x328 ; repeat: [1]        */
	volatile uint32_t channel_src_cfg3;                                                                                          /* offset: 0x32c ; repeat: [1]        */
	volatile uint32_t channel_dst_cfg0;                                                                                          /* offset: 0x330 ; repeat: [1]        */
	volatile uint32_t channel_dst_cfg1;                                                                                          /* offset: 0x334 ; repeat: [1]        */
	volatile uint32_t channel_dst_cfg2;                                                                                          /* offset: 0x338 ; repeat: [1]        */
	volatile uint32_t channel_dst_cfg3;
};

struct hailo15_gp_vdma_chan {
    char name[8];
	struct hailo15_gp_vdma_chan_regs __iomem *regs;
	spinlock_t desc_lock;		/* Descriptor operation lock */
	struct dma_chan common;		/* DMA common channel */
	//struct dma_pool *desc_pool;	/* Descriptors pool */
	struct hailo_descriptors_list_buffer dest_desc_list;
	struct hailo_descriptors_list_buffer src_desc_list;
	struct device *dev;		/* Channel device */
	int id;				/* Raw id of this channel */
	bool idle;			/* DMA controller is idle */
	struct dma_async_tx_descriptor async_tx;
	bool error;
};

typedef struct {
	volatile uint32_t engine_ap_intr_mask[4];                                                                                       /* offset: 0x0 ; repeat: [4]        */
	volatile uint32_t engine_ap_intr_status[4];                                                                                     /* offset: 0x10 ; repeat: [4]        */
	volatile uint32_t engine_ap_intr_w1c[4];                                                                                        /* offset: 0x20 ; repeat: [4]        */
	volatile uint32_t engine_ap_intr_w1s[4];                                                                                        /* offset: 0x30 ; repeat: [4]        */
	volatile uint32_t VdmaSoftReset;                                                                                                /* offset: 0x40 ; repeat: [1]        */
	volatile uint32_t vdma_sharedbus[4];                                                                                            /* offset: 0x44 ; repeat: [4]        */
	volatile uint32_t ReadToQosValue;                                                                                               /* offset: 0x54 ; repeat: [1]        */
	volatile uint32_t WriteToQosValue;                                                                                              /* offset: 0x58 ; repeat: [1]        */
	volatile uint32_t DescReadQosValue;                                                                                             /* offset: 0x5c ; repeat: [1]        */
	volatile uint32_t DescWriteQosValue;                                                                                            /* offset: 0x60 ; repeat: [1]        */
	volatile uint32_t auto_address_err_cb_indication;                                                                               /* offset: 0x64 ; repeat: [1]        */
} hailo15_engine_config_regs;

struct hailo15_gp_vdma_device {
	void __iomem *regs;	
	struct device *dev;
	struct dma_device common;
	struct hailo15_gp_vdma_chan *chan[MAX_CHANNELS_PER_DEVICE];
    int irq;		/* IRQ ID */
	int irq_id;		/* IRQ id in hailo engine */	
};
#endif	/* __DMA_HAILODMA_H */