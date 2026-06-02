/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2026 Hailo Technologies Ltd. All rights reserved.
 *
 * Driver for the ARM CoreLink CMSDK GPIO
 *
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>

#define CMSDK_GPIO_MAX_NGPIO (16)
typedef struct GPIO_BLOCK_regs_s {
	volatile uint32_t DATA;
	volatile uint32_t DATAOUT;
	volatile uint32_t reserved_0[2];
	volatile uint32_t OUTENSET;
	volatile uint32_t OUTENCLR;
	volatile uint32_t ALTFUNCSET;
	volatile uint32_t ALTFUNCCLR;
	volatile uint32_t INTENSET;
	volatile uint32_t INTENCLR;
	volatile uint32_t INTTYPESET;
	volatile uint32_t INTTYPECLR;
	volatile uint32_t INTPOLSET;
	volatile uint32_t INTPOLCLR;
	union {
		volatile uint32_t INTSTATUS;
		volatile uint32_t INTCLEAR;
	};
	volatile uint32_t reserved_1[241];
	volatile uint32_t MASKLOWBYTE;
	volatile uint32_t
		reserved_2[255];
	volatile uint32_t MASKHIGHBYTE;
	volatile uint32_t
		reserved_3[499];
	volatile uint32_t PID4;
	volatile uint32_t PID5;
	volatile uint32_t PID6;
	volatile uint32_t PID7;
	volatile uint32_t PID0;
	volatile uint32_t PID1;
	volatile uint32_t PID2;
	volatile uint32_t PID3;
	volatile uint32_t CID0;
	volatile uint32_t CID1;
	volatile uint32_t CID2;
	volatile uint32_t CID3;
} GPIO_BLOCK_regs_s;

typedef struct GPIO_MANAGER_CONFIG_regs_s  {
	volatile uint32_t gpio_int_mask;
	volatile uint32_t gpio_int_status;
	volatile uint32_t gpio_int_w1c;
} GPIO_MANAGER_CONFIG_t;

struct cmsdk_gpio {
	raw_spinlock_t lock;
	GPIO_BLOCK_regs_s __iomem *base;

	/* Our GPIO instances share the same config registers.
	   Please access these registers with caution. */
	GPIO_MANAGER_CONFIG_t __iomem *config;

	struct gpio_chip gc;
	struct irq_chip irq_chip;

	struct irq_domain *irq_domain;
};

static void cmsdk_irq_ack(struct irq_data *data) {
}

static void cmsdk_irq_mask(struct irq_data *data) {
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(data);
	struct cmsdk_gpio *gpio = gc->private;
	volatile void __iomem *INTENCLR = &gpio->base->INTENCLR;
	int pin = data->hwirq;
	uint32_t mask = BIT(pin);

	writel(readl(INTENCLR) | mask, INTENCLR);
}

static void cmsdk_irq_unmask(struct irq_data *data) {
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(data);
	struct cmsdk_gpio *gpio = gc->private;
	volatile void __iomem *INTENSET = &gpio->base->INTENSET;
	int pin = data->hwirq;
	uint32_t mask = BIT(pin);

	writel(readl(INTENSET) | mask, INTENSET);
}

static inline void cmsdk_reverse_polarity(struct cmsdk_gpio *cmsdk_gpio, int pin) {
	uint32_t mask = BIT(pin);
	uint32_t polarity = readl(&cmsdk_gpio->base->INTPOLSET);

	if (mask & polarity)
		writew(mask, &cmsdk_gpio->base->INTPOLCLR);
	else
		writew(mask, &cmsdk_gpio->base->INTPOLSET);
}

