/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Core pinctrl driver for Hailo pinctrl logic
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 */

#include "pinctrl-hailo15.h"
#include "pinctrl-hailo15-descriptions.h"
#include "pinctrl-hailo15-cpld.h"
#include <linux/of_platform.h>
#include <linux/pinctrl/pinconf-generic.h>

#include <linux/soc/hailo/scmi_hailo_protocol.h>

#define H15__SCU_BOOT_BIT_MASK (3)

static const char *hailo15_boot_options[] = { "Flash", "UART", "PCIe",
						   "N/A" };

static const unsigned char drive_strength_lookup[16] = {
	0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
	0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf,
};

static const struct scmi_hailo_ops *hailo_protocol_ops;

enum h15_pin_set_value hailo15_get_pin_set(unsigned int pin_number)
{
	enum h15_pin_set_value set;
	if (pin_number <= 1) {
		set = H15_PIN_SET_VALUE__1_0;
	} else if (pin_number <= 3) {
		set = H15_PIN_SET_VALUE__3_2;
	} else if (pin_number <= 5) {
		set = H15_PIN_SET_VALUE__5_4;
	} else if (pin_number <= 7) {
		set = H15_PIN_SET_VALUE__7_6;
	} else if (pin_number <= 15) {
		set = H15_PIN_SET_VALUE__15_8;
	} else if (pin_number <= 31) {
		set = H15_PIN_SET_VALUE__31_16;
	} else {
		return -EIO;
	}
	return set;
}

static int hailo15_get_groups_count(struct pinctrl_dev *pctrl_dev)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	return pinctrl->num_groups;
}

static const char *hailo15_get_group_name(struct pinctrl_dev *pctrl_dev,
					  unsigned selector)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	return pinctrl->groups[selector].name;
}

static int hailo15_get_group_pins(struct pinctrl_dev *pctrl_dev,
				  unsigned selector, const unsigned **pins,
				  unsigned *num_pins)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);

	*pins = pinctrl->groups[selector].pins;
	*num_pins = pinctrl->groups[selector].num_pins;
	return 0;
}

static void hailo15_pin_dbg_show(struct pinctrl_dev *pctrl_dev,
				 struct seq_file *s, unsigned offset)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	if (offset < H15_PINMUX_PIN_COUNT) {
		enum h15_pin_set_value set = hailo15_get_pin_set(offset);
		struct h15_pin_set_runtime_data *pin_set_runtime_data =
			&pinctrl->pin_set_runtime_data[set];
		struct h15_pin_data *pin_data = pinctrl->pins[offset].drv_data;

		seq_printf(
			s,
			"%s, pin set number: %d, possible set modes (bit mask): 0x%x, current set mode: %d",
			dev_name(pctrl_dev->dev), (int)set,
			pin_set_runtime_data->possible_modes,
			pin_set_runtime_data->current_mode);

		if (pin_data->func_selector !=
		    H15_PINMUX_INVALID_FUNCTION_SELECTOR) {
			seq_printf(s, ", function name: %s",
				   pctrl_dev->desc->pmxops->get_function_name(
					   pctrl_dev, pin_data->func_selector));
		}
	} else {
		seq_printf(s, "%s, is non-muxable pin",
			   dev_name(pctrl_dev->dev));
	}
}

static const struct pinctrl_ops hailo15_pctrl_ops = {
	.get_groups_count = hailo15_get_groups_count,
	.get_group_name = hailo15_get_group_name,
	.get_group_pins = hailo15_get_group_pins,
	.pin_dbg_show = hailo15_pin_dbg_show,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinctrl_utils_free_map,
};

static int hailo15_get_functions_count(struct pinctrl_dev *pctrl_dev)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	return pinctrl->num_functions;
}

static const char *hailo15_get_function_name(struct pinctrl_dev *pctrl_dev,
					     unsigned selector)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	return pinctrl->functions[selector].name;
}

static int hailo15_get_function_groups(struct pinctrl_dev *pctrl_dev,
				       unsigned selector,
				       const char *const **groups,
				       unsigned *const num_groups)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);

	*groups = pinctrl->functions[selector].groups;
	*num_groups = pinctrl->functions[selector].num_groups;
	return 0;
}

