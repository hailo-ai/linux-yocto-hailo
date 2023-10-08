// SPDX-License-Identifier: GPL-2.0-only
/* 
 * Driver for Texas Instruments INA226 power monitor chips
 *
 *
 * INA226:
 * Bi-Directional Current/Power Monitor with I2C Interface
 * Datasheet: https://www.ti.com/product/ina226
 *
 * INA230:
 * Bi-directional Current/Power Monitor with I2C Interface
 * Datasheet: https://www.ti.com/product/ina230
 * 
 * INA231:
 * Bi-directional Current/Power Monitor with I2C Interface
 * Datasheet: https://www.ti.com/product/ina231
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/jiffies.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/util_macros.h>
#include <linux/regmap.h>

/* common register definitions */
#define INA2XX_CONFIG 0x00
#define INA2XX_SHUNT_VOLTAGE 0x01 /* readonly */
#define INA2XX_BUS_VOLTAGE 0x02 /* readonly */
#define INA2XX_POWER 0x03 /* readonly */
#define INA2XX_CURRENT 0x04 /* readonly */
#define INA2XX_CALIBRATION 0x05

/* INA226 register definitions */
#define INA226_MASK_ENABLE 0x06
#define INA226_ALERT_LIMIT 0x07

/* register count */
#define INA226_REGISTERS 8

#define INA2XX_MAX_DELAY 69 /* worst case delay in ms */

#define INA2XX_RSHUNT_uOHMS_DEFAULT 10000
#define INA2XX_MAX_CURRENT_mA_DEFAULT 8000

/*
 * SHUNT_VOLTAGE_COEFFIECIENT in floating point is: 0.0025
 * INA2XX_SHUNT_VOLTAGE_COEFFIECIENT / INA2XX_SHUNT_VOLTAGE_COEFFIECIENT_SCALE = 0.0025
 */
#define INA2XX_SHUNT_VOLTAGE_COEFFIECIENT (25)
#define INA2XX_SHUNT_VOLTAGE_COEFFIECIENT_SCALE (10000)

/*
 * BUS_VOLTAGE_COEFFIECIENT in floating point is: 1.25
 * INA2XX_BUS_VOLTAGE_COEFFIECIENT / INA2XX_BUS_VOLTAGE_COEFFIECIENT_SCALE = 1.25
 */
#define INA2XX_BUS_VOLTAGE_COEFFIECIENT (125)
#define INA2XX_BUS_VOLTAGE_COEFFIECIENT_SCALE (100)

#define INA2XX_POWER_COEFFIECIENT (25)

/* bit mask for reading the averaging setting in the configuration register */
#define INA226_AVG_RD_MASK (0x0E00)
#define INA226_BUS_CONVERSION_TIME_MASK (0x01C0)
#define INA226_SHUNT_CONVERSION_TIME_MASK (0x0038)
#define INA226_MODE_MASK (0x0007)

#define INA226_CONFIGURATION_MODE_DEFAULT (0x07)

/* Shunt Voltage Conversion Time in Configuration Register. Sets the conversion time for the shunt voltage measurement. */
#define INA226_SHIFT_SHUNT_CONV_TIME(val) ((val) << 3)
/* Bus Voltage Conversion Time in Configuration Register. Sets the conversion time for the bus voltage measurement. */
#define INA226_SHIFT_BUS_CONV_TIME(val) ((val) << 6)

#define INA226_READ_AVG(reg) (((reg)&INA226_AVG_RD_MASK) >> 9)
#define INA226_SHIFT_AVG(val) ((val) << 9)

/* bit number of alert functions in Mask/Enable Register */
#define INA226_SHUNT_OVER_VOLTAGE_BIT 15
#define INA226_SHUNT_UNDER_VOLTAGE_BIT 14
#define INA226_BUS_OVER_VOLTAGE_BIT 13
#define INA226_BUS_UNDER_VOLTAGE_BIT 12
#define INA226_POWER_OVER_LIMIT_BIT 11

