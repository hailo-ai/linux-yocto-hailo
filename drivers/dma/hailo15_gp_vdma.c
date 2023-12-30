// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Linux device driver for GP DMA Controller
 *
 * Copyright (c) 2019 - 2024 Hailo Technologies Ltd. All rights reserved.
 */
#include <linux/of_platform.h>
#include <linux/bitops.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/clk.h>

#include "dmaengine.h"
#include "hailo15_gp_vdma.h"
#include <linux/delay.h>

//#define HAILO15_DMA_TEST 1
#define CHAN_REG_SIZE 0x20
#define HAILO15_VDMA_CHANNEL_DEPTH 16

#define VDMA_DESC_ADDRESS_BITS_16_TO_31__MASK (0x00000000FFFF0000)
#define VDMA_DESC_ADDRESS_BITS_32_TO_63__MASK (0xFFFFFFFF00000000)
#define VDMA_DESC_ADDRESS_BITS_16_TO_31__SHIFT (16)
#define VDMA_DESC_ADDRESS_BITS_32_TO_63__SHIFT (32)
#define VDMA_DESC_ADDRESS_BITS_16_TO_31__GET(address)                          \
	(((uint64_t)(address)&VDMA_DESC_ADDRESS_BITS_16_TO_31__MASK) >>        \
	 VDMA_DESC_ADDRESS_BITS_16_TO_31__SHIFT)
#define VDMA_DESC_ADDRESS_BITS_32_TO_63__GET(address)                          \
	(((uint64_t)(address)&VDMA_DESC_ADDRESS_BITS_32_TO_63__MASK) >>        \
	 VDMA_DESC_ADDRESS_BITS_32_TO_63__SHIFT)

static const struct of_device_id hailo15_gp_vdma_of_ids[] = {
	{
		.compatible = "hailo,hailo15-gp_vdma",
	},
	{}
};

static inline uint32_t hailo15_gp_vdma_get_chan_id(struct device *dev,
						   struct resource chan_base)
{
	uint32_t chan_id = (chan_base.start & 0x0000FFF);
	chan_id /= CHAN_REG_SIZE;
	dev_dbg(dev, " DMA channel start %lld chan_id %d\n", chan_base.start,
		chan_id);
	return chan_id;
}
static void hailo15_gp_vdma_chan_remove(struct hailo15_gp_vdma_chan *chan)
{
	list_del(&chan->common.device_node);
	iounmap(chan->regs);
	kfree(chan);
}

static void hailo15_gp_vdma_start_channel(struct hailo15_gp_vdma_chan *chan)
{
	uint16_t addr_l = 0x0;
	uint32_t addr_h = 0x0;

	dev_dbg(chan->dev,
		" DMA START Channel %d src desc_count %d dst desc_count %d\n",
		chan->id, chan->src_desc_list.desc_count,
		chan->dest_desc_list.desc_count);

	// set the src address
	addr_l = VDMA_DESC_ADDRESS_BITS_16_TO_31__GET(
		chan->src_desc_list.dma_address);
	addr_h = VDMA_DESC_ADDRESS_BITS_32_TO_63__GET(
		chan->src_desc_list.dma_address);

	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__MODIFY(
		chan->regs->channel_src_cfg2, addr_l);
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__MODIFY(
		chan->regs->channel_src_cfg3, addr_h);

	// set the destination address
	addr_l = VDMA_DESC_ADDRESS_BITS_16_TO_31__GET(
		chan->dest_desc_list.dma_address);
	addr_h = VDMA_DESC_ADDRESS_BITS_32_TO_63__GET(
		chan->dest_desc_list.dma_address);

	DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__MODIFY(
		chan->regs->channel_dst_cfg2, addr_l);
	DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__MODIFY(
		chan->regs->channel_dst_cfg3, addr_h);

	// Start
	DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__START_ABORT__SET(
		chan->regs->channel_dst_cfg0);
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__SET(
		chan->regs->channel_src_cfg0);

	// set the SRCDESCNUM_AVAILABLE DESTDESCNUM_AVAILABLE
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRCDESCNUM_AVAILABLE__MODIFY(
		chan->regs->channel_src_cfg0, chan->src_desc_list.desc_count);
	DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DESTDESCNUM_AVAILABLE__MODIFY(
		chan->regs->channel_dst_cfg0, chan->dest_desc_list.desc_count);
}

