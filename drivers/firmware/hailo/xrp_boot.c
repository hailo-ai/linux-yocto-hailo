// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include "xrp_boot.h"

#include "xrp_firmware.h"
#include "xrp_kernel_dsp_interface.h"
#include "xrp_io.h"
#include "xrp_hw.h"

#include <linux/slab.h>
#include <linux/module.h>

#define XRP_DEFAULT_TIMEOUT 10

int firmware_command_timeout = XRP_DEFAULT_TIMEOUT;
bool unsafe_enable_reset = false;

module_param(unsafe_enable_reset, bool, 0644);
MODULE_PARM_DESC(unsafe_enable_reset, 
    "Enables DSP reset on file closure");

module_param(firmware_command_timeout, int, 0644);
MODULE_PARM_DESC(firmware_command_timeout,
    "Firmware command timeout in seconds");

static void xrp_sync_v2(struct xvp *xvp, void *hw_sync_data, size_t sz)
{
    struct xrp_dsp_sync_v2 __iomem *shared_sync = xvp->comm.addr;
    void __iomem *addr = shared_sync->hw_sync_data;

    xrp_comm_write(xrp_comm_put_tlv(&addr, XRP_DSP_SYNC_TYPE_HW_SPEC_DATA,
                    sz),
               hw_sync_data, sz);
    if (xvp->n_queues > 1) {
        struct xrp_dsp_sync_v2 __iomem *queue_sync;
        unsigned i;

        xrp_comm_write(
            xrp_comm_put_tlv(&addr, XRP_DSP_SYNC_TYPE_HW_QUEUES,
                     xvp->n_queues * sizeof(u32)),
            xvp->queue_priority, xvp->n_queues * sizeof(u32));
        for (i = 1; i < xvp->n_queues; ++i) {
            queue_sync = xvp->queue[i].comm;
            xrp_comm_write32(&queue_sync->sync, XRP_DSP_SYNC_IDLE);
        }
    }
    xrp_comm_put_tlv(&addr, XRP_DSP_SYNC_TYPE_LAST, 0);
}

static int xrp_sync_complete_v2(struct xvp *xvp, size_t sz)
{
    struct xrp_dsp_sync_v2 __iomem *shared_sync = xvp->comm.addr;
    void __iomem *addr = shared_sync->hw_sync_data;
    u32 type, len;

    xrp_comm_get_tlv(&addr, &type, &len);
    if (len != sz) {
        dev_err(xvp->dev, "HW spec data size modified by the DSP\n");
        return -EINVAL;
    }
    if (!(type & XRP_DSP_SYNC_TYPE_ACCEPT))
        dev_info(xvp->dev, "HW spec data not recognized by the DSP\n");

    if (xvp->n_queues > 1) {
        void __iomem *p = xrp_comm_get_tlv(&addr, &type, &len);

        if (len != xvp->n_queues * sizeof(u32)) {
            dev_err(xvp->dev,
                "Queue priority size modified by the DSP\n");
            return -EINVAL;
        }
        if (type & XRP_DSP_SYNC_TYPE_ACCEPT) {
            xrp_comm_read(p, xvp->queue_priority,
                      xvp->n_queues * sizeof(u32));
        } else {
            dev_info(
                xvp->dev,
                "Queue priority data not recognized by the DSP\n");
            xvp->n_queues = 1;
        }
    }
    return 0;
}

static void *get_hw_sync_data(struct xvp *xvp, size_t *sz)
{
    xrp_hailo_sync_data_t *hw_sync_data = 
        kmalloc(sizeof(*hw_sync_data), GFP_KERNEL);

    if (!hw_sync_data) {
        return NULL;
    }

    *sz = sizeof(*hw_sync_data);

    return hw_sync_data;
}

