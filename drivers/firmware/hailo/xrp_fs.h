// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef XRP_FS_H
#define XRP_FS_H

#include <linux/fs.h>

bool xrp_is_known_file(struct file *filp);

void xrp_add_known_file(struct file *filp);

void xrp_remove_known_file(struct file *filp);

#endif