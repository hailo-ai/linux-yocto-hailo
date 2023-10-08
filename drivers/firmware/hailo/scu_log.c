// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/panic_notifier.h>

struct scu_log_data {
	struct resource *log_res;
	void __iomem *log_mem;
	u64 log_size;
	u64 idx;
	char *buffer;
	struct miscdevice miscdev;
	struct notifier_block panic_block;
};

static inline struct scu_log_data *filep_to_data(struct file *fp)
{
	/* In misc devices, fp->private_data is a pointer to the `struct miscdevice` */
	return container_of(fp->private_data, struct scu_log_data, miscdev);
}

static inline struct scu_log_data *panic_block_to_data(struct notifier_block *panic_block)
{
	return container_of(panic_block, struct scu_log_data, panic_block);
}

static inline char scu_log_read_byte(struct scu_log_data *data)
{
	return readb(data->log_mem + data->idx);
}

static inline void scu_log_advance(struct scu_log_data *data)
{
	data->idx = (data->idx + 1) % data->log_size;
}

static ssize_t __scu_log_read(struct scu_log_data *data, size_t count)
{
	int log_idx;
	char ch;

	for (log_idx = 0; log_idx < count; log_idx++) {
		ch = scu_log_read_byte(data);
		if (!ch) {
			break;
		}
		data->buffer[log_idx] = ch;
		scu_log_advance(data);
	}

	return log_idx;
}

static ssize_t scu_log_read(struct file *fp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct scu_log_data *data = filep_to_data(fp);
	ssize_t bytes_read = 0;

	bytes_read = __scu_log_read(data, min((size_t)data->log_size, count));
	if (copy_to_user(buf, data->buffer, bytes_read)) {
		return -EFAULT;
	}

	return bytes_read;
}


static const struct file_operations scu_log_fops = {
	.owner          = THIS_MODULE,
	.read           = scu_log_read,
};

static int panic_event(struct notifier_block *this,
		       unsigned long         event,
		       void                  *ptr)
{
	struct scu_log_data *data = panic_block_to_data(this);
	int bytes_read;

	bytes_read = __scu_log_read(data, data->log_size);
	pr_emerg("======================\nPANIC: dumping scu log\n%.*s\n======================",
		 bytes_read, data->buffer);

	return NOTIFY_DONE;
}

static int scu_log_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct scu_log_data *data;
	int err;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "cannot allocate driver data\n");
		return -ENOMEM;
	}

	data->log_mem = devm_platform_get_and_ioremap_resource(pdev, 0, &data->log_res);
	if (IS_ERR(data->log_mem)) {
		dev_err(dev, "cannot get log memory resource\n");
		return PTR_ERR(data->log_mem);
	}

	data->log_size = resource_size(data->log_res);

	data->buffer = devm_kzalloc(dev, data->log_size + 1, GFP_KERNEL);
	if (!data->buffer) {
		dev_err(dev, "cannot allocate log buffer\n");
		return -ENOMEM;
	}

	data->miscdev = (struct miscdevice) {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "scu_log",
		.fops = &scu_log_fops
	};

	data->panic_block.notifier_call	= panic_event;
	data->panic_block.priority	= 200; /* priority: INT_MAX >= x >= 0 */

	err = misc_register(&data->miscdev);
	if (err) {
		dev_err(dev, "cannot register device\n");
		return err;
	}

	platform_set_drvdata(pdev, data);

	atomic_notifier_chain_register(&panic_notifier_list, &data->panic_block);

	return 0;
}

static int scu_log_remove(struct platform_device *pdev)
{
	struct scu_log_data *data = platform_get_drvdata(pdev);
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &data->panic_block);
	misc_deregister(&data->miscdev);
	return 0;
}

static const struct of_device_id scu_log_of_match[] = {
	{ .compatible = "hailo,scu-log" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, scu_log_of_match);

static struct platform_driver scu_log_driver = {
	.probe  = scu_log_probe,
	.remove = scu_log_remove,
	.driver = {
		.name = "scu_log",
		.of_match_table = of_match_ptr(scu_log_of_match),
	},
};

module_platform_driver(scu_log_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SCU firmware log");
