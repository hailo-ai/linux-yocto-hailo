// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 - 2024 Hailo Technologies Ltd.
 * Based on Copyright (C) 2020 InvenSense:
 * - Linux inv_icm42600
 * - Arduino https://github.com/tdk-invn-oss/motion.arduino.ICM42670P.git
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/property.h>

#include "inv_icm42670.h"

#define TIMEOUT_US 1000000 /* 1 sec */

static int inv_icm42670_switch_on_mclk(struct i2c_client *i2c)
{
	int status = 0;
	int data;
	const struct device *dev = &i2c->dev;
	struct inv_icm42670_state *st;

	st = dev_get_drvdata(dev);

	if (i2c == NULL)
		return -EINVAL;

	/* set IDLE bit only if it is not set yet */
	if (st->need_mclk_cnt == 0) {
		uint64_t start;

		data = i2c_smbus_read_byte_data(
			i2c, (uint8_t)INV_ICM42670_PWR_MGMT0);
		if (data < 0)
			return data;
		data |= INV_ICM42670_PWR_MGMT0_IDLE_MASK;
		status |= i2c_smbus_write_byte_data(
			i2c, (uint8_t)INV_ICM42670_PWR_MGMT0, data);

		if (status)
			return status;

		/* Check if MCLK is ready */
		start = ktime_divns(ktime_get_raw_ns(), NSEC_PER_USEC);
		do {
			data = i2c_smbus_read_byte_data(
				i2c, (uint8_t)INV_ICM42670_MCLK_RDY);
			if (data < 0)
				return data;

			/* Timeout */
			if (ktime_divns(ktime_get_raw_ns(), NSEC_PER_USEC) -
				    start >
			    TIMEOUT_US)
				return -ETIMEDOUT;

		} while (!(data & INV_ICM42670_MCLK_RDY_MCLK_RDY_MASK));
	} else {
		/* Make sure it is already on */
		data = i2c_smbus_read_byte_data(
			i2c, (uint8_t)INV_ICM42670_PWR_MGMT0);
		if (data < 0)
			return data;
		if (0 == (data &= INV_ICM42670_PWR_MGMT0_IDLE_MASK))
			status |= -EBUSY;
	}

	/* Increment the counter to keep track of number of MCLK requesters */
	st->need_mclk_cnt++;

	return status;
}

static int inv_icm42670_switch_off_mclk(struct i2c_client *i2c)
{
	int status = 0;
	int data;
	const struct device *dev = &i2c->dev;
	struct inv_icm42670_state *st;

	st = dev_get_drvdata(dev);

	if (i2c == NULL)
		return -EINVAL;

	/* Reset the IDLE but only if there is one requester left */
	if (st->need_mclk_cnt == 1) {
		data = i2c_smbus_read_byte_data(
			i2c, (uint8_t)INV_ICM42670_PWR_MGMT0);

		if (data < 0)
			return data;
		data &= ~INV_ICM42670_PWR_MGMT0_IDLE_MASK;
		status |= i2c_smbus_write_byte_data(
			i2c, (uint8_t)INV_ICM42670_PWR_MGMT0, data);
	} else {
		/* Make sure it is still on */
		data = i2c_smbus_read_byte_data(
			i2c, (uint8_t)INV_ICM42670_PWR_MGMT0);

		if (data < 0)
			return data;
		if (0 == (data &= INV_ICM42670_PWR_MGMT0_IDLE_MASK))
			status |= -EBUSY;
	}

	/* Decrement the counter */
	st->need_mclk_cnt--;

	return status;
}

static int inv_icm42670_write_mclk_reg(struct i2c_client *i2c, u16 regaddr,
				       u8 wr_cnt, const u8 *buf)
{
	uint8_t data, i;
	uint8_t blk_sel = (regaddr & 0xFF00) >> 8;
	int status = 0;

	if (i2c == NULL)
		return -EINVAL;

	/* Have IMU not in IDLE mode to access MCLK domain */
	status |= inv_icm42670_switch_on_mclk(i2c);

	/* optimize by changing BLK_SEL only if not NULL */
	if (blk_sel)
		status |= i2c_smbus_write_byte_data(i2c, (uint8_t)INV_ICM42670_BLK_SEL_W, blk_sel);

	data = (regaddr & 0x00FF);
	status |= i2c_smbus_write_byte_data(i2c, (uint8_t)INV_ICM42670_MADDR_W, data);
	for (i = 0; i < wr_cnt; i++) {
		status |= i2c_smbus_write_byte_data(i2c, (uint8_t)INV_ICM42670_M_W, buf[i]);
		udelay(10);
	}

	if (blk_sel) {
		data = 0;
		status |= i2c_smbus_write_byte_data(i2c, (uint8_t)INV_ICM42670_BLK_SEL_W, data);
	}

	status |= inv_icm42670_switch_off_mclk(i2c);

	return status;
}

static int inv_icm42670_read_mclk_reg(struct i2c_client *i2c, uint16_t regaddr,
				      uint8_t rd_cnt, uint8_t *buf)
{
	uint8_t data;
	uint8_t blk_sel = (regaddr & 0xFF00) >> 8;
	int status = 0;

	if (i2c == NULL)
		return -EINVAL;

	/* Have IMU not in IDLE mode to access MCLK domain */
	status |= inv_icm42670_switch_on_mclk(i2c);

	/* optimize by changing BLK_SEL only if not NULL */
	if (blk_sel)
		status |= i2c_smbus_write_byte_data(i2c, (uint8_t)INV_ICM42670_BLK_SEL_R, blk_sel);

	data = (regaddr & 0x00FF);
	status |= i2c_smbus_write_byte_data(i2c, (uint8_t)INV_ICM42670_MADDR_R, data);
	udelay(10);
	i2c_smbus_read_i2c_block_data(i2c, (uint8_t)INV_ICM42670_M_R, rd_cnt, buf);
	udelay(10);

	if (blk_sel) {
		data = 0;
		status |= i2c_smbus_write_byte_data(i2c, (uint8_t)INV_ICM42670_BLK_SEL_R, blk_sel);
	}

	/* switch OFF MCLK if needed */
	status |= inv_icm42670_switch_off_mclk(i2c);

	return status;
}

