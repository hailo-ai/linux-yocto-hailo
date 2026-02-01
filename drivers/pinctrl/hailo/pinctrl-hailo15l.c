
#include "pinctrl-hailo15l.h"
#include "pinctrl-hailo15l-descriptions.h"
#include "../core.h"
#include "../pinctrl-utils.h"

#include <linux/errno.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/printk.h>
#include <linux/gpio/driver.h>

#define GENERAL_PADS_CONFIG__PADS_PINMUX_BASE (0xA4)
#define GPIO_PADS_CONFIG__DS__SIZE (0x4)
#define GPIO_PADS_CONFIG__PADS_GPIO_DS_0 (0xC)
#define H15L_MODE_INACTIVE (0b1111)

/* Pctrl ops */

static int hailo15l_get_groups_count(struct pinctrl_dev *pctrl_dev)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);

	return pinctrl->num_groups;
}

static const char *hailo15l_get_group_name(struct pinctrl_dev *pctrl_dev,
					  unsigned selector)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);

	return pinctrl->groups[selector].name;
}

static int hailo15l_get_group_pins(struct pinctrl_dev *pctrl_dev,
				   unsigned selector,
				   const unsigned **pins,
				   unsigned *num_pins)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);

	*pins = &pinctrl->groups[selector].pin;
	*num_pins = 1;

	return 0;
}


static void hailo15l_pin_dbg_show(struct pinctrl_dev *pctrl_dev,
				  struct seq_file *s, unsigned offset)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	struct h15l_pin_data *pin_data;

	if (offset >= pinctrl->pctl_desc.npins) {
		seq_printf(s, "Invalid pin number %d\n", offset);
		return;
	}

	pin_data = pinctrl->pins[offset].drv_data;

	if (pin_data->is_muxable) {
		seq_printf(
			s,
			"%s",
			dev_name(pctrl_dev->dev));

		if (pin_data->is_occupied) {
			seq_printf(s, ", function name: %s",
				   pctrl_dev->desc->pmxops->get_function_name(
					   pctrl_dev, pin_data->func_selector));
		}
	} else {
		seq_printf(s, "%s, is non-muxable pin",
			   dev_name(pctrl_dev->dev));
	}
}

/* Pinmux ops */

static int hailo15l_gpio_request_enable(struct pinctrl_dev *pctrl_dev,
					struct pinctrl_gpio_range *range,
					unsigned pin)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	int ret, i;
	char const *grp;
	char const *const *groups;
	unsigned num_groups;
	unsigned func_select;
	unsigned grp_select;
	unsigned num_pins = 0;
	const unsigned *pins = NULL;

	/*
		func_select for gpio's is the the number of the gpio (gpio-func for gpio 0 is gpio-func-0),
		so the driver find the number of the desired gpio.
		pin - is the pin number at the chip (at pinctrl numbering)
		range->pin_base - is the base pin number of the GPIO range (the pin number of the first gpio at the current gpio-range)
		range->id - is the start offset in the current gpio_chip number space (the first gpio number at the current gpio-range)
		range->gc->offset - is the gpio-offset for the gpio bus that the current gpio is contained on

		(pin - range->pin_base) give us the index of the gpio at the current gpio-range
		((pin - range->pin_base) + range->id) give us the index of the gpio at the current gpio-bus
		(((pin - range->pin_base) + range->id) + range->gc->offset) give us the index of the gpio at the chip,
			taking into account gpio-bus-offst of gpio-bus
	*/
	func_select = pin - range->pin_base + range->id + range->gc->offset;
	ret = pctrl_dev->desc->pmxops->get_function_groups(
		pctrl_dev, func_select, &groups, &num_groups);
	if (ret < 0) {
		dev_err(pinctrl->dev, "can't query groups for function %u\n",
			func_select);
		return ret;
	}
	if (!num_groups) {
		dev_err(pinctrl->dev,
			"function %u can't be selected on any group\n",
			func_select);
		return -EINVAL;
	}

	for (i = 0; i < num_groups; i++) {
		grp = groups[i];
		grp_select = pinctrl_get_group_selector(pctrl_dev, grp);
		if (grp_select < 0) {
			dev_err(pinctrl->dev, "invalid group %s in map table\n", grp);
			return ret;
		}
		pctrl_dev->desc->pctlops->get_group_pins(pctrl_dev, grp_select,
							&pins, &num_pins);
		if (pins[0] == pin) {
			break;
		}
	}

	return pctrl_dev->desc->pmxops->set_mux(pctrl_dev, func_select, grp_select);
}

