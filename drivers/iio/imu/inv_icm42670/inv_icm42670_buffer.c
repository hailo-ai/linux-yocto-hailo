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
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <asm/unaligned.h>
#include <linux/iio/buffer.h>

#include "inv_icm42670.h"
#include "inv_icm42670_timestamp.h"
#include "inv_icm42670_buffer.h"

/* FIFO header: 1 byte */
#define INV_ICM42670_FIFO_HEADER_MSG		BIT(7)
#define INV_ICM42670_FIFO_HEADER_ACCEL		BIT(6)
#define INV_ICM42670_FIFO_HEADER_GYRO		BIT(5)
#define INV_ICM42670_FIFO_HEADER_HIGHRES_20	BIT(4)
#define INV_ICM42670_FIFO_HEADER_TMST_FSYNC	GENMASK(3, 2)
#define INV_ICM42670_FIFO_HEADER_ODR_ACCEL	BIT(1)
#define INV_ICM42670_FIFO_HEADER_ODR_GYRO	BIT(0)

struct inv_icm42670_fifo_1sensor_packet {
	uint8_t header;
	struct inv_icm42670_fifo_sensor_data data;
	int8_t temp;
} __packed;

struct inv_icm42670_fifo_2sensors_packet {
	uint8_t header;
	struct inv_icm42670_fifo_sensor_data accel;
	struct inv_icm42670_fifo_sensor_data gyro;
	int8_t temp;
	__be16 timestamp;
} __packed;

struct inv_icm42670_fifo_2sensors_highres_packet {
	uint8_t header;
	struct inv_icm42670_fifo_sensor_data accel;
	struct inv_icm42670_fifo_sensor_data gyro;
	__be16 temp;
	__be16 timestamp;
	struct inv_icm42670_fifo_sensor_data_highres highres;
} __packed;

ssize_t inv_icm42670_fifo_decode_packet(const void *packet, const void **accel,
					const void **gyro, const int8_t **temp,
					const void **timestamp, unsigned int *odr)
{
	const struct inv_icm42670_fifo_1sensor_packet *pack1 = packet;
	const struct inv_icm42670_fifo_2sensors_packet *pack2 = packet;
	// TODO add support for High resolution packet
	uint8_t header = *((const uint8_t *)packet);

	/* FIFO empty */
	if (header & INV_ICM42670_FIFO_HEADER_MSG) {
		*accel = NULL;
		*gyro = NULL;
		*temp = NULL;
		*timestamp = NULL;
		*odr = 0;
		return 0;
	}

	/* handle odr flags */
	*odr = 0;
	if (header & INV_ICM42670_FIFO_HEADER_ODR_GYRO)
		*odr |= INV_ICM42670_SENSOR_GYRO;
	if (header & INV_ICM42670_FIFO_HEADER_ODR_ACCEL)
		*odr |= INV_ICM42670_SENSOR_ACCEL;

	/* accel + gyro */
	if ((header & INV_ICM42670_FIFO_HEADER_ACCEL) &&
	    (header & INV_ICM42670_FIFO_HEADER_GYRO)) {
		*accel = &pack2->accel;
		*gyro = &pack2->gyro;
		*temp = &pack2->temp;
		*timestamp = &pack2->timestamp;
		if ((header & INV_ICM42670_FIFO_HEADER_TMST_FSYNC) == INV_ICM42670_FIFO_HEADER_TMST_FSYNC) {
			((struct inv_icm42670_fifo_2sensors_packet *)pack2)->gyro.x |= cpu_to_be16(0x1);
			((struct inv_icm42670_fifo_2sensors_packet *)pack2)->gyro.y |= cpu_to_be16(0x1);
			((struct inv_icm42670_fifo_2sensors_packet *)pack2)->gyro.z |= cpu_to_be16(0x1);
		} else {
			((struct inv_icm42670_fifo_2sensors_packet *)pack2)->gyro.x &= ~cpu_to_be16(0x1);
			((struct inv_icm42670_fifo_2sensors_packet *)pack2)->gyro.y &= ~cpu_to_be16(0x1);
			((struct inv_icm42670_fifo_2sensors_packet *)pack2)->gyro.z &= ~cpu_to_be16(0x1);
		}

		return INV_ICM42670_FIFO_2SENSORS_PACKET_SIZE;
	}

	/* accel only */
	if (header & INV_ICM42670_FIFO_HEADER_ACCEL) {
		*accel = &pack1->data;
		*gyro = NULL;
		*temp = &pack1->temp;
		*timestamp = NULL;
		return INV_ICM42670_FIFO_1SENSOR_PACKET_SIZE;
	}

	/* gyro only */
	if (header & INV_ICM42670_FIFO_HEADER_GYRO) {
		*accel = NULL;
		*gyro = &pack1->data;
		*temp = &pack1->temp;
		*timestamp = NULL;
		return INV_ICM42670_FIFO_1SENSOR_PACKET_SIZE;
	}

	pr_info("in case of ERROR, header : %x\n", header);

	/* invalid packet if here */
	return -EINVAL;
}

