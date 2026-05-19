// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 - 2024 Hailo Technologies Ltd.
 * Based on Copyright (C) 2020 InvenSense:
 * - Linux inv_icm42600
 * - Arduino https://github.com/tdk-invn-oss/motion.arduino.ICM42670P.git
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <asm/unaligned.h>

#include "inv_icm42670.h"
#include "inv_icm42670_temp.h"

static int inv_icm42670_temp_read(struct inv_icm42670_state *st, int16_t *temp)
{
	struct device *dev = regmap_get_device(st->map);
	int ret;

	pm_runtime_get_sync(dev);
	mutex_lock(&st->lock);

	ret = inv_icm42670_set_temp_conf(st, true, NULL);
	if (ret)
		goto exit;

	ret = regmap_bulk_read(st->map, INV_ICM42670_TEMP_DATA1, st->buffer, 2);
	if (ret)
		goto exit;

	*temp = (int16_t)get_unaligned_be16(st->buffer);
	if (*temp == INV_ICM42670_DATA_INVALID)
		ret = -EINVAL;

exit:
	mutex_unlock(&st->lock);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}

int inv_icm42670_temp_read_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int *val, int *val2, long mask)
{
	struct inv_icm42670_state *st = iio_device_get_drvdata(indio_dev);
	int16_t temp;
	int ret;

	if (chan->type != IIO_TEMP)
		return -EINVAL;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(indio_dev);
		if (ret)
			return ret;
		ret = inv_icm42670_temp_read(st, &temp);
		iio_device_release_direct_mode(indio_dev);
		if (ret)
			return ret;
		*val = temp;
		return IIO_VAL_INT;
	/*
	 * T°C = (temp / 128) + 25
	 * scale: 1 / 128 ~= 7.8125 mili
	 * offset: 25000 mili
	 */
	case IIO_CHAN_INFO_SCALE:
		*val = 7;
		*val2 = 812500;
		return IIO_VAL_INT_PLUS_MICRO;
	case IIO_CHAN_INFO_OFFSET:
		*val = 25000;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}