static int cmsdk_irq_set_type(struct irq_data *data, unsigned int type) {
	struct irq_chip_generic *icg = irq_data_get_irq_chip_data(data);
	struct cmsdk_gpio *gpio = icg->private;
	int pin = irqd_to_hwirq(data);
	uint32_t mask = BIT(pin);

	type &= IRQ_TYPE_SENSE_MASK;

	if (type != IRQ_TYPE_EDGE_RISING
	    && type != IRQ_TYPE_EDGE_FALLING
	    && type != IRQ_TYPE_LEVEL_HIGH
	    && type != IRQ_TYPE_LEVEL_LOW
	    && type != IRQ_TYPE_EDGE_BOTH)
		return -EINVAL;

	/* Set INTTYPE */
	if (type == IRQ_TYPE_EDGE_RISING || type == IRQ_TYPE_EDGE_FALLING || type == IRQ_TYPE_EDGE_BOTH)
		writew(mask, &gpio->base->INTTYPESET);
	else
		writew(mask, &gpio->base->INTTYPECLR);

	/* Set INTPOL */
	if (type == IRQ_TYPE_EDGE_BOTH)
		cmsdk_reverse_polarity(gpio, pin);
	else if (type == IRQ_TYPE_LEVEL_HIGH || type == IRQ_TYPE_EDGE_RISING)
		writew(mask, &gpio->base->INTPOLSET);
	else
		writew(mask, &gpio->base->INTPOLCLR);

	return 0;
}

static int cmsdk_gpio_get_value(struct gpio_chip *gc, unsigned int offset)
{
	struct cmsdk_gpio *cmsdk_gpio = gpiochip_get_data(gc);

	return !!(readl(&cmsdk_gpio->base->DATA) & BIT(offset));
}

static void cmsdk_gpio_set_value_inner(struct cmsdk_gpio *cmsdk_gpio,
				       unsigned int offset, int value)
{
	unsigned long flags;
	unsigned int data_reg;

	raw_spin_lock_irqsave(&cmsdk_gpio->lock, flags);
	data_reg = readl(&cmsdk_gpio->base->DATAOUT);
	if (value)
		data_reg |= BIT(offset);
	else
		data_reg &= ~BIT(offset);
	writel(data_reg, &cmsdk_gpio->base->DATAOUT);
	raw_spin_unlock_irqrestore(&cmsdk_gpio->lock, flags);
}

static void cmsdk_gpio_set_value(struct gpio_chip *gc, unsigned int offset,
				 int value)
{
	struct cmsdk_gpio *cmsdk_gpio = gpiochip_get_data(gc);
	cmsdk_gpio_set_value_inner(cmsdk_gpio, offset, value);
}

static int cmsdk_gpio_get_direction(struct gpio_chip *gc, unsigned int offset)
{
	struct cmsdk_gpio *cmsdk_gpio = gpiochip_get_data(gc);

	/* By the Programmers model for OUTENCLR:
        Read back 0 - Indicate the signal direction as input.
        Read back 1 - Indicate the signal direction as output. */
	if (readl(&cmsdk_gpio->base->OUTENCLR) & BIT(offset))
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}

static int cmsdk_gpio_set_direction_input(struct cmsdk_gpio *cmsdk_gpio,
					  unsigned int offset)
{
	unsigned long flags;
	unsigned int gpio_ddr;

	raw_spin_lock_irqsave(&cmsdk_gpio->lock, flags);
	/* Set pin as input, assumes software controlled IP.
       By the Programmers model for OUTENCLR:
        Write 1 - Clear the output enable bit.
        Write 0 - No effect.
        Only asserted bit is affected with BIT macro, while the others remain unchanged. */
	gpio_ddr = BIT(offset);
	writel(gpio_ddr, &cmsdk_gpio->base->OUTENCLR);
	raw_spin_unlock_irqrestore(&cmsdk_gpio->lock, flags);
	return 0;
}

