/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Core pinctrl driver for Hailo pinctrl logic
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 */

#ifndef _PINCTRL_HAILO15_H
#define _PINCTRL_HAILO15_H

#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include "../core.h"
#include "../pinctrl-utils.h"

#define H15_PINMUX_PIN_COUNT (32)
#define H15_GENERAL_PIN_COUNT (44)
#define H15_PINCTRL_PIN_COUNT (H15_PINMUX_PIN_COUNT + H15_GENERAL_PIN_COUNT)

/* In the chip, the first H15_PINMUX_OUTPUT_ONLY_PINS_AMOUNT pins can't set as input because collision issues.
   On early boot stage, the state of these lines is initial_debug_bus_out. */
#define H15_PINMUX_OUTPUT_ONLY_PINS_AMOUNT (6)

#define GPIO_PADS_CONFIG__GPIO_PE (0x0)
#define GPIO_PADS_CONFIG__GPIO_PS (0x4)
#define GPIO_PADS_CONFIG__GPIO_IO_INPUT_EN_CTRL_BYPASS_EN (0x2c)
#define GPIO_PADS_CONFIG__GPIO_IO_OUTPUT_EN_N_CTRL_BYPASS_EN (0x34)

#define GENERAL_PADS_CONFIG__DS_SHIFT (11)
#define GENERAL_PADS_CONFIG__DS_MASK (0xF << GENERAL_PADS_CONFIG__DS_SHIFT)
#define GENERAL_PADS_CONFIG__DS_MODIFY(src, dst)                               \
	(dst = ((dst & ~GENERAL_PADS_CONFIG__DS_MASK) |                        \
		(src) << GENERAL_PADS_CONFIG__DS_SHIFT))
#define GENERAL_PADS_CONFIG__OFFSET (4)
#define GENERAL_PADS_CONFIG__PADS_PINMUX (0xb8)

#define H15_PIN_DIRECTION_IN 0x0
#define H15_PIN_DIRECTION_OUT 0x1
#define H15_PIN_DIRECTION_BIDIR 0x2

int hailo15_pinctrl_check_valid_direction(unsigned int offset, bool input);

/*
	Struct for a certain pin set.
*/
struct h15_pin_set_runtime_data {
	/*
	 * Bit mask of the possible modes for this set.
	 * This value is updated depending on the selected functions and their pin group and modes.
	*/
	uint32_t possible_modes;

	/*
	 * The one mode that currently selected from the possible modes.
	 * The first set bit in possible_modes.
	*/
	uint32_t current_mode;
};

struct h15_pin_data {
	/*
	 * The selected function for that pin.
	*/
	unsigned func_selector;
};

enum h15_pin_set_value {
	H15_PIN_SET_VALUE__1_0,
	H15_PIN_SET_VALUE__3_2,
	H15_PIN_SET_VALUE__5_4,
	H15_PIN_SET_VALUE__7_6,
	H15_PIN_SET_VALUE__15_8,
	H15_PIN_SET_VALUE__31_16,
	H15_PIN_SET_VALUE__COUNT
};

struct hailo15_pinctrl {
	/*
	 * Lock to protect register access.
	*/
	raw_spinlock_t register_lock;

	/*
	 * Lock to protect set_mux access.
	*/
	raw_spinlock_t set_mux_lock;

	/*
	 * pointer to pinctrl_dev.
	*/
	struct pinctrl_dev *pctl;

	/*
	 * Pointer to device.
	*/
	struct device *dev;

	/*
	 * Register base address of general_pads_config.
	*/
	void __iomem *general_pads_config_base;

	/*
	 * Register base address of gpio_pads_config.
	*/
	void __iomem *gpio_pads_config_base;

	/*
	 * Pointer to array of groups.
	*/
	const struct h15_pin_group *groups;

	/*
	 * Total number of groups.
	*/
	unsigned num_groups;

	/*
	 * Pointer to array of functions
	*/
	const struct h15_pin_function *functions;

	/*
	 * Total number of functions
	*/
	unsigned num_functions;

	/*
	 * Array of structs that contains for every pin set the possible modes and the selected mode.
	*/
	struct h15_pin_set_runtime_data
		pin_set_runtime_data[H15_PIN_SET_VALUE__COUNT];

	/*
	 * Array of structs that contains for every pin his description.
	*/
	struct pinctrl_pin_desc pins[H15_PINCTRL_PIN_COUNT];

	/*
	 * i2c client of the EVB CPLD
	*/
	struct i2c_client *pinctrl_cpld_i2c_client;

