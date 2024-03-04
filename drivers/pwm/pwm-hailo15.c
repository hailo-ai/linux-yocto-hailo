/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * Driver for the Hailo15 PWM driver
 *
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <asm/delay.h>

#define PWM_HAILO15__MAX_NPWM_CHANNELS (6)
#define PWM_HAIL015__RESOLUTION (4096)

/*
 * period = div * (RESOLUTION / CLK_RATE) = div * (4096 / (200 * 10^6)) = div * (20480 * 10^-9)
 * The minimum value for div is 0. in This case period = (20480 * 10^-9) s = 20480 ns
 * The maximum value for div is 0xFFFFF (1048575). in This case period = (20480 * 10^-9) * 1048575 = 21.475 s = 21474816000 ns
*/

#define PWM_HAIL015__PERIOD_NS__MIN_VALUE (20480)
#define PWM_HAIL015__PERIOD_NS__MAX_VALUE (21474816000)
#define PWM_HAIL015__CLK_DIV__MAX_VALUE (0xFFFFF)
#define PWM_HAIL015__SLEEP_BEFORE_DISABLE__MICRO_SEC (10000)

#define PWM_CONFIG__PWM_CLOCK__DIV_FACTOR (0x0)
#define PWM_CONFIG__PWM_MODULE__ENABLE (0x4)
#define PWM_CONFIG__PWM0_COUNTER (0x8)
#define PWM_CONFIG__PWMX_COUNTER__ENABLE (0)
#define PWM_CONFIG__PWMX_COUNTER__START (1)
#define PWM_CONFIG__PWMX_COUNTER__FINISH (13)
#define PWM_CONFIG__PWMX_COUNTER__SHIFT (4)
#define PWM_CONFIG__SETTINGS__UPDATE (0x20)

#define PWM_CHANNEL_ADDRESS_OFFSET(pwm_channel)                                \
	(PWM_CONFIG__PWM0_COUNTER +                                            \
	 (pwm_channel * PWM_CONFIG__PWMX_COUNTER__SHIFT))

/**
 * struct hailo15_pwm_chip - Hailo15 PWM chip structure
 * @chip: The PWM chip structure
 * @clk: Pointer to the clock structure
 * @base: Pointer to the base address of the PWM registers
 * @lock: Spinlock for protecting concurrent access to the PWM registers
 * @channels: Array of channel values for each PWM channel
 * @mask_enable_channels: Bitmask indicating which channels are enabled
 * @pwm_module_period_ns: Period of the PWM module in nanoseconds
 *
 * This structure represents the Hailo15 PWM chip. It contains various members
 * that store information related to the PWM chip, such as the chip structure,
 * clock structure, base address of the PWM registers, spinlock for protecting
 * concurrent access to the PWM registers, array of channel values for each PWM
 * channel, bitmask indicating which channels are enabled, and the period of the
 * PWM module in nanoseconds.
 */
struct hailo15_pwm_chip {
	struct pwm_chip chip;
	struct clk *clk;
	void __iomem *base;
	spinlock_t lock;
	uint32_t channels[PWM_HAILO15__MAX_NPWM_CHANNELS];
	uint8_t mask_enable_channels;
	uint64_t pwm_module_period_ns;
};

static inline struct hailo15_pwm_chip *
to_hailo15_pwm_chip(struct pwm_chip *chip)
{
	return container_of(chip, struct hailo15_pwm_chip, chip);
}

/**
 * Configures the Hailo15 PWM channel.
 *
 * This function is responsible for configuring the specified PWM channel on the Hailo15 chip.
 *
 * @param chip The PWM chip structure.
 * @param pwm_channel The PWM channel number.
 * @param duty_ns The desired duty cycle in nanoseconds.
 * @param period_ns The desired period in nanoseconds.
 * @return 0 on success, a negative error code on failure.
 */