static int cmsdk_gpio_set_direction_output(struct cmsdk_gpio *cmsdk_gpio,
					   unsigned int offset)
{
	unsigned long flags;
	unsigned int gpio_ddr;

	raw_spin_lock_irqsave(&cmsdk_gpio->lock, flags);
	/* Set pin as output, assumes software controlled IP.
       By the Programmers model for OUTENSET:
        Write 1 - Set the output enable bit.
        Write 0 - No effect.
        Only asserted bit is affected with BIT macro, while the others remain unchanged. */
	gpio_ddr = BIT(offset);
	writel(gpio_ddr, &cmsdk_gpio->base->OUTENSET);
	raw_spin_unlock_irqrestore(&cmsdk_gpio->lock, flags);
	return 0;
}

/*
Setting a GPIO x to input:
1. Set required pull
2. Set in the GPIO IP register the direction to input
3. write to the pinctrl register direction [x] ← '0' (pinctrl_gpio_direction_input())
*/
static int cmsdk_gpio_direction_input(struct gpio_chip *gc, unsigned int offset)
{
	struct cmsdk_gpio *cmsdk_gpio = gpiochip_get_data(gc);
	int err;

	/* Sets the GPIO direction */
	cmsdk_gpio_set_direction_input(cmsdk_gpio, offset);

	err = pinctrl_gpio_direction_input(gc->base + offset);
	if (err < 0) {
		/*
		* Revert the stage: Set in the GPIO IP register the direction to input
		*/
		cmsdk_gpio_set_direction_output(cmsdk_gpio, offset);
		return err;
	}
	return 0;
}

/*
Setting a GPIO x to output:
1. write to the pinctrl register direction [x] ← '1' (pinctrl_gpio_direction_output)
2. Set the output level, ‘0’ or '1’
3. Set in the GPIO IP register the direction to output
*/
static int cmsdk_gpio_direction_output(struct gpio_chip *gc,
				       unsigned int offset, int value)
{
	struct cmsdk_gpio *cmsdk_gpio = gpiochip_get_data(gc);
	int err;

	err = pinctrl_gpio_direction_output(gc->base + offset);
	if (err < 0) {
		return err;
	}

	/* Sets the GPIO value */
	cmsdk_gpio_set_value_inner(cmsdk_gpio, offset, value);

	/* Sets the GPIO direction */
	cmsdk_gpio_set_direction_output(cmsdk_gpio, offset);
	return 0;
}

static int cmsdk_irq_request_resources(struct irq_data *data)
{
	struct irq_chip_generic *icg = irq_data_get_irq_chip_data(data);
	struct cmsdk_gpio *cmsdk_gpio = icg->private;
	int ret;

	ret = cmsdk_gpio->gc.direction_input(&cmsdk_gpio->gc, irqd_to_hwirq(data));
	if (ret) {
		dev_err(cmsdk_gpio->gc.parent,
			"failed to set pin %lu to input direction, ret = %d\n",
			irqd_to_hwirq(data), ret);
		return ret;
	}

	return gpiochip_reqres_irq(&cmsdk_gpio->gc, irqd_to_hwirq(data));
}

static void cmsdk_irq_release_resources(struct irq_data *data)
{
	struct irq_chip_generic *icg = irq_data_get_irq_chip_data(data);
	struct cmsdk_gpio *cmsdk_gpio = icg->private;

	gpiochip_relres_irq(&cmsdk_gpio->gc, irqd_to_hwirq(data));
}

static int cmsdk_gpio_to_irq(struct gpio_chip *gc, unsigned int pin)
{
	struct cmsdk_gpio *cmsdk_gpio = gpiochip_get_data(gc);
	return irq_create_mapping(cmsdk_gpio->irq_domain, pin);
}

static irqreturn_t cmsdk_irq_handler(int irq __maybe_unused, void *dev_id)
{
	struct cmsdk_gpio *gpio = dev_id;
	unsigned long status;
	int i;
	int ret;

	if (gpio == NULL)
		return IRQ_NONE;

	status = readw(&gpio->base->INTSTATUS);
	if (!status)
		return IRQ_NONE;

	writew(status, &gpio->base->INTCLEAR);
	writel(status << gpio->gc.offset, &gpio->config->gpio_int_w1c);

	for_each_set_bit(i, &status, gpio->gc.ngpio) {
		unsigned int mapped_irq;
		uint32_t type;

		mapped_irq = irq_find_mapping(gpio->irq_domain, i);
		type = irq_get_trigger_type(mapped_irq);

		if (type == IRQ_TYPE_EDGE_BOTH)
			cmsdk_reverse_polarity(gpio, i);

		ret = generic_handle_irq(mapped_irq);
	}

	return IRQ_HANDLED;
}

