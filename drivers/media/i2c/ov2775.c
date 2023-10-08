/*
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2018 NXP
 * Copyright (c) 2020 VeriSilicon Holdings Co., Ltd.
 */
/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_graph.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/v4l2-mediabus.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>

#include <asm/unaligned.h>

#include "ov2775_regs.h"

#define PLATFORM_HAILO15_FPGA

#define OV2775_VOLTAGE_ANALOG                   2800000
#define OV2775_VOLTAGE_DIGITAL_CORE             1500000
#define OV2775_VOLTAGE_DIGITAL_IO               1800000

#define OV2775_XCLK_MIN                         6000000
#define OV2775_XCLK_MAX                         24000000

#define OV2775_CHIP_ID_HIGH_BYTE                0x300A
#define OV2775_CHIP_ID_LOW_BYTE                 0x300B

#define OV2775_REG_ISP_DP_CONF                  0x3250
#define OV2775_REG_PRE_CTRL0                    0x3253

#define OV2775_ISP_DP_CONF_ISP_EN               0x1
#define OV2775_PRE_CTRL0_TEST_PATTERN_MASK      0x3
#define OV2775_PRE_CTRL_TEST_PATTERN_BAR        0x80

#define OV2775_TEST_PATTERN_MIN                 0x0
#define OV2775_TEST_PATTERN_MAX                 0xFF
#define OV2775_TEST_PATTERN_STEP                0x1

#define OV2775_CHIP_ID                          0x2770

#define OV2775_SENS_PAD_SOURCE                  0
#define OV2775_SENS_PADS_NUM                    1

#ifndef PLATFORM_HAILO15_FPGA
#define OV2775_SUPPORT_MAX_MODE_COUNT           3
#else
#define OV2775_SUPPORT_MAX_MODE_COUNT           1
#endif /*PLATFORM_HAILO15_FPGA*/

#define OV2775_CAP_BUS_INFO_I2C_ADAPTER_NR_POS  8

struct ov2775_datafmt {
	u32 code;
	enum v4l2_colorspace colorspace;
};

struct ov2775_capture_properties {
	__u64 max_lane_frequency;
	__u64 max_pixel_frequency;
	__u64 max_data_rate;

};

typedef struct ov2775_ae_info_s {
	uint32_t DefaultFrameLengthLines;
	uint32_t CurFrameLengthLines;
	uint32_t one_line_exp_time_ns;
	uint32_t max_integration_time;
	uint32_t min_integration_time;
	uint32_t integration_accuracy;
	uint32_t max_gain;
	uint32_t min_gain;
	uint32_t gain_accuracy;
	uint32_t cur_fps;
	uint32_t hdr_radio;
} ov2775_ae_info_t;

typedef struct sensor_expand_curve_s
{
	uint32_t x_bit;
	uint32_t y_bit;
	uint8_t expand_px[64];
	uint32_t expand_x_data[65];
	uint32_t expand_y_data[65];
}sensor_expand_curve_t;

typedef struct sensor_data_compress_s
{
	uint32_t enable;
	uint32_t x_bit;
	uint32_t y_bit;
}sensor_data_compress_t;

typedef struct sensor_blc_s
{
	uint32_t red;
	uint32_t gr;
	uint32_t gb;
	uint32_t blue;
}sensor_blc_t;

typedef struct sensor_white_balance_s
{
	uint32_t     r_gain;
	uint32_t     gr_gain;
	uint32_t     gb_gain;
	uint32_t     b_gain;
}sensor_white_balance_t;

typedef struct ov2775_mode_info {
	uint32_t index;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t hdr_mode;
	uint32_t stitching_mode;
	uint32_t bit_width;
	sensor_data_compress_t data_compress;
	ov2775_ae_info_t ae_info;
	void *preg_data;
	uint32_t reg_data_count;

} ov2775_mode_info_t;

typedef struct ov2775_mode_infoarray {
	uint32_t count;
	struct ov2775_mode_info modes[OV2775_SUPPORT_MAX_MODE_COUNT];
} ov2775_mode_infoarray_t;

struct ov2775 {
	struct regulator *io_regulator;
	struct regulator *core_regulator;
	struct regulator *analog_regulator;
	struct v4l2_subdev subdev;
	struct v4l2_device *v4l2_dev;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	const struct ov2775_datafmt *fmt;
	struct v4l2_captureparm streamcap;
	struct media_pad pads[OV2775_SENS_PADS_NUM];
	bool on;

	/* controls */
    struct v4l2_ctrl_handler ctrl_handler;
    struct v4l2_ctrl *test_pattern;

	u32 mclk;
	u32 sclk;
	struct clk *sensor_clk;
	int csi;

	void (*io_init) (struct ov2775 *);
	int pwn_gpio, rst_gpio;
	int hdr;
	int fps;
	ov2775_mode_info_t cur_mode;
	sensor_blc_t blc;
	sensor_white_balance_t wb;
	struct mutex lock;
	struct ov2775_capture_properties ocp;
};

#define client_to_ov2775(client)\
	container_of(i2c_get_clientdata(client), struct ov2775, subdev)
#ifdef OV2775_PRIV_IOCTL_SUPPORT
long ov2775_priv_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg);
#endif /*OV2775_PRIV_IOCTL_SUPPORT*/
static void ov2775_stop(struct ov2775 *sensor);
s32 ov2775_write_reg(struct ov2775 *sensor, u16 reg, u8 val);
static int ov2775_set_ctrl(struct v4l2_ctrl *ctrl);

enum sensor_hdr_mode_e
{
	SENSOR_MODE_LINEAR ,
	SENSOR_MODE_HDR_STITCH,
	SENSOR_MODE_HDR_NATIVE,
};

static struct ov2775_mode_info ov2775_mode_info[] = {
#ifndef PLATFORM_HAILO15_FPGA
	{
		.index     = 0,
		.width     = 1920,
		.height    = 1080,
		.fps       = 30,
		.hdr_mode  = SENSOR_MODE_LINEAR,
		.bit_width = 12,
		.data_compress.enable = 0,
		.ae_info = {
			.DefaultFrameLengthLines = 0x466,
			.one_line_exp_time_ns = 29625,
			.max_integration_time = 0x466 - 2,
			.min_integration_time = 1,
			.gain_accuracy = 1024,
			.max_gain = 21 * 1024,
			.min_gain = 1 * 1024,
		},
		.preg_data = ov2775_init_setting_1080p,
		.reg_data_count = ARRAY_SIZE(ov2775_init_setting_1080p),
	},
	{
		.index     = 1,
		.width    = 1920,
		.height   = 1080,
		.fps      = 30,
		.hdr_mode = SENSOR_MODE_HDR_STITCH,
		.stitching_mode = SENSOR_STITCHING_DUAL_DCG,
		.bit_width = 12,
		.data_compress.enable = 0,
		.ae_info = {
			.DefaultFrameLengthLines = 0x466,
			.one_line_exp_time_ns = 59167,
			.max_integration_time = 0x466 - 64 - 2,
			.min_integration_time = 1,
			.gain_accuracy = 1024,
			.max_gain = 21 * 1024,
			.min_gain = 1 * 1024,
		},
		.preg_data = ov2775_init_setting_1080p_hdr,
		.reg_data_count = ARRAY_SIZE(ov2775_init_setting_1080p_hdr),
	},
	{
		.index     = 2,
		.width    = 1920,
		.height   = 1080,
		.fps      = 30,
		.hdr_mode = SENSOR_MODE_HDR_NATIVE,
		.bit_width = 12,
		.data_compress.enable = 1,
		.data_compress.x_bit = 16,
		.data_compress.y_bit = 12,
		.ae_info = {
			.DefaultFrameLengthLines = 0x466,
			.one_line_exp_time_ns = 59167,
			.max_integration_time = 0x466 - 2,
			.min_integration_time = 1,
			.gain_accuracy = 1024,
			.max_gain = 21 * 1024,
			.min_gain = 1 * 1024,
		},
		.preg_data = ov2775_1080p_native_hdr_regs,
		.reg_data_count = ARRAY_SIZE(ov2775_1080p_native_hdr_regs),
	},
#else
	{
		.index     = 0,
		.width     = 1920,
		.height    = 1080,
		.fps       = 30,
		.hdr_mode  = SENSOR_MODE_LINEAR,
		.bit_width = 12,
		.data_compress.enable = 0,

		.ae_info = {
			.DefaultFrameLengthLines = 0x466,
			.one_line_exp_time_ns = 29625,
			.max_integration_time = 0x466 - 2,
			.min_integration_time = 1,
			.gain_accuracy = 1024,
			.max_gain = 21 * 1024,
			.min_gain = 1 * 1024,
		},
		.preg_data = ov2775_init_setting_1080p,
		.reg_data_count = ARRAY_SIZE(ov2775_init_setting_1080p),
	},
#endif
};

