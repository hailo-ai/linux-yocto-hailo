// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include "xrp_exec.h"

#include "xrp_share.h"
#include "xrp_hw.h"
#include "xrp_kernel_dsp_interface.h"
#include "xrp_boot.h"
#include "xrp_io.h"

#include <linux/uaccess.h>
#include <linux/mmap_lock.h>
#include <linux/nospec.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/timekeeping.h>

static bool support_shadow_copy = true;
module_param(support_shadow_copy, bool, 0644);
MODULE_PARM_DESC(support_shadow_copy,
    "If enabled, the driver will create physically contiguous shadow copy of data buffers if required."
    "If disabled, the driver will fail the request if buffers are not already physically contiguous");

struct xrp_request {
    // Request data from userspace
    struct xrp_ioctl_queue ioctl_queue;

    // nsid (inline)
    u8 nsid[XRP_DSP_CMD_NAMESPACE_ID_SIZE];

    // in_data mapping or inline data
    union {
        struct {
            phys_addr_t in_data_phys;
            uint32_t dsp_in_data_phys;
            struct xrp_mapping in_data_mapping;
        };
        u8 in_data[XRP_DSP_CMD_INLINE_DATA_SIZE];
    };

    // out_data mapping or inline data
    union {
        struct {
            phys_addr_t out_data_phys;
            uint32_t dsp_out_data_phys;
            struct xrp_mapping out_data_mapping;
        };
        u8 out_data[XRP_DSP_CMD_INLINE_DATA_SIZE];
    };

    // Number of buffers in request
    size_t n_buffers;

    // buffers descriptors mapping or inline
    union {
        struct {
            phys_addr_t buffers_descriptors_phys;
            uint32_t dsp_buffers_descriptors_phys;
            struct xrp_mapping buffers_descriptors_mapping;
        };
        struct xrp_dsp_buffer
            buffers_descriptors_data[XRP_DSP_CMD_INLINE_BUFFER_COUNT];
    };

    // Pointer to array of mapping of the request buffers (allocated by the driver)
    struct xrp_mapping *buffers_mapping;

    // Pointer to array of buffers descriptors 
    // points to buffers_descriptors_data if n_buffers <= XRP_DSP_CMD_INLINE_BUFFER_COUNT or allocated by the driver
    struct xrp_dsp_buffer *buffers_descriptors;
};

#define REGISTER_PERF_TIME(event, perf_stats, perf_stats_enabled) \
    do { \
        if (perf_stats_enabled) perf_stats->event = ktime_get_ns(); \
    } while(0)

static long xrp_unmap_request(struct xvp *xvp, struct xrp_request *rq,
    bool writeback)
{
    size_t n_buffers = rq->n_buffers;
    size_t i;
    long ret = 0;
    long rc;

    dev_dbg(xvp->dev, "%s: unsharing request (writeback: %d)\n", __func__, writeback);

    if (rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
        dev_dbg(xvp->dev, "%s: unsharing in_data\n", __func__);
        xrp_unshare(xvp, &rq->in_data_mapping,
            writeback ? XRP_FLAG_READ : 0);
    }
    
    if (rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
        dev_dbg(xvp->dev, "%s: unsharing out_data\n", __func__);
        rc = xrp_unshare(xvp, &rq->out_data_mapping,
                writeback ? XRP_FLAG_WRITE : 0);

        if (rc < 0) {
            dev_err(xvp->dev, "%s: out_data could not be unshared\n", __func__);
            ret = rc;
        }
    } else if (writeback) {
        if (copy_to_user((void __user *)(unsigned long)rq->ioctl_queue.out_data_addr,
                 rq->out_data, rq->ioctl_queue.out_data_size)) {
            dev_err(xvp->dev, "%s: out_data could not be copied\n", __func__);
            ret = -EFAULT;
        }
    }

    if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT) {
        dev_dbg(xvp->dev, "%s: unsharing buffers descriptors\n", __func__);
        xrp_unshare(xvp, &rq->buffers_descriptors_mapping,
            writeback ? XRP_FLAG_READ_WRITE : 0);
    }

    for (i = 0; i < n_buffers; ++i) {
        dev_dbg(xvp->dev, "%s: unsharing buffer %zd\n", __func__, i);
        rc = xrp_unshare(xvp, rq->buffers_mapping + i,
            writeback ? rq->buffers_descriptors[i].map_flags : 0);
        if (rc < 0) {
            dev_err(xvp->dev, "%s: buffer %zd could not be unshared\n",
                 __func__, i);
            ret = rc;
        }
    }

    if (n_buffers) {
        kfree(rq->buffers_mapping);
        if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT) {
            kfree(rq->buffers_descriptors);
        }
        rq->n_buffers = 0;
    }

    return ret;
}