static int hailo15_pwm_config(struct pwm_chip *chip, unsigned int pwm_channel,
			      uint64_t duty_ns, uint64_t period_ns)
{
	/**
	 * @brief Initializes the variables used in the code snippet.
	 *
	 * This function initializes the variables used in the code snippet, including:
	 * - hpc: A pointer to the hailo15_pwm_chip structure.
	 * - finish_cnt: The finish count value.
	 * - start_cnt: The start count value.
	 * - enable: The enable value.
	 * - data_reg: The data register value.
	 * - clk_rate_pre: The pre-calculated clock rate.
	 * - clk_div: The clock divider value.
	 */
	struct hailo15_pwm_chip *hpc = to_hailo15_pwm_chip(chip);
	uint32_t finish_cnt = 0, start_cnt = 1, enable, data_reg;
	uint64_t clk_rate_pre, clk_div;

	clk_rate_pre = clk_get_rate(hpc->clk);

	clk_div = DIV_ROUND_CLOSEST(period_ns * clk_rate_pre, PWM_HAIL015__RESOLUTION);
	clk_div = DIV_ROUND_CLOSEST(clk_div, NSEC_PER_SEC);
	if (clk_div > PWM_HAIL015__CLK_DIV__MAX_VALUE) {
		clk_div = PWM_HAIL015__CLK_DIV__MAX_VALUE;
	}

	/**
	 * Calculates the finish count for a PWM signal based on the given duty cycle and period.
	 * The finish count is calculated using the start count because the PWM signal is generated
	 * by counting from the start count to the finish count.
	 * If the finish count is equal to the start count, it is incremented by 1.
	 */
	finish_cnt = DIV_ROUND_CLOSEST(duty_ns * PWM_HAIL015__RESOLUTION, period_ns) + start_cnt;
	if (finish_cnt >= PWM_HAIL015__RESOLUTION) {
		finish_cnt = PWM_HAIL015__RESOLUTION - 1;
	} else if (finish_cnt == start_cnt) {
		finish_cnt++;
	}

	writel(clk_div, hpc->base + PWM_CONFIG__PWM_CLOCK__DIV_FACTOR);

	data_reg = readl(hpc->base + PWM_CHANNEL_ADDRESS_OFFSET(pwm_channel));

	enable = data_reg & (1 << PWM_CONFIG__PWMX_COUNTER__ENABLE);

	data_reg = (enable << PWM_CONFIG__PWMX_COUNTER__ENABLE) |
		   (start_cnt << PWM_CONFIG__PWMX_COUNTER__START) |
		   (finish_cnt << PWM_CONFIG__PWMX_COUNTER__FINISH);
	writel(data_reg, hpc->base + PWM_CHANNEL_ADDRESS_OFFSET(pwm_channel));

	hpc->pwm_module_period_ns = period_ns;

	return 0;
}

static int hailo15_pwm_enable(struct pwm_chip *chip)
{
	struct hailo15_pwm_chip *hpc = to_hailo15_pwm_chip(chip);

	writel(1, hpc->base + PWM_CONFIG__PWM_MODULE__ENABLE);
	return 0;
}

static int hailo15_pwm_disable(struct pwm_chip *chip)
{
	struct hailo15_pwm_chip *hpc = to_hailo15_pwm_chip(chip);

	writel(0, hpc->base + PWM_CONFIG__PWM_MODULE__ENABLE);
	return 0;
}

static void hailo15_channel_pwm_enable(struct pwm_chip *chip,
				       unsigned int pwm_channel)
{
	uint32_t data_reg;
	struct hailo15_pwm_chip *hpc = to_hailo15_pwm_chip(chip);

	data_reg = readl(hpc->base + PWM_CHANNEL_ADDRESS_OFFSET(pwm_channel));
	data_reg |= (1 << PWM_CONFIG__PWMX_COUNTER__ENABLE);

	writel(data_reg, hpc->base + PWM_CHANNEL_ADDRESS_OFFSET(pwm_channel));
}

static void hailo15_channel_pwm_disable(struct pwm_chip *chip,
					unsigned int pwm_channel)
{
	uint32_t data_reg;
	struct hailo15_pwm_chip *hpc = to_hailo15_pwm_chip(chip);

	/**
	 * Set the duty cycle of the Hailo15 PWM channel to 0.
	 * 
	 * This function sets the duty cycle of the specified PWM channel to 0 by configuring the Hailo15 PWM module.
	 * It performs the following steps:
	 * 1. Calls hailo15_pwm_config() to set the duty cycle to 0, without change the period.
	 * 2. Delays for PWM_HAIL015__SLEEP_BEFORE_DISABLE__MICRO_SEC milliseconds using udelay().
	 * 3. Reads the current value of the data register for the PWM channel.
	 * 4. Clears the enable bit and the start value bits in the data register.
	 * 5. Writes the updated data register value back to the PWM channel address.
	 * 
	 */
	hailo15_pwm_config(chip, pwm_channel, 0, hpc->pwm_module_period_ns);

	udelay(PWM_HAIL015__SLEEP_BEFORE_DISABLE__MICRO_SEC);

	data_reg = readl(hpc->base + PWM_CHANNEL_ADDRESS_OFFSET(pwm_channel));
	data_reg &= ~(1 << PWM_CONFIG__PWMX_COUNTER__ENABLE);
	data_reg &= ~((PWM_HAIL015__RESOLUTION - 1) << PWM_CONFIG__PWMX_COUNTER__START);

	writel(data_reg, hpc->base + PWM_CHANNEL_ADDRESS_OFFSET(pwm_channel));
}