/* bit mask for alert config bits of Mask/Enable Register */
#define INA226_ALERT_CONFIG_MASK 0xFC00
#define INA226_ALERT_FUNCTION_FLAG BIT(4)

/* common attrs, ina226 attrs and NULL */
#define INA2XX_MAX_ATTRIBUTE_GROUPS 3

/*
 * Both bus voltage and shunt voltage conversion times for ina226 are set
 * to 0b0111 on POR, which translates to (8244*2) = 16488 microseconds in total.
 * The user can set the update_interval	(The interval at which the chip will update readings).
 * The sensor samples the power every sampling_period {ms} and averages every
 * averaging_factor samples. The sensor provides a new value every: update_interval = 2 * averaging_factor  * sampling_period{ms}.
 * In case the user update the update_interval, the driver find the closest averaging_factor to fit it.
 * The sampling_period stay in is default values and didn't changed.
 */
#define INA226_TOTAL_CONV_TIME_DEFAULT (16488)
#define INA226_TOTAL_CONV_TIME_REG_VALUE_DEFAULT (7)

#define INA226_AVERAGING_FACTOR_DEFAULT (16)

/* Largest calibration value for a 15-bit valid range */
#define INA2XX_CALIBRATION_MAX 0x7FFF

#define INA2XX_LSB_FACTOR (2 << (15 - 1))

/*
 * CAL_COEFFIECIENT in floating point is: 0.00512
 * CAL_COEFFIECIENT = INA2XX_CAL_COEFFIECIENT / INA2XX_CAL_COEFFIECIENT_SCALE = 0.00512
 */
#define INA2XX_CAL_COEFFIECIENT (512)
#define INA2XX_CAL_COEFFIECIENT_SCALE (100000) // 10^5

static struct regmap_config ina2xx_precise_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
};

struct ina2xx_precise_config {
	int registers;
	int shunt_div;
	int bus_div;
	int power_coff;
};

struct ina2xx_precise_data {
	const struct ina2xx_precise_config *config;

	long shunt_uohms;
	long max_current_mA;
	long calibration_factor;
	long current_LSB_scaled;

	struct mutex config_lock;
	struct regmap *regmap;

	const struct attribute_group *groups[INA2XX_MAX_ATTRIBUTE_GROUPS];
};

static const struct ina2xx_precise_config ina2xx_precise_config = {
	.registers = INA226_REGISTERS,
	.shunt_div = INA2XX_SHUNT_VOLTAGE_COEFFIECIENT,
	.bus_div = INA2XX_BUS_VOLTAGE_COEFFIECIENT,
	.power_coff = INA2XX_POWER_COEFFIECIENT,
};

/*
 * Available averaging rates for ina226. The indices correspond with
 * the bit values expected by the chip (according to the ina226 datasheet,
 * table 3 AVG bit settings) found at
 * https://www.ti.com/lit/ds/symlink/ina226.pdf.
 */
static const int ina226_avg_tab[] = { 1, 4, 16, 64, 128, 256, 512, 1024 };

static int ina226_reg_to_interval(u16 config)
{
	int avg = ina226_avg_tab[INA226_READ_AVG(config)];

	/*
	 * Multiply the total conversion time by the number of averages.
	 * Return the result in milliseconds.
	 */
	return DIV_ROUND_CLOSEST(avg * INA226_TOTAL_CONV_TIME_DEFAULT, 1000);
}

/*
 * Return the new, shifted AVG field value of CONFIG register,
 * to use with regmap_update_bits
 */
static u16 ina226_interval_to_reg(int interval)
{
	int avg, avg_bits;

	avg = DIV_ROUND_CLOSEST(interval * 1000,
				INA226_TOTAL_CONV_TIME_DEFAULT);
	avg_bits =
		find_closest(avg, ina226_avg_tab, ARRAY_SIZE(ina226_avg_tab));

	return INA226_SHIFT_AVG(avg_bits);
}

/*
 * Calculate calibration value and configure it
 *
 */
