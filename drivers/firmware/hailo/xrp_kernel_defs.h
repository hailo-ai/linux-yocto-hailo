/*
 * XRP driver IOCTL codes and data structures
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Alternatively you can use and distribute this file under the terms of
 * the GNU General Public License version 2 or later.
 */

#ifndef _XRP_KERNEL_DEFS_H
#define _XRP_KERNEL_DEFS_H


/************************************************************************************
** This header describe the interface between XRP Kernel Driver and XRP user space.
** Each component has a copy of this header
** WHEN UPDATING THIS FILE, MAKE SURE TO UPDATE ALL COPIES
************************************************************************************/

#define XRP_IOCTL_MAGIC 'r'
#define XRP_IOCTL_ALLOC		_IO(XRP_IOCTL_MAGIC, 1)
#define XRP_IOCTL_FREE		_IO(XRP_IOCTL_MAGIC, 2)
#define XRP_IOCTL_QUEUE		_IO(XRP_IOCTL_MAGIC, 3)
#define XRP_IOCTL_QUEUE_NS	_IO(XRP_IOCTL_MAGIC, 4)
#define XRP_IOCTL_DMA_SYNC  _IO(XRP_IOCTL_MAGIC, 5)

struct xrp_ioctl_alloc {
    __u32 size;
    __u32 align;
    __u64 addr;
};

enum ioctl_buffer_flags {
    XRP_FLAG_READ = 0x1,
    XRP_FLAG_WRITE = 0x2,
    XRP_FLAG_READ_WRITE = 0x3,
};

struct xrp_ioctl_buffer {
    __u32 flags;
    __u32 size;
    __u64 addr;
};

enum {
    XRP_QUEUE_FLAG_NSID = 0x4,
    XRP_QUEUE_FLAG_PRIO = 0xff00,
    XRP_QUEUE_FLAG_PRIO_SHIFT = 8,

    XRP_QUEUE_VALID_FLAGS =
        XRP_QUEUE_FLAG_NSID |
        XRP_QUEUE_FLAG_PRIO,
};

struct xrp_ioctl_queue {
    __u32 flags;
    __u32 in_data_size;
    __u32 out_data_size;
    __u32 buffer_size;
    __u64 in_data_addr;
    __u64 out_data_addr;
    __u64 buffer_addr;
    __u64 nsid_addr;
};

enum ioctl_sync_access_time {
    XRP_FLAG_BUFFER_SYNC_START,
    XRP_FLAG_BUFFER_SYNC_END,
};

struct xrp_ioctl_sync_buffer {
    __u32 direction;
    __u32 access_time;
    __u32 size;
    __u64 addr;
};

#endif
