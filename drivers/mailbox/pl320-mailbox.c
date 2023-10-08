/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * Arm PL320 Mailbox Controller
 */

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <dt-bindings/mailbox/pl320-mailbox.h>

#include "mailbox.h"

#define IPCMxSOURCE(m)		((m) * 0x40)
#define IPCMxDSET(m)		(((m) * 0x40) + 0x004)
#define IPCMxDCLEAR(m)		(((m) * 0x40) + 0x008)
#define IPCMxDSTATUS(m)		(((m) * 0x40) + 0x00C)
#define IPCMxMODE(m)		(((m) * 0x40) + 0x010)
#define IPCMxMSET(m)		(((m) * 0x40) + 0x014)
#define IPCMxMCLEAR(m)		(((m) * 0x40) + 0x018)
#define IPCMxMSTATUS(m)		(((m) * 0x40) + 0x01C)
#define IPCMxSEND(m)		(((m) * 0x40) + 0x020)
#define IPCMxDR(m, dr)		(((m) * 0x40) + ((dr) * 4) + 0x024)

#define IPCMMIS(irq)		(((irq) * 8) + 0x800)
#define IPCMRIS(irq)		(((irq) * 8) + 0x804)

#define MBOX_MASK(n)		(1UL << (n))
#define CHAN_MASK(n)		(1UL << (n))

#define MBOX_SEND_CLEAR (0)
#define MBOX_SEND_DESTINATION (1)
#define MBOX_SEND_SOURCE (2)

#define PL320_MBOX_MAX_CHANNELS (8)

/**
 * PL320 Mailbox channel information
 *
 * @mdev:	Pointer to parent Mailbox device
 * @dst_channel_index:	Channel index of the destination CPU in the PL320 prespective
 * @in_mbox:	The Mailbox index incoming messages are coming from
 * @out_mbox:	The Mailbox index we use for trigerring IRQ to destination
 */
struct pl320_channel {
	unsigned int dst_channel_index;
	unsigned int in_mbox;
	unsigned int out_mbox;
	unsigned int txdone_method;
	struct mbox_controller	*mbox;
};

/**
 * PL320 Mailbox device data
 *
 * @dev:	Device to which it is attached
 * @mbox:	Representation of a communication channel controller
 * @base:	Base address of the register mapping region
 * @device_channel_index:	Channel index of this CPU in the PL320 prespective
 * @channel_info:	Information describing the channels.
 */
struct pl320_mbox_device {
	struct device		*dev;
	struct mbox_controller	*mbox_ack; /* for PL320_MBOX_TXDONE_BY_ACK */
	struct mbox_controller	*mbox_irq; /* for PL320_MBOX_TXDONE_BY_IRQ */
	void __iomem		*base;
	unsigned int device_channel_index;
	struct pl320_channel channel_info[PL320_MBOX_MAX_CHANNELS];
};

static int pl320_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct pl320_channel *chan_info = chan->con_priv;
	struct pl320_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);

	if (!data && chan->cl->tx_block) {
		dev_warn_once(mdev->dev, "data=NULL & tx_block=true will cause mbox to block until timeout");
	}

	// Trigger IRQ to destination
	writel_relaxed(MBOX_SEND_DESTINATION, mdev->base + IPCMxSEND(chan_info->out_mbox));

	dev_dbg(mdev->dev, "Sent IRQ to channel index %d via mailbox %d\n",
		chan_info->dst_channel_index, chan_info->out_mbox);

	return 0;
}

static int pl320_mbox_startup_chan(struct mbox_chan *chan)
{
	struct pl320_channel *chan_info = chan->con_priv;
	struct pl320_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);

	uint32_t source;
	dev_dbg(mdev->dev, "Startup channel called\n");

	// Try to acquire the out_mbox by writing to IPCMxSOURCE, then verify if the acquire was successful
	writel_relaxed(CHAN_MASK(mdev->device_channel_index), mdev->base + IPCMxSOURCE(chan_info->out_mbox));
	source = readl_relaxed(mdev->base + IPCMxSOURCE(chan_info->out_mbox));
	if (unlikely(source != CHAN_MASK(mdev->device_channel_index))) {
		dev_err(mdev->dev, "Mailbox %u already used by channelId %u\n", chan_info->out_mbox, source);
		return -EBUSY;
	}

	// Init mailbox destination, IRQ masking, send and DR0
	writel_relaxed(CHAN_MASK(chan_info->dst_channel_index), mdev->base + IPCMxDSET(chan_info->out_mbox));
	writel_relaxed(CHAN_MASK(mdev->device_channel_index) | CHAN_MASK(chan_info->dst_channel_index), 
					mdev->base + IPCMxMSET(chan_info->out_mbox));
	writel_relaxed(MBOX_SEND_CLEAR, mdev->base + IPCMxSEND(chan_info->out_mbox));
	writel_relaxed(0, mdev->base + IPCMxDR(chan_info->out_mbox, 0));

	return 0;
}