static int ina2xx_precise_calibrate(struct ina2xx_precise_data *data)
{
	return regmap_write(data->regmap, INA2XX_CALIBRATION,
			    data->calibration_factor);
}

static int ina226_conversion_time_write(struct ina2xx_precise_data *data)
{
	int status;

	status = regmap_update_bits(
		data->regmap, INA2XX_CONFIG, INA226_BUS_CONVERSION_TIME_MASK,
		INA226_SHIFT_BUS_CONV_TIME(
			INA226_TOTAL_CONV_TIME_REG_VALUE_DEFAULT));
	if (status < 0)
		return status;

	status = regmap_update_bits(
		data->regmap, INA2XX_CONFIG, INA226_SHUNT_CONVERSION_TIME_MASK,
		INA226_SHIFT_SHUNT_CONV_TIME(
			INA226_TOTAL_CONV_TIME_REG_VALUE_DEFAULT));

	return status;
}

static int ina226_averaging_factor_write(struct ina2xx_precise_data *data)
{
	int status, avg_bits;

	avg_bits = find_closest(INA226_AVERAGING_FACTOR_DEFAULT, ina226_avg_tab,
				ARRAY_SIZE(ina226_avg_tab));

	status = regmap_update_bits(data->regmap, INA2XX_CONFIG,
				    INA226_AVG_RD_MASK,
				    INA226_SHIFT_AVG(avg_bits));

	return status;
}

static int ina226_mode_write(struct ina2xx_precise_data *data)
{
	int status;

	status = regmap_update_bits(data->regmap, INA2XX_CONFIG,
				    INA226_MODE_MASK,
				    INA226_CONFIGURATION_MODE_DEFAULT);

	return status;
}

static int ina2xx_precise_init(struct ina2xx_precise_data *data)
{
	int ret;
	long current_LSB_before_rounded_scaled;
	long shunt_mohms = data->shunt_uohms / 1000;

	mutex_lock(&data->config_lock);

	/*
	 * According to the Equation 2 in the datasheet.
	 * current_LSB_before_rounded scaled by 10^8 to the more precision.
	 * max_current_Amps = data->max_current_mA / 10^3;
	 * current_LSB_before_rounded = (max_current_Amps / INA2XX_LSB_FACTOR)
	 * current_LSB_before_rounded_scaled = current_LSB_before_rounded * 10^8
	 * current_LSB_before_rounded_scaled = (max_current_Amps / INA2XX_LSB_FACTOR) * 10^8
	 * current_LSB_before_rounded_scaled = (max_current_Amps * 10^8) / INA2XX_LSB_FACTOR
	 * current_LSB_before_rounded_scaled = ((data->max_current_mA / 10^3) * 10^8) / INA2XX_LSB_FACTOR
	 * current_LSB_before_rounded_scaled = (data->max_current_mA * 10^5) / INA2XX_LSB_FACTOR
	 */
	current_LSB_before_rounded_scaled =
		(data->max_current_mA * 100000) / INA2XX_LSB_FACTOR;

	/*
	 * According to the Equation 1 in the datasheet.
	 * Note: result of calculation is truncated according to datasheet.
	 * shunt_ohms = data->shunt_mohms / 10^3;
	 * calibration_factor = CAL_COEFFIECIENT / (current_LSB_before_rounded * shunt_ohms)
	 * calibration_factor = CAL_COEFFIECIENT / (current_LSB_before_rounded * (data->shunt_mohms / 10^3))
	 * calibration_factor = (INA2XX_CAL_COEFFIECIENT / INA2XX_CAL_COEFFIECIENT_SCALE) / (current_LSB_before_rounded * (data->shunt_mohms / 10^3))
	 * calibration_factor = (INA2XX_CAL_COEFFIECIENT / 10^5) / ((current_LSB_before_rounded_scaled / 10^8) * (data->shunt_mohms / 10^3))
	 * calibration_factor = (INA2XX_CAL_COEFFIECIENT * 10^6) / (current_LSB_before_rounded_scaled * data->shunt_mohms)
	 */
	data->calibration_factor = DIV_ROUND_UP_ULL(
		(INA2XX_CAL_COEFFIECIENT * 1000000),
		(current_LSB_before_rounded_scaled * shunt_mohms));

	if (data->calibration_factor > INA2XX_CALIBRATION_MAX) {
		data->calibration_factor = INA2XX_CALIBRATION_MAX;
	}

	/*
	 * current_LSB scaled by 10^8 to the more precision.
	 * shunt_ohms = data->shunt_mohms / 10^3;
	 * current_LSB_scaled = current_LSB * 10^8
	 * current_LSB = CAL_COEFFIECIENT / (calibration_factor * shunt_ohms)
	 * current_LSB = (INA2XX_CAL_COEFFIECIENT / INA2XX_CAL_COEFFIECIENT_SCALE) / (calibration_factor * (data->shunt_mohms / 10^3))
	 * current_LSB = (INA2XX_CAL_COEFFIECIENT / 10^5) / (calibration_factor * (data->shunt_mohms / 10^3))
	 * current_LSB = (INA2XX_CAL_COEFFIECIENT / 10^2) / (calibration_factor * data->shunt_mohms)
	 * current_LSB_scaled = (INA2XX_CAL_COEFFIECIENT / 10^2) / (calibration_factor * data->shunt_mohms) * 10^8
	 * current_LSB_scaled = (INA2XX_CAL_COEFFIECIENT * 10^6) / (calibration_factor * data->shunt_mohms)
	 */
	data->current_LSB_scaled = (INA2XX_CAL_COEFFIECIENT * 1000000) /
				   (data->calibration_factor * shunt_mohms);

	ret = ina2xx_precise_calibrate(data);
	if (ret < 0)
		return ret;

	ret = ina226_conversion_time_write(data);
	if (ret < 0)
		return ret;

	ret = ina226_averaging_factor_write(data);
	if (ret < 0)
		return ret;

	ret = ina226_mode_write(data);
	if (ret < 0)
		return ret;

	mutex_unlock(&data->config_lock);

	return ret;
}