static long xrp_map_request(struct file *filp, struct xrp_request *rq,
                struct mm_struct *mm)
{
    struct xvp *xvp = filp->private_data;
    struct xrp_ioctl_buffer __user *buffer;
    size_t n_buffers = rq->ioctl_queue.buffer_size /
        sizeof(struct xrp_ioctl_buffer);

    size_t i;
    long ret = 0;

    // copy nsid from userspace (if exists)
    if ((rq->ioctl_queue.flags & XRP_QUEUE_FLAG_NSID) &&
        copy_from_user(rq->nsid,
               (void __user *)(unsigned long)rq->ioctl_queue.nsid_addr,
               sizeof(rq->nsid))) {
        dev_err(xvp->dev, "%s: nsid could not be copied\n", __func__);
        return -EINVAL;
    }

    // allocate buffers_mapping and buffers_descriptors (if needed)
    rq->n_buffers = n_buffers;
    if (n_buffers) {
        rq->buffers_mapping =
            kzalloc(n_buffers * sizeof(*rq->buffers_mapping), GFP_KERNEL);
        if (!rq->buffers_mapping)
            return -ENOMEM;
        if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT) {
            rq->buffers_descriptors = kmalloc(n_buffers * sizeof(*rq->buffers_descriptors),
                GFP_KERNEL);
            if (!rq->buffers_descriptors) {
                kfree(rq->buffers_mapping);
                return -ENOMEM;
            }
        } else {
            rq->buffers_descriptors = rq->buffers_descriptors_data;
        }
    }


    // share in_data or copy it inline
    if (rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
        dev_dbg(xvp->dev, "%s: sharing in_data\n", __func__);

        mmap_read_lock(mm);
        ret = xrp_share_block(filp, (void *)rq->ioctl_queue.in_data_addr,
        rq->ioctl_queue.in_data_size, XRP_FLAG_READ, &rq->in_data_phys,
        &rq->dsp_in_data_phys, &rq->in_data_mapping,
        LUT_MAPPING_IN_DATA, true, true);
        mmap_read_unlock(mm);
        if(ret < 0) {
            dev_err(xvp->dev, "%s: in_data could not be shared\n",
                 __func__);
            goto share_err;
        }
    } else {
        if (copy_from_user(rq->in_data,
                   (void __user *)(unsigned long)rq->ioctl_queue.in_data_addr,
                   rq->ioctl_queue.in_data_size)) {
            dev_err(xvp->dev, "%s: in_data could not be copied\n",
                 __func__);
            ret = -EFAULT;
            goto share_err;
        }
    }

    // share out_data or copy it inline
    if (rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE) {
        dev_dbg(xvp->dev, "%s: sharing out_data\n", __func__);
        mmap_read_lock(mm);
        ret = xrp_share_block(filp, (void *)rq->ioctl_queue.out_data_addr,
        rq->ioctl_queue.out_data_size, XRP_FLAG_WRITE, 
        &rq->out_data_phys, &rq->dsp_out_data_phys,
        &rq->out_data_mapping, LUT_MAPPING_OUT_DATA, true, true);
        mmap_read_unlock(mm);
        if (ret < 0) {
            dev_err(xvp->dev, "%s: out_data could not be shared\n",
                 __func__);
            goto share_err;
        }
    }

    buffer = (void __user *)(unsigned long)rq->ioctl_queue.buffer_addr;

    for (i = 0; i < n_buffers; ++i) {
        struct xrp_ioctl_buffer ioctl_buffer;
        phys_addr_t buffer_phys = ~0ul;

        if (copy_from_user(&ioctl_buffer, buffer + i,
                   sizeof(ioctl_buffer))) {
            dev_err(xvp->dev, "%s: buffer %zd could not be copied\n",
                    __func__, i);
            ret = -EFAULT;
            goto share_err;
        }
        if (ioctl_buffer.flags & XRP_FLAG_READ_WRITE) {
            dev_dbg(xvp->dev, "%s: sharing buffer %zd\n", __func__, i);
            if (ioctl_buffer.memory_type == XRP_MEMORY_TYPE_USERPTR) {
                dev_dbg(xvp->dev, "%s: sharing buffer %zd (virtual address) \n", __func__, i);
                mmap_read_lock(mm);
		        ret = xrp_share_block(filp, (void *)ioctl_buffer.addr,
                ioctl_buffer.size, ioctl_buffer.flags, &buffer_phys, NULL,
                rq->buffers_mapping + i, NO_LUT_MAPPING, true, false);
                mmap_read_unlock(mm);
            } else {
                dev_dbg(xvp->dev, "%s: sharing buffer %zd (dmabuf) \n", __func__, i);
                mmap_read_lock(mm);
                ret = xrp_share_dmabuf(filp, ioctl_buffer.fd, ioctl_buffer.size, ioctl_buffer.flags, &buffer_phys,
                    rq->buffers_mapping + i);   
                mmap_read_unlock(mm);
            }
            if (ret < 0) {
                dev_err(xvp->dev, "%s: buffer %zd could not be shared\n", __func__, i);
                goto share_err;
            }

            if ((rq->buffers_mapping[i].type == XRP_MAPPING_ALIEN) && 
                (rq->buffers_mapping[i].alien_mapping.type == ALIEN_COPY)) {
                    // shadow copy was created for buffer
                    // in case we support it, warn user once to let them know about degradation in performance
                    if (support_shadow_copy) {
                        dev_warn_once(xvp->dev, "%s: A buffer used in the request is not optimal for DSP operation, "
                        "resulting in degraded performance due to the creation of a buffer data copy. " 
                        "It is recommended to address the issue to improve performance", __func__);
                    } else {
                        dev_err(xvp->dev, "%s: buffer %zd is unsuitable for DSP operation\n", __func__, i);
                        ret = -EINVAL;
                        goto share_err;
                    }
                }
        }

        rq->buffers_descriptors[i] = (struct xrp_dsp_buffer){
            .access_flags = ioctl_buffer.flags,
            .map_flags = 0,
            .size = ioctl_buffer.size,
            .addr = buffer_phys,
        };
    }

    // share buffers descriptors (if needed)
    if (n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT) {
        dev_dbg(xvp->dev, "%s: sharing buffers descriptors\n", __func__);
        mmap_read_lock(mm);
        ret = xrp_share_kernel(xvp, rq->buffers_descriptors,
            n_buffers * sizeof(*rq->buffers_descriptors), XRP_FLAG_READ_WRITE,
            &rq->buffers_descriptors_phys, &rq->dsp_buffers_descriptors_phys, &rq->buffers_descriptors_mapping,
            LUT_MAPPING_BUFFER_METADATA, true);
        mmap_read_unlock(mm);
        if(ret < 0) {
            dev_err(xvp->dev, "%s: buffers descriptors could not be shared\n",
                 __func__);
            goto share_err;
        }
    }