void inv_icm42670_buffer_update_fifo_period(struct inv_icm42670_state *st)
{
	uint32_t period_gyro, period_accel, period;

	if (st->fifo.en & INV_ICM42670_SENSOR_GYRO)
		period_gyro = inv_icm42670_odr_to_period(st->conf.gyro.odr);
	else
		period_gyro = U32_MAX;

	if (st->fifo.en & INV_ICM42670_SENSOR_ACCEL)
		period_accel = inv_icm42670_odr_to_period(st->conf.accel.odr);
	else
		period_accel = U32_MAX;

	period = min(period_gyro, period_accel);

	st->fifo.period = period;
}

int inv_icm42670_buffer_set_fifo_en(struct inv_icm42670_state *st,
				    unsigned int fifo_en)
{
	unsigned int mask, val;
	int ret;

	/* update only FIFO EN bits */
	mask = INV_ICM42670_FIFO_CONFIG5_FIFO_TMST_FSYNC_EN_MASK |
	       INV_ICM42670_FIFO_CONFIG5_FIFO_GYRO_EN_MASK |
	       INV_ICM42670_FIFO_CONFIG5_FIFO_ACCEL_EN_MASK;

	val = 0;
	if (fifo_en & INV_ICM42670_FIFO_CONFIG5_FIFO_TMST_FSYNC_EN_MASK) {
		val |= INV_ICM42670_FIFO_CONFIG5_TMST_FSYNC_EN;
	}
	if (fifo_en & INV_ICM42670_FIFO_CONFIG5_FIFO_GYRO_EN_MASK) {
		val |= INV_ICM42670_FIFO_CONFIG5_GYRO_EN;
	}
	if (fifo_en & INV_ICM42670_FIFO_CONFIG5_FIFO_ACCEL_EN_MASK) {
		val |= INV_ICM42670_FIFO_CONFIG5_ACCEL_EN;
	}

	ret = regmap_update_bits(st->map, INV_ICM42670_FIFO_CONFIG5_MREG1, mask, val);
	if (ret)
		return ret;

	st->fifo.en = fifo_en;
	inv_icm42670_buffer_update_fifo_period(st);

	return 0;
}

static size_t inv_icm42670_get_packet_size(struct inv_icm42670_state *st)
{
	size_t packet_size;
	//const struct inv_icm42670_hw *hw = &inv_icm42670_hw[st->chip];


	if ((st->fifo.en & INV_ICM42670_FIFO_CONFIG5_FIFO_GYRO_EN_MASK) &&
	    (st->fifo.en & INV_ICM42670_FIFO_CONFIG5_FIFO_ACCEL_EN_MASK)) {
		//if (hw->conf->fifo_highres_enabled) {
		//	packet_size = INV_ICM42670_FIFO_2SENSORS_HIGHRES_PACKET_SIZE;
		//} else {
			packet_size = INV_ICM42670_FIFO_2SENSORS_PACKET_SIZE;
		//}
	} else
		packet_size = INV_ICM42670_FIFO_1SENSOR_PACKET_SIZE;

	return packet_size;
}

static unsigned int inv_icm42670_wm_truncate(unsigned int watermark,
					     size_t packet_size)
{
	size_t wm_size;
	unsigned int wm;

	wm_size = watermark * packet_size;
	if (wm_size > INV_ICM42670_FIFO_WATERMARK_MAX)
		wm_size = INV_ICM42670_FIFO_WATERMARK_MAX;

	wm = wm_size / packet_size;

	return wm;
}