	/*
	 * The value of the board config that need to be set to the CPLD.
	*/
	volatile uint8_t board_config;

	struct pinctrl_desc pctl_desc;
};

struct hailo15_pinctrl_data {
	const struct pinctrl_desc *pctl_desc;
	int (*init)(struct hailo15_pinctrl *pinctrl, struct device_node *node);
};

struct h15_pin_set_modes {
	/*
	 * Pin set index.
	*/
	enum h15_pin_set_value set;

	/*
	 * Bit mask of pinmux mode for the pin set index.
	*/
	uint32_t modes;
};

#define H15_PIN_SET_MODES(set_value, modes_value)                              \
	{                                                                      \
		.set = set_value, .modes = modes_value,                        \
	}

#define __H15_PIN_DIRECTION(pin_direction) H15_PIN_DIRECTION_##pin_direction

#define __H15_PIN_DIRECTION_1(a, ...) __H15_PIN_DIRECTION(a)
#define __H15_PIN_DIRECTION_2(a, ...)                                          \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_1(__VA_ARGS__)
#define __H15_PIN_DIRECTION_3(a, ...)                                          \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_2(__VA_ARGS__)
#define __H15_PIN_DIRECTION_4(a, ...)                                          \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_3(__VA_ARGS__)
#define __H15_PIN_DIRECTION_5(a, ...)                                          \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_4(__VA_ARGS__)
#define __H15_PIN_DIRECTION_6(a, ...)                                          \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_5(__VA_ARGS__)
#define __H15_PIN_DIRECTION_7(a, ...)                                          \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_6(__VA_ARGS__)
#define __H15_PIN_DIRECTION_8(a, ...)                                          \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_7(__VA_ARGS__)
#define __H15_PIN_DIRECTION_9(a, ...)                                          \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_8(__VA_ARGS__)
#define __H15_PIN_DIRECTION_10(a, ...)                                         \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_9(__VA_ARGS__)
#define __H15_PIN_DIRECTION_11(a, ...)                                         \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_10(__VA_ARGS__)
#define __H15_PIN_DIRECTION_12(a, ...)                                         \
	__H15_PIN_DIRECTION(a), __H15_PIN_DIRECTION_11(__VA_ARGS__)
#define H15_PIN_DIRECTIONS(...)                                                \
	{                                                                      \
		CONCATENATE(__H15_PIN_DIRECTION_, COUNT_ARGS(__VA_ARGS__))     \
		(__VA_ARGS__)                                                  \
	}

struct h15_pin_group {
	/*
	 * Array of pins used by this group.
	*/
	const unsigned *pins;

	/*
	 * Array of pins used by this group.
	*/
	const unsigned *pin_directions;

	/*
	 * Total number of pins used by this group.
	*/
	unsigned num_pins;

	/*
	 * Name of the group.
	*/
	const char *name;

	/*
	 * For the pins in the group, store the pin set indexes and for every pin set index the pinmux mode value.
	*/
	const struct h15_pin_set_modes *set_modes;

	/*
	 * Total number of set_modes.
	*/
	unsigned num_set_modes;
};

#define H15_PIN_GROUP(group_name)                                              \
	{                                                                      \
		.name = __stringify(group_name) "_grp",                        \
		.pins = group_name##_pins,                                     \
		.pin_directions = group_name##_pin_directions,                 \
		.num_pins = ARRAY_SIZE(group_name##_pins),                     \
		.set_modes = group_name##_set_modes,                           \
		.num_set_modes = ARRAY_SIZE(group_name##_set_modes),           \
	}

struct h15_pin_function {
	/*
	 * Name of the function.
	*/
	const char *name;

	/*
	 * Array of groups that can be supported by this function
	*/
	const char *const *groups;

	/*
	 * Total number of groups that can be supported by this function.
	*/
	unsigned num_groups;
};

#define H15_PIN_FUNCTION(func)                                                 \
	{                                                                      \
		.name = #func, .groups = func##_grps,                          \
		.num_groups = ARRAY_SIZE(func##_grps),                         \
	}

struct h15_pin_set {
	/*
	 * The number of possible modes for that pin set
	*/
	uint8_t modes_count;

	/*
	 * Name of the pin set.
	*/
	const char *name;
};

#define H15_PIN_SET(set_name, modes_count_value)                               \
	[set_name] = { .name = __stringify(set_name),                          \
		       .modes_count = modes_count_value }

#endif /* _PINCTRL_HAILO15_H */