static int ov2775_probe(struct i2c_client *adapter,
			const struct i2c_device_id *device_id);
static int ov2775_remove(struct i2c_client *client);
static void ov2775_stop(struct ov2775 *sensor);

static const struct i2c_device_id ov2775_id[] = {
	{"ov2775", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ov2775_id);

static int ov2775_retrieve_capture_properties(struct ov2775 *sensor,struct ov2775_capture_properties* ocp)
{
	struct device *dev = &sensor->i2c_client->dev;
	__u64 mlf = 0;
	__u64 mpf = 0;
	__u64 mdr = 0;

	struct device_node *ep;
	int ret;
	/*Collecting the information about limits of capture path
	* has been centralized to the sensor
	* * also into the sensor endpoint itself.
	*/

	ep = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!ep) {
	dev_err(dev, "missing endpoint node\n");
		return -ENODEV;

	}

	ret = fwnode_property_read_u64(of_fwnode_handle(ep),
		 "max-lane-frequency", &mlf);
	if (ret || mlf == 0) {
		dev_dbg(dev, "no limit for max-lane-frequency\n");

	}
	ret = fwnode_property_read_u64(of_fwnode_handle(ep),
	"max-pixel-frequency", &mpf);
	if (ret || mpf == 0) {
	dev_dbg(dev, "no limit for max-pixel-frequency\n");
	}

	ret = fwnode_property_read_u64(of_fwnode_handle(ep),
		"max-data-rate", &mdr);
	if (ret || mdr == 0) {
		dev_dbg(dev, "no limit for max-data_rate\n");
	}

	ocp->max_lane_frequency = mlf;
	ocp->max_pixel_frequency = mpf;
	ocp->max_data_rate = mdr;

	return ret;
}

static int __maybe_unused ov2775_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ov2775 *sensor = client_to_ov2775(client);

	if (sensor->on) {
		ov2775_stop(sensor);
	}

	return 0;
}

static int __maybe_unused ov2775_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ov2775 *sensor = client_to_ov2775(client);

	if (sensor->on) {
		ov2775_write_reg(sensor, 0x3012, 0x01);
	}
	return 0;
}


static const struct dev_pm_ops ov2775_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ov2775_suspend, ov2775_resume)
};

static const struct of_device_id ov2775_dt_ids[] = {
	{ .compatible = "ovti,ov2775" },
	{}
};

MODULE_DEVICE_TABLE(of, ov2775_dt_ids);

static struct i2c_driver ov2775_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = "ov2775",
		.pm = &ov2775_pm_ops,
		.of_match_table	= ov2775_dt_ids,
	},
	.probe  = ov2775_probe,
	.remove = ov2775_remove,
	.id_table = ov2775_id,
};



static const struct ov2775_datafmt ov2775_colour_fmts[] = {
	{MEDIA_BUS_FMT_SBGGR12_1X12, V4L2_COLORSPACE_JPEG},
};