/*
 * In the SCU-FW in boot time, for all the pins gpio_io_output_en_n_ctrl_bypass_en set to 1,
 * And for all the pins (but the shutdown gpio pin) gpio_io_input_en_ctrl_bypass_en set to 1.
 * Now, when some pin was requested by a specific driver, the pinctrl driver need to allow the
 * relevant driver who needs to "control" this pin to be able to do so.
 * To enable some pin the pinctrl driver need to turn off the bypass option.
*/
static void hailo15_enable_pin(struct hailo15_pinctrl *pinctrl,
			       unsigned int pin_number)
{
	unsigned long flags;
	uint32_t data_reg;

	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	data_reg = readl(pinctrl->gpio_pads_config_base +
			 GPIO_PADS_CONFIG__GPIO_IO_OUTPUT_EN_N_CTRL_BYPASS_EN);
	data_reg &= (~(1 << pin_number));
	writel(data_reg,
	       (pinctrl->gpio_pads_config_base +
		GPIO_PADS_CONFIG__GPIO_IO_OUTPUT_EN_N_CTRL_BYPASS_EN));

	data_reg = readl(pinctrl->gpio_pads_config_base +
			 GPIO_PADS_CONFIG__GPIO_IO_INPUT_EN_CTRL_BYPASS_EN);
	data_reg &= (~(1 << pin_number));
	writel(data_reg, (pinctrl->gpio_pads_config_base +
			  GPIO_PADS_CONFIG__GPIO_IO_INPUT_EN_CTRL_BYPASS_EN));

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);
}

static int hailo15_set_pads_pinmux_mode(struct hailo15_pinctrl *pinctrl,
					enum h15_pin_set_value set_number,
					uint32_t pads_pinmux_value)
{
	unsigned long flags;
	uint32_t data_reg;
	uint32_t data_reg_mask;

	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	data_reg = readl(pinctrl->general_pads_config_base +
			 GENERAL_PADS_CONFIG__PADS_PINMUX);
	/*
	* Turn off the bits of the relevant pins set in data_reg.
	*/
	data_reg_mask = pads_pinmux_mode_reg_mask_value[set_number];
	data_reg_mask =
		(~(data_reg_mask << pads_pinmux_mode_shift_value[set_number]));
	data_reg = data_reg & data_reg_mask;

	/*
	* Set data_reg in the bits of the relevant pins set to the selected mode.
	*/
	data_reg = data_reg | (pads_pinmux_value
			       << (pads_pinmux_mode_shift_value[set_number]));
	writel(data_reg, (pinctrl->general_pads_config_base +
			  GENERAL_PADS_CONFIG__PADS_PINMUX));

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	pr_debug("data_reg: %d\n", data_reg);
	return 0;
}

static int hailo15_general_pad_set_strength(struct pinctrl_dev *pctldev,
					    unsigned general_pad_index,
					    unsigned strength_value)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	unsigned long flags;
	uint32_t data_reg;
	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	data_reg = readl(pinctrl->general_pads_config_base +
			 general_pad_index * GENERAL_PADS_CONFIG__OFFSET);

	/*  NOTICE!!!
	    DS bits are written in reverse, meaning that the LSB is the fourth bit, and the MSB is the first bit.
	    it's important to acknowledge that if wanting to change the DS, having to high value might burn the board.
	*/

	GENERAL_PADS_CONFIG__DS_MODIFY(drive_strength_lookup[strength_value],
				       data_reg);

	writel(data_reg,
	       pinctrl->general_pads_config_base +
		       (general_pad_index * GENERAL_PADS_CONFIG__OFFSET));

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	pr_debug(
		"general_pad_index:%u, set drive strength:%d mA, data_reg %d\n",
		general_pad_index, strength_value, data_reg);
	return 0;
}

static int hailo15_pin_set_strength(struct pinctrl_dev *pctldev, unsigned pin,
				    unsigned strength_value)
{
	if (strength_value > 16) {
		pr_err("Error: make sure drive strength is supported");
		return -ENOTSUPP;
	}

	if (pin < H15_PINMUX_PIN_COUNT) {
		pr_err("Error: drive strength for pinmux pins is currently not supported");
		return -ENOTSUPP;
	} else {
		return hailo15_general_pad_set_strength(
			pctldev, pin - H15_PINMUX_PIN_COUNT, strength_value);
	}
}

static int hailo15_gpio_pin_set_pull(struct pinctrl_dev *pctldev,
				     unsigned gpio_pad_index,
				     unsigned long config)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	unsigned long flags;
	uint32_t data_reg;
	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	data_reg = readl(pinctrl->gpio_pads_config_base +
			 GPIO_PADS_CONFIG__GPIO_PE);

	data_reg |= (1 << gpio_pad_index);
	writel(data_reg,
	       (pinctrl->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PE));

	data_reg = readl(pinctrl->gpio_pads_config_base +
			 GPIO_PADS_CONFIG__GPIO_PS);

	data_reg &= (~(1 << gpio_pad_index));

	if (config == PIN_CONFIG_BIAS_PULL_UP) {
		data_reg |= (1 << gpio_pad_index);
	}

	writel(data_reg,
	       (pinctrl->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PS));

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	pr_debug("gpio_pad_index:%u, %s, data_reg %d\n", gpio_pad_index,
		 config == PIN_CONFIG_BIAS_PULL_UP ? "PULL_UP" : "PULL_DOWN",
		 data_reg);
	return 0;
}