static void hailo15l_gpio_disable_free(struct pinctrl_dev *pctrl_dev,
				      struct pinctrl_gpio_range *range,
				      unsigned pin)
{
	/*
	* TODO: https://hailotech.atlassian.net/browse/MSW-2477
	*/
	dev_dbg(pctrl_dev->dev, "hailo15l_gpio_disable_free pin=%u", pin);
}

static int hailo15l_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);

	return pinctrl->num_functions;
}

static const char *hailo15l_get_function_name(struct pinctrl_dev *pctldev,
					unsigned selector)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);

	return pinctrl->functions[selector].name;

}

static int hailo15l_get_function_groups(struct pinctrl_dev *pctldev,
					 unsigned selector,
					 const char * const **groups,
					 unsigned *num_groups)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);

	*groups = pinctrl->functions[selector].groups;
	*num_groups = pinctrl->functions[selector].num_groups;

	return 0;
}


static void hailo15l_set_pads_pinmux_mode(struct hailo15l_pinctrl *pinctrl,
					  unsigned int mux_index,
					  uint32_t mode)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	writel(mode, (pinctrl->general_pads_config_base +
			  GENERAL_PADS_CONFIG__PADS_PINMUX_BASE +
			  mux_index * sizeof(uint32_t)));

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	dev_dbg(pinctrl->dev, "Setting mode 0x%x for mux index %d\n", mode, mux_index);
}

static uint32_t hailo15l_get_pads_pinmux_mode(struct hailo15l_pinctrl *pinctrl,
					      unsigned int pin)
{
	uint32_t value;
	unsigned long flags;

	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	value = readl(pinctrl->general_pads_config_base +
			GENERAL_PADS_CONFIG__PADS_PINMUX_BASE +
			pin * sizeof(uint32_t));

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	return value;
}

static void hailo15l_disable_all_pads(struct hailo15l_pinctrl *pinctrl)
{
	int i;

	for (i = 0; i < H15L_PINMUX_PIN_COUNT; i++) {
		struct h15l_pin_data *pin_data = pinctrl->pins[i].drv_data;
		if (H15L_IS_SDIO_PIN(i) || !pin_data->is_muxable) {
			continue;
		}

		hailo15l_set_pads_pinmux_mode(pinctrl, i, H15L_MODE_INACTIVE);
	}
}

static int hailo15l_set_mux(struct pinctrl_dev *pctrl_dev,
			    unsigned func_select,
			    unsigned grp_select)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	const struct h15l_pin_group *grp = &pinctrl->groups[grp_select];
	unsigned int pin = grp->pin;
	struct h15l_pin_data *pin_data = pinctrl->pins[pin].drv_data;
	uint32_t new_mode = grp->mode;

	unsigned long flags;
	unsigned int i;

	if (pin_data->is_occupied && pin_data->func_selector == func_select) {
		dev_dbg(pinctrl->dev,
			"Group %s is already set to function %s\n",
			grp->name,
			pctrl_dev->desc->pmxops->get_function_name(
				pctrl_dev, func_select));
		return 0;
	}

	if (H15L_IS_SDIO_PIN(pin)) {
		dev_err(pinctrl->dev,
			"Error applying group %s - SDIO pins are not "
			"currently supported\n",
			grp->name);
		return -ENOTSUPP;
	}

	if (!pin_data->is_muxable) {
		dev_dbg(pinctrl->dev,
			"group %s - pin is not muxable, skipping\n",
			grp->name);
		return 0;
	}

	raw_spin_lock_irqsave(&pinctrl->set_mux_lock, flags);

	/* Check if pin is free */
	if (pin_data->is_occupied) {
		dev_err(pinctrl->dev,
			"Error applying group %s - pin is already occupied\n",
			grp->name);
		raw_spin_unlock_irqrestore(&pinctrl->set_mux_lock, flags);
		return -EBUSY;
	}

	/* Check if function is free */
	for (i = 0; i < pinctrl->pctl_desc.npins; i++) {
		struct h15l_pin_data *current_data = pinctrl->pins[i].drv_data;
		if (func_select == current_data->func_selector) {
			dev_err(pinctrl->dev,
				"Error applying group %s - function is "
				"already occupied\n",
				grp->name);
			raw_spin_unlock_irqrestore(
				&pinctrl->set_mux_lock, flags);
			return -EBUSY;
		}
	}

	hailo15l_set_pads_pinmux_mode(pinctrl, pin_data->mux_index, new_mode);
	pin_data->func_selector = func_select;
	pin_data->is_occupied = true;
	raw_spin_unlock_irqrestore(&pinctrl->set_mux_lock, flags);

	return 0;

}