/**
 * inv_icm42670_buffer_update_watermark - update watermark FIFO threshold
 * @st:	driver internal state
 *
 * Returns 0 on success, a negative error code otherwise.
 *
 * FIFO watermark threshold is computed based on the required watermark values
 * set for gyro and accel sensors. Since watermark is all about acceptable data
 * latency, use the smallest setting between the 2. It means choosing the
 * smallest latency but this is not as simple as choosing the smallest watermark
 * value. Latency depends on watermark and ODR. It requires several steps:
 * 1) compute gyro and accel latencies and choose the smallest value.
 * 2) adapt the choosen latency so that it is a multiple of both gyro and accel
 *    ones. Otherwise it is possible that you don't meet a requirement. (for
 *    example with gyro @100Hz wm 4 and accel @100Hz with wm 6, choosing the
 *    value of 4 will not meet accel latency requirement because 6 is not a
 *    multiple of 4. You need to use the value 2.)
 * 3) Since all periods are multiple of each others, watermark is computed by
 *    dividing this computed latency by the smallest period, which corresponds
 *    to the FIFO frequency. Beware that this is only true because we are not
 *    using 500Hz frequency which is not a multiple of the others.
 */
int inv_icm42670_buffer_update_watermark(struct inv_icm42670_state *st)
{
	size_t packet_size, wm_size;
	unsigned int wm_gyro, wm_accel, watermark;
	uint32_t period_gyro, period_accel, period;
	uint32_t latency_gyro, latency_accel, latency;
	bool restore;
	__le16 raw_wm;
	int ret;

	packet_size = inv_icm42670_get_packet_size(st);

	/* compute sensors latency, depending on sensor watermark and odr */
	wm_gyro = inv_icm42670_wm_truncate(st->fifo.watermark.gyro, packet_size);
	wm_accel = inv_icm42670_wm_truncate(st->fifo.watermark.accel, packet_size);
	/* use us for odr to avoid overflow using 32 bits values */
	period_gyro = inv_icm42670_odr_to_period(st->conf.gyro.odr) / 1000UL;
	period_accel = inv_icm42670_odr_to_period(st->conf.accel.odr) / 1000UL;
	latency_gyro = period_gyro * wm_gyro;
	latency_accel = period_accel * wm_accel;

	/* 0 value for watermark means that the sensor is turned off */
	if (latency_gyro == 0) {
		watermark = wm_accel;
	} else if (latency_accel == 0) {
		watermark = wm_gyro;
	} else {
		/* compute the smallest latency that is a multiple of both */
		if (latency_gyro <= latency_accel)
			latency = latency_gyro - (latency_accel % latency_gyro);
		else
			latency = latency_accel - (latency_gyro % latency_accel);
		/* use the shortest period */
		period = min(period_gyro, period_accel);
		/* all this works because periods are multiple of each others */
		watermark = latency / period;
		if (watermark < 1)
			watermark = 1;
	}

	/* compute watermark value in bytes */
	wm_size = watermark * packet_size;

	/* changing FIFO watermark requires to turn off watermark interrupt */
	ret = regmap_update_bits_check(st->map, INV_ICM42670_INT_SOURCE0,
				       INV_ICM42670_INT_SOURCE0_FIFO_THS_INT1_EN_MASK,
				       0, &restore);
	if (ret)
		return ret;

	raw_wm = INV_ICM42670_FIFO_WATERMARK_VAL(wm_size);
	memcpy(st->buffer, &raw_wm, sizeof(raw_wm));
	ret = regmap_bulk_write(st->map, INV_ICM42670_FIFO_WATERMARK,
				st->buffer, sizeof(raw_wm));
	if (ret)
		return ret;

	/* restore watermark interrupt */
	if (restore) {
		ret = regmap_update_bits(st->map, INV_ICM42670_INT_SOURCE0,
					 INV_ICM42670_INT_SOURCE0_FIFO_THS_INT1_EN_MASK,
					 INV_ICM42670_INT_SOURCE0_FIFO_THS_INT1_EN_MASK);
		if (ret)
			return ret;
	}

	return 0;
}

static int inv_icm42670_buffer_preenable(struct iio_dev *indio_dev)
{
	struct inv_icm42670_state *st = iio_device_get_drvdata(indio_dev);
	struct device *dev = regmap_get_device(st->map);

	pm_runtime_get_sync(dev);

	return 0;
}

