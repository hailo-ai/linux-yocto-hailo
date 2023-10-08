/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Hailo-15 reset module.
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 */

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/reset.h>
#include <linux/notifier.h>

/* 
 * Hailo-15 SoC reset support 
 */

/*! @brief reset control pointer */
static struct reset_control *system_reset_ctrl = NULL;

/*!
 * @brief Hailo-15 SoC reset via SCMI.
 * @note  We are using assert and not reset because SCU doesn't supports reset (auto-reset).
 *
 * @param [in] nb - pointer to hailo15_reset_nb
 * @param [in] action - unused
 * @param [in] data - unused
 *
 * @return 0-ok, otherwise-fail.
 */
static int hailo15_reset(struct notifier_block *nb, unsigned long action, void *data)
{
	int rc = reset_control_assert(system_reset_ctrl);
	if (rc) {
		pr_err("hailo15 system-reset failed, invoking hang() instead: ret[%d].\n", rc);
	}

	return NOTIFY_DONE;
}

/*!
 * @brief Hailo-15 SoC reset notifier block.
 */
static struct notifier_block hailo15_reset_nb = {
	.notifier_call = hailo15_reset,
	.priority = 128,
};

/*!
 * @brief Hailo-15 SoC reset probe.
 * @note  DT node example:
 *			power_reset {
 *			compatible = "hailo15-reset";
 *			resets = <&scmi_reset HAILO15_SCMI_RESET_IDX_SYSTEM_RESET>;
 *			reset-names = "system-reset";
 *			};
 * 
 * @return 0-ok, otherwise-fail.
 */
static int hailo15_reset_probe(struct platform_device *pdev)
{
	/* resolve system-reset reset control object. */
	system_reset_ctrl = devm_reset_control_get(&pdev->dev, "system-reset");
	if (IS_ERR(system_reset_ctrl)) {
		if (PTR_ERR(system_reset_ctrl) != -EPROBE_DEFER) {
			pr_err("hailo-reset: failed to get system-reset control, error = %pe.\n", system_reset_ctrl);
		}
		return PTR_ERR(system_reset_ctrl);
	}
	pr_info("hailo-reset: register system-reset control successfully\n");
	register_restart_handler(&hailo15_reset_nb);

	return 0;
}

static const struct of_device_id of_hailo15_reset_match[] = {
	{ .compatible = "hailo15-reset" },
	{},
};

static struct platform_driver hailo15_reset_driver = {
	.probe = hailo15_reset_probe,
	.driver = {
		.name = "hailo15-reset",
		.of_match_table = of_hailo15_reset_match,
	},
};
builtin_platform_driver(hailo15_reset_driver);