static int ina2xx_precise_read_reg(struct device *dev, int reg,
				   unsigned int *regval)
{
	struct ina2xx_precise_data *data = dev_get_drvdata(dev);
	int ret, retry;

	dev_dbg(dev, "Starting register %d read\n", reg);

	for (retry = 5; retry; retry--) {
		ret = regmap_read(data->regmap, reg, regval);
		if (ret < 0)
			return ret;

		dev_dbg(dev, "read %d, val = 0x%04x\n", reg, *regval);

		/*
		 * If the current value in the calibration register is 0, the
		 * power and current registers will also remain at 0. In case
		 * the chip has been reset let's check the calibration
		 * register and reinitialize if needed.
		 * We do that extra read of the calibration register if there
		 * is some hint of a chip reset.
		 */
		if (*regval == 0) {
			unsigned int cal;

			ret = regmap_read(data->regmap, INA2XX_CALIBRATION,
					  &cal);
			if (ret < 0)
				return ret;

			if (cal == 0) {
				dev_warn(
					dev,
					"chip not calibrated, reinitializing\n");

				ret = ina2xx_precise_init(data);
				if (ret < 0)
					return ret;
				/*
				 * Let's make sure the power and current
				 * registers have been updated before trying
				 * again.
				 */
				msleep(INA2XX_MAX_DELAY);
				continue;
			}
		}
		return 0;
	}

	/*
	 * If we're here then although all write operations succeeded, the
	 * chip still returns 0 in the calibration register. Nothing more we
	 * can do here.
	 */
	dev_err(dev, "unable to reinitialize the chip\n");
	return -ENODEV;
}