/* Find a data format by a pixel code in an array */
static const struct ov2775_datafmt
*ov2775_find_datafmt(u32 code)
{
	int i;

	pr_debug("enter %s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(ov2775_colour_fmts); i++)
		if (ov2775_colour_fmts[i].code == code)
			return ov2775_colour_fmts + i;

	return NULL;
}

static inline void ov2775_power_up(struct ov2775 *sensor)
{
	pr_debug("enter %s\n", __func__);
	if (!gpio_is_valid(sensor->pwn_gpio))
		return;

	gpio_set_value_cansleep(sensor->pwn_gpio, 1);
}

static inline void ov2775_power_down(struct ov2775 *sensor)
{
	pr_debug("enter %s\n", __func__);
	if (!gpio_is_valid(sensor->pwn_gpio))
		return;

	gpio_set_value_cansleep(sensor->pwn_gpio, 0);
}

static inline void ov2775_reset(struct ov2775 *sensor)
{
	pr_debug("enter %s\n", __func__);
	if (!gpio_is_valid(sensor->rst_gpio))
		return;

	gpio_set_value_cansleep(sensor->rst_gpio, 0);
	msleep(20);

	gpio_set_value_cansleep(sensor->rst_gpio, 1);
	msleep(20);
}

static int ov2775_regulator_enable(struct ov2775 *sensor)
{
	int ret = 0;
	struct device *dev = &(sensor->i2c_client->dev);

	pr_debug("enter %s\n", __func__);

	if (sensor->io_regulator)
	{
		regulator_set_voltage(sensor->io_regulator,
					OV2775_VOLTAGE_DIGITAL_IO,
					OV2775_VOLTAGE_DIGITAL_IO);
		ret = regulator_enable(sensor->io_regulator);
		if (ret < 0) {
			dev_err(dev, "set io voltage failed\n");
			return ret;
		}
	}

	if (sensor->analog_regulator)
	{
		regulator_set_voltage(sensor->analog_regulator,
				      OV2775_VOLTAGE_ANALOG,
				      OV2775_VOLTAGE_ANALOG);
		ret = regulator_enable(sensor->analog_regulator);
		if (ret)
		{
			dev_err(dev, "set analog voltage failed\n");
			goto err_disable_io;
		}

	}

	if (sensor->core_regulator)
	{
		regulator_set_voltage(sensor->core_regulator,
				      OV2775_VOLTAGE_DIGITAL_CORE,
				      OV2775_VOLTAGE_DIGITAL_CORE);
		ret = regulator_enable(sensor->core_regulator);
		if (ret) {
			dev_err(dev, "set core voltage failed\n");
			goto err_disable_analog;
		}
	}

	return 0;

err_disable_analog:
	regulator_disable(sensor->analog_regulator);
err_disable_io:
	regulator_disable(sensor->io_regulator);
	return ret;
}

static void ov2775_regulator_disable(struct ov2775 *sensor)
{
	int ret = 0;
	struct device *dev = &(sensor->i2c_client->dev);

	if (sensor->core_regulator)
	{
		ret = regulator_disable(sensor->core_regulator);
		if (ret < 0)
			dev_err(dev, "core regulator disable failed\n");
	}

	if (sensor->analog_regulator)
	{
		ret = regulator_disable(sensor->analog_regulator);
		if (ret < 0)
			dev_err(dev, "analog regulator disable failed\n");
	}

	if (sensor->io_regulator)
	{
		ret = regulator_disable(sensor->io_regulator);
		if (ret < 0)
			dev_err(dev, "io regulator disable failed\n");
	}
	return ;
}

s32 ov2775_write_reg(struct ov2775 *sensor, u16 reg, u8 val)
{
	struct device *dev = &sensor->i2c_client->dev;
	struct i2c_msg msgs[2];
	unsigned char send_buf[3];
	unsigned int send_len = 0;
	int ret = 0;

	if (sensor == NULL) {
		dev_err(dev, "%s, sensor is null\n",__func__);
		return -1;
	}

	memset(msgs,0,sizeof(msgs));
	memset(send_buf,0,sizeof(send_buf));

	send_buf[0] = reg >> 8;
	send_buf[1] = reg & 0xff;
	send_buf[2] = val;
	send_len = sizeof(send_buf);

	msgs[0].addr  = sensor->i2c_client->addr >> 1;;
	msgs[0].flags = sensor->i2c_client->flags & I2C_M_TEN;
	msgs[0].len   = send_len;
	msgs[0].buf   = send_buf;

	dev_dbg(dev, "-->%s:  addr[0x%04x] = 0x%04x!\n",__func__,reg ,val);

	ret = i2c_transfer(sensor->i2c_client->adapter, msgs, 1);
	if (ret != 1)
	{
		dev_err(dev,"-->%s: i2c transfer failed\n", __func__);
		return -1;
	}

	return 0;
}

s32 ov2775_read_reg(struct ov2775 *sensor, u16 reg, u8 *val)
{
	struct device *dev = &sensor->i2c_client->dev;
	struct i2c_msg msgs[2];
	unsigned char send_buf[2];
	unsigned char read_buf[1] = {0,};

	int ret = 0;
 	
	memset(msgs, 0, sizeof(msgs));
	memset(send_buf, 0, sizeof(send_buf));
	memset(read_buf, 0, sizeof(read_buf));

	put_unaligned_be16(reg, send_buf);
	send_buf[0] = reg >> 8;
	send_buf[1] = reg & 0xff;

	msgs[0].addr  = sensor->i2c_client->addr >> 1;
	msgs[0].flags = sensor->i2c_client->flags & I2C_M_TEN;
	msgs[0].len   = sizeof(send_buf);
	msgs[0].buf   = send_buf;

	msgs[1].addr  = sensor->i2c_client->addr >> 1;
	msgs[1].flags = sensor->i2c_client->flags & I2C_M_TEN;
	msgs[1].flags |= I2C_M_RD;
	msgs[1].len   = sizeof(read_buf);
	msgs[1].buf   = read_buf;

	ret = i2c_transfer(sensor->i2c_client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(dev, "Read reg error, ret =%d : reg=%x\n", ret, reg);
		return -1;
	}

	*val = *read_buf;

	return 0;
}

static int ov2775_set_clk_rate(struct ov2775 *sensor)
{
	u32 tgt_xclk;		/* target xclk */
	int ret;

	/* mclk */
	tgt_xclk = sensor->mclk;
	tgt_xclk = min_t(u32, tgt_xclk, (u32) OV2775_XCLK_MAX);
	tgt_xclk = max_t(u32, tgt_xclk, (u32) OV2775_XCLK_MIN);
	sensor->mclk = tgt_xclk;

	pr_debug("   Setting mclk to %d MHz\n", tgt_xclk / 1000000);
	ret = clk_set_rate(sensor->sensor_clk, sensor->mclk);
	if (ret < 0)
		pr_debug("set rate filed, rate=%d\n", sensor->mclk);
	return ret;
}

/* download ov2775 settings to sensor through i2c */
static int ov2775_download_firmware(struct ov2775 *sensor,
				    struct ov2775_reg_value_t *mode_setting,
				    s32 size)
{
	register u32 delay_ms = 0;
	register u16 reg_addr = 0;
	register u8 mask = 0;
	register u8 val = 0;
	u8 reg_val = 0;
	int i, retval = 0;
	u8	 *reg_buf;
	// struct ov2775_reg_value_t *mode_setting_next;

	dev_info(&sensor->i2c_client->dev,"enter %s\n", __func__);
	//sensor soft rest
	ov2775_write_reg(sensor, 0x3013, 0x1);
	msleep(10);

	reg_buf = (u8 *)kmalloc(size + 2, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	for (i = 0; i < size; ++i, ++mode_setting) {
		delay_ms = mode_setting->delay;
		reg_addr = mode_setting->addr;
		val = mode_setting->val;
		mask = mode_setting->mask;

		dev_dbg(&sensor->i2c_client->dev,"%s Write addr 0x%x data 0x%x\n", __func__, reg_addr, val);
		if(unlikely(mask || delay_ms)) {
			if (mask) {
				retval = ov2775_read_reg(sensor, reg_addr, &reg_val);
				if (retval < 0)
					break;

				reg_val &= ~(u8)mask;
				val &= mask;
				val |= reg_val;
			}

			retval = ov2775_write_reg(sensor, reg_addr, val);
			if (retval < 0)
				break;

			if (delay_ms)
				msleep(delay_ms);
		} else {
			// mode_setting_next = mode_setting + 1;
			// reg_buf[0] = reg_addr >> 8;
			// reg_buf[1] = reg_addr & 0xff;
			// reg_buf[2] = val;
			// reg_len = 3;

			// while(i + 1 < size &&
			// 	  !mode_setting_next->mask && !mode_setting_next->delay &&
			// 	   mode_setting_next->addr == reg_addr + 1) {
			// 	reg_buf[reg_len++] = mode_setting_next->val;
			// 	i++;
			// 	mode_setting++;
			// 	mode_setting_next++;
			// 	reg_addr++;
			// }
			// msg.addr	= i2c_client->addr;
			// msg.flags	= i2c_client->flags;
			// msg.len 	= reg_len;
			// msg.buf 	= reg_buf;
			// pr_debug("start reg_addr=0x%02x%02x reg_num=%d\n",
			// 			reg_buf[0], reg_buf[1], reg_len-2);
			// retval = i2c_transfer(i2c_client->adapter, &msg, 1);
			// if (retval < 0) {
			// 	dev_err(&i2c_client->dev, "i2c transfer error\n");
			// 	break;
			// }
			retval = ov2775_write_reg(sensor, reg_addr, val);

		}
	}

	kfree(reg_buf);
	return retval;
}

static void ov2775_start(struct ov2775 *sensor)
{
	u8 val = 0;
	pr_debug("enter %s\n", __func__);
	ov2775_write_reg(sensor, 0x3012, 0x01);
	ov2775_read_reg(sensor, 0x3012, &val);

	msleep(100);
}

static void ov2775_stop(struct ov2775 *sensor)
{
	// int i, size, retval = 0;
	// struct ov2775_reg_value_t *mode_setting, *mode_setting_next;
	// struct i2c_msg msg;
	// register u16 reg_addr = 0;
	// u16 reg_len;
	// u8 *reg_buf;
	u8 val = 0;
	pr_debug("enter %s\n", __func__);

	ov2775_write_reg(sensor, 0x3012, 0x00);
	ov2775_read_reg(sensor, 0x3012, &val);

	/* TODO if the sensor re-enter streaming from standby mode
	 * all registers starting with 0x7000 must be resent
	 * before setting 0x3012[0]=1.
	 */
}

/*!
 * ov2775_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int ov2775_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2775 *sensor = client_to_ov2775(client);

	pr_debug("enter %s\n", __func__);
	if (on)
		clk_prepare_enable(sensor->sensor_clk);
	else
		clk_disable_unprepare(sensor->sensor_clk);

	sensor->on = on;
	return 0;
}

static int ov2775_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2775 *sensor = client_to_ov2775(client);

	dev_info(&sensor->i2c_client->dev,"enter %s\n", __func__);

	if (enable)
		ov2775_start(sensor);
	else
		ov2775_stop(sensor);

	sensor->on = enable;
	return 0;
}

static int ov2775_set_fmt(struct v4l2_subdev *sd,
                struct v4l2_subdev_state *state,
                struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	const struct ov2775_datafmt *fmt = ov2775_find_datafmt(mf->code);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2775 *sensor = client_to_ov2775(client);
	unsigned int i;
	struct ov2775_reg_value_t *mode_setting = NULL;
	int array_size = 0;

	dev_info(&sensor->i2c_client->dev,"enter %s\n", __func__);

#ifndef PLATFORM_HAILO15_FPGA
	if(sensor->ocp.max_pixel_frequency == 266000000) {
		ov2775_mode_info[1].preg_data = ov2775_init_setting_1080p_hdr_low_freq;
		ov2775_mode_info[1].reg_data_count = ARRAY_SIZE(ov2775_init_setting_1080p_hdr_low_freq);
		ov2775_mode_info[1].ae_info.one_line_exp_time_ns = 60784;
	} else {
		ov2775_mode_info[1].preg_data = ov2775_init_setting_1080p_hdr;
		ov2775_mode_info[1].reg_data_count = ARRAY_SIZE(ov2775_init_setting_1080p_hdr);
		ov2775_mode_info[1].ae_info.one_line_exp_time_ns = 59167;
	}
#endif
	if (format->pad) {
		return -EINVAL;
	}

	if (!fmt) {
		mf->code = ov2775_colour_fmts[0].code;
		mf->colorspace = ov2775_colour_fmts[0].colorspace;
	}

	mf->field = V4L2_FIELD_NONE;
	/*  old search method,  vsi need change to
	    search resolution by width/height */
	/*  try_to_find_resolution(sensor, mf); */
	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	for (i=0; i<ARRAY_SIZE(ov2775_mode_info); i++)
	{
		if (mf->width == ov2775_mode_info[i].width &&
			mf->height == ov2775_mode_info[i].height &&
			ov2775_mode_info[i].index == sensor->cur_mode.index)
		{
			memcpy(&(sensor->cur_mode), &ov2775_mode_info[i], sizeof(struct ov2775_mode_info));
			mode_setting = ov2775_mode_info[i].preg_data;
			array_size = ov2775_mode_info[i].reg_data_count;

			return ov2775_download_firmware(sensor, mode_setting, array_size);
		}
	}

	pr_err("%s search error: %d %d\n", __func__, mf->width, mf->height);
	return -EINVAL;
}

static int ov2775_get_fmt(struct v4l2_subdev *sd,
                struct v4l2_subdev_state *state,
                struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2775 *sensor = client_to_ov2775(client);

	dev_info(&sensor->i2c_client->dev,"enter %s\n", __func__);
	if (format->pad)
		return -EINVAL;

	memset(mf, 0, sizeof(struct v4l2_mbus_framefmt));

	mf->code = ov2775_colour_fmts[0].code;
	mf->colorspace = ov2775_colour_fmts[0].colorspace;
	mf->width = sensor->pix.width;
	mf->height = sensor->pix.height;
	mf->field = V4L2_FIELD_NONE;

	dev_dbg(&client->dev,
		"%s code=0x%x, w/h=(%d,%d), colorspace=%d, field=%d\n",
		__func__, mf->code, mf->width, mf->height, mf->colorspace,
		mf->field);

	return 0;
}

static int ov2775_enum_code(struct v4l2_subdev *sd,
                struct v4l2_subdev_state *state,
                struct v4l2_subdev_mbus_code_enum *code)
{
	pr_debug("enter %s\n", __func__);
	if (code->pad || code->index >= ARRAY_SIZE(ov2775_colour_fmts))
		return -EINVAL;

	code->code = ov2775_colour_fmts[code->index].code;
	return 0;
}

/*!
 * ov2775_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ov2775_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *state,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	pr_debug("enter %s\n", __func__);

	if (fse->index > ARRAY_SIZE(ov2775_mode_info))
		return -EINVAL;

	fse->min_width = ov2775_mode_info[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = ov2775_mode_info[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

/*!
 * ov2775_enum_frameintervals - V4L2 sensor interface handler for
 *				   VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ov2775_enum_frameintervals(struct v4l2_subdev *sd,
                        struct v4l2_subdev_state *state,
                        struct v4l2_subdev_frame_interval_enum
                        *fie)
{
	pr_debug("enter %s\n", __func__);
	if (fie->index < 0 || fie->index > ARRAY_SIZE(ov2775_mode_info))
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 || fie->code == 0) {
		pr_warn("Please assign pixel format, width and height.\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = ov2775_mode_info[fie->index].fps;

	return 0;
}

static int ov2775_link_setup(struct media_entity *entity,
			     const struct media_pad *local,
			     const struct media_pad *remote, u32 flags)
{
	return 0;
}

static struct v4l2_subdev_video_ops ov2775_subdev_video_ops = {
	.s_stream = ov2775_s_stream,
};

static const struct v4l2_subdev_pad_ops ov2775_subdev_pad_ops = {
	.enum_frame_size = ov2775_enum_framesizes,
	.enum_frame_interval = ov2775_enum_frameintervals,
	.enum_mbus_code = ov2775_enum_code,
	.set_fmt = ov2775_set_fmt,
	.get_fmt = ov2775_get_fmt,
};

static struct v4l2_subdev_core_ops ov2775_subdev_core_ops = {
	.s_power = ov2775_s_power,
#ifdef OV2775_PRIV_IOCTL_SUPPORT
	.ioctl = ov2775_priv_ioctl,
#endif /*OV2775_PRIV_IOCTL_SUPPORT*/
};