static int xrp_synchronize(struct xvp *xvp)
{
    size_t sz;
    void *hw_sync_data;
    unsigned long deadline = jiffies + firmware_command_timeout * HZ;
    struct xrp_dsp_sync_v1 __iomem *shared_sync = xvp->comm.addr;
    int ret;
    u32 v, v1;

    hw_sync_data = get_hw_sync_data(xvp, &sz);
    if (!hw_sync_data) {
        ret = -ENOMEM;
        goto err;
    }
    ret = -ENODEV;

    xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_START);

    mb();
    do {
        v = xrp_comm_read32(&shared_sync->sync);
        if (v != XRP_DSP_SYNC_START)
            break;
        schedule();
    } while (time_before(jiffies, deadline));

    switch (v) {
    case XRP_DSP_SYNC_DSP_READY_V1:
        if (xvp->n_queues > 1) {
            dev_info(
                xvp->dev,
                "Queue priority data not recognized by the DSP\n");
            xvp->n_queues = 1;
        }
        xrp_comm_write(&shared_sync->hw_sync_data, hw_sync_data, sz);
        break;
    case XRP_DSP_SYNC_DSP_READY_V2:
        xrp_sync_v2(xvp, hw_sync_data, sz);
        break;
    case XRP_DSP_SYNC_START:
        dev_err(xvp->dev, "DSP is not ready for synchronization\n");
        goto err;
    default:
        dev_err(xvp->dev,
            "DSP response to XRP_DSP_SYNC_START is not recognized\n");
        goto err;
    }

    mb();
    xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_HOST_TO_DSP);

    do {
        mb();
        v1 = xrp_comm_read32(&shared_sync->sync);
        if (v1 == XRP_DSP_SYNC_DSP_TO_HOST)
            break;
        schedule();
    } while (time_before(jiffies, deadline));

    if (v1 != XRP_DSP_SYNC_DSP_TO_HOST) {
        dev_err(xvp->dev,
            "DSP haven't confirmed initialization data reception\n");
        goto err;
    }

    if (v == XRP_DSP_SYNC_DSP_READY_V2) {
        ret = xrp_sync_complete_v2(xvp, sz);
        if (ret < 0)
            goto err;
    }

    xrp_send_device_irq(xvp);

    ret = wait_for_completion_timeout(&xvp->queue[0].completion, firmware_command_timeout * HZ);
    if (ret == 0) {
        ret = -ENODEV;
        dev_err(xvp->dev, "host IRQ mode is requested, but DSP couldn't deliver IRQ during synchronization\n");
        goto err;
    }
    
    ret = 0;
err:
    kfree(hw_sync_data);
    xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_IDLE);
    return ret;
}

static int xrp_shutdown_hw(struct xvp *xvp)
{
    if (xvp->state == DSP_STATE_FATAL_ERROR) {
        return -EPERM;
    }

    xrp_halt_dsp(xvp);

    if (unsafe_enable_reset) {
        int ret = xrp_disable_dsp(xvp);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

static int xrp_start(struct xvp *xvp)
{
    int ret = 0;
    struct xrp_dsp_sync_v1 __iomem *shared_sync = xvp->comm.addr;

    xrp_comm_write32(&shared_sync->sync, XRP_DSP_SYNC_IDLE);
    mb();

    xrp_release_dsp(xvp);

    ret = xrp_synchronize(xvp);
    if (ret < 0) {
        xrp_halt_dsp(xvp);
        dev_err(xvp->dev,
            "%s: couldn't synchronize with the DSP core\n",
            __func__);
        goto exit;
    }

exit:
    return ret;
}

int xrp_boot(struct xvp *xvp)
{
    unsigned i;
    int ret = 0;

    dev_dbg(xvp->dev, "%s\n", __func__);

    if (xvp->state == DSP_STATE_FATAL_ERROR) {
        dev_err(xvp->dev,
            "DSP encountered fatal error before. Reboot requied. Aborting\n");
        ret = -EPERM;
        goto fatal_out;
    }

    // only run on first usage
    if (1 != atomic_inc_return(&xvp->open_counter)) {
        goto out;
    }

    if (xvp->state == DSP_STATE_RUNNING) {
        if (unsafe_enable_reset) {
            ret = xrp_shutdown_hw(xvp);
            if (ret < 0) {
                goto out;
            }    
        } else {
            xrp_release_dsp(xvp);
            goto out;     
            
        }
    }

    ret = xrp_init_firmware(xvp);
    if (ret < 0)
        goto out;

    for (i = 0; i < xvp->n_queues; ++i)
        mutex_lock(&xvp->queue[i].lock);

    ret = xrp_enable_dsp(xvp);
    if (ret < 0) {
        dev_err(xvp->dev, "couldn't enable DSP\n");
        goto mutex_out;
    }

    ret = xrp_load_firmware(xvp);
    if (ret < 0)
        goto mutex_out;

    ret = xrp_start(xvp);
    if (ret < 0)
        goto mutex_out;

    xvp->state = DSP_STATE_RUNNING;

mutex_out:
    if (ret < 0) {
        (void)xrp_shutdown_hw(xvp);
        xrp_release_firmware(xvp);
    }
    
    for (i = 0; i < xvp->n_queues; ++i)
        mutex_unlock(&xvp->queue[i].lock);

out:
    if (ret < 0)
        atomic_dec(&xvp->open_counter);

fatal_out:
    return ret;
}

int xrp_shutdown(struct xvp *xvp)
{
    dev_dbg(xvp->dev, "%s\n", __func__);

    // only run when counter is zero
    if (!atomic_dec_and_test(&xvp->open_counter)) {
        goto ret;
    }

    (void)xrp_shutdown_hw(xvp);

    if (unsafe_enable_reset) {
        xrp_release_firmware(xvp);
        xvp->state = DSP_STATE_CLOSED;
    }

ret:
    return 0;
}