static int ina2xx_precise_get_value(struct ina2xx_precise_data *data, u8 reg,
				    unsigned int regval)
{
	int val;

	switch (reg) {
	case INA2XX_SHUNT_VOLTAGE:
		/*
		 * Unit: millivolt
		 * According to the Table 8-1 in the datasheet.
		 * shunt_voltage_value = regval * shunt_voltage_step
		 * shunt_voltage_step = 2.5 uV = 2.5 * 10^(-6) V = 0.0025 mV
		 * shunt_voltage_step = INA2XX_SHUNT_VOLTAGE_COEFFIECIENT / INA2XX_SHUNT_VOLTAGE_COEFFIECIENT_SCALE
		 * shunt_div = INA2XX_SHUNT_VOLTAGE_COEFFIECIENT
		 */
		val = (s16)regval * data->config->shunt_div;
		val = DIV_ROUND_CLOSEST(
			val, INA2XX_SHUNT_VOLTAGE_COEFFIECIENT_SCALE);
		break;
	case INA2XX_BUS_VOLTAGE:
		/*
		 * Unit: millivolt
		 * According to the Table 8-1 in the datasheet.
		 * bus_voltage_value = regval * bus_voltage_step
		 * bus_voltage_step = 1.25 mV = 1.25 * 10^(-3) V = 0.125 V
		 * bus_voltage_step = INA2XX_BUS_VOLTAGE_COEFFIECIENT / INA2XX_BUS_VOLTAGE_COEFFIECIENT_SCALE
		 * bus_div = INA2XX_BUS_VOLTAGE_COEFFIECIENT
		 */
		val = regval * data->config->bus_div;
		val = DIV_ROUND_CLOSEST(val,
					INA2XX_BUS_VOLTAGE_COEFFIECIENT_SCALE);
		break;
	case INA2XX_POWER:
		/*
		 * Unit: microWatt
		 * According the datasheet at p.25:
		 * The Power register LSB is internally programmed to equal 25 times the programmed value of the Current_LSB.
		 * power_value = regval * power_step
		 * power_step = (25 * current_LSB) mW
		 * power_step = (25 * current_LSB * 10^6) mW
		 * power_step = (25 * (current_LSB_scaled / 10^8) * 10^6) mW
		 * power_step = ((25 * current_LSB_scaled) / 10^2) mW
		 * power_coff = INA2XX_POWER_COEFFIECIENT = 25
		 */
		val = regval * data->config->power_coff *
		      data->current_LSB_scaled;
		val = DIV_ROUND_CLOSEST(val, 100);
		break;
	case INA2XX_CURRENT:
		/*
		 * Unit: milliampere
		 * According to the Table 8-1 in the datasheet.
		 * current_value = regval * current_step
		 * current_step = current_LSB uA
		 * current_step = (current_LSB * 10^3) mA
		 * current_step = ((current_LSB_scaled / 10^8) * 10^3) mA
		 * current_step = (current_LSB_scaled / 10^5) mA
		 */
		val = (s16)regval * data->current_LSB_scaled;
		val = DIV_ROUND_CLOSEST(val, 100000);
		break;
	case INA2XX_CALIBRATION:
		val = regval;
		break;
	default:
		/* programmer goofed */
		WARN_ON_ONCE(1);
		val = 0;
		break;
	}

	return val;
}

static ssize_t ina2xx_precise_value_show(struct device *dev,
					 struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct ina2xx_precise_data *data = dev_get_drvdata(dev);
	unsigned int regval;

	int err = ina2xx_precise_read_reg(dev, attr->index, &regval);

	if (err < 0)
		return err;

	return sysfs_emit(buf, "%d\n",
			  ina2xx_precise_get_value(data, attr->index, regval));
}

static int ina226_reg_to_alert(struct ina2xx_precise_data *data, u8 bit,
			       u16 regval)
{
	int reg;

	switch (bit) {
	case INA226_SHUNT_OVER_VOLTAGE_BIT:
	case INA226_SHUNT_UNDER_VOLTAGE_BIT:
		reg = INA2XX_SHUNT_VOLTAGE;
		break;
	case INA226_BUS_OVER_VOLTAGE_BIT:
	case INA226_BUS_UNDER_VOLTAGE_BIT:
		reg = INA2XX_BUS_VOLTAGE;
		break;
	case INA226_POWER_OVER_LIMIT_BIT:
		reg = INA2XX_POWER;
		break;
	default:
		/* programmer goofed */
		WARN_ON_ONCE(1);
		return 0;
	}

	return ina2xx_precise_get_value(data, reg, regval);
}