static void disable_gpio_irqs(struct cmsdk_gpio *cmsdk_gpio)
{
	writew(0xFFFF, &cmsdk_gpio->base->INTCLEAR);
	writel(0xFFFF << cmsdk_gpio->gc.offset, &cmsdk_gpio->config->gpio_int_w1c);
	writew(0xFFFF, &cmsdk_gpio->base->INTENCLR);
}

static void enable_gpio_irqs(struct cmsdk_gpio *cmsdk_gpio)
{
	writew(0xFFFF, &cmsdk_gpio->base->INTCLEAR);
	/* Enable interrupts of both GPIOs */
	writel(0xFFFFFFFF, &cmsdk_gpio->config->gpio_int_mask);
}

static int cmsdk_setup_irq(struct platform_device *pdev, struct cmsdk_gpio *cmsdk_gpio)
{
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;
	int irq, ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	cmsdk_gpio->irq_domain = irq_domain_add_linear(pdev->dev.of_node,
						       cmsdk_gpio->gc.ngpio,
						       &irq_generic_chip_ops,
						       &cmsdk_gpio->gc);
	if (!cmsdk_gpio->irq_domain) {
		dev_err(&pdev->dev, "Failed to create IRQ domain\n");
		return -ENOMEM;
	}

	ret = irq_alloc_domain_generic_chips(
		cmsdk_gpio->irq_domain,
		cmsdk_gpio->gc.ngpio, 1, dev_name(&pdev->dev),
		handle_level_irq, 0, 0, 0);
	if (ret) {
		dev_err(&pdev->dev, "Failed to allocate IRQ chips\n");
		irq_domain_remove(cmsdk_gpio->irq_domain);
		return ret;
	}

	gc = irq_get_domain_generic_chip(cmsdk_gpio->irq_domain, 0);
	gc->private = cmsdk_gpio;
	ct = &gc->chip_types[0];
	ct->chip.irq_ack = cmsdk_irq_ack;
	ct->chip.irq_mask = cmsdk_irq_mask;
	ct->chip.irq_unmask = cmsdk_irq_unmask;
	ct->chip.irq_set_type = cmsdk_irq_set_type;
	ct->chip.irq_request_resources = cmsdk_irq_request_resources;
	ct->chip.irq_release_resources = cmsdk_irq_release_resources;
	ct->chip.name = cmsdk_gpio->gc.label;

	/* Request shared IRQ (both GPIOs use the same IRQ) */
	ret = devm_request_irq(&pdev->dev, irq, cmsdk_irq_handler,
			       IRQF_SHARED, dev_name(&pdev->dev), cmsdk_gpio);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ\n");
		irq_domain_remove(cmsdk_gpio->irq_domain);
		return -ENOENT;
	}

	/* Enable interrupts */
	enable_gpio_irqs(cmsdk_gpio);

	dev_dbg(&pdev->dev, "CMSDK GPIO registered interrupts\n");

	return 0;
}

