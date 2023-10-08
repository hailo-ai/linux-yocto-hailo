// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_EXEC_H
#define XRP_EXEC_H

#include "xrp_common.h"
#include "xrp_kernel_defs.h"

#include <linux/types.h>

long xrp_ioctl_submit_sync(struct xvp *xvp, struct xrp_ioctl_queue __user *p);

#endif