static void hailo15_gp_vdma_stop_channel(struct hailo15_gp_vdma_chan *chan)
{
	// Abort
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__CLR(
		chan->regs->channel_src_cfg0);
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__CLR(
		chan->regs->channel_dst_cfg0);
}
void hailo15_gp_vdma_desc_list_release(
	struct device *dev, struct hailo_descriptors_list_buffer *descriptors)
{
	if (descriptors->kernel_address) {
		dma_free_coherent(dev, descriptors->buffer_size,
				  descriptors->kernel_address,
				  descriptors->dma_address);
		descriptors->kernel_address = NULL;
	}
}

void hailo_vdma_program_descriptor(struct hailo15_gp_vdma_descriptor *descriptor,
				   uint64_t dma_address, size_t page_size,
				   uint8_t data_id, bool is_last)
{
	descriptor->PageSize_DescControl =
		(uint32_t)((page_size << DESCRIPTOR_PAGE_SIZE_SHIFT) +
			   DESCRIPTOR_DESC_CONTROL);
	descriptor->AddrL_rsvd_DataID =
		(uint32_t)(((dma_address & DESCRIPTOR_ADDR_L_MASK)) | data_id);
	descriptor->AddrH = (uint32_t)(dma_address >> 32);
	descriptor->RemainingPageSize_Status = 0;

	// Update the desc_control
	if (is_last)
		descriptor->PageSize_DescControl |=
			(DESC_REQUREST_IRQ_PROCESSED | DESC_REQUREST_IRQ_ERR |
			 DESC_IRQ_AXI_1);
}

static int hailo15_gp_vdma_allocate_descriptors_list(
	struct device *dev, uint32_t descriptors_count, bool is_circular,
	struct hailo_descriptors_list_buffer *descriptors)
{
	size_t buffer_size = 0;
	const uint64_t align =
		VDMA_DESCRIPTOR_LIST_ALIGN; //First addr must be aligned on 64 KB  (from the VDMA registers documentation)

	if (descriptors_count > (1 << HAILO15_VDMA_CHANNEL_DEPTH)) {
		dev_err(dev,
			"Failed to allocate descriptors list, desc_count 0x%x, is bigger than max \n",
			descriptors_count);
		return -ENOMEM;
	}
	buffer_size =
		descriptors_count * sizeof(struct hailo15_gp_vdma_descriptor);
	buffer_size = ALIGN(buffer_size, align);

	descriptors->kernel_address =
		dma_alloc_coherent(dev, buffer_size, &descriptors->dma_address,
				   GFP_KERNEL | __GFP_ZERO);
	if (descriptors->kernel_address == NULL) {
		dev_err(dev,
			"Failed to allocate descriptors list, desc_count 0x%x, buffer_size 0x%zx, This failure means there is not a sufficient amount of CMA memory "
			"(contiguous physical memory), This usually is caused by lack of general system memory. Please check you have sufficent memory.\n",
			descriptors_count, buffer_size);
		return -ENOMEM;
	}

	descriptors->buffer_size = buffer_size;
	descriptors->desc_list = descriptors->kernel_address;
	descriptors->desc_count = descriptors_count;
	descriptors->is_circular = is_circular;