/*
 * update_scan_mode callback is turning sensors on and setting data FIFO enable
 * bits.
 */
static int inv_icm42670_buffer_postenable(struct iio_dev *indio_dev)
{
	struct inv_icm42670_state *st = iio_device_get_drvdata(indio_dev);
	int ret;

	mutex_lock(&st->lock);

	/* exit if FIFO is already on */
	if (st->fifo.on) {
		ret = 0;
		goto out_on;
	}

	/* set FIFO threshold interrupt */
	ret = regmap_update_bits(st->map, INV_ICM42670_INT_SOURCE0,
				 INV_ICM42670_INT_SOURCE0_FIFO_THS_INT1_EN_MASK,
				 INV_ICM42670_INT_SOURCE0_FIFO_THS_INT1_EN);
	if (ret)
		goto out_unlock;

	/* flush FIFO data */
	ret = regmap_write(st->map, INV_ICM42670_SIGNAL_PATH_RESET,
			   INV_ICM42670_SIGNAL_PATH_RESET_FIFO_FLUSH_EN);
	if (ret)
		goto out_unlock;

	/* set FIFO in streaming mode */
	ret = regmap_write(st->map, INV_ICM42670_FIFO_CONFIG1,
			   INV_ICM42670_FIFO_CONFIG1_FIFO_BYPASS_OFF | INV_ICM42670_FIFO_CONFIG1_FIFO_MODE_STREAM);
	if (ret)
		goto out_unlock;

	/* workaround: first read of FIFO count after reset is always 0 */
	ret = regmap_bulk_read(st->map, INV_ICM42670_FIFO_COUNTH, st->buffer, 2);
	if (ret)
		goto out_unlock;

	/* Start from Falling edge of FSYNC pulse to measure FSYNC interval */
	ret = regmap_update_bits(st->map, INV_ICM42670_FSYNC_CONFIG_MREG1,
				INV_ICM42670_FSYNC_CONFIG_FSYNC_POLARITY_MASK,
				INV_ICM42670_FSYNC_CONFIG_FSYNC_POLARITY_MASK);
	if (ret)
		goto out_unlock;

out_on:
	/* increase FIFO on counter */
	st->fifo.on++;
out_unlock:
	mutex_unlock(&st->lock);
	return ret;
}

static int inv_icm42670_buffer_predisable(struct iio_dev *indio_dev)
{
	struct inv_icm42670_state *st = iio_device_get_drvdata(indio_dev);
	int ret;

	mutex_lock(&st->lock);

	/* exit if there are several sensors using the FIFO */
	if (st->fifo.on > 1) {
		ret = 0;
		goto out_off;
	}

	/* set FIFO in bypass mode */
	ret = regmap_write(st->map, INV_ICM42670_FIFO_CONFIG1,
			   INV_ICM42670_FIFO_CONFIG1_FIFO_BYPASS_ON);
	if (ret)
		goto out_unlock;

	/* flush FIFO data */
	ret = regmap_write(st->map, INV_ICM42670_SIGNAL_PATH_RESET,
			   INV_ICM42670_SIGNAL_PATH_RESET_FIFO_FLUSH_EN);
	if (ret)
		goto out_unlock;

	/* disable FIFO threshold interrupt */
	ret = regmap_update_bits(st->map, INV_ICM42670_INT_SOURCE0,
				 INV_ICM42670_INT_SOURCE0_FIFO_THS_INT1_EN_MASK, 0);
	if (ret)
		goto out_unlock;

out_off:
	/* decrease FIFO on counter */
	st->fifo.on--;
out_unlock:
	mutex_unlock(&st->lock);
	return ret;
}