static int hailo15_pin_set_pull(struct pinctrl_dev *pctldev, unsigned pin,
				unsigned long config)
{
	if (pin < H15_PINMUX_PIN_COUNT) {
		return hailo15_gpio_pin_set_pull(pctldev, pin, config);
	} else {
		pr_err("Error: pull for un-muxable pins is currently not supported");
		return -ENOTSUPP;
	}
}

static int hailo15_pin_config_set(struct pinctrl_dev *pctldev, unsigned pin,
				  unsigned long *configs, unsigned num_configs)
{
	int i, ret = 0;
	enum pin_config_param param;
	enum pin_config_param argument;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		argument = pinconf_to_config_argument(configs[i]);
		switch (param) {
		case PIN_CONFIG_DRIVE_STRENGTH:
			ret = hailo15_pin_set_strength(pctldev, pin, argument);
			if (ret) {
				return ret;
			}
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
		case PIN_CONFIG_BIAS_PULL_DOWN:
			ret = hailo15_pin_set_pull(pctldev, pin, param);
			if (ret) {
				return ret;
			}
			break;
		default:
			pr_err("Error: unsupported operation for pin config");
			return -ENOTSUPP;
		}
	}
	return 0;
}

static const struct pinconf_ops hailo15_pinconf_ops = {
	.is_generic = true,
	.pin_config_set = hailo15_pin_config_set,
};

static int hailo15_set_mux(struct pinctrl_dev *pctrl_dev, unsigned func_select,
			   unsigned grp_select)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	const struct h15_pin_function *func = &pinctrl->functions[func_select];
	const struct h15_pin_group *grp = &pinctrl->groups[grp_select];
	uint32_t new_mode = 0;
	unsigned long flags;
	int i, err;
	unsigned pin_offset, pin_direction;

	raw_spin_lock_irqsave(&pinctrl->set_mux_lock, flags);

	/*
	* For every pin set in the pins group, find the permitted pinmux mode.
	*/
	for (i = 0; i < grp->num_set_modes; i++) {
		struct h15_pin_set_runtime_data *pin_set_runtime_data =
			&pinctrl->pin_set_runtime_data[grp->set_modes[i].set];

		/*
		* For every pin set, check if there is at least one mode that also exists in the possible_modes
		* (that established so far) and in the permitted modes in the new group.
		*/
		if ((pin_set_runtime_data->possible_modes &
		     grp->set_modes[i].modes) == 0) {
			dev_err(pinctrl->dev,
				"Error for (function name: %s, group name: %s) there is no correct mode that settles with the others selected function so far\n",
				func->name, grp->name);
			raw_spin_unlock_irqrestore(&pinctrl->set_mux_lock,
						   flags);
			return -EIO;
		}
	}

	/*
	 * Since the driver "decodes" what is the correct mode for each pin group
	 * by the functions requested so far, it can happen that it chooses a mode
	 * that is correct for the functions selected so far, but it is not the "correct" mode for this board.
	 * This temporary mode can set wrong functionality to some pins.
	 * The "correct" mode will be selected when all the functions defined in the device tree will be processed.
	 * So the driver start all 32 gpio pads in disable state to prevent set wrong functionality to some pins,
	 * and the driver enable only the pins that is requested by a specific driver in hailo15_set_mux function.
	*/
	for (i = 0; i < grp->num_pins; i++) {
		hailo15_enable_pin(pinctrl, grp->pins[i]);
	}

	for (i = 0; i < grp->num_set_modes; i++) {
		struct h15_pin_set_runtime_data *pin_set_runtime_data =
			&pinctrl->pin_set_runtime_data[grp->set_modes[i].set];

		pin_set_runtime_data->possible_modes &= grp->set_modes[i].modes;

		/*
		* Find the first set bit in pin_set_runtime_data->possible_modes to determine the selected mode.
		*/
		new_mode = __builtin_ctz(pin_set_runtime_data->possible_modes);

		/*
		* Check if the driver need to update the pinmux mode for that set.
		*/
		if (new_mode != pin_set_runtime_data->current_mode) {
			pin_set_runtime_data->current_mode = new_mode;
			hailo15_set_pads_pinmux_mode(
				pinctrl, grp->set_modes[i].set,
				pin_set_runtime_data->current_mode);
		}
	}
	raw_spin_unlock_irqrestore(&pinctrl->set_mux_lock, flags);

	for (i = 0; i < grp->num_pins; i++) {
		pin_offset = grp->pins[i];
		pin_direction = grp->pin_directions[i];

		err = pctrl_dev->desc->pmxops->gpio_set_direction(
			pctrl_dev, NULL, pin_offset,
			(H15_PIN_DIRECTION_IN == pin_direction));
		if (err < 0) {
			return err;
		}

		((struct h15_pin_data *)(pinctrl->pins[pin_offset].drv_data))
			->func_selector = func_select;
	}

	return 0;
}