static int hailo15_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			     const struct pwm_state *channel_new_state)
{
	struct hailo15_pwm_chip *hpc = to_hailo15_pwm_chip(chip);
	bool channel_current_enable_state = pwm->state.enabled;
	unsigned int pwm_channel = hpc->channels[pwm->hwpwm];
	unsigned int num_pwm_channels_enabled = 0, pwm_channels_enable = 0;
	int err;

	if (channel_new_state->polarity != PWM_POLARITY_NORMAL) {
		dev_err(chip->dev,
			"Error: Polarity inversion is not supported\n");
		return -EINVAL;
	}

	if ((channel_new_state->period < PWM_HAIL015__PERIOD_NS__MIN_VALUE) ||
	    (channel_new_state->period > PWM_HAIL015__PERIOD_NS__MAX_VALUE)) {
		dev_err(chip->dev,
			"Error: The period is not in the right range: [%d - %ld]\n",
			PWM_HAIL015__PERIOD_NS__MIN_VALUE,
			PWM_HAIL015__PERIOD_NS__MAX_VALUE);
		return -EIO;
	}

	/**
	 * Disables the PWM channel if it is not already enabled.
	 * If the channel is currently enabled, it disables the channel,
	 * updates the mask of enabled channels, and checks if all channels
	 * are disabled. If all channels are disabled, it disables the PWM IP.
	 *
	 */
	if (!channel_new_state->enabled) {
		if (channel_current_enable_state) {
			hailo15_channel_pwm_disable(chip, pwm_channel);
			spin_lock(&hpc->lock);
			hpc->mask_enable_channels &= (~BIT(pwm_channel));
			num_pwm_channels_enabled =
				__arch_hweight8(hpc->mask_enable_channels);
			spin_unlock(&hpc->lock);
			/* If all the channels are disabled, disable the PWM IP */
			if (0 == num_pwm_channels_enabled) {
				hailo15_pwm_disable(chip);
			}
		}
		return 0;
	}

	/* If the user try to set different period from what is configured now,
	 * and there is more than one channel configured, the driver will prevent this.
	*/
	if (channel_new_state->period != hpc->pwm_module_period_ns) {
		num_pwm_channels_enabled =
			__arch_hweight8(hpc->mask_enable_channels);
		pwm_channels_enable = __builtin_ctz(hpc->mask_enable_channels);
		if (((1 == num_pwm_channels_enabled) &&
		     (pwm_channels_enable != pwm_channel)) ||
		    (num_pwm_channels_enabled > 1)) {
			dev_err(chip->dev,
				"Error: Failed to set period for the PWM channels since more then one channel is enabled\n");
			return -EINVAL;
		}
	}

	err = hailo15_pwm_config(chip, pwm_channel, channel_new_state->duty_cycle,
				 channel_new_state->period);
	if (err) {
		return err;
	}

	/**
	 * Enable the PWM channel if it is not already enabled and update the mask of enabled channels.
	 * If this is the first enabled channel, also enable the PWM IP.
	 *
	 */
	if (!channel_current_enable_state) {
		hailo15_channel_pwm_enable(chip, pwm_channel);
		spin_lock(&hpc->lock);
		hpc->mask_enable_channels |= BIT(pwm_channel);
		num_pwm_channels_enabled =
			__arch_hweight8(hpc->mask_enable_channels);
		spin_unlock(&hpc->lock);
		/* If one of the channels is enable, enable the PWM IP */
		if (1 == num_pwm_channels_enabled) {
			hailo15_pwm_enable(chip);
		}
	}

	return 0;
}

static const struct pwm_ops hailo15_pwm_ops = {
	.apply = hailo15_pwm_apply,
	.owner = THIS_MODULE,
};