/* Pinconf ops */

static int hailo15l_gpio_get_strength(struct pinctrl_dev *pctldev,
				      unsigned gpio_pad_index)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);

	uint32_t values[GPIO_PADS_CONFIG__DS__SIZE] = {0, };
	unsigned int strength_value = 0;

	unsigned long flags;
	int i;

	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	/* Read values */
	for (i = 0; i < GPIO_PADS_CONFIG__DS__SIZE; i++) {
		values[i] = readl(pinctrl->gpio_pads_config_base 
				  + GPIO_PADS_CONFIG__PADS_GPIO_DS_0
				  + i * sizeof(uint32_t));
	}

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	/* Get drive-strength value */
	for (i = 0; i < GPIO_PADS_CONFIG__DS__SIZE; i++) {
		strength_value |= (((values[i] >> gpio_pad_index) & 1) << i);
	}

	return strength_value;
}

static int hailo15l_gpio_set_strength(struct pinctrl_dev *pctldev,
				      unsigned pin,
				      unsigned strength_value)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);

	uint32_t values[GPIO_PADS_CONFIG__DS__SIZE] = {0, };

	unsigned long flags;
	int i;

	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	/* Read values */
	for (i = 0; i < GPIO_PADS_CONFIG__DS__SIZE; i++) {
		values[i] = readl(pinctrl->gpio_pads_config_base 
				  + GPIO_PADS_CONFIG__PADS_GPIO_DS_0
				  + i * sizeof(uint32_t));
	}

	/* Set drive-strength value */
	for (i = 0; i < GPIO_PADS_CONFIG__DS__SIZE; i++) {
		values[i] = (values[i] & (~(1 << pin)))
			    | (((strength_value >> i) & 1) << pin);
	}

	/* Store values */
	for (i = 0; i < GPIO_PADS_CONFIG__DS__SIZE; i++) {
		writel(values[i], 
		       pinctrl->gpio_pads_config_base 
			 + GPIO_PADS_CONFIG__PADS_GPIO_DS_0
			 + i * sizeof(uint32_t));
	}

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	return 0;
}

static int hailo15l_pad_get_strength(struct pinctrl_dev *pctldev,
				     uint32_t *pad)
{
	/* Drive-strength reverse lookup is the same as forward lookup */
	return PADS_CONFIG__DS__GET(readl(pad));
}

static int hailo15l_pad_set_strength(struct pinctrl_dev *pctldev,
				     uint32_t *pad,
				     unsigned strength_value)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	uint32_t value;
	unsigned long flags;

	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	value = readl(pad);
	PADS_CONFIG__DS__MODIFY(value, strength_value);
	writel(value, pad);

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	return 0;
}

static int hailo15l_pin_get_strength(struct pinctrl_dev *pctldev, unsigned pin)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	struct h15l_pin_data *pin_data = pinctrl->pins[pin].drv_data;
	uint32_t *pad;

	switch (pin_data->pad_type) {
		case H15L_GENERAL_PAD:
			pad = pinctrl->general_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_get_strength(pctldev, pad);
		case H15L_SLOW_PAD:
			pad = pinctrl->slow_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_get_strength(pctldev, pad);
		case H15L_GPIO_PAD:
			return hailo15l_gpio_get_strength(pctldev, pin_data->pad_index);
		default:
			break;
	}

	pr_err("Error: pin does not support drive strength");
	return -ENOTSUPP;
}