/*
 * Turns alert limit values into register values.
 * Opposite of the formula in ina2xx_precise_get_value().
 */
static s16 ina226_alert_to_reg(struct ina2xx_precise_data *data, u8 bit,
			       int val)
{
	switch (bit) {
	case INA226_SHUNT_OVER_VOLTAGE_BIT:
	case INA226_SHUNT_UNDER_VOLTAGE_BIT:
		val = DIV_ROUND_CLOSEST(
			val * INA2XX_SHUNT_VOLTAGE_COEFFIECIENT_SCALE,
			data->config->shunt_div);
		return clamp_val(val, SHRT_MIN, SHRT_MAX);
	case INA226_BUS_OVER_VOLTAGE_BIT:
	case INA226_BUS_UNDER_VOLTAGE_BIT:
		val = DIV_ROUND_CLOSEST(
			val * INA2XX_BUS_VOLTAGE_COEFFIECIENT_SCALE,
			data->config->bus_div);
		return clamp_val(val, 0, SHRT_MAX);
	case INA226_POWER_OVER_LIMIT_BIT:
		val = DIV_ROUND_CLOSEST(val * 100,
					data->config->power_coff *
						data->current_LSB_scaled);
		return clamp_val(val, 0, USHRT_MAX);
	default:
		/* programmer goofed */
		WARN_ON_ONCE(1);
		return 0;
	}
}

static ssize_t ina226_alert_show(struct device *dev,
				 struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct ina2xx_precise_data *data = dev_get_drvdata(dev);
	int regval;
	int val = 0;
	int ret;

	mutex_lock(&data->config_lock);
	ret = regmap_read(data->regmap, INA226_MASK_ENABLE, &regval);
	if (ret)
		goto abort;

	if (regval & BIT(attr->index)) {
		ret = regmap_read(data->regmap, INA226_ALERT_LIMIT, &regval);
		if (ret)
			goto abort;
		val = ina226_reg_to_alert(data, attr->index, regval);
	}

	ret = sysfs_emit(buf, "%d\n", val);
abort:
	mutex_unlock(&data->config_lock);
	return ret;
}

static ssize_t ina226_alert_store(struct device *dev,
				  struct device_attribute *da, const char *buf,
				  size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct ina2xx_precise_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	/*
	 * Clear all alerts first to avoid accidentally triggering ALERT pin
	 * due to register write sequence. Then, only enable the alert
	 * if the value is non-zero.
	 */
	mutex_lock(&data->config_lock);
	ret = regmap_update_bits(data->regmap, INA226_MASK_ENABLE,
				 INA226_ALERT_CONFIG_MASK, 0);
	if (ret < 0)
		goto abort;

	ret = regmap_write(data->regmap, INA226_ALERT_LIMIT,
			   ina226_alert_to_reg(data, attr->index, val));
	if (ret < 0)
		goto abort;

	if (val != 0) {
		ret = regmap_update_bits(data->regmap, INA226_MASK_ENABLE,
					 INA226_ALERT_CONFIG_MASK,
					 BIT(attr->index));
		if (ret < 0)
			goto abort;
	}

	ret = count;
abort:
	mutex_unlock(&data->config_lock);
	return ret;
}

static ssize_t ina226_alarm_show(struct device *dev,
				 struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct ina2xx_precise_data *data = dev_get_drvdata(dev);
	int regval;
	int alarm = 0;
	int ret;

	ret = regmap_read(data->regmap, INA226_MASK_ENABLE, &regval);
	if (ret)
		return ret;

	alarm = (regval & BIT(attr->index)) &&
		(regval & INA226_ALERT_FUNCTION_FLAG);

	return sysfs_emit(buf, "%d\n", alarm);
}