	return 0;
}
static dma_cookie_t
hailo15_gp_vdma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct hailo15_gp_vdma_chan *chan = to_hailo15_gp_vdma_chan(tx->chan);
	dma_cookie_t cookie = -EINVAL;

	cookie = dma_cookie_assign(&chan->async_tx);

	return cookie;
}
static struct dma_async_tx_descriptor *
hailo15_gp_vdma_prep_memcpy(struct dma_chan *dchan, dma_addr_t dma_dst,
			    dma_addr_t dma_src, size_t len, unsigned long flags)
{
	struct hailo15_gp_vdma_chan *chan;
	uint32_t descriptors_count = 0;
	uint32_t desc_index = 0;
	uint64_t src_addr = dma_src;
	uint64_t dst_addr = dma_dst;
	uint16_t desc_page_size = VDMA_MAX_PAGE_SIZE;
	bool is_last = false;
	int err;

	if (!dchan) {
		dev_err(chan->dev, "No valid channel\n");
		return NULL;
	}

	if (!len) {
		dev_err(chan->dev, "No copy len\n");
		return NULL;
	}

	chan = to_hailo15_gp_vdma_chan(dchan);

	chan->idle = false;

	dma_async_tx_descriptor_init(&chan->async_tx, &chan->common);
	chan->async_tx.tx_submit = hailo15_gp_vdma_tx_submit;

	descriptors_count =
		(len /
		 VDMA_MAX_PAGE_SIZE); // check this against the depth of the channel
	if ((len % VDMA_MAX_PAGE_SIZE) != 0)
		descriptors_count += 1;
	dev_dbg(chan->dev, "descriptors_count is %d \n", descriptors_count);
	err = hailo15_gp_vdma_allocate_descriptors_list(
		chan->dev, descriptors_count, false, &chan->dest_desc_list);
	if (err) {
		dev_err(chan->dev, "Fail to allocate dest descriptors list\n");
		return NULL;
	}

	err = hailo15_gp_vdma_allocate_descriptors_list(
		chan->dev, descriptors_count, false, &chan->src_desc_list);
	if (err) {
		dev_err(chan->dev, "Fail to allocate dest descriptors list\n");
		return NULL;
	}

	while (desc_index < descriptors_count) {
		if ((desc_index + 1) == descriptors_count) {
			is_last = true;
			desc_page_size = len;
		}
		/* Source descriptors (read) should be configured to data_id=0 */
		hailo_vdma_program_descriptor(
			&chan->src_desc_list.desc_list[desc_index], src_addr,
			desc_page_size, 0, is_last);
		/* Destination descriptors (write) should be configured to data_id=1 */
		hailo_vdma_program_descriptor(
			&chan->dest_desc_list.desc_list[desc_index], dst_addr,
			desc_page_size, 1, is_last);

		src_addr += VDMA_MAX_PAGE_SIZE;
		dst_addr += VDMA_MAX_PAGE_SIZE;
		len -= VDMA_MAX_PAGE_SIZE;
		desc_index++;
	}

	return &chan->async_tx;
}

/**
 * hailo15_gp_vdma_memcpy_issue_pending - Issue the DMA start command
 * @chan : Hailo vdma DMA channel
 */
static void hailo15_gp_vdma_memcpy_issue_pending(struct dma_chan *dchan)
{
	struct hailo15_gp_vdma_chan *chan = to_hailo15_gp_vdma_chan(dchan);
	hailo15_gp_vdma_start_channel(chan);
}

static void hailo15_gp_vdma_free_chan_resources(struct dma_chan *dchan)
{
	struct hailo15_gp_vdma_chan *chan = to_hailo15_gp_vdma_chan(dchan);

	/* Run all cleanup for descriptors which have been completed */
	if (chan->idle) {
		hailo15_gp_vdma_desc_list_release(chan->dev,
						  &chan->dest_desc_list);
		hailo15_gp_vdma_desc_list_release(chan->dev,
						  &chan->src_desc_list);
	}
}

static int hailo15_gp_vdma_terminate_all(struct dma_chan *dchan)
{
	struct hailo15_gp_vdma_chan *chan;

	if (!dchan)
		return -EINVAL;

	chan = to_hailo15_gp_vdma_chan(dchan);

	if (chan->idle) {
		hailo15_gp_vdma_free_chan_resources(dchan);
	} else {
		dev_err(chan->dev,
			"Channel Cleanup was called while channel isn't idle, force channel stop \n");
		chan->idle = true;
		/* Stop the channel */
		hailo15_gp_vdma_stop_channel(chan);
		hailo15_gp_vdma_free_chan_resources(dchan);
		return -EPERM;
	}

	return 0;
}

static enum dma_status hailo15_gp_vdma_tx_status(struct dma_chan *dchan,
						 dma_cookie_t cookie,
						 struct dma_tx_state *txstate)
{
	struct hailo15_gp_vdma_chan *chan = to_hailo15_gp_vdma_chan(dchan);
	enum dma_status ret;

