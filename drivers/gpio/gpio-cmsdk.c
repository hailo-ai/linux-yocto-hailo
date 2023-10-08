/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * Driver for the ARM CoreLink CMSDK GPIO
 *
 */

#include <linux/module.h>
#include <linux/gpio/driver.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>

#define CMSDK_GPIO_MAX_NGPIO (16)
typedef struct GPIO_BLOCK_regs_s {
	volatile uint32_t DATA; /* offset: 0x0 ; repeat: [1]        */
	volatile uint32_t DATAOUT; /* offset: 0x4 ; repeat: [1]        */
	volatile uint32_t reserved_0[2]; /* offset: 0x8 ; repeat: [2]        */
	volatile uint32_t OUTENSET; /* offset: 0x10 ; repeat: [1]        */
	volatile uint32_t OUTENCLR; /* offset: 0x14 ; repeat: [1]        */
	volatile uint32_t ALTFUNCSET; /* offset: 0x18 ; repeat: [1]        */
	volatile uint32_t ALTFUNCCLR; /* offset: 0x1c ; repeat: [1]        */
	volatile uint32_t INTENSET; /* offset: 0x20 ; repeat: [1]        */
	volatile uint32_t INTENCLR; /* offset: 0x24 ; repeat: [1]        */
	volatile uint32_t INTTYPESET; /* offset: 0x28 ; repeat: [1]        */
	volatile uint32_t INTTYPECLR; /* offset: 0x2c ; repeat: [1]        */
	volatile uint32_t INTPOLSET; /* offset: 0x30 ; repeat: [1]        */
	volatile uint32_t INTPOLCLR; /* offset: 0x34 ; repeat: [1]        */
	volatile uint32_t INTSTATUS; /* offset: 0x38 ; repeat: [1]        */
	volatile uint32_t reserved_1[241]; /* offset: 0x3c ; repeat: [241]      */
	volatile uint32_t MASKLOWBYTE; /* offset: 0x400 ; repeat: [1]        */
	volatile uint32_t
		reserved_2[255]; /* offset: 0x404 ; repeat: [255]      */
	volatile uint32_t MASKHIGHBYTE; /* offset: 0x800 ; repeat: [1]        */
	volatile uint32_t
		reserved_3[499]; /* offset: 0x804 ; repeat: [499]      */
	volatile uint32_t PID4; /* offset: 0xfd0 ; repeat: [1]        */
	volatile uint32_t PID5; /* offset: 0xfd4 ; repeat: [1]        */
	volatile uint32_t PID6; /* offset: 0xfd8 ; repeat: [1]        */
	volatile uint32_t PID7; /* offset: 0xfdc ; repeat: [1]        */
	volatile uint32_t PID0; /* offset: 0xfe0 ; repeat: [1]        */
	volatile uint32_t PID1; /* offset: 0xfe4 ; repeat: [1]        */
	volatile uint32_t PID2; /* offset: 0xfe8 ; repeat: [1]        */
	volatile uint32_t PID3; /* offset: 0xfec ; repeat: [1]        */
	volatile uint32_t CID0; /* offset: 0xff0 ; repeat: [1]        */
	volatile uint32_t CID1; /* offset: 0xff4 ; repeat: [1]        */
	volatile uint32_t CID2; /* offset: 0xff8 ; repeat: [1]        */
	volatile uint32_t CID3; /* offset: 0xffc ; repeat: [1]        */
} GPIO_BLOCK_regs_s;

struct cmsdk_gpio {
	raw_spinlock_t lock;
	void __iomem *base;
	struct gpio_chip gc;
	struct irq_chip irq_chip;
	int parent_irq;
	unsigned int gpio_index_module_offset;
};

static int cmsdk_gpio_get_value(struct gpio_chip *gc, unsigned int offset)
{
	struct cmsdk_gpio *cmsdk_gpio = gpiochip_get_data(gc);
	GPIO_BLOCK_regs_s *CMSDK_GPIO_REGS =
		((GPIO_BLOCK_regs_s *)cmsdk_gpio->base);

	return !!(readl(&CMSDK_GPIO_REGS->DATA) & BIT(offset));
}

