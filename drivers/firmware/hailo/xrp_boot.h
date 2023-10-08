// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_BOOT_H
#define XRP_BOOT_H

#include "xrp_common.h"

extern int firmware_command_timeout;

int xrp_boot(struct xvp *xvp);

int xrp_shutdown(struct xvp *xvp);

#endif