static const struct of_device_id hailo15_pwm_match[] = {
	{ .compatible = "hailo15,pwm" },
	{},
};
MODULE_DEVICE_TABLE(of, hailo15_pwm_match);

static int hailo15_pwm_probe(struct platform_device *pdev)
{
	struct hailo15_pwm_chip *hpc;
	int ret, index;
	int npwm_channels;

	hpc = devm_kzalloc(&pdev->dev, sizeof(*hpc), GFP_KERNEL);
	if (!hpc) {
		return -ENOMEM;
	}

	hpc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(hpc->base)) {
		dev_err(&pdev->dev, "failed get pwm base addrerss\n");
		ret = PTR_ERR(hpc->base);
		goto free_pltfm;
	}

	hpc->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(hpc->clk)) {
		dev_err_probe(&pdev->dev, PTR_ERR(hpc->clk), "Cannot claim pwm clock\n");
		ret = PTR_ERR(hpc->clk);
		goto free_pltfm;
	}

	ret = clk_prepare_enable(hpc->clk);
	if (ret) {
		dev_err(&pdev->dev, "couldn't prepare-enable pwm clock\n");
		goto free_pltfm;
	}

	/*
	 * Parse the supported PWM channels.
	 * In the device tree it need to appear like that: "hailo15_pwm,supported-channels = <2 3>;"
	 * This is a example (in this board the PWM channels 2 and 3 are used).
	 * In every board the user can choose which PWM channels he want to use.
	 * If this property didn't exist, the driver set all the PWM_HAILO15__MAX_NPWM_CHANNELS channels.
	 * hpc->channels is a map between the driver channel index to the PWM IP channel index.
	*/
	npwm_channels = of_property_read_variable_u32_array(
		pdev->dev.of_node, "hailo15_pwm,supported-channels",
		hpc->channels, 1, ARRAY_SIZE(hpc->channels));

	if (npwm_channels == -EINVAL) {
		dev_dbg(&pdev->dev,
			"%s: No \"hailo15_pwm,supported-channels\", use all the channels\n",
			__func__);
		hpc->chip.npwm = PWM_HAILO15__MAX_NPWM_CHANNELS;

		for (index = 0; index < PWM_HAILO15__MAX_NPWM_CHANNELS;
		     index++) {
			hpc->channels[index] = index;
		}
	} else if (npwm_channels < 0) {
		dev_err(&pdev->dev,
			"%s: Error getting \"hailo15_pwm,supported-channels\": %d\n",
			__func__, npwm_channels);
		ret = npwm_channels;
		goto err_clk;
	} else {
		hpc->chip.npwm = npwm_channels;
		for (index = 0; index < npwm_channels; index++) {
			if (hpc->channels[index] >=
			    PWM_HAILO15__MAX_NPWM_CHANNELS) {
				dev_err(&pdev->dev,
					"%s: Error getting \"hailo15_pwm,supported-channels\": %d\n",
					__func__, hpc->channels[index]);
				ret = -EIO;
				goto err_clk;
			}
		}
	}

	hpc->chip.dev = &pdev->dev;
	hpc->chip.ops = &hailo15_pwm_ops;
	hpc->mask_enable_channels = 0;

	spin_lock_init(&hpc->lock);

	ret = pwmchip_add(&hpc->chip);

	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add PWM chip: %d\n", ret);
		goto err_clk;
	}

	platform_set_drvdata(pdev, hpc);

	/* This register is responsible for activating the PWM IP registers changes */
	writel(1, hpc->base + PWM_CONFIG__SETTINGS__UPDATE);

	dev_info(&pdev->dev, "PWM chip registered\n");

	return 0;

err_clk:
	clk_disable_unprepare(hpc->clk);
free_pltfm:
	return ret;
}

static int hailo15_pwm_remove(struct platform_device *pdev)
{
	struct hailo15_pwm_chip *hpc = platform_get_drvdata(pdev);

	pwmchip_remove(&hpc->chip);

	clk_disable_unprepare(hpc->clk);

	return 0;
}
static struct platform_driver hailo15_pwm_driver = {
	.probe = hailo15_pwm_probe,
	.remove = hailo15_pwm_remove,
	.driver = {
		.name = "hailo15-pwm",
		.of_match_table = hailo15_pwm_match,
	},
};
module_platform_driver(hailo15_pwm_driver);