share_err:
    if (ret < 0)
        xrp_unmap_request(xvp, rq, false);
    return ret;
}

static long xrp_wait_for_cmd_completion(struct xvp *xvp, struct xrp_comm *comm)
{
    long timeout = firmware_command_timeout * HZ;

    if (xrp_is_cmd_complete(xvp, comm))
        return 0;
    do {
        timeout = wait_for_completion_timeout(&comm->completion, timeout);
        if (xrp_is_cmd_complete(xvp, comm))
            return 0;
    } while (timeout > 0);

    if (timeout == 0) {
        dev_err(xvp->dev, "%s: timeout waiting for command completion\n", __func__);
        return -EBUSY;
    }
    return timeout;
}

static void xrp_fill_hw_request(struct xvp *xvp,
                struct xrp_dsp_cmd *cmd,
                struct xrp_request *rq)
{
    
    xrp_comm_write32(&cmd->in_data_size, rq->ioctl_queue.in_data_size);
    xrp_comm_write32(&cmd->out_data_size, rq->ioctl_queue.out_data_size);
    xrp_comm_write32(&cmd->buffer_size,
             rq->n_buffers * sizeof(struct xrp_dsp_buffer));

    if (rq->ioctl_queue.in_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
        xrp_comm_write32(&cmd->in_data_addr, rq->dsp_in_data_phys);
    else
        xrp_comm_write(&cmd->in_data,
            rq->in_data, rq->ioctl_queue.in_data_size);

    if (rq->ioctl_queue.out_data_size > XRP_DSP_CMD_INLINE_DATA_SIZE)
        xrp_comm_write32(&cmd->out_data_addr, rq->dsp_out_data_phys);
    else
        xrp_comm_write(&cmd->out_data,
            rq->out_data, rq->ioctl_queue.out_data_size);

    if (rq->n_buffers > XRP_DSP_CMD_INLINE_BUFFER_COUNT)
        xrp_comm_write32(&cmd->buffer_addr, rq->dsp_buffers_descriptors_phys);
    else
        xrp_comm_write(&cmd->buffer_data, rq->buffers_descriptors,
            rq->n_buffers * sizeof(struct xrp_dsp_buffer));

    if (rq->ioctl_queue.flags & XRP_QUEUE_FLAG_NSID)
        xrp_comm_write(&cmd->nsid, rq->nsid, sizeof(rq->nsid));

#ifdef DEBUG
    {
        struct xrp_dsp_cmd dsp_cmd;
        xrp_comm_read(cmd, &dsp_cmd, sizeof(dsp_cmd));
        dev_dbg(xvp->dev, "%s: cmd for DSP: %p: %*ph\n",
             __func__, cmd,
             (int)sizeof(dsp_cmd), &dsp_cmd);
    }
#endif

    wmb();
    /* update flags */
    xrp_comm_write32(&cmd->flags,
                (rq->ioctl_queue.flags & ~XRP_DSP_CMD_FLAG_RESPONSE_VALID) |
                XRP_DSP_CMD_FLAG_REQUEST_VALID);
}

static long xrp_complete_hw_request(struct xvp *xvp, struct xrp_dsp_cmd *cmd,
                    struct xrp_request *rq)
{
    u32 flags = xrp_comm_read32(&cmd->flags);