static void pl320_mbox_shutdown_chan(struct mbox_chan *chan)
{
	struct pl320_channel *chan_info = chan->con_priv;
	struct pl320_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);

	dev_dbg(mdev->dev, "Shutdown channel called\n");
	writel_relaxed(0, mdev->base + IPCMxSOURCE(chan_info->out_mbox));
	chan->con_priv = NULL;
	chan_info->mbox = NULL;
}

static irqreturn_t pl320_mbox_interrupt(int irq, void *p)
{
	struct pl320_mbox_device *mdev = p;
	struct pl320_channel *chan_info = NULL;
	struct mbox_chan *chan = NULL;
	int ch = 0;

	uint32_t mis = readl_relaxed(mdev->base + IPCMMIS(mdev->device_channel_index));
	for (ch = 0; ch < PL320_MBOX_MAX_CHANNELS; ch++) {
		chan_info = &mdev->channel_info[ch];

		if (!chan_info->mbox) {
			continue;
		}

		chan = &chan_info->mbox->chans[ch];

		if (mis & MBOX_MASK(chan_info->out_mbox)) {
			if (chan_info->txdone_method != PL320_MBOX_TXDONE_BY_IRQ) {
				dev_err(mdev->dev, "Got TX acknowledge interrupt on channel but txdone method is not IRQ");
				continue;
			}
			// Received an acknowledge interrupt - clear interrupt
			writel_relaxed(MBOX_SEND_CLEAR, mdev->base + IPCMxSEND(chan_info->out_mbox));
			mbox_chan_txdone(chan, 0);
		}

		if (mis & MBOX_MASK(chan_info->in_mbox)) {
			mbox_chan_received_data(chan, NULL);
			if (chan_info->txdone_method == PL320_MBOX_TXDONE_BY_IRQ) {
				// send acknowledge interrupt to source
				writel_relaxed(MBOX_SEND_SOURCE, mdev->base + IPCMxSEND(chan_info->in_mbox));
			} else {
				// only clear the interrupt
				writel_relaxed(MBOX_SEND_CLEAR, mdev->base + IPCMxSEND(chan_info->in_mbox));
			}
		}
	}

	return IRQ_HANDLED;
}

static struct mbox_chan *pl320_mbox_xlate(struct mbox_controller *req_mbox,
					  const struct of_phandle_args *spec)
{
	struct pl320_mbox_device *mdev = dev_get_drvdata(req_mbox->dev);
	struct mbox_controller *mbox = NULL;
	struct mbox_chan *alloc_chan = NULL;
	struct pl320_channel *chan_info = NULL;
	int i;
	unsigned int dst_channel_index;
	unsigned int in_mbox;
	unsigned int out_mbox;
	unsigned int txdone_method;

	if (spec->args_count != 4)
		return ERR_PTR(-EINVAL);

	dst_channel_index = spec->args[0];
	in_mbox = spec->args[1];
	out_mbox = spec->args[2];
	txdone_method = spec->args[3];

	switch (txdone_method) {
		case PL320_MBOX_TXDONE_BY_IRQ:
			mbox = mdev->mbox_irq;
			break;
		case PL320_MBOX_TXDONE_BY_ACK:
			mbox = mdev->mbox_ack;
			break;
		default:
			return ERR_PTR(-EINVAL);
	}

	/* requsted mailbox should match the txdone method */
	if (mbox != req_mbox)
		return ERR_PTR(-EINVAL);

	/* check that we don't allocate the same in_mbox to multiple channels */
	for (i = 0; i < PL320_MBOX_MAX_CHANNELS; i++) {
		chan_info = &mdev->channel_info[i];
		if (chan_info->mbox &&
		    ((in_mbox == chan_info->in_mbox) || (out_mbox == chan_info->out_mbox)))
			return ERR_PTR(-EBUSY);
	}

	/* find empty channel */
	for (i = 0; i < PL320_MBOX_MAX_CHANNELS; i++) {
		chan_info = &mdev->channel_info[i];
		if (!chan_info->mbox) {
			alloc_chan = &mbox->chans[i];
			break;
		}
	}

	/* check if channel found */
	if (!alloc_chan)
		return ERR_PTR(-EBUSY);

	chan_info->dst_channel_index = dst_channel_index;
	chan_info->in_mbox = in_mbox;
	chan_info->out_mbox = out_mbox;
	chan_info->txdone_method = txdone_method;
	chan_info->mbox = mbox;
	alloc_chan->con_priv = chan_info;

	return alloc_chan;
}

