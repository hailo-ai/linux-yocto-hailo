#ifndef _PINCTRL_HAILO12_L_H
#define _PINCTRL_HAILO12_L_H

#include <linux/io.h>
#include <linux/pinctrl/pinctrl.h>

/* Drive strength */
#define PADS_CONFIG__DS__GET(_reg_offset) (((_reg_offset)&0x00007800L) >> 11)

#define PADS_CONFIG__DS__MODIFY(_reg_offset, _value)                           \
	(_reg_offset) = (((_reg_offset) & ~0x00007800L) |                      \
			 (((uint32_t)(_value) << 11) & 0x00007800L))

/* Pull selector */
#define PADS_CONFIG__PS__GET(reg_offset) (((reg_offset)&0x00000200L) >> 9)

#define PADS_CONFIG__PS__MODIFY(reg_offset, value)                             \
	(reg_offset) = (((reg_offset) & ~0x00000200L) |                        \
			(((uint32_t)(value) << 9) & 0x00000200L))

/* Pull enabled */
#define PADS_CONFIG__PE__GET(reg_offset) (((reg_offset)&0x00000100L) >> 8)

#define PADS_CONFIG__PE__SET(reg_offset) (reg_offset) |= (0x00000100L)

#define PADS_CONFIG__PE__CLR(reg_offset) (reg_offset) &= (~0x00000100L)

#define GPIO_PADS_CONFIG__GPIO_PE (0x0)
#define GPIO_PADS_CONFIG__GPIO_PS (0x4)

enum hailo12l_pad {
	H12L_GENERAL_PAD,
	H12L_SLOW_PAD,
	H12L_GPIO_PAD,
	H12L_NO_PAD,
	H12L_AON_PAD,
	H12L_AON_GPIO_PAD,
};

struct hailo12l_pinctrl {
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
	 * Register base address of slow_pads_config.
	*/
	void __iomem *slow_pads_config_base;

	/*
	 * Register base address of aon_pads_config.
	*/
	void __iomem *aon_pads_config_base;

	/*
	 * Register base address of aon_config. (for aon pinmux registers  )
	*/
	void __iomem *aon_pinmux_config_base;

	/*
	 * Pointer to array of groups.
	*/
	const struct h12l_pin_group *groups;

	/*
	 * Total number of groups.
	*/
	unsigned num_groups;

	/*
	 * Pointer to array of functions
	*/
	const struct h12l_pin_function *functions;

	/*
	 * Total number of functions
	*/
	unsigned num_functions;

	/*
	 * Array of structs that contains for every pin his description.
	*/
	struct pinctrl_pin_desc const *pins;

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

struct h12l_pin_group {
	/*
	 * Pin used in this group.
	*/
	const unsigned pin;

	/*
	 * Name of the group.
	*/
	const char *name;

	/*
	 * Mode used in this group.
	*/
	unsigned mode;
};

struct h12l_pin_data {
	/* The index of the mux pad in general_pads_config. */
	unsigned int mux_index;

	/*
	 * The type of drive-strength/bias pad.
	*/
	enum hailo12l_pad pad_type;

	/*
	 * The index of the drive-strength/bias pad in the pad array.
	*/
	unsigned int pad_index;

	/*
	 * The selected function for that pin.
	*/
	unsigned func_selector;

	/*
	 * Flag to indicate if the pin is muxable or not.
	*/
	bool is_muxable;

	/*
	 * Flag to indicate if the pin is occupied or not.
	*/
	bool is_occupied;
};

struct h12l_pin_function {
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

#endif /* _PINCTRL_HAILO12_L_H */