static int inv_icm42670_regmap_bus_smbus_i2c_write(void *context,
						   const void *data,
						   size_t count)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	uint16_t regaddr = (((uint8_t *)data)[0] << 8) | (((uint8_t *)data)[1] << 0);
	const uint8_t *val = &((uint8_t *)data)[2];
	enum inv_icm42670_bank bank = (enum inv_icm42670_bank)(regaddr >> 8);

	if (count < 2)
		return -EINVAL;
	count -= 2;

	switch (bank) {
	case BANK0:
		return i2c_smbus_write_i2c_block_data(i2c, (uint8_t)(regaddr & 0xFF), count, val);
	case MREG1:
	case MREG2:
	case MREG3:
		return inv_icm42670_write_mclk_reg(i2c, regaddr, count, val);
	default:
		break;
	}

	return -EFAULT;
}

static int inv_icm42670_regmap_i2c_smbus_i2c_read(void *context,
						  const void *reg_buf,
						  size_t reg_size,
						  void *val_buf,
						  size_t val_size)
{
	struct device *dev = context;
	struct i2c_client *i2c = to_i2c_client(dev);
	uint16_t regaddr = (((uint8_t *)reg_buf)[0] << 8) | (((uint8_t *)reg_buf)[1] << 0);
	uint8_t *val = &((uint8_t *)val_buf)[0];
	enum inv_icm42670_bank bank = (enum inv_icm42670_bank)(regaddr >> 8);
	uint8_t len = val_size;
	int ret = 0;

	if (reg_size != 2 || val_size < 1)
		return -EINVAL;

	switch (bank) {
	case BANK0:
		ret = i2c_smbus_read_i2c_block_data(
			i2c, (uint8_t)(regaddr & 0xFF), len, val);
		if (ret == val_size)
			return 0;
		else if (ret < 0)
			return ret;
		else
			return -EIO;
		break;
	case MREG1:
	case MREG2:
	case MREG3:
		ret = inv_icm42670_read_mclk_reg(i2c, regaddr, len, val);
		break;
	default:
		return -EFAULT;
	}

	return ret;
}

static const struct regmap_bus inv_icm42670_i2c_regmap_bus = {
	.write = inv_icm42670_regmap_bus_smbus_i2c_write,
	.read = inv_icm42670_regmap_i2c_smbus_i2c_read,
	.max_raw_read = I2C_SMBUS_BLOCK_MAX,
	.max_raw_write = I2C_SMBUS_BLOCK_MAX,
};

static int inv_icm42670_i2c_bus_setup(struct inv_icm42670_state *st)
{
	unsigned int mask;
	int ret;

	/* Enable I2C 50ns spike filtering */
	mask = INV_ICM42670_INTF_CONFIG1_I3C_SDR_EN_MASK |
	       INV_ICM42670_INTF_CONFIG1_I3C_DDR_EN_MASK;
	ret = regmap_update_bits(st->map, INV_ICM42670_INTF_CONFIG1, mask, 0);
	if (ret)
		return ret;

	/* set slew rates for I2C and SPI */
	ret = regmap_update_bits(st->map, INV_ICM42670_DRIVE_CONFIG2,
				 INV_ICM42670_DRIVE_CONFIG2_I2C_SLEW_RATE_MASK,
				 INV_ICM42670_DRIVE_CONFIG2_I2C_SLEW_RATE(
					 INV_ICM42670_SLEW_RATE_12_36NS));
	if (ret)
		return ret;

	ret = regmap_update_bits(st->map, INV_ICM42670_DRIVE_CONFIG3,
				 INV_ICM42670_DRIVE_CONFIG3_SPI_SLEW_RATE_MASK,
				 INV_ICM42670_DRIVE_CONFIG3_SPI_SLEW_RATE(
					 INV_ICM42670_SLEW_RATE_12_36NS));
	if (ret)
		return ret;

	return ret;
}

static int inv_icm42670_probe(struct i2c_client *client)
{
	const void *match;
	enum inv_icm42670_chip chip;
	struct regmap *regmap;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -ENOTSUPP;

	match = device_get_match_data(&client->dev);
	if (!match)
		return -EINVAL;
	chip = (enum inv_icm42670_chip)match;

	regmap = devm_regmap_init(&client->dev, &inv_icm42670_i2c_regmap_bus,
				  &client->dev, &inv_icm42670_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return inv_icm42670_core_probe(regmap, chip, client->irq,
				       inv_icm42670_i2c_bus_setup);
}

static const struct of_device_id inv_icm42670_of_matches[] = {
	{
		.compatible = "invensense,icm42670",
		.data = (void *)INV_CHIP_ICM42670,
	},
	{}
};
MODULE_DEVICE_TABLE(of, inv_icm42670_of_matches);

static struct i2c_driver inv_icm42670_driver = {
	.driver = {
		.name = "inv-icm42670-i2c",
		.of_match_table = inv_icm42670_of_matches,
		.pm = &inv_icm42670_pm_ops,
	},
	.probe_new = inv_icm42670_probe,
};
module_i2c_driver(inv_icm42670_driver);

MODULE_AUTHOR("InvenSense, Inc.");
MODULE_DESCRIPTION("InvenSense ICM-42670 I2C driver");
MODULE_LICENSE("GPL");