static struct v4l2_subdev_ops ov2775_subdev_ops = {
	.core = &ov2775_subdev_core_ops,
	.video = &ov2775_subdev_video_ops,
	.pad = &ov2775_subdev_pad_ops,
};

static const struct media_entity_operations ov2775_sd_media_ops = {
	.link_setup = ov2775_link_setup,
};

static const struct v4l2_ctrl_ops ov2775_ctrl_ops = {
	.s_ctrl = ov2775_set_ctrl,
};

static const char * const test_pattern[] = {
        "Disabled",
        "Vertical Bars",
        "Vertical Bars Vertical Grad",
        "Vertical Bars Horizontal Grad",
        "Vertical Bars Diagonal Grad",
        "Vertical Bars Rolling Line",
        "Random image",
        "Coloc Squares",
        "Black and White",
        "Color Chart",
};

static int ov2775_init_controls(struct ov2775 *ov2775)
{
	struct v4l2_ctrl_handler *ctrl_hdlr;
	int ret = 0;

	ctrl_hdlr = &ov2775->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
	if (ret) {
	    dev_err(&ov2775->i2c_client->dev,"%s failed to init controls with ret=%d\n", __func__, ret);
		return ret;
    }

	ctrl_hdlr->lock = &ov2775->lock;

    ov2775->test_pattern = v4l2_ctrl_new_std_menu_items(&ov2775->ctrl_handler, &ov2775_ctrl_ops,
                    V4L2_CID_TEST_PATTERN, ARRAY_SIZE(test_pattern) - 1, 0,
                    0, test_pattern);

    // ov2775->test_pattern = v4l2_ctrl_new_std(ctrl_hdlr, &ov2775_ctrl_ops,
	// 				     V4L2_CID_TEST_PATTERN,
	// 				     OV2775_TEST_PATTERN_MIN, OV2775_TEST_PATTERN_MAX,
	// 				     OV2775_TEST_PATTERN_STEP,
	// 				     OV2775_TEST_PATTERN_MIN);
	if (ctrl_hdlr->error) {
        dev_err(&ov2775->i2c_client->dev,"%s failed to add test pattern control \n", __func__);
		return ctrl_hdlr->error;
    }

	ov2775->subdev.ctrl_handler = ctrl_hdlr;

	return 0;
}


/*!
 * ov2775 I2C probe function
 *
 * @param adapter struct i2c_adapter *
 * @return  Error code indicating success or failure
 */