static int hailo15_gpio_request_enable(struct pinctrl_dev *pctrl_dev,
				       struct pinctrl_gpio_range *range,
				       unsigned pin)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);

	if (pin >= H15_PINMUX_PIN_COUNT) {
		dev_err(pinctrl->dev,
			"The pin number %d there was no matching gpio group\n",
			pin);
		return -EIO;
	}
	/*
	 * The group selector for every GPIO pin is the offset pin.
	*/
	return pctrl_dev->desc->pmxops->set_mux(
		pctrl_dev, H15_PINMUX_GPIO_FUNCTION_SELECTOR, pin);
}

static void hailo15_gpio_disable_free(struct pinctrl_dev *pctrl_dev,
				      struct pinctrl_gpio_range *range,
				      unsigned pin)
{
	/*
	* TODO: https://hailotech.atlassian.net/browse/MSW-2477
	*/
	dev_dbg(pctrl_dev->dev, "hailo15_gpio_disable_free pin=%u", pin);
}

int hailo15_pinctrl_check_valid_direction(unsigned int offset, bool input)
{
	if (input && (offset < H15_PINMUX_OUTPUT_ONLY_PINS_AMOUNT)) {
		pr_err("The first %d pins can't set as input because of collision issues.\n",
		       H15_PINMUX_OUTPUT_ONLY_PINS_AMOUNT);
		return -EIO;
	}
	return 0;
}

static int hailo15_gpio_set_direction(struct pinctrl_dev *pctrl_dev,
				      struct pinctrl_gpio_range *range,
				      unsigned int offset, bool input)
{
	return hailo15_pinctrl_check_valid_direction(offset, input);
}

static const struct pinmux_ops hailo15_pinmux_ops = {
	.get_functions_count = hailo15_get_functions_count,
	.get_function_name = hailo15_get_function_name,
	.get_function_groups = hailo15_get_function_groups,
	.set_mux = hailo15_set_mux,
	.gpio_request_enable = hailo15_gpio_request_enable,
	.gpio_disable_free = hailo15_gpio_disable_free,
	.gpio_set_direction = hailo15_gpio_set_direction,
	.strict = true,
};

static const struct pinmux_ops hailo15_cpld_pinmux_ops = {
	.get_functions_count = hailo15_get_functions_count,
	.get_function_name = hailo15_get_function_name,
	.get_function_groups = hailo15_get_function_groups,
	.set_mux = hailo15_set_mux,
	.gpio_request_enable = hailo15_gpio_request_enable,
	.gpio_disable_free = hailo15_gpio_disable_free,
	.gpio_set_direction = hailo15_cpld_gpio_set_direction,
	.strict = true,
};

static const struct pinctrl_desc hailo15_pinctrl_desc = {
	.pctlops = &hailo15_pctrl_ops,
	.pmxops = &hailo15_pinmux_ops,
	.confops = &hailo15_pinconf_ops,
	.name = "pinctrl-hailo15",
};

static const struct pinctrl_desc hailo15_cpld_pinctrl_desc = {
	.pctlops = &hailo15_pctrl_ops,
	.pmxops = &hailo15_cpld_pinmux_ops,
	.confops = &hailo15_pinconf_ops,
	.name = "pinctrl-cpld-hailo15",
};

static const struct hailo15_pinctrl_data hailo15_pinctrl_data = {
	.pctl_desc = &hailo15_pinctrl_desc,
};

static const struct hailo15_pinctrl_data hailo15_cpld_pinctrl_data = {
	.pctl_desc = &hailo15_cpld_pinctrl_desc,
	.init = hailo15_cpld_init,
};

static const struct of_device_id hailo15_pinctrl_of_match[] = {
	{ .compatible = "hailo15,pinctrl", .data = &hailo15_pinctrl_data },
	{
		.compatible = "hailo15-cpld,pinctrl",
		.data = &hailo15_cpld_pinctrl_data,
	},
	{},
};