	if (chan->idle)
		ret = DMA_COMPLETE;
	else
		ret = DMA_IN_PROGRESS;

	dma_cookie_status(dchan, cookie, txstate);

	return ret;
}

static void hailo15_gp_vdma_channel_handler(struct hailo15_gp_vdma_chan *chan)
{
	if (chan == NULL)
		return;
	chan->idle = true;
	/* Stop the channel */
	hailo15_gp_vdma_stop_channel(chan);

	dmaengine_desc_get_callback_invoke(&chan->async_tx, NULL);
	/* TBD check that no error on the channel */

	/* TBD check num processed and num availble that both of them are zero  */
}
static irqreturn_t hailo15_gp_vdma_irqhandler(int irq, void *data)
{
	struct hailo15_gp_vdma_device *hdev = data;
	uint32_t channels_bitmap = 0;
	irqreturn_t return_value = IRQ_NONE;
	hailo15_engine_config_regs __iomem *engine_registers = hdev->regs;
	uint32_t i = 0;
	channels_bitmap =
		DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_STATUS__VAL__READ(
			engine_registers->engine_ap_intr_status[0]);
	dev_dbg(hdev->dev, "SW DMA GOT IRQ: %d channel_bitmap %u \n", irq,
		channels_bitmap);

	/* TBD  need to add a converation between engine_ap_intr_w1c[0] to the interrupt number */
	for (i = 0; i < MAX_CHANNELS_PER_DEVICE; i++) {
		if (channels_bitmap & BIT(i)) {
			engine_registers->engine_ap_intr_w1c[0] = BIT(i);
			if (hdev->chan[i]) {
				hailo15_gp_vdma_channel_handler(hdev->chan[i]);
			}
			dev_dbg(hdev->dev,
				"SW DMA IRQ_HANDLED for channel id %d\n", i);
			return_value = IRQ_HANDLED;
		}
	}

	return return_value;
}
static void
hailo15_gp_vdma_channel_irq_mask_enable(struct hailo15_gp_vdma_device *hdev,
					int chan_id)
{
	hailo15_engine_config_regs __iomem *engine_registers = hdev->regs;
	// Setup the irq mask
	engine_registers->engine_ap_intr_mask[0] |= BIT(chan_id);
	// TBD add converation from irq to engine_ap_intr_mask index - save the irq numver to chan struct
}

static void hailo15_gp_vdma_channel_init(struct hailo15_gp_vdma_chan *chan)
{
	chan->idle = true;
	/* Reset the channel */

	/* set depth  to 8 -> max desc is 2^8 before doing a cycle */
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRC_DEPTH__MODIFY(
		chan->regs->channel_src_cfg0, HAILO15_VDMA_CHANNEL_DEPTH);
	DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DEST_DEPTH__MODIFY(
		chan->regs->channel_dst_cfg0, HAILO15_VDMA_CHANNEL_DEPTH);

	// set addr_h and addr_l to 0 for src and dst
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__MODIFY(
		chan->regs->channel_src_cfg2, 0);
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__MODIFY(
		chan->regs->channel_src_cfg3, 0);

	DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__MODIFY(
		chan->regs->channel_dst_cfg2, 0);
	DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__MODIFY(
		chan->regs->channel_dst_cfg3, 0);

	// reset vDMA CHANNEL
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__CLR(
		chan->regs->channel_src_cfg0);
	DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__CLR(
		chan->regs->channel_dst_cfg0);

	// open the  DMA IRQ events should be reported on the AXI Domain 1 interface. DMA IRQ is required on error.
}