static int inv_icm42670_buffer_postdisable(struct iio_dev *indio_dev)
{
	struct inv_icm42670_state *st = iio_device_get_drvdata(indio_dev);
	struct device *dev = regmap_get_device(st->map);
	unsigned int sensor;
	unsigned int *watermark;
	struct inv_icm42670_timestamp *ts;
	struct inv_icm42670_sensor_conf conf = INV_ICM42670_SENSOR_CONF_INIT;
	unsigned int sleep_temp = 0;
	unsigned int sleep_sensor = 0;
	unsigned int sleep;
	int ret;

	if (indio_dev == st->indio_gyro) {
		sensor = INV_ICM42670_SENSOR_GYRO;
		watermark = &st->fifo.watermark.gyro;
		ts = iio_priv(st->indio_gyro);
	} else if (indio_dev == st->indio_accel) {
		sensor = INV_ICM42670_SENSOR_ACCEL;
		watermark = &st->fifo.watermark.accel;
		ts = iio_priv(st->indio_accel);
	} else {
		return -EINVAL;
	}

	mutex_lock(&st->lock);

	ret = inv_icm42670_buffer_set_fifo_en(st, st->fifo.en & ~sensor);
	if (ret)
		goto out_unlock;

	*watermark = 0;
	ret = inv_icm42670_buffer_update_watermark(st);
	if (ret)
		goto out_unlock;

	conf.mode = INV_ICM42670_SENSOR_MODE_OFF;
	if (sensor == INV_ICM42670_SENSOR_GYRO)
		ret = inv_icm42670_set_gyro_conf(st, &conf, &sleep_sensor);
	else
		ret = inv_icm42670_set_accel_conf(st, &conf, &sleep_sensor);
	if (ret)
		goto out_unlock;

	/* if FIFO is off, turn temperature off */
	if (!st->fifo.on)
		ret = inv_icm42670_set_temp_conf(st, false, &sleep_temp);

	inv_icm42670_timestamp_reset(ts);

out_unlock:
	mutex_unlock(&st->lock);

	/* sleep maximum required time */
	sleep = max(sleep_sensor, sleep_temp);
	if (sleep)
		msleep(sleep);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}

const struct iio_buffer_setup_ops inv_icm42670_buffer_ops = {
	.preenable = inv_icm42670_buffer_preenable,
	.postenable = inv_icm42670_buffer_postenable,
	.predisable = inv_icm42670_buffer_predisable,
	.postdisable = inv_icm42670_buffer_postdisable,
};

int inv_icm42670_buffer_fifo_read(struct inv_icm42670_state *st,
				  unsigned int max)
{
	size_t max_count;
	ssize_t i, size;
	const void *accel, *gyro, *timestamp;
	const int8_t *temp;
	unsigned int odr;
	int ret;

	/* reset all samples counters */
	st->fifo.count = 0;
	st->fifo.nb.gyro = 0;
	st->fifo.nb.accel = 0;
	st->fifo.nb.total = 0;

	/* compute maximum FIFO read size */
	if (max == 0)
		max_count = sizeof(st->fifo.data);
	else
		max_count = max * inv_icm42670_get_packet_size(st);

	/* read FIFO count value */
	ret = regmap_bulk_read(st->map, INV_ICM42670_FIFO_COUNTH,
			       st->buffer, 2);
	if (ret)
		return ret;
	st->fifo.count = get_unaligned_be16(st->buffer);

	/* check and clamp FIFO count value */
	if (st->fifo.count == 0)
		return 0;
	if (st->fifo.count > max_count)
		st->fifo.count = max_count;

	/* read all FIFO data in internal buffer */
	ret = regmap_noinc_read(st->map, INV_ICM42670_FIFO_DATA,
				st->fifo.data, st->fifo.count);
	if (ret)
		return ret;

	/* compute number of samples for each sensor */
	for (i = 0; i < st->fifo.count; i += size) {
		size = inv_icm42670_fifo_decode_packet(&st->fifo.data[i],
				&accel, &gyro, &temp, &timestamp, &odr);
		if (size <= 0)
			break;
		if (gyro != NULL && inv_icm42670_fifo_is_data_valid(gyro))
			st->fifo.nb.gyro++;
		if (accel != NULL && inv_icm42670_fifo_is_data_valid(accel))
			st->fifo.nb.accel++;
		st->fifo.nb.total++;
	}

	return 0;
}