static int hailo15l_pin_set_strength(struct pinctrl_dev *pctldev, unsigned pin,
				    unsigned strength_value)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	struct h15l_pin_data *pin_data = pinctrl->pins[pin].drv_data;
	uint32_t *pad;

	if (strength_value > 16) {
		pr_err("Error: make sure drive strength is supported");
		return -ENOTSUPP;
	}

	switch (pin_data->pad_type) {
		case H15L_GENERAL_PAD:
			pad = pinctrl->general_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_set_strength(pctldev, pad, strength_value);
		case H15L_SLOW_PAD:
			pad = pinctrl->slow_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_set_strength(pctldev, pad, strength_value);
		case H15L_GPIO_PAD:
			return hailo15l_gpio_set_strength(pctldev, pin_data->pad_index, strength_value);
		default:
			break;
	}

	pr_err("Error: pin does not support drive strength");
	return -ENOTSUPP;
}

static int hailo15l_pad_get_pull_selector(struct pinctrl_dev *pctldev, uint32_t *pad)
{
	return PADS_CONFIG__PS__GET(readl(pad)) ? PIN_CONFIG_BIAS_PULL_UP : PIN_CONFIG_BIAS_PULL_DOWN;
}

static int hailo15l_pad_set_pull(struct pinctrl_dev *pctldev,
				 uint32_t *pad,
				 enum pin_config_param pull)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	uint32_t data_reg;
	uint32_t selector_value = (pull == PIN_CONFIG_BIAS_PULL_UP) ? 0x1 : 0x0;
	unsigned long flags;

	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);
	data_reg = readl(pad);

	/* Pull selector */
	PADS_CONFIG__PS__MODIFY(data_reg, selector_value);
	/* Pull enabled */
	PADS_CONFIG__PE__SET(data_reg);

	writel(data_reg, pad);
	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	return 0;
}

static enum pin_config_param hailo15l_gpio_get_pull_selector(
	struct pinctrl_dev *pctldev, unsigned gpio_pad_index)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	uint32_t data_reg;

	data_reg = readl(pinctrl->gpio_pads_config_base +
			 GPIO_PADS_CONFIG__GPIO_PS);

	return (data_reg & (1 << gpio_pad_index)) ? 
		PIN_CONFIG_BIAS_PULL_UP : PIN_CONFIG_BIAS_PULL_DOWN;
}

static int hailo15l_gpio_set_pull(struct pinctrl_dev *pctldev,
				  unsigned gpio_pad_index,
				  enum pin_config_param pull)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	unsigned long flags;
	uint32_t data_reg;
	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	data_reg = readl(pinctrl->gpio_pads_config_base +
			 GPIO_PADS_CONFIG__GPIO_PS);

	data_reg &= (~(1 << gpio_pad_index));

	if (pull == PIN_CONFIG_BIAS_PULL_UP) {
		data_reg |= (1 << gpio_pad_index);
	}

	writel(data_reg,
	       (pinctrl->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PS));


	data_reg = readl(pinctrl->gpio_pads_config_base +
			 GPIO_PADS_CONFIG__GPIO_PE);

	data_reg |= (1 << gpio_pad_index);
	writel(data_reg,
	       (pinctrl->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PE));

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	pr_debug("gpio_pad_index:%u, %s, data_reg %d\n", gpio_pad_index,
		 pull == PIN_CONFIG_BIAS_PULL_UP ? "PULL_UP" : "PULL_DOWN",
		 data_reg);
	return 0;
}

static int hailo15l_pin_get_pull_selector(struct pinctrl_dev *pctldev, unsigned pin)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	struct h15l_pin_data *pin_data = pinctrl->pins[pin].drv_data;
	uint32_t *pad;

	switch (pin_data->pad_type) {
		case H15L_GENERAL_PAD:
			pad = pinctrl->general_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_get_pull_selector(pctldev, pad);
		case H15L_SLOW_PAD:
			pad = pinctrl->slow_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_get_pull_selector(pctldev, pad);
		case H15L_GPIO_PAD:
			return hailo15l_gpio_get_pull_selector(pctldev, pin_data->pad_index);
		default:
			break;
	}

	pr_err("Error: pin does not support pull");
	return -ENOTSUPP;
}

static int hailo15l_pin_set_pull(struct pinctrl_dev *pctldev, unsigned pin,
				 enum pin_config_param pull)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	struct h15l_pin_data *pin_data = pinctrl->pins[pin].drv_data;
	uint32_t *pad;

	switch (pin_data->pad_type) {
		case H15L_GENERAL_PAD:
			pad = pinctrl->general_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_set_pull(pctldev, pad, pull);
		case H15L_SLOW_PAD:
			pad = pinctrl->slow_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_set_pull(pctldev, pad, pull);
		case H15L_GPIO_PAD:
			return hailo15l_gpio_set_pull(pctldev, pin_data->pad_index, pull);
		default:
			break;
	}

	pr_err("Error: pin does not support drive strength");
	return -ENOTSUPP;
}