static void __attribute__((unused))
hailo15_gp_vDMA_Test(struct hailo15_gp_vdma_chan *chan)
{
	dma_addr_t src_addr, dst_addr;
	uint8_t *src_buf, *dst_buf;

	src_buf = dma_alloc_coherent(chan->dev, 8192, &src_addr, GFP_KERNEL);
	dst_buf = dma_alloc_coherent(chan->dev, 8192, &dst_addr, GFP_KERNEL);

	memset(src_buf, 0x12, 8192);
	memset(dst_buf, 0x0, 8192);

	dev_info(
		chan->dev,
		"my dma_copy Before DMA Transfer: src_buf address %llx src_buf[0] = %x\n",
		src_addr, src_buf[0]);
	dev_info(
		chan->dev,
		"my dma_copy Before DMA Transfer: dst_buf address %llx dst_buf[0] = %x\n",
		dst_addr, dst_buf[0]);

	hailo15_gp_vdma_prep_memcpy(&chan->common, dst_addr, src_addr, 8192,
				    DMA_MEM_TO_MEM);
	hailo15_gp_vdma_memcpy_issue_pending(&chan->common);
	msleep(3000);

	dev_info(chan->dev, "my dma_copy After DMA Transfer: src_buf[0] = %x\n",
		 src_buf[0]);
	dev_info(
		chan->dev,
		"my dma_copy After DMA Transfer: dst_buf[0] = %x, dst_buf[100] = %x\n",
		dst_buf[0], dst_buf[100]);

	hailo15_gp_vdma_free_chan_resources(&chan->common);

	dma_free_coherent(chan->dev, 8192, src_buf, src_addr);
	dma_free_coherent(chan->dev, 8192, dst_buf, dst_addr);
}

static int hailo15_gp_vdma_alloc_chan_resources(struct dma_chan *dchan)
{
	struct hailo15_gp_vdma_chan *chan = to_hailo15_gp_vdma_chan(dchan);

	hailo15_gp_vdma_channel_init(chan);

	return 0;
}

static int hailo15_dma_chan_probe(struct hailo15_gp_vdma_device *hdev,
				  struct device_node *node)
{
	struct hailo15_gp_vdma_chan *chan;
	struct resource of_resource;
	int err;

	/* alloc channel */
	chan = kzalloc(sizeof(*chan), GFP_KERNEL);
	if (!chan) {
		err = -ENOMEM;
		goto out_return;
	}

	/* ioremap registers for use */
	chan->regs = of_iomap(node, 0);
	if (!chan->regs) {
		dev_err(hdev->dev, "unable to ioremap registers\n");
		err = -ENOMEM;
		goto out_free_chan;
	}

	err = of_address_to_resource(node, 0, &of_resource);
	if (err) {
		dev_err(hdev->dev, "unable to find 'reg' property\n");
		goto out_iounmap_regs;
	}

	chan->dev = hdev->dev;

	chan->id = hailo15_gp_vdma_get_chan_id(hdev->dev, of_resource);
	if (chan->id >= MAX_CHANNELS_PER_DEVICE) {
		dev_err(hdev->dev, "too many channels for device\n");
		err = -EINVAL;
		goto out_iounmap_regs;
	}

	hdev->chan[chan->id] = chan;
	snprintf(chan->name, sizeof(chan->name), "chan%d", chan->id);

	/* Initialize the channel */
	hailo15_gp_vdma_channel_init(chan);
	hailo15_gp_vdma_channel_irq_mask_enable(hdev, chan->id);
	chan->common.device = &hdev->common;
	/* Add the channel to DMA device channel list */
	list_add_tail(&chan->common.device_node, &hdev->common.channels);
	dev_info(hdev->dev, "dma probe channel id %d (Hailo GP dma),\n",
		 chan->id);
	return 0;

out_iounmap_regs:
	iounmap(chan->regs);
out_free_chan:
	kfree(chan);
out_return:
	return err;
}