static ssize_t ina2xx_precise_shunt_show(struct device *dev,
					 struct device_attribute *da, char *buf)
{
	struct ina2xx_precise_data *data = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%li\n", data->shunt_uohms);
}

static ssize_t ina2xx_precise_max_current_show(struct device *dev,
					       struct device_attribute *da,
					       char *buf)
{
	struct ina2xx_precise_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lu\n", data->max_current_mA);
}

static ssize_t ina226_interval_store(struct device *dev,
				     struct device_attribute *da,
				     const char *buf, size_t count)
{
	struct ina2xx_precise_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int status;

	status = kstrtoul(buf, 10, &val);
	if (status < 0)
		return status;

	if (val > INT_MAX || val == 0)
		return -EINVAL;

	status = regmap_update_bits(data->regmap, INA2XX_CONFIG,
				    INA226_AVG_RD_MASK,
				    ina226_interval_to_reg(val));
	if (status < 0)
		return status;

	return count;
}

static ssize_t ina226_interval_show(struct device *dev,
				    struct device_attribute *da, char *buf)
{
	struct ina2xx_precise_data *data = dev_get_drvdata(dev);
	int status;
	unsigned int regval;

	status = regmap_read(data->regmap, INA2XX_CONFIG, &regval);
	if (status)
		return status;

	return sysfs_emit(buf, "%d\n", ina226_reg_to_interval(regval));
}

/* shunt voltage */
static SENSOR_DEVICE_ATTR_RO(in0_input, ina2xx_precise_value,
			     INA2XX_SHUNT_VOLTAGE);
/* shunt voltage over/under voltage alert setting and alarm */
static SENSOR_DEVICE_ATTR_RW(in0_crit, ina226_alert,
			     INA226_SHUNT_OVER_VOLTAGE_BIT);
static SENSOR_DEVICE_ATTR_RW(in0_lcrit, ina226_alert,
			     INA226_SHUNT_UNDER_VOLTAGE_BIT);
static SENSOR_DEVICE_ATTR_RO(in0_crit_alarm, ina226_alarm,
			     INA226_SHUNT_OVER_VOLTAGE_BIT);
static SENSOR_DEVICE_ATTR_RO(in0_lcrit_alarm, ina226_alarm,
			     INA226_SHUNT_UNDER_VOLTAGE_BIT);

/* bus voltage */
static SENSOR_DEVICE_ATTR_RO(in1_input, ina2xx_precise_value,
			     INA2XX_BUS_VOLTAGE);
/* bus voltage over/under voltage alert setting and alarm */
static SENSOR_DEVICE_ATTR_RW(in1_crit, ina226_alert,
			     INA226_BUS_OVER_VOLTAGE_BIT);
static SENSOR_DEVICE_ATTR_RW(in1_lcrit, ina226_alert,
			     INA226_BUS_UNDER_VOLTAGE_BIT);
static SENSOR_DEVICE_ATTR_RO(in1_crit_alarm, ina226_alarm,
			     INA226_BUS_OVER_VOLTAGE_BIT);
static SENSOR_DEVICE_ATTR_RO(in1_lcrit_alarm, ina226_alarm,
			     INA226_BUS_UNDER_VOLTAGE_BIT);

/* calculated current */
static SENSOR_DEVICE_ATTR_RO(curr1_input, ina2xx_precise_value, INA2XX_CURRENT);

/* calculated power */
static SENSOR_DEVICE_ATTR_RO(power1_input, ina2xx_precise_value, INA2XX_POWER);
/* over-limit power alert setting and alarm */
static SENSOR_DEVICE_ATTR_RW(power1_crit, ina226_alert,
			     INA226_POWER_OVER_LIMIT_BIT);
static SENSOR_DEVICE_ATTR_RO(power1_crit_alarm, ina226_alarm,
			     INA226_POWER_OVER_LIMIT_BIT);

/* shunt resistance */
static SENSOR_DEVICE_ATTR_RO(shunt_resistor, ina2xx_precise_shunt,
			     INA2XX_CALIBRATION);

