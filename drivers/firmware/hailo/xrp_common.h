// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Internal XRP structures definition.
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_COMMON_H
#define XRP_COMMON_H

#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/mailbox_client.h>
#include <linux/types.h>

struct device;
struct firmware;
struct xrp_allocation_pool;

enum dsp_state {
    DSP_STATE_CLOSED,
    DSP_STATE_RUNNING,
    DSP_STATE_FATAL_ERROR
};

enum ddr_mem_ranges {
    DDR_MEM_RANGES_CODE,

    DDR_MEM_RANGES_COUNT
};

enum local_mem_ranges {
    LOCAL_MEM_RANGES_IRAM,
    LOCAL_MEM_RANGES_DRAM0,
    LOCAL_MEM_RANGES_DRAM1,

    LOCAL_MEM_RANGES_COUNT
};

struct xrp_comm {
    struct mutex lock;
    void __iomem *comm;
    struct completion completion;
    u32 priority;
};

struct xrp_mapping;
struct cyclic_log {
    struct xrp_mapping *mapping;
    void *addr;
    phys_addr_t paddr;
    uint32_t dsp_paddr;
    size_t size;
};

struct mem_range {
    phys_addr_t start;
    phys_addr_t end;
};

union mem_ranges {
    __attribute__((__packed__)) struct {
        struct mem_range ddr[DDR_MEM_RANGES_COUNT];
        struct mem_range local[LOCAL_MEM_RANGES_COUNT];
    };
    struct mem_range all[0];
};

struct comm_buffer {
  void *addr;
  phys_addr_t paddr;
  dma_addr_t dma_addr;
  uint32_t dsp_paddr;
  size_t size;
};

struct xvp {
    struct device *dev;
    const char *firmware_name;
    const struct firmware *firmware;
    struct miscdevice miscdev;
    struct miscdevice misclogdev;
    unsigned n_queues;

    u32 *queue_priority;
    struct xrp_comm *queue;
    struct xrp_comm **queue_ordered;
    struct comm_buffer comm;
    phys_addr_t pmem;

    struct xrp_allocation_pool *pool;
    int nodeid;

    void __iomem *dsp_config;
    phys_addr_t dsp_config_phys;
    struct reset_control *dsp_reset;
    struct clk *dsp_config_clock;
    struct clk *dsp_pll_clock;
    struct clk *dsp_clock;
    struct mbox_client mbox_client;
    struct mbox_chan *mbox_chan;
    struct cyclic_log cyclic_log;
    union mem_ranges mem_ranges;
    u32 reset_vector_address;
    atomic_t open_counter;
    enum dsp_state state;

    spinlock_t busy_list_lock;
    struct xrp_allocation *busy_list;
};

#endif