    if (rq->ioctl_queue.out_data_size <= XRP_DSP_CMD_INLINE_DATA_SIZE)
        xrp_comm_read(&cmd->out_data, rq->out_data,
                  rq->ioctl_queue.out_data_size);
    if (rq->n_buffers <= XRP_DSP_CMD_INLINE_BUFFER_COUNT)
        xrp_comm_read(&cmd->buffer_data, rq->buffers_descriptors,
                  rq->n_buffers * sizeof(struct xrp_dsp_buffer));
    xrp_comm_write32(&cmd->flags, 0);

    if (flags & XRP_DSP_CMD_FLAG_RESPONSE_DELIVERY_FAIL) { 
        dev_err(xvp->dev, "%s: firmware returned error for request\n", __func__);
        return -ENXIO;
    }

    return 0;
}

long xrp_ioctl_submit_sync(struct file *filp, struct xrp_ioctl_queue __user *p)
{
    struct xvp *xvp = filp->private_data;
    struct xrp_comm *queue = xvp->queue;
    struct xrp_request *rq;
    long ret = 0;
    ktime_t dsp_start_time, dsp_end_time;
    uint64_t dsp_command_time_us;
    uint8_t current_threads;
    kernel_perf_stats_t kernel_perf_stats;
    
    rq = kzalloc(sizeof(struct xrp_request), GFP_KERNEL);
    if (!rq) {
        dev_err(xvp->dev, "%s: allocation failed\n", __func__);
        return -ENOMEM;
    }

    if (copy_from_user(&rq->ioctl_queue, p, sizeof(*p))) {
        dev_err(xvp->dev, "%s: copy from user failed\n", __func__);
        ret =-EFAULT;
        goto err;
    }

    REGISTER_PERF_TIME(kernel_received_ioctl, (&kernel_perf_stats), rq->ioctl_queue.perf_stats_enabled);

    if (rq->ioctl_queue.flags & ~XRP_QUEUE_VALID_FLAGS) {
        dev_err(xvp->dev, "%s: invalid flags 0x%08x\n",
            __func__, rq->ioctl_queue.flags);
        ret = -EINVAL;
        goto err;
    }

    if (xvp->n_queues > 1) {
        unsigned n = (rq->ioctl_queue.flags & XRP_QUEUE_FLAG_PRIO) >>
            XRP_QUEUE_FLAG_PRIO_SHIFT;

        if (n >= xvp->n_queues)
            n = xvp->n_queues - 1;
        n = array_index_nospec(n, xvp->n_queues);
        queue = xvp->queue_ordered[n];
        dev_dbg(xvp->dev, "%s: priority: %d -> %d\n",
            __func__, n, queue->priority);
    }

    atomic_inc(&xvp->stats.current_threads);
    REGISTER_PERF_TIME(waiting_on_mutex, (&kernel_perf_stats), rq->ioctl_queue.perf_stats_enabled);
    mutex_lock(&queue->lock);
    REGISTER_PERF_TIME(mutex_acquired, (&kernel_perf_stats), rq->ioctl_queue.perf_stats_enabled);

    // we've aquired the mutex
    // further statistics accesses don't have to be atomic
    current_threads = atomic_read(&xvp->stats.current_threads);
    if (current_threads > xvp->stats.max_threads) {
        xvp->stats.max_threads = current_threads;
    }

    ret = xrp_map_request(filp, rq, current->mm);
    if (ret < 0) {
        dev_err(xvp->dev, "%s: map request failed %ld\n", __func__, ret);
        goto mutex_err;
    }

    xrp_fill_hw_request(xvp, queue->comm, rq);

    xrp_send_device_irq(xvp);
    REGISTER_PERF_TIME(irq_sent, (&kernel_perf_stats), rq->ioctl_queue.perf_stats_enabled);

    dsp_start_time = ktime_get();
    ret = xrp_wait_for_cmd_completion(xvp, queue);
    if (ret != 0) {
        dev_err(xvp->dev, "%s: failed waiting for command %ld\n", __func__, ret);
        goto mutex_err;
    }
    dsp_end_time = ktime_get();
    REGISTER_PERF_TIME(fw_finished, (&kernel_perf_stats), rq->ioctl_queue.perf_stats_enabled);
    dev_dbg(xvp->dev, "%s: cmd completed", __func__);

    dsp_command_time_us = ktime_us_delta(dsp_end_time, dsp_start_time);
    if (dsp_command_time_us > xvp->stats.max_dsp_time_us) {
        xvp->stats.max_dsp_time_us = dsp_command_time_us;
    }
    xvp->stats.total_dsp_time_us += dsp_command_time_us;
    xvp->stats.total_dsp_commands++;

    /* copy back inline data */
    ret = xrp_complete_hw_request(xvp, queue->comm, rq);

    if (ret != 0) {
        dev_err(xvp->dev, "%s: failed completing hw request %ld\n", __func__, ret);
        (void)xrp_unmap_request(xvp, rq, false);
        goto mutex_err;
    }

    ret = xrp_unmap_request(xvp, rq, true);
    if (ret != 0) {
        dev_err(xvp->dev, "%s: failed unmaping request %ld\n", __func__, ret);
        goto mutex_err;
    }

mutex_err:
    mutex_unlock(&queue->lock);
     if (rq->ioctl_queue.perf_stats_enabled){
        REGISTER_PERF_TIME(mutex_released, (&kernel_perf_stats), rq->ioctl_queue.perf_stats_enabled);
        if (copy_to_user((void __user *)(unsigned long)rq->ioctl_queue.kernel_perf_stats_addr,
                         &kernel_perf_stats, sizeof(kernel_perf_stats))) {
            dev_err(xvp->dev, "%s: kernel perf stats could not be copied\n", __func__);
            ret = -EFAULT;
        }
    }
    atomic_dec(&xvp->stats.current_threads);
err:
    kfree(rq);

    return ret;
}

long xrp_ioctl_get_stats(struct file *filp, struct xrp_ioctl_stats __user *p)
{
    struct xvp *xvp = filp->private_data;
    struct xrp_ioctl_stats xrp_ioctl_stats;

    if (copy_from_user(&xrp_ioctl_stats, p, sizeof(*p))) {
        dev_err(xvp->dev, "%s: copy_from_user() failed\n", __func__);
        return -EFAULT;
    }

    // I set this before acquiring the mutex to avoid acquisition after all
    // threads are done and thus always getting 0 current threads
    xrp_ioctl_stats.current_threads_using_dsp =
	    atomic_read(&xvp->stats.current_threads);

    mutex_lock(&xvp->queue->lock);

    if (xrp_ioctl_stats.reset) {
	    xvp->stats.total_dsp_time_us = 0;
	    xvp->stats.max_dsp_time_us = 0;
	    xvp->stats.total_dsp_commands = 0;
	    xvp->stats.max_threads = 0;

	    mutex_unlock(&xvp->queue->lock);
    } else {
	    xrp_ioctl_stats.total_dsp_time_us = xvp->stats.total_dsp_time_us;
	    xrp_ioctl_stats.max_dsp_command_time_us = xvp->stats.max_dsp_time_us;
	    xrp_ioctl_stats.max_threads_using_dsp = xvp->stats.max_threads;
	    xrp_ioctl_stats.total_dsp_commands = xvp->stats.total_dsp_commands;

	    mutex_unlock(&xvp->queue->lock);

	    if (copy_to_user(p, &xrp_ioctl_stats, sizeof(*p))) {
		    dev_err(xvp->dev, "%s: copy_to_user() failed\n", __func__);
		    return -EFAULT;
	    }
    }

    return 0;
}