static int hailo15l_pad_get_pull_enabled(struct pinctrl_dev *pctldev, uint32_t *pad)
{
	return PADS_CONFIG__PE__GET(readl(pad));
}

static int hailo15l_pad_bias_disable(struct pinctrl_dev *pctldev, uint32_t *pad)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	uint32_t data_reg;
	unsigned long flags;

	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);
	data_reg = readl(pad);

	/* Pull enabled */
	PADS_CONFIG__PE__CLR(data_reg);

	writel(data_reg, pad);
	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	return 0;
}

static int hailo15l_gpio_get_pull_enabled(struct pinctrl_dev *pctldev, unsigned gpio_pad_index)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);

	uint32_t data_reg = readl(pinctrl->gpio_pads_config_base +
				  GPIO_PADS_CONFIG__GPIO_PE);

	return (data_reg & (1 << gpio_pad_index));
}

static int hailo15l_gpio_bias_disable(struct pinctrl_dev *pctldev, unsigned gpio_pad_index)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	unsigned long flags;
	uint32_t data_reg;
	raw_spin_lock_irqsave(&pinctrl->register_lock, flags);

	data_reg = readl(pinctrl->gpio_pads_config_base +
			 GPIO_PADS_CONFIG__GPIO_PE);

	data_reg &= (~(1 << gpio_pad_index));

	writel(data_reg,
	       (pinctrl->gpio_pads_config_base + GPIO_PADS_CONFIG__GPIO_PE));

	raw_spin_unlock_irqrestore(&pinctrl->register_lock, flags);

	pr_debug("gpio_pad_index:%u, %s, data_reg %d\n", gpio_pad_index,
		 "PULL_DISABLED",
		 data_reg);
	return 0;
}

static int hailo15l_pin_get_pull_enabled(struct pinctrl_dev *pctldev, unsigned pin)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	struct h15l_pin_data *pin_data = pinctrl->pins[pin].drv_data;
	uint32_t *pad;

	switch (pin_data->pad_type) {
		case H15L_GENERAL_PAD:
			pad = pinctrl->general_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_get_pull_enabled(pctldev, pad);
		case H15L_SLOW_PAD:
			pad = pinctrl->slow_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_get_pull_enabled(pctldev, pad);
		case H15L_GPIO_PAD:
			return hailo15l_gpio_get_pull_enabled(pctldev, pin_data->pad_index);
		default:
			break;
	}

	pr_err("Error: pin does not support pull enable");
	return -ENOTSUPP;
}

static int hailo15l_pin_bias_disable(struct pinctrl_dev *pctldev, unsigned pin)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	struct h15l_pin_data *pin_data = pinctrl->pins[pin].drv_data;
	uint32_t *pad;

	switch (pin_data->pad_type) {
		case H15L_GENERAL_PAD:
			pad = pinctrl->general_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_bias_disable(pctldev, pad);
		case H15L_SLOW_PAD:
			pad = pinctrl->slow_pads_config_base + pin_data->pad_index * sizeof(uint32_t);
			return hailo15l_pad_bias_disable(pctldev, pad);
		case H15L_GPIO_PAD:
			return hailo15l_gpio_bias_disable(pctldev, pin_data->pad_index);
		default:
			break;
	}

	pr_err("Error: pin does not support bias disable");
	return -ENOTSUPP;
}