int inv_icm42670_buffer_fifo_parse(struct inv_icm42670_state *st)
{
	struct inv_icm42670_timestamp *ts;
	int ret;

	if (st->fifo.nb.total == 0)
		return 0;

	/* handle gyroscope timestamp and FIFO data parsing */
	ts = iio_priv(st->indio_gyro);
	inv_icm42670_timestamp_interrupt(ts, st->fifo.period, st->fifo.nb.total,
					 st->fifo.nb.gyro, st->timestamp.gyro);
	if (st->fifo.nb.gyro > 0) {
		ret = inv_icm42670_gyro_parse_fifo(st->indio_gyro);
		if (ret)
			return ret;
	}

	/* handle accelerometer timestamp and FIFO data parsing */
	ts = iio_priv(st->indio_accel);
	inv_icm42670_timestamp_interrupt(ts, st->fifo.period, st->fifo.nb.total,
					 st->fifo.nb.accel, st->timestamp.accel);
	if (st->fifo.nb.accel > 0) {
		ret = inv_icm42670_accel_parse_fifo(st->indio_accel);
		if (ret)
			return ret;
	}

	return 0;
}

int inv_icm42670_buffer_hwfifo_flush(struct inv_icm42670_state *st,
				     unsigned int count)
{
	struct inv_icm42670_timestamp *ts;
	int64_t gyro_ts, accel_ts;
	int ret;

	gyro_ts = iio_get_time_ns(st->indio_gyro);
	accel_ts = iio_get_time_ns(st->indio_accel);

	ret = inv_icm42670_buffer_fifo_read(st, count);
	if (ret)
		return ret;

	if (st->fifo.nb.total == 0)
		return 0;

	if (st->fifo.nb.gyro > 0) {
		ts = iio_priv(st->indio_gyro);
		inv_icm42670_timestamp_interrupt(ts, st->fifo.period,
						 st->fifo.nb.total, st->fifo.nb.gyro,
						 gyro_ts);
		ret = inv_icm42670_gyro_parse_fifo(st->indio_gyro);
		if (ret)
			return ret;
	}

	if (st->fifo.nb.accel > 0) {
		ts = iio_priv(st->indio_accel);
		inv_icm42670_timestamp_interrupt(ts, st->fifo.period,
						 st->fifo.nb.total, st->fifo.nb.accel,
						 accel_ts);
		ret = inv_icm42670_accel_parse_fifo(st->indio_accel);
		if (ret)
			return ret;
	}

	return 0;
}

int inv_icm42670_buffer_init(struct inv_icm42670_state *st)
{
	unsigned int val, mask;
	int ret;

	/*
	 * Default FIFO configuration
	 * - FIFO count in bytes
	 * - FIFO count in big endian
	 */
	val = (INV_ICM42670_INTF_CONFIG0_FIFO_COUNT_BIG_ENDIAN | INV_ICM42670_INTF_CONFIG0_FIFO_COUNT_REC_BYTE);
	mask = (INV_ICM42670_INTF_CONFIG0_FIFO_COUNT_ENDIAN_MASK | INV_ICM42670_INTF_CONFIG0_FIFO_COUNT_FORMAT_MASK);
	ret = regmap_update_bits(st->map, INV_ICM42670_INTF_CONFIG0, mask, val);
	if (ret)
		return ret;

	/*
	 * Enable FIFO partial read and continuous watermark interrupt.
	 * Disable all FIFO EN bits.
	 */
	val = (INV_ICM42670_FIFO_CONFIG5_FIFO_RESUME_PARTIAL_RD_EN | INV_ICM42670_FIFO_CONFIG5_WM_GT_TH_EN);
	mask = (INV_ICM42670_FIFO_CONFIG5_FIFO_RESUME_PARTIAL_RD_MASK | INV_ICM42670_FIFO_CONFIG5_FIFO_WM_GT_TH_MASK);
	ret = regmap_update_bits(st->map, INV_ICM42670_FIFO_CONFIG5_MREG1, mask, val);
	if (ret)
		return ret;

	/*
	 * FIFO snapshot mode i.e drop the data when the FIFO overflows
	 * Disable all FIFO EN bits.
	 */
	mask = (INV_ICM42670_FIFO_CONFIG1_FIFO_MODE_MASK | INV_ICM42670_FIFO_CONFIG1_FIFO_BYPASS_MASK);
	val = (INV_ICM42670_FIFO_CONFIG1_FIFO_MODE_SNAPSHOT | INV_ICM42670_FIFO_CONFIG1_FIFO_BYPASS_OFF);
	ret = regmap_update_bits(st->map, INV_ICM42670_FIFO_CONFIG1, mask, val);
	if (ret)
		return ret;

	return ret;
}
