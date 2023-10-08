/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * Driver for the Hailo15 CPLD-pinctrl device.
 * The CPLD is only applicable for HAILO15-evb.
 *
 */

#ifndef _PINCTRL_HAILO15_CPLD_H
#define _PINCTRL_HAILO15_CPLD_H

#include "pinctrl-hailo15.h"
#include <linux/i2c.h>

#define H15_PINCTRL_CPLD_I2C_CLIENT_ADDRESS (0x11)
#define H15_CPLD__VERSION (0x0)
#define H15_CPLD__GPIO07_06_DIRECTION (0x5)
#define H15_CPLD__GPIO15_08_DIRECTION (0x6)
#define H15_CPLD__GPIO23_16_DIRECTION (0x7)
#define H15_CPLD__GPIO31_24_DIRECTION (0x8)
#define H15_CPLD__BOARD_CONFIG (0x9)
#define H15_CPLD__BOOTSTRAP_STATUS (0xA)
#define H15_CPLD__POWER_STATUS_0 (0xE)
#define H15_CPLD__RESET_STATUS_0 (0x11)
#define H15_CPLD__RESET_STATUS_1 (0x1B)
#define H15_CPLD__SDIO_ROUTE_STATUS (0x19)

#define H15_CPLD__MINOR_VERSION_BIT_OFFSET (0)
#define H15_CPLD__MINOR_VERSION_MASK (0x1F)
#define H15_CPLD__BOARD_VERSION_BIT_OFFSET (5)
#define H15_CPLD__BOARD_VERSION_MASK (7)

#define H15_CPLD__IO_SELECT_BIT_OFFSET (3)
#define H15_CPLD__IO_SELECT_BIT_MASK (3)
#define H15_CPLD__POWER_INSER_BIT_OFFSET (5)
#define H15_CPLD__POWER_INSER_BIT_MASK (1)

#define H15_CPLD__POWER_ENABLE_BIT_OFFSET (0)
#define H15_CPLD__POWER_ENABLE_MASK (1)
#define H15_CPLD__POWER_GOOD_BIT_OFFSET (1)
#define H15_CPLD__POWER_GOOD_MASK (1)

#define H15_CPLD__RESET_STATUS_MASK (1)
#define H15_CPLD__RESET_STATUS_0_H15_BIT_OFFSET (0)
#define H15_CPLD__RESET_STATUS_0_FTDI_BIT_OFFSET (1)
#define H15_CPLD__RESET_STATUS_0_CAM0_BIT_OFFSET (2)
#define H15_CPLD__RESET_STATUS_0_CAM1_BIT_OFFSET (3)
#define H15_CPLD__RESET_STATUS_0_DSI_TO_HDMI_BIT_OFFSET (4)
#define H15_CPLD__RESET_STATUS_0_ETH_PHY_BIT_OFFSET (5)
#define H15_CPLD__RESET_STATUS_0_USB_HUB_BIT_OFFSET (6)
#define H15_CPLD__RESET_STATUS_0_AUDIO_CODEC_BIT_OFFSET (7)
#define H15_CPLD__RESET_STATUS_1_FLASH_BIT_OFFSET (0)
#define H15_CPLD__RESET_STATUS_1_EMMC_BIT_OFFSET (1)

#define H15_CPLD__SDIO_ROUTE_MASK (3)
#define H15_CPLD__SDIO_ROUTE_0_BIT_OFFSET (0)
#define H15_CPLD__SDIO_ROUTE_1_BIT_OFFSET (2)

int hailo15_cpld_init(struct hailo15_pinctrl *pinctrl,
		      struct device_node *node);

int hailo15_cpld_gpio_set_direction(struct pinctrl_dev *pctrl_dev,
				    struct pinctrl_gpio_range *range,
				    unsigned int offset, bool input);
#endif /* _PINCTRL_HAILO15_CPLD_H */