static int hailo15l_pin_config_set(struct pinctrl_dev *pctldev, unsigned pin,
				  unsigned long *configs, unsigned num_configs)
{
	int i, ret = 0;
	enum pin_config_param param;
	enum pin_config_param argument;

	if (H15L_IS_SDIO_PIN(pin)) {
		pr_err("Error: setting an SDIO pin is not currently supported\n");
		return -ENOTSUPP;
	}

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		argument = pinconf_to_config_argument(configs[i]);
		switch (param) {
		case PIN_CONFIG_DRIVE_STRENGTH:
			ret = hailo15l_pin_set_strength(pctldev, pin, argument);
			if (ret) {
				return ret;
			}
			break;
		case PIN_CONFIG_BIAS_PULL_UP:
		case PIN_CONFIG_BIAS_PULL_DOWN:
			ret = hailo15l_pin_set_pull(pctldev, pin, param);
			if (ret) {
				return ret;
			}
			break;
		case PIN_CONFIG_BIAS_DISABLE:
			ret = hailo15l_pin_bias_disable(pctldev, pin);
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

static int hailo15l_pin_config_group_set(struct pinctrl_dev *pctldev,
					 unsigned selector,
					 unsigned long *configs,
					 unsigned num_configs)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	const struct h15l_pin_group *grp;
	unsigned int pin;

	if (selector >= pinctrl->num_groups) {
		pr_err("Error: invalid group selector %d\n", selector);
		return -EINVAL;
	}

	grp = &pinctrl->groups[selector];
	pin = grp->pin;
	
	return hailo15l_pin_config_set(pctldev, pin, configs, num_configs);
}

static void hailo15l_pin_config_dbg_show(struct pinctrl_dev *pctldev,
					 struct seq_file *s,
					 unsigned offset)
{
	struct hailo15l_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctldev);
	struct h15l_pin_data *pin_data;

	if (offset >= pinctrl->pctl_desc.npins) {
		seq_printf(s, "Invalid pin number %d\n", offset);
		return;
	}

	pin_data = pinctrl->pins[offset].drv_data;

	if (pin_data->pad_type == H15L_NO_PAD) {
		/* No data */
		seq_printf(s, "-");
		return;
	}

	if (!hailo15l_pin_get_pull_enabled(pctldev, offset)) {
		seq_printf(s, "pull-disabled ");
	} else if (hailo15l_pin_get_pull_selector(pctldev, offset) == PIN_CONFIG_BIAS_PULL_UP) {
		seq_printf(s, "pull-up ");
	} else {
		seq_printf(s, "pull-down ");
	
	}

	seq_printf(s, "drive-strength %d", hailo15l_pin_get_strength(pctldev, offset));
}

static const struct pinctrl_ops hailo15l_pctrl_ops = {
	.get_groups_count = hailo15l_get_groups_count,
	.get_group_name = hailo15l_get_group_name,
	.get_group_pins = hailo15l_get_group_pins,
	.pin_dbg_show = hailo15l_pin_dbg_show,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_group,
	.dt_free_map = pinctrl_utils_free_map,
};

static const struct pinmux_ops hailo15l_pinmux_ops = {
	.get_functions_count = hailo15l_get_functions_count,
	.get_function_name = hailo15l_get_function_name,
	.get_function_groups = hailo15l_get_function_groups,
	.set_mux = hailo15l_set_mux,
	.gpio_request_enable = hailo15l_gpio_request_enable,
	.gpio_disable_free = hailo15l_gpio_disable_free,
	.strict = true,
};

static const struct pinconf_ops hailo15l_pinconf_ops = {
	.is_generic = true,
	.pin_config_group_set = hailo15l_pin_config_group_set,
	.pin_config_dbg_show = hailo15l_pin_config_dbg_show,
};

static const struct pinctrl_desc hailo15l_pinctrl_desc = {
	.pctlops = &hailo15l_pctrl_ops,
	.pmxops = &hailo15l_pinmux_ops,
	.confops = &hailo15l_pinconf_ops,
	.name = "pinctrl-hailo15l",
};

static const struct of_device_id hailo15l_pinctrl_of_match[] = {
	{ .compatible = "hailo15l,pinctrl", .data = &hailo15l_pinctrl_desc },
	{},
};

/*
 * Initialize pins with restrictions from U-boot.
 */