static int hailo15_pinctrl_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	const struct hailo15_pinctrl_data *data;
	struct hailo15_pinctrl *pinctrl;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct device_node *node = dev->of_node;
	struct h15_pin_data *pin_data;
	unsigned int i;
	int ret;
	unsigned num_pins;
	unsigned num_functions;
	uint8_t boot_source;

	num_pins = ARRAY_SIZE(hailo15_pins);
	num_functions = ARRAY_SIZE(h15_pin_functions);

	pinctrl = devm_kzalloc(dev, sizeof(*pinctrl), GFP_KERNEL);
	if (!pinctrl)
		return -ENOMEM;

	pinctrl->dev = dev;

	hailo_protocol_ops = scmi_hailo_get_ops();
	if (hailo_protocol_ops == NULL) {
		return -EPROBE_DEFER;
	}
	if (IS_ERR(hailo_protocol_ops)) {
		/* Hailo SCMI ops unreachable, try setting CONFIG_HAILO_SCMI_PROTOCOL=y */
		dev_err(pinctrl->dev, "hailo scmi ops unreachable\n");
		return PTR_ERR(hailo_protocol_ops);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "general_pads_config_base");
	if (!res) {
		dev_err(dev, "Error getting resource\n");
		return -ENODEV;
	}
	pinctrl->general_pads_config_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pinctrl->general_pads_config_base)) {
		return PTR_ERR(pinctrl->general_pads_config_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "gpio_pads_config_base");
	if (!res) {
		dev_err(dev, "Error getting resource\n");
		return -ENODEV;
	}
	pinctrl->gpio_pads_config_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pinctrl->gpio_pads_config_base)) {
		return PTR_ERR(pinctrl->gpio_pads_config_base);
	}

	id = of_match_node(hailo15_pinctrl_of_match, node);
	if (!id)
		return -ENODEV;

	data = id->data;
	if (!data || !data->pctl_desc)
		return -EINVAL;

	pinctrl->pctl_desc = *data->pctl_desc;

	if (data->init) {
		ret = data->init(pinctrl, node);
		if (ret != 0) {
			return ret;
		}
	}

	hailo_protocol_ops->get_scu_boot_source(&boot_source);
	pr_info("SCU booted from:                %s",
		hailo15_boot_options[boot_source & H15__SCU_BOOT_BIT_MASK]);

	platform_set_drvdata(pdev, pinctrl);

	for (i = 0; i < H15_PIN_SET_VALUE__COUNT; i++) {
		/*
		* Create a contiguous bitmask starting at bit position h15_pin_sets[i].modes_count and ending at position 0.
		*/
		pinctrl->pin_set_runtime_data[i].possible_modes =
			GENMASK(h15_pin_sets[i].modes_count, 0);
		pinctrl->pin_set_runtime_data[i].current_mode = 0;
	}

	pin_data = devm_kcalloc(dev, num_pins, sizeof(struct h15_pin_data),
				GFP_KERNEL);

	for (i = 0; i < num_pins; i++) {
		pinctrl->pins[i].number = hailo15_pins[i].number;
		pinctrl->pins[i].name = hailo15_pins[i].name;
		pin_data[i].func_selector =
			H15_PINMUX_INVALID_FUNCTION_SELECTOR;
		pinctrl->pins[i].drv_data = &(pin_data[i]);
	}

	raw_spin_lock_init(&pinctrl->register_lock);

	pinctrl->groups = h15_pin_groups;
	pinctrl->num_groups = ARRAY_SIZE(h15_pin_groups);
	pinctrl->functions = h15_pin_functions;
	pinctrl->num_functions = num_functions;
	pinctrl->pctl_desc.pins = pinctrl->pins;
	pinctrl->pctl_desc.npins = num_pins;

	ret = devm_pinctrl_register_and_init(dev, &pinctrl->pctl_desc, pinctrl,
					     &pinctrl->pctl);

	if (ret) {
		dev_err(dev, "hailo15 pin controller registration failed\n");
		return ret;
	}

	ret = pinctrl_enable(pinctrl->pctl);
	if (ret) {
		dev_err(dev, "hailo15 pin controller failed to start\n");
		return ret;
	}
	dev_info(dev, "%s registered\n", pinctrl->pctl_desc.name);
	return 0;
}

static struct platform_driver hailo15_pinctrl_driver = {
	.probe	= hailo15_pinctrl_probe,
	.driver = {
		.name = "hailo15-pinctrl",
		.of_match_table = hailo15_pinctrl_of_match,
		.suppress_bind_attrs = true,
	},
};

static int __init hailo15_pinctrl_init(void)
{
	return platform_driver_register(&hailo15_pinctrl_driver);
}
arch_initcall(hailo15_pinctrl_init);