static int hailo15_gp_vdma_probe(struct platform_device *pdev)
{
	struct hailo15_gp_vdma_device *hdev;
	struct device_node *child;
	unsigned int i;
	int err;

	hdev = kzalloc(sizeof(*hdev), GFP_KERNEL);
	if (!hdev) {
		err = -ENOMEM;
		goto out_return;
	}

	hdev->dev = &pdev->dev;
	INIT_LIST_HEAD(&hdev->common.channels);

	/* ioremap the registers for use */
	hdev->regs = devm_platform_ioremap_resource(pdev, 0);
	if (!hdev->regs) {
		dev_err(&pdev->dev, "unable to ioremap engine registers\n");
		err = -ENOMEM;
		goto out_free;
	}

	/* map the channel IRQ if it exists, but don't hookup the handler yet */
	hdev->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (hdev->irq < 0) {
		dev_err(&pdev->dev, "Error receiving irq for %pOF. err %d\n",
			pdev->dev.of_node, hdev->irq);
		return hdev->irq;
	}

	dma_cap_set(DMA_MEMCPY, hdev->common.cap_mask);

	hdev->common.device_prep_dma_memcpy = hailo15_gp_vdma_prep_memcpy;
	hdev->common.device_tx_status = hailo15_gp_vdma_tx_status;
	hdev->common.device_issue_pending =
		hailo15_gp_vdma_memcpy_issue_pending;
	hdev->common.device_alloc_chan_resources =
		hailo15_gp_vdma_alloc_chan_resources;
	hdev->common.device_free_chan_resources =
		hailo15_gp_vdma_free_chan_resources;
	hdev->common.device_terminate_all = hailo15_gp_vdma_terminate_all;
	hdev->common.dev = &pdev->dev;
	hdev->common.directions = BIT(DMA_MEM_TO_MEM);
	hdev->common.copy_align = VDMA_PAGE_ALIGNMENT;

	dma_set_mask(&(pdev->dev), DMA_BIT_MASK(64));

	platform_set_drvdata(pdev, hdev);

	/* First stage we have 1 channel */
	for_each_child_of_node (pdev->dev.of_node, child) {
		dev_dbg(&pdev->dev, "Got channel node \n");
		hailo15_dma_chan_probe(hdev, child);
	}

	/* setting the interrupt after configuring the channels irq bitmap */
	err = request_irq(hdev->irq, hailo15_gp_vdma_irqhandler, IRQF_SHARED,
			  dev_name(hdev->dev), hdev);
	if (err < 0) {
		dev_err(&pdev->dev,
			"Failed setting up hailo15 sw vDMA interrupts. err %d\n",
			err);
		goto out_free_hdev;
	}

	dma_async_device_register(&hdev->common);

#ifdef HAILO15_DMA_TEST
	for (i = 0; i < MAX_CHANNELS_PER_DEVICE; i++) {
		if (hdev->chan[i])
			hailo15_gp_vDMA_Test(hdev->chan[i]);
	}
#endif
	dev_info(hdev->dev, "Probing successfully \n");

	return 0;

out_free_hdev:
	for (i = 0; i < MAX_CHANNELS_PER_DEVICE; i++) {
		if (hdev->chan[i])
			hailo15_gp_vdma_chan_remove(hdev->chan[i]);
	}
	irq_dispose_mapping(hdev->irq);
	iounmap(hdev->regs);
out_free:
	kfree(hdev);
out_return:
	return err;
}

static void hailo15_gp_vdma_free_irq(struct hailo15_gp_vdma_device *hdev)
{
	free_irq(hdev->irq, hdev);
	/*TBD Free all IRQ once more then 1 is supported */
}

static int hailo15_gp_vdma_remove(struct platform_device *op)
{
	struct hailo15_gp_vdma_device *hdev;
	unsigned int i;

	hdev = platform_get_drvdata(op);
	dma_async_device_unregister(&hdev->common);

	hailo15_gp_vdma_free_irq(hdev);

	for (i = 0; i < MAX_CHANNELS_PER_DEVICE; i++) {
		if (hdev->chan[i])
			hailo15_gp_vdma_chan_remove(hdev->chan[i]);
	}

	iounmap(hdev->regs);
	kfree(hdev);

	return 0;
}

static struct platform_driver hailo15_gp_vdma_driver = {
	.driver = {
		.name = "hailo15-gp-dma",
		.of_match_table = hailo15_gp_vdma_of_ids,
	},
	.probe = hailo15_gp_vdma_probe,
	.remove = hailo15_gp_vdma_remove,
};

module_platform_driver(hailo15_gp_vdma_driver);

MODULE_AUTHOR("Hailo Technologies Ltd.");
MODULE_DESCRIPTION("Hailo GP DMA driver");
MODULE_LICENSE("GPL v2");