static const struct mbox_chan_ops pl320_mbox_ops = {
	.startup	= pl320_mbox_startup_chan,
	.shutdown	= pl320_mbox_shutdown_chan,
	.send_data	= pl320_mbox_send_data,
};

static const struct of_device_id pl320_mailbox_match[] = {
	{
		.compatible = "arm,pl320-mailbox"
	},
	{ }
};
MODULE_DEVICE_TABLE(of, pl320_mailbox_match);

static int pl230_init_mbox_controller(struct device *dev, bool txdone_irq, struct mbox_controller **out_mbox)
{
	struct mbox_chan *chans;
	struct mbox_controller *mbox;
	int ret;

	mbox = devm_kzalloc(dev, sizeof(*mbox), GFP_KERNEL);
	if (!mbox) {
		dev_err(dev, "Error allocating memory for mailbox\n");
		return -ENOMEM;
	}

	mbox->num_chans = PL320_MBOX_MAX_CHANNELS;
	mbox->txdone_irq = txdone_irq;
	mbox->ops = &pl320_mbox_ops;
	mbox->dev = dev;
	mbox->of_xlate = pl320_mbox_xlate;
	mbox->chans = devm_kcalloc(dev, mbox->num_chans, sizeof(*chans), GFP_KERNEL);

	if (!mbox->chans) {
		dev_err(dev, "Error allocating memory for mailbox channels\n");
		return -ENOMEM;
	}

	ret = devm_mbox_controller_register(dev, mbox);
	if (ret) {
		dev_err(dev, "Error registering mbox controller\n");
		return ret;
	}

	*out_mbox = mbox;
	return 0;
}

static int pl320_mbox_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct pl320_mbox_device *mdev;
	struct resource *res;
	int irq, ret;

	match = of_match_device(pl320_mailbox_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "No configuration found\n");
		return -ENODEV;
	}

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "no OF information\n");
		return -EINVAL;
	}

	mdev = devm_kzalloc(&pdev->dev, sizeof(*mdev), GFP_KERNEL);
	if (!mdev) {
		dev_err(&pdev->dev, "Error allocating memory for device\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, mdev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Error getting resource\n");
		return -ENODEV;
	}
	mdev->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mdev->base)) {
		dev_err(&pdev->dev, "Error receiving base address for device\n");
		return PTR_ERR(mdev->base);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Error receiving irq for device\n");
		return irq;
	}

	if (of_property_read_u32(pdev->dev.of_node, "arm,dev-ch-idx", &mdev->device_channel_index)) {
		dev_err(&pdev->dev, "Missing arm,dev-ch-idx\n");
		return -ENODEV;
	}

	mdev->dev		= &pdev->dev;

	ret = pl230_init_mbox_controller(&pdev->dev, true, &mdev->mbox_irq);
	if (ret) {
		dev_err(&pdev->dev, "Initializing mailbox controller (tx done by IRQ) failed\n");
		return -ENODEV;
	}

	ret = pl230_init_mbox_controller(&pdev->dev, false, &mdev->mbox_ack);
	if (ret) {
		dev_err(&pdev->dev, "Initializing mailbox controller (tx done by ACK) failed\n");
		return -ENODEV;
	}

	// registering irq should be last to prevent race conditions
	// that IRQ handler is called before the rest of the driver is ready
	ret = devm_request_irq(&pdev->dev, irq, pl320_mbox_interrupt, 0, dev_name(&pdev->dev), mdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register a mailbox IRQ handler: %d\n", ret);
		return ret;
	}

	return 0;
}

static struct platform_driver pl320_mbox_driver = {
	.probe = pl320_mbox_probe,
	.driver = {
		.name = "pl320-mailbox",
		.of_match_table = pl320_mailbox_match,
	},
};
module_platform_driver(pl320_mbox_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Arm PL320 Mailbox Controller");
MODULE_ALIAS("platform:pl320-mailbox");
