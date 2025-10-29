// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver example for Hailo AF
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved. 
 */

#include <linux/module.h>
#include <linux/workqueue.h>
#include "hailo15-events.h"

MODULE_AUTHOR("Tommy Hefner <tommyh@hailo.ai>");
MODULE_DESCRIPTION("Hailo AF Monitor Example");
MODULE_LICENSE("GPL v2");

extern struct hailo15_af_kevent af_kevent;

struct workqueue_struct *monitor_wq;
struct work_struct monitor_w;

bool interrupted = false;
spinlock_t interrupt_lock;

void monitor_af_statistics(struct work_struct *work) {
	int ret;
	pr_info("%s - starting af monitor\n", __func__);
	while (1) {
		ret = wait_event_interruptible(af_kevent.wait_q,
						   (af_kevent.ready != 0) || (interrupted));

		spin_lock(&interrupt_lock);
		if (interrupted) {
			pr_info("Work function interrupted explicitly.\n");
			spin_unlock(&interrupt_lock);
			return;
		}
		spin_unlock(&interrupt_lock);

		if (ret) {
			pr_info("%s - monitor interrupted, exiting\n",
				__func__);
			return;
		}

		pr_debug("%s - af event ready\n", __func__);
		mutex_lock(&af_kevent.data_lock);
		pr_debug(
			"%s - af data: sum_a = %u, sum_b = %u, sum_c = %u, lum_a = %u, lum_b = %u, lum_c = %u, vdid = %d\n",
			__func__, af_kevent.sum_a, af_kevent.sum_b,
			af_kevent.sum_c, af_kevent.lum_a, af_kevent.lum_b,
			af_kevent.lum_c, af_kevent.vdid);
		af_kevent.ready = 0;
		mutex_unlock(&af_kevent.data_lock);
	}

	return;
}

static int __init monitor_init(void)
{
	pr_info("creating af monitor work queue\n");
	spin_lock_init(&interrupt_lock);
	monitor_wq = alloc_ordered_workqueue("monitor_wq", WQ_HIGHPRI);
	if (!monitor_wq) {
		pr_err("can't create af workqueue\n");
		return -ENOMEM;
	}

	INIT_WORK(&monitor_w, monitor_af_statistics);
	queue_work(monitor_wq, &monitor_w);
	pr_info("Module loaded.\n");
	return 0;
}

static void __exit monitor_remove(void)
{
	if(monitor_wq) {
		pr_info("destroying af monitor work queue\n");
		spin_lock(&interrupt_lock);
		interrupted = true;
		spin_unlock(&interrupt_lock);

		// Wake up the waiting work
		wake_up_interruptible(&af_kevent.wait_q);
		cancel_work_sync(&monitor_w);
		destroy_workqueue(monitor_wq);
	}

	pr_info("%s - done\n", __func__);
}

module_init(monitor_init);
module_exit(monitor_remove);
