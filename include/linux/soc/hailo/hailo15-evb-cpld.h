/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2026 Hailo Technologies Ltd. All rights reserved.
 *
 * CPLD driver API for Hailo15 EVB boards.
 */

#ifndef _HAILO15_EVB_CPLD_H
#define _HAILO15_EVB_CPLD_H

/* Opaque type — full definition is private to hailo15-evb-cpld.c */
struct hailo15_evb_cpld;

int hailo15_evb_cpld_set_gpio_direction(struct hailo15_evb_cpld *cpld,
				    	unsigned int offset, bool input);

#endif /* _HAILO15_EVB_CPLD_H */