static void hailo15l_initialize_current_state(struct device *dev, struct hailo15l_pinctrl *pinctrl) {
	unsigned int num_groups = pinctrl->num_groups;
	unsigned int num_functions = pinctrl->num_functions;
	unsigned int num_active_groups = 0;

	uint32_t current_modes[ARRAY_SIZE(hailo15l_pins)] = {0, };
	const struct h15l_pin_group *active_groups[ARRAY_SIZE(hailo15l_pins)] = {NULL, };

	struct h15l_pin_data *pin_data;
	int i;

	for (i = 0; i < ARRAY_SIZE(current_modes); i++) {
		pin_data = (struct h15l_pin_data *)pinctrl->pins[i].drv_data;
		current_modes[i] = hailo15l_get_pads_pinmux_mode(pinctrl, pin_data->mux_index);
	}

	/*
	 * Detect active groups.
	 */
	for (i = 0; i < num_groups; ++i) {
		const struct h15l_pin_group *group = &h15l_pin_groups[i];

		/* Check if the modes match */
		if (current_modes[group->pin] != group->mode) {
			pr_debug("Group %s is not active because of modes mismatch\n", group->name);
			continue;
		}

		/* Group is active */

		active_groups[num_active_groups++] = group;
		pr_debug("Group %s is active\n", group->name);
	}

	/* Update functions */
	for (i = 0; i < num_functions; ++i) {
		const struct h15l_pin_function *function = &h15l_pin_functions[i];
		int j;

		/* Check if any group is active */
		for (j = 0; j < function->num_groups; ++j) {
			const char *const group_name = function->groups[j];
			const struct h15l_pin_group *group = NULL;
			int k;

			for (k = 0; k < num_active_groups; ++k) {
				if (strcmp(active_groups[k]->name, group_name) == 0) {
					group = active_groups[k];
					break;
				}
			}
			if (!group) {
				continue;
			}

			/* Function is active for this group */
			pr_debug("Pin %s is active for function %s of group %s\n",
			         pinctrl->pins[group->pin].name, function->name, group_name);
			pin_data[group->pin].func_selector = i;
			pin_data[group->pin].is_occupied = false;

			/* No more than one group can be active for a given function */
			break;
		}
	}
}

static int hailo15l_pinctrl_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;
	const struct hailo15l_pinctrl_data *data;
	struct hailo15l_pinctrl *pinctrl;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int ret;

	pinctrl = devm_kzalloc(dev, sizeof(*pinctrl), GFP_KERNEL);
	if (!pinctrl)
		return -ENOMEM;

	pinctrl->dev = dev;

	pinctrl->general_pads_config_base =
		devm_platform_ioremap_resource_byname(
			pdev, "general_pads_config_base");
	if (IS_ERR(pinctrl->general_pads_config_base)) {
		return PTR_ERR(pinctrl->general_pads_config_base);
	}

	pinctrl->gpio_pads_config_base =
		devm_platform_ioremap_resource_byname(
			pdev, "gpio_pads_config_base");
	if (IS_ERR(pinctrl->gpio_pads_config_base)) {
		return PTR_ERR(pinctrl->gpio_pads_config_base);
	}

	pinctrl->slow_pads_config_base =
		devm_platform_ioremap_resource_byname(
			pdev, "slow_pads_config_base");
	if (IS_ERR(pinctrl->gpio_pads_config_base)) {
		return PTR_ERR(pinctrl->gpio_pads_config_base);
	}

	id = of_match_node(hailo15l_pinctrl_of_match, node);
	if (!id)
		return -ENODEV;

	data = id->data;
	if (!data)
		return -EINVAL;

	memcpy(&pinctrl->pctl_desc, data, sizeof(pinctrl->pctl_desc));

	pinctrl->pins = hailo15l_pins;
	pinctrl->groups = h15l_pin_groups;
	pinctrl->num_groups = ARRAY_SIZE(h15l_pin_groups);
	pinctrl->functions = h15l_pin_functions;
	pinctrl->num_functions = ARRAY_SIZE(h15l_pin_functions);
	pinctrl->pctl_desc.pins = pinctrl->pins;
	pinctrl->pctl_desc.npins = ARRAY_SIZE(hailo15l_pins);

	/* TODO: call hailo15l_initialize_current_state and delete
		 hailo15l_disable_all_pads when U-boot pinmux is
		 implemented. */
	(void)hailo15l_initialize_current_state;
	hailo15l_disable_all_pads(pinctrl);

	platform_set_drvdata(pdev, pinctrl);

	raw_spin_lock_init(&pinctrl->register_lock);
	raw_spin_lock_init(&pinctrl->set_mux_lock);

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

static struct platform_driver hailo15l_pinctrl_driver = {
	.probe	= hailo15l_pinctrl_probe,
	.driver = {
		.name = "hailo15l-pinctrl",
		.of_match_table = hailo15l_pinctrl_of_match,
		.suppress_bind_attrs = true,
	},
};

static int __init hailo15l_pinctrl_init(void)
{
	return platform_driver_register(&hailo15l_pinctrl_driver);
}
arch_initcall(hailo15l_pinctrl_init);

