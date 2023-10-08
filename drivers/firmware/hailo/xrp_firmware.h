// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * xrp_firmware: firmware manipulation for the XRP
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_FIRMWARE_H
#define XRP_FIRMWARE_H

#include "xrp_common.h"

#include "linux/types.h"

int xrp_init_firmware(struct xvp *xvp);

int xrp_load_firmware(struct xvp *xvp);

void xrp_release_firmware(struct xvp *xvp);

#endif
