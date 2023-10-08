// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_LOG_H
#define XRP_LOG_H

#include "xrp_common.h"

#include <linux/types.h>

#define CYCLIC_LOG_MEM_SIZE (0x100000)

size_t xrp_cyclic_log_read(struct xvp *xvp, char __user *buffer, size_t size);

#endif