static SENSOR_DEVICE_ATTR_RW(update_interval, ina226_interval, 0);

/* max_current_mA */
static SENSOR_DEVICE_ATTR_RO(max_current_mA, ina2xx_precise_max_current, 0);

/* pointers to created device attributes */
static struct attribute *ina2xx_precise_attrs[] = {
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_curr1_input.dev_attr.attr,
	&sensor_dev_attr_power1_input.dev_attr.attr,
	&sensor_dev_attr_shunt_resistor.dev_attr.attr,
	&sensor_dev_attr_max_current_mA.dev_attr.attr,
	&sensor_dev_attr_in0_crit.dev_attr.attr,
	&sensor_dev_attr_in0_lcrit.dev_attr.attr,
	&sensor_dev_attr_in0_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_in0_lcrit_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_crit.dev_attr.attr,
	&sensor_dev_attr_in1_lcrit.dev_attr.attr,
	&sensor_dev_attr_in1_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_lcrit_alarm.dev_attr.attr,
	&sensor_dev_attr_power1_crit.dev_attr.attr,
	&sensor_dev_attr_power1_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_update_interval.dev_attr.attr,
	NULL,
};

static const struct attribute_group ina2xx_precise_group = {
	.attrs = ina2xx_precise_attrs,
};

static const struct i2c_device_id ina2xx_precise_id[];

static int ina2xx_precise_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ina2xx_precise_data *data;
	struct device *hwmon_dev;
	u32 val;
	int ret, group = 0;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* set the device type */
	data->config = &ina2xx_precise_config;
	mutex_init(&data->config_lock);

	if (of_property_read_u32(dev->of_node, "shunt-resistor", &val)) {
		val = INA2XX_RSHUNT_uOHMS_DEFAULT;
	}

	data->shunt_uohms = val;

	if (of_property_read_u32(dev->of_node, "max-current", &val)) {
		val = INA2XX_MAX_CURRENT_mA_DEFAULT;
	}

	data->max_current_mA = val;

	ina2xx_precise_regmap_config.max_register = data->config->registers;

	data->regmap =
		devm_regmap_init_i2c(client, &ina2xx_precise_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(dev, "failed to allocate register map\n");
		return PTR_ERR(data->regmap);
	}

	ret = ina2xx_precise_init(data);
	if (ret < 0) {
		dev_err(dev, "error configuring the device: %d\n", ret);
		return -ENODEV;
	}

	data->groups[group++] = &ina2xx_precise_group;

	hwmon_dev = devm_hwmon_device_register_with_groups(dev, client->name,
							   data, data->groups);
	if (IS_ERR(hwmon_dev))
		return PTR_ERR(hwmon_dev);

	dev_info(
		dev,
		"power monitor %s (shunt_uohms = %li uOhm, max_current_mA = %li mA) \n",
		client->name, data->shunt_uohms, data->max_current_mA);

	return 0;
}

static const struct i2c_device_id ina2xx_precise_id[] = { { "ina226", 0 },
							  { "ina230", 0 },
							  { "ina231", 0 },
							  {} };
MODULE_DEVICE_TABLE(i2c, ina2xx_precise_id);

static const struct of_device_id __maybe_unused ina2xx_precise_of_match[] = {
	{ .compatible = "ti,ina226_precise" },
	{ .compatible = "ti,ina230_precise" },
	{ .compatible = "ti,ina231_precise" },
	{},
};
MODULE_DEVICE_TABLE(of, ina2xx_precise_of_match);

static struct i2c_driver ina2xx_precise_driver = {
	.driver = {
		.name	= "ina2xx_precise",
		.of_match_table = of_match_ptr(ina2xx_precise_of_match),
	},
	.probe_new	= ina2xx_precise_probe,
	.id_table	= ina2xx_precise_id,
};

module_i2c_driver(ina2xx_precise_driver);

MODULE_DESCRIPTION("ina2xx precise driver");
MODULE_ALIAS("platform:ina2xx_precise");
MODULE_LICENSE("GPL");