static int ov2775_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct v4l2_subdev *sd;
	int retval;
	struct ov2775 *sensor;
	u8  reg_val = 0;
	u16 chip_id = 0;

	pr_info("enter %s\n", __func__);

	sensor = devm_kmalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;
	memset(sensor, 0, sizeof(*sensor));
	sensor->i2c_client = client;

	/* request power down pin */
	sensor->pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(sensor->pwn_gpio))
		dev_warn(dev, "No sensor pwdn pin available");
	else {
		retval = devm_gpio_request_one(dev, sensor->pwn_gpio,
					       GPIOF_OUT_INIT_HIGH,
					       "ov2775_mipi_pwdn");
		if (retval < 0) {
			dev_warn(dev, "Failed to set power pin\n");
			dev_warn(dev, "retval=%d\n", retval);
			return retval;
		}
	}

	/* request reset pin */
	sensor->rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(sensor->rst_gpio))
		dev_warn(dev, "No sensor reset pin available");
	else {
		retval = devm_gpio_request_one(dev, sensor->rst_gpio,
					       GPIOF_OUT_INIT_HIGH,
					       "ov2775_mipi_reset");
		if (retval < 0) {
			dev_warn(dev, "Failed to set reset pin\n");
			return retval;
		}
	}

	/* Set initial values for the sensor struct. */

	sensor->sensor_clk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sensor->sensor_clk)) {
		/* assuming clock enabled by default */
		sensor->sensor_clk = NULL;
		dev_err(dev, "clock-frequency missing or invalid\n");
		return PTR_ERR(sensor->sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "clock-frequency", &(sensor->mclk));

	if (retval) {
		dev_err(dev, "mclk missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi-id", &(sensor->csi));
	if (retval) {
		dev_err(dev, "csi id missing or invalid\n");
		return retval;
	}

	sensor->io_regulator = devm_regulator_get(dev, "DOVDD");
	if (IS_ERR(sensor->io_regulator))
	{
		dev_err(dev, "cannot get io regulator\n");
		return PTR_ERR(sensor->io_regulator);
	}

	sensor->core_regulator = devm_regulator_get(dev, "DVDD");
	if (IS_ERR(sensor->core_regulator))
	{
		dev_err(dev, "cannot get core regulator\n");
		return PTR_ERR(sensor->core_regulator);
	}

	sensor->analog_regulator = devm_regulator_get(dev, "AVDD");
	if (IS_ERR(sensor->analog_regulator))
	{
		dev_err(dev, "cannot get analog  regulator\n");
		return PTR_ERR(sensor->analog_regulator);
	}

	retval = ov2775_regulator_enable(sensor);
	if (retval) {
		dev_err(dev, "regulator enable failed\n");
		return retval;
	}

	/* Set mclk rate before clk on */
	ov2775_set_clk_rate(sensor);
	retval = clk_prepare_enable(sensor->sensor_clk);
	if (retval < 0) {
		dev_err(dev, "%s: enable sensor clk fail\n", __func__);
		goto probe_err_regulator_disable;
	}

	ov2775_power_up(sensor);
	ov2775_reset(sensor);

	sensor->io_init = ov2775_reset;
	sensor->pix.pixelformat = V4L2_PIX_FMT_UYVY;
	sensor->pix.width = ov2775_mode_info[0].width;
	sensor->pix.height = ov2775_mode_info[0].height;
	sensor->streamcap.capability = V4L2_MODE_HIGHQUALITY |
	    V4L2_CAP_TIMEPERFRAME;
	sensor->streamcap.capturemode = 0;
	sensor->streamcap.timeperframe.denominator = ov2775_mode_info[0].fps;
	sensor->streamcap.timeperframe.numerator = 1;

	sensor->blc.blue = 64;
	sensor->blc.gb   = 64;
	sensor->blc.gr   = 64;
	sensor->blc.red  = 64;
	sensor->wb.r_gain  = 0x1f9;
	sensor->wb.gr_gain = 0x104;
	sensor->wb.gb_gain = 0x104;
	sensor->wb.b_gain  = 0x23e;

	chip_id = 0;
	ov2775_read_reg(sensor, OV2775_CHIP_ID_HIGH_BYTE, &reg_val);
	chip_id |= reg_val << 8;
	ov2775_read_reg(sensor, OV2775_CHIP_ID_LOW_BYTE, &reg_val);
	chip_id |= reg_val;
	if (chip_id != OV2775_CHIP_ID) {
		pr_warn("camera ov2775 is not found\n");
		goto probe_err_power_down;
	}

	retval = ov2775_init_controls(sensor);
	if (retval) {
		dev_err(&client->dev, "failed to init controls: %d", retval);
		goto probe_err_power_down;
	}

	sd = &sensor->subdev;
	v4l2_i2c_subdev_init(sd, client, &ov2775_subdev_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	sensor->pads[OV2775_SENS_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;

	retval = media_entity_pads_init(&sd->entity, OV2775_SENS_PADS_NUM,
					sensor->pads);
	sd->entity.ops = &ov2775_sd_media_ops;
	if (retval < 0)
		goto probe_err_power_down;

	retval = v4l2_async_register_subdev_sensor(sd);
	if (retval < 0) {
		dev_err(&client->dev,"%s--Async register failed, ret=%d\n", __func__,retval);
		goto probe_err_entity_cleanup;
	}

	mutex_init(&sensor->lock);
	ov2775_retrieve_capture_properties(sensor,&sensor->ocp);

	dev_info(&client->dev, "%s camera mipi ov2775 is found\n", __func__);

	return 0;

probe_err_entity_cleanup:
	media_entity_cleanup(&sd->entity);
probe_err_power_down:
	ov2775_power_down(sensor);
	clk_disable_unprepare(sensor->sensor_clk);
probe_err_regulator_disable:
	ov2775_regulator_disable(sensor);
	return retval;
}

/*!
 * ov2775 I2C detach function
 *
 * @param client struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int ov2775_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	// struct ov2775 *sensor = client_to_ov2775(client);

	// pr_info("enter %s\n", __func__);
	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	// ov2775_power_down(sensor);
	// clk_disable_unprepare(sensor->sensor_clk);
	// ov2775_regulator_disable(sensor);
	// mutex_destroy(&sensor->lock);

	return 0;
}

module_i2c_driver(ov2775_i2c_driver);
MODULE_DESCRIPTION("OV2775 MIPI Camera Subdev Driver");
MODULE_LICENSE("GPL");

int sensor_calc_gain(__u32 total_gain, __u32 *pagain, __u32 *pdgain, __u32 *phcg)
{
	if (total_gain <= 3072)
		total_gain = 3072;
	else if ((total_gain >= 22528) && (total_gain < 23552))
		total_gain = 22528;

	if (total_gain < 4480) {
		*pagain = 1;
		*phcg = 1;
	} else if (total_gain < 8960) {
		*pagain = 2;
		*phcg = 1;
	} else if (total_gain < 22528) {
		*pagain = 3;
		*phcg = 1;
	} else if (total_gain < 44990) {
		*pagain = 0;
		*phcg = 11;
	} else if (total_gain < 89320) {
		*pagain = 1;
		*phcg = 11;
	} else if (total_gain < 179498) {
		*pagain = 2;
		*phcg = 11;
	} else {
		*pagain = 3;
		*phcg = 11;
	}

	*pdgain = ((total_gain << 8) >> (*pagain)) / (1024 * (*phcg));
	return 0;
}

int ov2775_s_long_gain(struct ov2775 *sensor, __u32 new_gain)
{
	int ret = 0;
	__u8 reg_val;
	__u32 hcg;
	__u32 hcg_gain;
	__u32 hcg_again = 0;
	__u32 hcg_dgain = 0;

	hcg_gain = new_gain/10;

	sensor_calc_gain(hcg_gain, &hcg_again, &hcg_dgain, &hcg);

	ret = ov2775_read_reg(sensor, 0x30bb, &reg_val);
	reg_val &= ~0x03;
	reg_val |= hcg_again  & 0x03;

	ret  = ov2775_write_reg(sensor, 0x3467, 0x00);
	ret |= ov2775_write_reg(sensor, 0x3464, 0x04);

	ret |= ov2775_write_reg(sensor, 0x315a, (hcg_dgain>>8) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x315b, hcg_dgain & 0xff);

	ret |= ov2775_write_reg(sensor, 0x30bb, reg_val);
	ret |= ov2775_write_reg(sensor, 0x3464, 0x14);
	ret |= ov2775_write_reg(sensor, 0x3467, 0x01);

	return ret;
}

int ov2775_s_gain(struct ov2775 *sensor, __u32 new_gain)
{
	int ret = 0;
	__u8 reg_val;
	__u32 hcg = 0;
	__u32 again, dgain;

	__u32 lcg_gain;
	__u32 lcg_again = 0;
	__u32 lcg_dgain = 0;

	if (sensor->hdr == SENSOR_MODE_LINEAR) {
		sensor_calc_gain(new_gain, &again, &dgain, &hcg);
		ret = ov2775_read_reg(sensor, 0x30bb, &reg_val);
		if (hcg == 1) {
			reg_val &= ~(1 << 6);
		} else {
			reg_val |= (1 << 6);
		}
		reg_val &= ~0x03;
		reg_val |= again;
		ret = ov2775_write_reg(sensor, 0x3467, 0x00);
		ret |= ov2775_write_reg(sensor, 0x3464, 0x04);
		ret |= ov2775_write_reg(sensor, 0x315a, (dgain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x315b, dgain & 0xff);
		ret |= ov2775_write_reg(sensor, 0x30bb, reg_val);
		ret |= ov2775_write_reg(sensor, 0x3464, 0x14);
		ret |= ov2775_write_reg(sensor, 0x3467, 0x01);
	}else {
		lcg_gain = new_gain;
		sensor_calc_gain(lcg_gain, &lcg_again, &lcg_dgain, &hcg);

		ret = ov2775_read_reg(sensor, 0x30bb, &reg_val);
		reg_val &= ~(0x03 << 2);
		reg_val |= (lcg_again & 0x03) << 2;

		ret  = ov2775_write_reg(sensor, 0x3467, 0x00);
		ret |= ov2775_write_reg(sensor, 0x3464, 0x04);

		ret |= ov2775_write_reg(sensor, 0x315c, (lcg_dgain>>8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x315d, lcg_dgain & 0xff);

		ret |= ov2775_write_reg(sensor, 0x30bb, reg_val);
		ret |= ov2775_write_reg(sensor, 0x3464, 0x14);
		ret |= ov2775_write_reg(sensor, 0x3467, 0x01);
	}
	return ret;
}

int ov2775_s_vsgain(struct ov2775 *sensor, __u32 new_gain)
{
	__u32 again = 0;
	__u32 dgain, hcg;
	__u8 reg_val;

	sensor_calc_gain(new_gain, &again, &dgain, &hcg);

	ov2775_read_reg(sensor, 0x30bb, &reg_val);
	reg_val &= ~0x30;
	reg_val |= (again & 0x03) << 4;

	ov2775_write_reg(sensor, 0x3467, 0x00);
	ov2775_write_reg(sensor, 0x3464, 0x04);

	ov2775_write_reg(sensor, 0x315e, (dgain & 0xFF00) >> 8);
	ov2775_write_reg(sensor, 0x315f, dgain & (0x00FF));
	ov2775_write_reg(sensor, 0x30bb, reg_val);

	ov2775_write_reg(sensor, 0x3464, 0x14);
	ov2775_write_reg(sensor, 0x3467, 0x01);
	return 0;
}

int ov2775_s_long_exp(struct ov2775 *sensor, __u32 exp)
{
	return 0;
}

int ov2775_s_exp(struct ov2775 *sensor, __u32 exp)
{
	/* pr_info("enter %s 0x%08x\n", __func__, exp); */
	ov2775_write_reg(sensor, 0x3467, 0x00);
	ov2775_write_reg(sensor, 0x3464, 0x04);

	ov2775_write_reg(sensor, 0x30b6, (exp & 0xFF00) >> 8);
	ov2775_write_reg(sensor, 0x30b7, exp & 0x00FF);

	ov2775_write_reg(sensor, 0x3464, 0x14);
	ov2775_write_reg(sensor, 0x3467, 0x01);
	return 0;
}

int ov2775_s_vsexp(struct ov2775 *sensor, __u32 exp)
{
	/* pr_info("enter %s 0x%08x\n", __func__, exp); */
	ov2775_write_reg(sensor, 0x3467, 0x00);
	ov2775_write_reg(sensor, 0x3464, 0x04);

	if (exp == 0x16)
		exp = 0x17;
	if (sensor->hdr == SENSOR_MODE_HDR_STITCH) {
		if (exp >0x2c)
			exp = 0x2c;
	}

	ov2775_write_reg(sensor, 0x30b8, (exp & 0xFF00) >> 8);
	ov2775_write_reg(sensor, 0x30b9, exp & 0x00FF);

	ov2775_write_reg(sensor, 0x3464, 0x14);
	ov2775_write_reg(sensor, 0x3467, 0x01);
	return 0;
}

int ov2775_g_version(struct ov2775 *sensor, __u32 *version)
{
	__u8 val = 0;

	ov2775_read_reg(sensor, 0x300a, &val);
	*version = val << 8;
	ov2775_read_reg(sensor, 0x300b, &val);
	*version |= val;
	return 0;
}


int ov2775_s_hdr(struct ov2775 *sensor, int hdr_mode)
{
	pr_debug("%s: %d\n", __func__, hdr_mode);
	sensor->hdr = hdr_mode;
	return 0;
}

int ov2775_s_sensor_mode(struct ov2775 *sensor, struct ov2775_mode_info *sensor_mode)
{

	if(sensor_mode->index < ARRAY_SIZE(ov2775_mode_info) && sensor_mode->index >= 0){
		sensor->cur_mode.index = sensor_mode->index;
	}else{
		pr_err("%s Set ov2775 mode index error",__func__);
		return -1;
	}

	return 0;
}


int ov2775_s_clk(struct ov2775 *sensor, __u32 clk)
{
	pr_debug("%s: %d\n", __func__, clk);
	ov2775_write_reg(sensor, 0x3005, clk);
	return 0;
}

int ov2775_g_clk(struct ov2775 *sensor, __u32 *clk)
{
	u8 val;
	ov2775_read_reg(sensor, 0x3005, &val);
	*clk = val;
	return 0;
}

int ov2775_g_sclk(struct ov2775 *sensor, __u32 *sclk_hz)
{
	int ret = 0;
	u8 pclk_pll_pre;//0x3004[2:0]
	u8 pclk_pll_pdiv;//0x3005[7:0]
	u8 pclk_pll_sdiv;//0x3006[0]
	u8 pclk_pll_post;//0x3007[3:0]
	u32 pclk_pll_m;
	u8  pclk_pll_pre_num, pclk_pll_pre_deno;
	ret = ov2775_read_reg(sensor, 0x3004, &pclk_pll_pre);
	ret |= ov2775_read_reg(sensor, 0x3005, &pclk_pll_pdiv);
	ret |= ov2775_read_reg(sensor, 0x3006, &pclk_pll_sdiv);
	ret |= ov2775_read_reg(sensor, 0x3007, &pclk_pll_post);
	switch(pclk_pll_pre&0x07)//0x3004[2:0]
	{
	case 0: pclk_pll_pre_num = 1; pclk_pll_pre_deno = 1; break;
	case 1: pclk_pll_pre_num = 2; pclk_pll_pre_deno = 3; break;
	case 2: pclk_pll_pre_num = 1; pclk_pll_pre_deno = 2; break;
	case 3: pclk_pll_pre_num = 2; pclk_pll_pre_deno = 5; break;
	case 4: pclk_pll_pre_num = 1; pclk_pll_pre_deno = 3; break;
	case 5: pclk_pll_pre_num = 1; pclk_pll_pre_deno = 4; break;
	case 6: pclk_pll_pre_num = 1; pclk_pll_pre_deno = 6; break;
	default: pclk_pll_pre_num = 1; pclk_pll_pre_deno = 8; break;
	}
	pclk_pll_post = (pclk_pll_post&0x0F) + 1;
	pclk_pll_m = 2*pclk_pll_pdiv + (pclk_pll_sdiv&0x01) + 4;
	*sclk_hz = (sensor->mclk/1000*pclk_pll_m*pclk_pll_pre_num/(pclk_pll_pre_deno*pclk_pll_post))*1000;
	sensor->sclk = *sclk_hz;
	return ret;
}

int ov2775_s_fps(struct ov2775 *sensor, __u32 fps)
{
	u32 vts;

	if (fps < 5) {
		fps = 5;
	}
	if (fps > sensor->cur_mode.fps){
		fps = sensor->cur_mode.fps;
	}
	sensor->fps = fps;

	vts =  sensor->cur_mode.fps * sensor->cur_mode.ae_info.DefaultFrameLengthLines / sensor->fps;

	ov2775_write_reg(sensor, 0x30B2, (u8)(vts >> 8));
	ov2775_write_reg(sensor, 0x30B3, (u8)(vts & 0xff));

	if (sensor->cur_mode.hdr_mode != SENSOR_MODE_LINEAR)
	{
		sensor->cur_mode.ae_info.cur_fps = sensor->fps;
		sensor->cur_mode.ae_info.CurFrameLengthLines = vts;
		sensor->cur_mode.ae_info.max_integration_time = vts - 64 - 2;
	}else
	{
		sensor->cur_mode.ae_info.cur_fps = sensor->fps;
		sensor->cur_mode.ae_info.CurFrameLengthLines = vts;
		sensor->cur_mode.ae_info.max_integration_time = vts - 2;
	}
	return 0;
}

int ov2775_g_fps(struct ov2775 *sensor,  __u32 *fps)
{
	*fps = sensor->fps;
	return 0;
}

int ov2775_g_chipid(struct ov2775 *sensor, __u32 *chip_id)
{
	int ret = 0;
	__u8 chip_id_high = 0;
	__u8 chip_id_low = 0;
	ret = ov2775_read_reg(sensor, 0x300a, &chip_id_high);
	ret |= ov2775_read_reg(sensor, 0x300b, &chip_id_low);

	*chip_id = ((chip_id_high & 0xff)<<8) | (chip_id_low & 0xff);
	return ret;
}

int ov2775_ioc_qcap(struct ov2775 *sensor, void *args)
{
	struct v4l2_capability *cap = (struct v4l2_capability *)args;

	strcpy((char *)cap->driver, "ov2775");
	sprintf((char *)cap->bus_info, "csi%d",sensor->csi);//bus_info[0:7]-csi number
	if(sensor->i2c_client->adapter)
	{//bus_info[8]-i2c bus dev number
		cap->bus_info[OV2775_CAP_BUS_INFO_I2C_ADAPTER_NR_POS] = (__u8)sensor->i2c_client->adapter->nr;
	}
	else
	{
		cap->bus_info[OV2775_CAP_BUS_INFO_I2C_ADAPTER_NR_POS] = 0xFF;
	}

	return 0;
}

int ov2775_ioc_query_mode(struct ov2775 *sensor, struct ov2775_mode_infoarray *array)
{
	array->count = ARRAY_SIZE(ov2775_mode_info);
#ifdef CONFIG_HARDENED_USERCOPY
	unsigned long copy_ret = 0;
	pr_debug("sensor %px\n", sensor);
	copy_ret = copy_to_user(&array->modes,ov2775_mode_info,sizeof(ov2775_mode_info));
#else
	memcpy(&array->modes,ov2775_mode_info,sizeof(ov2775_mode_info));
#endif

	return 0;
}

int ov2775_g_mode(struct ov2775 *sensor, struct ov2775_mode_info *pmode)
{
	int i = 0;
	struct ov2775_mode_info *pcur_mode = NULL;

	if (sensor->cur_mode.index == pmode->index &&
	    sensor->cur_mode.width != 0 &&
	    sensor->cur_mode.height != 0)
	{
		pcur_mode = &(sensor->cur_mode);
		memcpy(pmode,pcur_mode,sizeof(struct ov2775_mode_info));
		return 0;
	}
	for(i=0; i < ARRAY_SIZE(ov2775_mode_info); i++)
	{
		if (pmode->index == ov2775_mode_info[i].index)
		{
			pcur_mode = &ov2775_mode_info[i];
			sensor->fps = ov2775_mode_info[i].fps;
			break;
		}
	}
	if (pcur_mode == NULL)
	{
		return -1;
	}

	memcpy(pmode,pcur_mode,sizeof(struct ov2775_mode_info));
	return 0;
}

int ov2775_s_blc(struct ov2775 *sensor, sensor_blc_t *pblc)
{
	int ret = 0;
	int r_offset,gr_offset,gb_offset,b_offset;
	unsigned int r_gain,gr_gain,gb_gain,b_gain;

	r_gain  = sensor->wb.r_gain;
	gr_gain = sensor->wb.gr_gain;
	gb_gain = sensor->wb.gb_gain;
	b_gain  = sensor->wb.b_gain;

	if (r_gain < 0x100)
		r_gain = 0x100;
	if (gr_gain < 0x100)
		gr_gain = 0x100;
	if (gb_gain < 0x100)
		gb_gain = 0x100;
	if (b_gain < 0x100)
		b_gain = 0x100;

	r_offset  = (r_gain  - 0x100) * pblc->red;
	gr_offset = (gr_gain - 0x100) * pblc->gr;
	gb_offset = (gb_gain - 0X100) * pblc->gb;
	b_offset  = (b_gain  - 0X100) * pblc->blue;

	//R,Gr,Gb,B HCG Offset
	ret |= ov2775_write_reg(sensor, 0x3378, (r_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3379, (r_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x337a,  r_offset        & 0xff);

	ret |= ov2775_write_reg(sensor, 0x337b, (gr_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x337c, (gr_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x337d,  gr_offset        & 0xff);

	ret |= ov2775_write_reg(sensor, 0x337e, (gb_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x337f, (gb_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3380,  gb_offset        & 0xff);

	ret |= ov2775_write_reg(sensor, 0x3381, (b_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3382, (b_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3383,  b_offset        & 0xff);

	//R,Gr,Gb,B LCG Offset
	ret |= ov2775_write_reg(sensor, 0x3384, (r_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3385, (r_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3386,  r_offset        & 0xff);

	ret |= ov2775_write_reg(sensor, 0x3387, (gr_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3388, (gr_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3389,  gr_offset        & 0xff);

	ret |= ov2775_write_reg(sensor, 0x338a, (gb_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x338b, (gb_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x338c,  gb_offset        & 0xff);

	ret |= ov2775_write_reg(sensor, 0x338d, (b_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x338e, (b_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x338f,  b_offset        & 0xff);

	//R,Gr,Gb,B VS Offset
	ret |= ov2775_write_reg(sensor, 0x3390, (r_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3391, (r_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3392,  r_offset        & 0xff);

	ret |= ov2775_write_reg(sensor, 0x3393, (gr_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3394, (gr_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3395,  gr_offset        & 0xff);

	ret |= ov2775_write_reg(sensor, 0x3396, (gb_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3397, (gb_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x3398,  gb_offset        & 0xff);

	ret |= ov2775_write_reg(sensor, 0x3399, (b_offset >> 16) & 0xff);
	ret |= ov2775_write_reg(sensor, 0x339a, (b_offset >> 8)  & 0xff);
	ret |= ov2775_write_reg(sensor, 0x339b,  b_offset        & 0xff);

	memcpy(&sensor->blc,pblc,sizeof(sensor_blc_t));

	return ret;
}


int ov2775_s_wb(struct ov2775 *sensor, sensor_white_balance_t *wb)
{
	unsigned int r_gain,gr_gain,gb_gain,b_gain;
	int ret = 0;

	r_gain  = wb->r_gain; // wb->r_gain  =256 means gain 1.0
	gr_gain = wb->gr_gain;// wb->gr_gain =256 means gain 1.0
	gb_gain = wb->gb_gain;// wb->gb_gain =256 means gain 1.0
	b_gain  = wb->b_gain; // wb->b_gain  =256 means gain 1.0

	//Red,Gr,Gb,Blue HCG Channel
	if(sensor->wb.r_gain != wb->r_gain){

		ret  = ov2775_write_reg(sensor, 0x3360, (r_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x3361,  r_gain & 0xff);
	}
	if(sensor->wb.gr_gain != wb->gr_gain){

		ret  = ov2775_write_reg(sensor, 0x3362, (gr_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x3363,  gr_gain & 0xff);
	}
	if(sensor->wb.gb_gain != wb->gb_gain){

		ret  = ov2775_write_reg(sensor, 0x3364, (gb_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x3365,  gb_gain & 0xff);
	}
	if(sensor->wb.b_gain != wb->b_gain){

		ret  = ov2775_write_reg(sensor, 0x3366, (b_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x3367,  b_gain & 0xff);
	}

	//Red,Gr,Gb,Blue LCG Channel
	if(sensor->wb.r_gain != wb->r_gain){

		ret  = ov2775_write_reg(sensor, 0x3368, (r_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x3369,  r_gain & 0xff);
	}
	if(sensor->wb.gr_gain != wb->gr_gain){

		ret  = ov2775_write_reg(sensor, 0x336a, (gr_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x336b,  gr_gain & 0xff);
	}
	if(sensor->wb.gb_gain != wb->gb_gain){

		ret  = ov2775_write_reg(sensor, 0x336c, (gb_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x336d,  gb_gain & 0xff);
	}
	if(sensor->wb.b_gain != wb->b_gain){

		ret  = ov2775_write_reg(sensor, 0x336e, (b_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x336f,  b_gain & 0xff);
	}

	//Red,Gr,Gb,Blue VS Channel
	if(sensor->wb.r_gain != wb->r_gain){

		ret  = ov2775_write_reg(sensor, 0x3370, (r_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x3371,  r_gain & 0xff);
	}
	if(sensor->wb.gr_gain != wb->gr_gain){

		ret  = ov2775_write_reg(sensor, 0x3372, (gr_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x3373,  gr_gain & 0xff);
	}
	if(sensor->wb.gb_gain != wb->gb_gain){

		ret  = ov2775_write_reg(sensor, 0x3374, (gb_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x3375,  gb_gain & 0xff);
	}
	if(sensor->wb.b_gain != wb->b_gain){

		ret  = ov2775_write_reg(sensor, 0x3376, (b_gain >> 8) & 0xff);
		ret |= ov2775_write_reg(sensor, 0x3377,  b_gain & 0xff);
	}

	if((sensor->wb.gr_gain != wb->gr_gain)	||
	   (sensor->wb.r_gain  != wb->r_gain)	||
	   (sensor->wb.b_gain  != wb->b_gain)	||
	   (sensor->wb.gb_gain != wb->gb_gain)){

		ov2775_s_blc(sensor,&sensor->blc);
	}

	memcpy(&sensor->wb,wb,sizeof(sensor_white_balance_t));

	return ret;
}

int ov2775_get_expand_curve(struct ov2775 *sensor, sensor_expand_curve_t* pexpand_curve)
{
	int i;
	if ((pexpand_curve->x_bit) == 12 && (pexpand_curve->y_bit == 16))
	{
		uint8_t expand_px[64] = {6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
			                  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
					          6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
			                  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6};

		memcpy(pexpand_curve->expand_px,expand_px,sizeof(expand_px));

		pexpand_curve->expand_x_data[0] = 0;
		pexpand_curve->expand_y_data[0] = 0;
		for(i = 1; i<65; i++)
		{
			pexpand_curve->expand_x_data[i] = (1 << pexpand_curve->expand_px[i-1]) + pexpand_curve->expand_x_data[i-1];

			if (pexpand_curve->expand_x_data[i] < 512)
			{
				pexpand_curve->expand_y_data[i] = pexpand_curve->expand_x_data[i] << 1;

			}else if (pexpand_curve->expand_x_data[i] < 768)
			{
				pexpand_curve->expand_y_data[i] = (pexpand_curve->expand_x_data[i] - 256) << 2 ;

			}else if (pexpand_curve->expand_x_data[i] < 2560)
			{
				pexpand_curve->expand_y_data[i] = (pexpand_curve->expand_x_data[i] - 512) << 3 ;

			}else
			{
				pexpand_curve->expand_y_data[i] = (pexpand_curve->expand_x_data[i] - 2048) << 5;
			}
		}
		return 0;
	}
	return -1;
}

/*
Use USER_TO_KERNEL/KERNEL_TO_USER to fix "uaccess" exception on run time.
Also, use "copy_ret" to fix the build issue as below.
error: ignoring return value of function declared with 'warn_unused_result' attribute.
*/

#ifdef CONFIG_HARDENED_USERCOPY
#define USER_TO_KERNEL(TYPE) \
	do {\
		TYPE tmp; \
		unsigned long copy_ret; \
		arg = (void *)(&tmp); \
		copy_ret = copy_from_user(arg, arg_user, sizeof(TYPE));\
	} while (0)

#define KERNEL_TO_USER(TYPE) \
	do {\
		unsigned long copy_ret; \
		copy_ret = copy_to_user(arg_user, arg, sizeof(TYPE));\
	} while (0)
#else
#define USER_TO_KERNEL(TYPE)
#define KERNEL_TO_USER(TYPE)
#endif


static int ov2775_test_pattern(struct ov2775 *ov2775, u32 pattern)
{
    int ret = 0;
    u8 isp_ctrl_val = 0;

    dev_info(&ov2775->i2c_client->dev," %s: enter, pettern= %d\n", __func__, pattern);
    ret = ov2775_read_reg(ov2775, OV2775_REG_ISP_DP_CONF, &isp_ctrl_val);
    if (ret) {
        dev_err(&ov2775->i2c_client->dev," %s: failed getting ISP setting ret= %d\n", __func__, ret);
        return ret;
    }
    //isp enable
    isp_ctrl_val |= 1;
    ret = ov2775_write_reg(ov2775, OV2775_REG_ISP_DP_CONF, isp_ctrl_val);

    if (0 == pattern) {
        ov2775_write_reg(ov2775, OV2775_REG_PRE_CTRL0, 0);
    } else {
        ret |= ov2775_write_reg(ov2775, OV2775_REG_PRE_CTRL0, OV2775_PRE_CTRL_TEST_PATTERN_BAR);
    }
    if (ret) {
        dev_err(&ov2775->i2c_client->dev," %s: failed setting test pattern ret= %d\n", __func__, ret);
        return ret;
    }
    return ret;
}

static int ov2775_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov2775 *ov2775 = container_of(ctrl->handler,
					     struct ov2775, ctrl_handler);
	int ret = 0;

	switch (ctrl->id) {
        case V4L2_CID_TEST_PATTERN:
            ret = ov2775_test_pattern(ov2775, ctrl->val);
            break;

        default:
            ret = -EINVAL;
            break;
    }

	return ret;
}


#ifdef OV2775_PRIV_IOCTL_SUPPORT
long ov2775_priv_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg_user)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2775 *sensor = client_to_ov2775(client);
	struct vvcam_sccb_data reg;
	int ret = 0;
	void *arg = arg_user;

	/* pr_info("enter %s\n", __func__); */
	mutex_lock(&sensor->lock);
	switch (cmd) {
	case VVSENSORIOC_WRITE_REG: {
		USER_TO_KERNEL(struct vvcam_sccb_data);
		reg = *(struct vvcam_sccb_data *)arg;
		ret = ov2775_write_reg(sensor, reg.addr, (u8)reg.data) < 0;
		break;
	}
	case VVSENSORIOC_READ_REG: {
		struct vvcam_sccb_data *preg;
		u8 val;
		USER_TO_KERNEL(struct vvcam_sccb_data);
		preg = (struct vvcam_sccb_data *)arg;
		ret = ov2775_read_reg(sensor, (u16) preg->addr, &val) < 0;
		preg->data = val;
		KERNEL_TO_USER(struct vvcam_sccb_data);
		break;
	}
	case VVSENSORIOC_S_STREAM: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_s_stream(sd, *(__u32 *)arg);
		break;
	}
	case VVSENSORIOC_S_LONG_EXP: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_s_long_exp(sensor, *(__u32 *)arg);
		break;
	}
	case VVSENSORIOC_S_EXP: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_s_exp(sensor, *(__u32 *)arg);
		break;
	}
	case VVSENSORIOC_S_VSEXP: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_s_vsexp(sensor, *(__u32 *)arg);
		break;
	}
	case VVSENSORIOC_S_LONG_GAIN:{
		USER_TO_KERNEL(__u32);
		ret = ov2775_s_long_gain(sensor, *(__u32 *)arg);
		break;
	}
	case VVSENSORIOC_S_GAIN: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_s_gain(sensor, *(__u32 *)arg);
		break;
	}
	case VVSENSORIOC_S_VSGAIN: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_s_vsgain(sensor, *(__u32 *)arg);
		break;
	}
	case VVSENSORIOC_S_FPS: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_s_fps(sensor, *(__u32 *)arg);
		break;
	}
	case VVSENSORIOC_G_FPS: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_g_fps(sensor, (__u32 *)arg);
		KERNEL_TO_USER(__u32);
		break;
	}
	case VVSENSORIOC_S_CLK: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_s_clk(sensor, *(__u32 *)arg);
		break;
	}
	case VVSENSORIOC_G_CLK: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_g_clk(sensor, (__u32 *)arg);
		KERNEL_TO_USER(__u32);
		break;
	}
	case VIDIOC_QUERYCAP:
		ret = ov2775_ioc_qcap(sensor, arg);
		break;

	case VVSENSORIOC_G_CHIP_ID: {
		USER_TO_KERNEL(__u32);
		ret = ov2775_g_chipid(sensor, (__u32 *)arg);
		ret = (ret < 0) ? -1 : 0;
		KERNEL_TO_USER(__u32);
		break;
	}
	case VVSENSORIOC_G_RESERVE_ID: {
		__u32  correct_id = 0x2770;
		ret = copy_to_user(arg_user, &correct_id, sizeof(__u32));
		ret = ret ? -1 : 0;
		break;
	}
	case VVSENSORIOC_S_HDR_MODE: {
		USER_TO_KERNEL(int);
		ret = ov2775_s_hdr(sensor, *(int *)arg);
		break;
	}
	case VVSENSORIOC_QUERY: {
		//USER_TO_KERNEL(struct ov2775_mode_info_array);
		ret = ov2775_ioc_query_mode(sensor, arg);
		//KERNEL_TO_USER(struct ov2775_mode_info_array);
		break;
	}
	case VVSENSORIOC_G_SENSOR_MODE:{
		USER_TO_KERNEL(struct ov2775_mode_info);
		ret = ov2775_g_mode(sensor, arg);
		KERNEL_TO_USER(struct ov2775_mode_info);
		break;
	}
	case VVSENSORIOC_S_WB: {
		USER_TO_KERNEL(sensor_white_balance_t);
		ret = ov2775_s_wb(sensor,arg);
		break;
	}
	case VVSENSORIOC_S_BLC: {
		USER_TO_KERNEL(sensor_blc_t);
		ret = ov2775_s_blc(sensor,arg);
		break;
	}
	case VVSENSORIOC_G_EXPAND_CURVE:{
		//USER_TO_KERNEL(sensor_expand_curve_t);
		ret = ov2775_get_expand_curve(sensor, arg);
		//KERNEL_TO_USER(sensor_expand_curve_t);
		break;
	}
	case VVSENSORIOC_S_SENSOR_MODE: {
		USER_TO_KERNEL(struct ov2775_mode_info);
		ret = ov2775_s_sensor_mode(sensor, arg);
		KERNEL_TO_USER(struct ov2775_mode_info);
		break;
	}
	default:
		pr_err("unsupported ov2775 command %d.", cmd);
		ret = -1;
		break;
	}
	mutex_unlock(&sensor->lock);

	return ret;
}
#endif /*OV2775_PRIV_IOCTL_SUPPORT*/