static int cmsdk_gpio_probe(struct platform_device *pdev)
{
	struct cmsdk_gpio *cmsdk_gpio;
	struct resource *res;
	void __iomem *iomem;
	u32 reg;
	int ret;

	cmsdk_gpio = devm_kzalloc(&pdev->dev, sizeof(*cmsdk_gpio), GFP_KERNEL);
	if (cmsdk_gpio == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, cmsdk_gpio);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Error getting base resource\n");
		return -ENODEV;
	}
	iomem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(iomem))
		return PTR_ERR(iomem);
	cmsdk_gpio->base = iomem;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(&pdev->dev, "Error getting config resource\n");
		return -ENODEV;
	}

	/*
	 * We map a shared resource between all GPIO instances.
	 * In order to avoid resource-already-in-use error,
	 * we use devm_ioremap instead of devm_ioremap_resource.
	 */
	iomem = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(iomem)) {
		dev_err(&pdev->dev, "Error mapping config resource\n");
		return PTR_ERR(iomem);
	}
	cmsdk_gpio->config = iomem;

	/* Disable interrupts */
	disable_gpio_irqs(cmsdk_gpio);

	raw_spin_lock_init(&cmsdk_gpio->lock);

	if (of_property_read_u32(pdev->dev.of_node, "cmsdk_gpio,ngpio", &reg))
		/* By default assume maximum ngpio */
		cmsdk_gpio->gc.ngpio = CMSDK_GPIO_MAX_NGPIO;
	else
		cmsdk_gpio->gc.ngpio = reg;

	if (cmsdk_gpio->gc.ngpio > CMSDK_GPIO_MAX_NGPIO) {
		dev_warn(&pdev->dev,
			 "ngpio is greater than %d, defaulting to %d\n",
			 CMSDK_GPIO_MAX_NGPIO, CMSDK_GPIO_MAX_NGPIO);
		cmsdk_gpio->gc.ngpio = CMSDK_GPIO_MAX_NGPIO;
	}

	if (of_property_read_u32(pdev->dev.of_node, "cmsdk_gpio,gpio-offset",
				 &reg)) {
		dev_err(&pdev->dev, "Error getting gpio-offset\n");
		return -ENODEV;
	}

	cmsdk_gpio->gc.offset = reg;
	cmsdk_gpio->gc.request = gpiochip_generic_request;
	cmsdk_gpio->gc.free = gpiochip_generic_free;
	cmsdk_gpio->gc.base = -1;
	cmsdk_gpio->gc.get_direction = cmsdk_gpio_get_direction;
	cmsdk_gpio->gc.direction_input = cmsdk_gpio_direction_input;
	cmsdk_gpio->gc.direction_output = cmsdk_gpio_direction_output;
	cmsdk_gpio->gc.get = cmsdk_gpio_get_value;
	cmsdk_gpio->gc.set = cmsdk_gpio_set_value;
	cmsdk_gpio->gc.label = dev_name(&pdev->dev);
	cmsdk_gpio->gc.parent = &pdev->dev;
	cmsdk_gpio->gc.owner = THIS_MODULE;
	cmsdk_gpio->gc.to_irq = cmsdk_gpio_to_irq;

	ret = devm_gpiochip_add_data(&pdev->dev, &cmsdk_gpio->gc, cmsdk_gpio);
	if (ret)
		return ret;

	ret = platform_irq_count(pdev);
	if (ret < 0)
		return ret;

	/* Setup IRQ */
	if (ret > 0) {
		ret = cmsdk_setup_irq(pdev, cmsdk_gpio);
		if (ret)
			return ret;
	}

	dev_info(&pdev->dev, "CMSDK GPIO chip registered\n");

	return 0;
}

static const struct of_device_id cmsdk_gpio_of_match[] = {
	{ .compatible = "arm,cmsdk-gpio" },
	{}
};
MODULE_DEVICE_TABLE(of, cmsdk_gpio_of_match);
static struct platform_driver cmsdk_gpio_driver = {
    .probe = cmsdk_gpio_probe,
    .driver = {
        .name = "cmsdk-gpio",
        .of_match_table = cmsdk_gpio_of_match,
    },
};
module_platform_driver(cmsdk_gpio_driver);

MODULE_DESCRIPTION("ARM CoreLink CMSDK GPIO driver");
MODULE_ALIAS("platform:gpio-cmsdk_gpio");
MODULE_LICENSE("GPL");
