// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver example for Hailo AF
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved. 
 */

#include <linux/module.h>
#include "hailo15-events.h"

MODULE_AUTHOR("Tommy Hefner <tommyh@hailo.ai>");
MODULE_DESCRIPTION("Hailo AF Monitor Example");
MODULE_LICENSE("GPL v2");

extern struct hailo15_af_kevent af_kevent;

static int __init monitor_init(void)
{
	int ret;
	pr_info("%s - starting af monitor\n", __func__);
	while (1) {
		ret = wait_event_interruptible(af_kevent.wait_q,
					       af_kevent.ready != 0);
		if (ret) {
			pr_info("%s - monitor interrupted, exiting\n",
				__func__);
			return -EINTR;
		}

		pr_debug("%s - af event ready\n", __func__);
		mutex_lock(&af_kevent.data_lock);
		pr_debug(
			"%s - af data: sum_a = %u, sum_b = %u, sum_c = %u, lum_a = %u, lum_b = %u, lum_c = %u\n",
			__func__, af_kevent.sum_a, af_kevent.sum_b,
			af_kevent.sum_c, af_kevent.lum_a, af_kevent.lum_b,
			af_kevent.lum_c);
		af_kevent.ready = 0;
		mutex_unlock(&af_kevent.data_lock);
	}

	return 0;
}

static void __exit monitor_remove(void)
{
	pr_info("%s - af monitor\n", __func__);
}

module_init(monitor_init);
module_exit(monitor_remove);