static void cmsdk_gpio_set_value_inner(struct cmsdk_gpio *cmsdk_gpio,
				       unsigned int offset, int value)
{
	GPIO_BLOCK_regs_s *CMSDK_GPIO_REGS =
		((GPIO_BLOCK_regs_s *)cmsdk_gpio->base);
	unsigned long flags;
	unsigned int data_reg;

	raw_spin_lock_irqsave(&cmsdk_gpio->lock, flags);
	data_reg = readl(&CMSDK_GPIO_REGS->DATAOUT);
	if (value)
		data_reg |= BIT(offset);
	else
		data_reg &= ~BIT(offset);
	writel(data_reg, &CMSDK_GPIO_REGS->DATAOUT);
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
	GPIO_BLOCK_regs_s *CMSDK_GPIO_REGS =
		((GPIO_BLOCK_regs_s *)cmsdk_gpio->base);

	/* By the Programmers model for OUTENCLR:
        Read back 0 - Indicate the signal direction as input.
        Read back 1 - Indicate the signal direction as output. */
	if (readl(&CMSDK_GPIO_REGS->OUTENCLR) & BIT(offset))
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}

static int cmsdk_gpio_set_direction_input(struct cmsdk_gpio *cmsdk_gpio,
					  unsigned int offset)
{
	GPIO_BLOCK_regs_s *CMSDK_GPIO_REGS =
		((GPIO_BLOCK_regs_s *)cmsdk_gpio->base);
	unsigned long flags;
	unsigned int gpio_ddr;

	raw_spin_lock_irqsave(&cmsdk_gpio->lock, flags);
	/* Set pin as input, assumes software controlled IP.
       By the Programmers model for OUTENCLR:
        Write 1 - Clear the output enable bit.
        Write 0 - No effect.
        Only asserted bit is affected with BIT macro, while the others remain unchanged. */
	gpio_ddr = BIT(offset);
	writel(gpio_ddr, &CMSDK_GPIO_REGS->OUTENCLR);
	raw_spin_unlock_irqrestore(&cmsdk_gpio->lock, flags);
	return 0;
}

static int cmsdk_gpio_set_direction_output(struct cmsdk_gpio *cmsdk_gpio,
					   unsigned int offset)
{
	GPIO_BLOCK_regs_s *CMSDK_GPIO_REGS =
		((GPIO_BLOCK_regs_s *)cmsdk_gpio->base);
	unsigned long flags;
	unsigned int gpio_ddr;

	raw_spin_lock_irqsave(&cmsdk_gpio->lock, flags);
	/* Set pin as output, assumes software controlled IP.
       By the Programmers model for OUTENSET:
        Write 1 - Set the output enable bit.
        Write 0 - No effect.
        Only asserted bit is affected with BIT macro, while the others remain unchanged. */
	gpio_ddr = BIT(offset);
	writel(gpio_ddr, &CMSDK_GPIO_REGS->OUTENSET);
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

static int cmsdk_gpio_probe(struct platform_device *pdev)
{
	struct cmsdk_gpio *cmsdk_gpio;
	struct resource *res;
	int reg, ret;

	cmsdk_gpio = devm_kzalloc(&pdev->dev, sizeof(*cmsdk_gpio), GFP_KERNEL);
	if (cmsdk_gpio == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, cmsdk_gpio);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Error getting resource\n");
		return -ENODEV;
	}
	cmsdk_gpio->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(cmsdk_gpio->base))
		return PTR_ERR(cmsdk_gpio->base);

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

	of_property_read_u32(pdev->dev.of_node, "cmsdk_gpio,gpio-offset",
			     &cmsdk_gpio->gpio_index_module_offset);

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

	ret = devm_gpiochip_add_data(&pdev->dev, &cmsdk_gpio->gc, cmsdk_gpio);
	if (ret)
		return ret;

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