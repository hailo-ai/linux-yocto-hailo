// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include "xrp_log.h"

#include "xrp_common.h"
#include "xrp_share.h"

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/uaccess.h>

typedef struct {
    uint32_t read_index;
    uint32_t write_index;
} cyclic_log_header_t;

typedef struct {
    cyclic_log_header_t header;
    uint8_t data[];
} cyclic_log_buffer_t;

#define CYCLIC_LOG_DATA_SIZE(total_size) ((total_size) - sizeof(cyclic_log_header_t))

static size_t cyclic_log_get_data_size(struct xvp *xvp)
{
    cyclic_log_header_t *header = xvp->cyclic_log.addr;

    size_t log_size = 0;
    size_t read_index = header->read_index;
    size_t write_index = header->write_index;

    if (write_index >= read_index) {
        log_size = write_index - read_index;
    }
    else {
        log_size = CYCLIC_LOG_DATA_SIZE(xvp->cyclic_log.size) -
            (read_index - write_index);
    }

    return log_size;
}

size_t xrp_cyclic_log_read(struct xvp *xvp, char __user *buffer, size_t size)
{
    cyclic_log_buffer_t *log = xvp->cyclic_log.addr;
    const size_t log_data_size = CYCLIC_LOG_DATA_SIZE(xvp->cyclic_log.size);
    size_t read_size;
    uint32_t read_index;
    size_t chunk_size = 0;

    xrp_dma_sync_for_cpu(xvp, xvp->cyclic_log.paddr, xvp->cyclic_log.size, XRP_FLAG_READ);
    
    // if ready to read is bigger than the buffer size, read only buffer size bytes
    read_size = min(cyclic_log_get_data_size(xvp), size);
    read_index = log->header.read_index;

    // check if the offset should cycle back to beginning
    if (log->header.read_index + read_size >= log_data_size) {
        chunk_size = log_data_size - log->header.read_index;
        if (copy_to_user(buffer, log->data + read_index, read_size)) {
            dev_warn(xvp->dev, "Couldn't wirteback log to userspace\n");
        }

        buffer += chunk_size;
        read_size -= chunk_size;

        read_index = 0;
        chunk_size = read_size - chunk_size;
    }

    if (copy_to_user(buffer, log->data + read_index, read_size)) {
        dev_warn(xvp->dev, "Couldn't wirteback log to userspace\n");
    }

    log->header.read_index = read_index + read_size;
    
    xrp_dma_sync_for_cpu(xvp, xvp->cyclic_log.paddr, xvp->cyclic_log.size, XRP_FLAG_WRITE);

    return read_size;
}
