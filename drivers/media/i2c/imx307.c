// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sony imx307 sensor driver
 */
#include <asm/unaligned.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include "hailo_shared_sensor_data.h"

#define IMX307_MIN_GAIN 0x00
#define IMX307_MAX_GAIN 0xff
#define IMX307_GAIN_STEP 1
#define IMX307_GAIN_DEFAULT 0

#define IMX307_EXPOSURE_STEP 1
#define IMX307_EXPOSURE_DEFAULT 1000

#define IMX307_INCLK_RATE 24000000
#define IMX307_REG_VALUE_08BIT 1

#define IMX307_LINK_FREQ_111M 111370000
#define IMX307_LINK_FREQ_222M 222750000

/* IMX307 Configuration - 4 lane, 12-bit, 37.125MHz, 30fps, 1920x1080 */
#define IMX307_LANES 4
#define IMX307_BITS_PER_PIXEL 12
#define IMX307_WIDTH 1920
#define IMX307_HEIGHT 1080
#define IMX307_HTS_DEF 2200
#define IMX307_VTS_DEF 1125
#define IMX307_VTS_MAX 0x1FFFF
#define IMX307_REG_SHS1_H 0x3022
#define IMX307_REG_SHS1_M 0x3021
#define IMX307_REG_SHS1_L 0x3020

#define IMX307_REG_VTS_H 0x301a
#define IMX307_REG_VTS_M 0x3019
#define IMX307_REG_VTS_L 0x3018

#define IMX307_EXPOSURE_MIN 1
#define IMX307_EXPOSURE_MAX (IMX307_VTS_DEF - 4)

#define IMX307_REG_TEST_PATTERN 0x308c
#define IMX307_TEST_PATTERN_ENABLE BIT(0)
#define IMX307_REG_LF_GAIN 0x3014

#define IMX307_FLIP_REG 0x3007
#define MIRROR_BIT_MASK BIT(1)
#define FLIP_BIT_MASK BIT(0)

#define IMX307_REG_CTRL_MODE 0x3000
#define IMX307_MODE_STANDBY 0x01
#define IMX307_MODE_STREAMING 0x00

#define IMX307_REG_CHIP_ID_1 0x301e
#define IMX307_REG_CHIP_ID_2 0x301f

/* Custom RHS1 control (stub only - same ID as imx678 for userspace compatibility) */
#define IMX307_CID_BASE (V4L2_CID_USER_BASE + 0x2000)
#define IMX307_CID_CUSTOM_RHS1 (IMX307_CID_BASE + 13)

#define IMX307_FETCH_HIGH_BYTE_EXP(VAL)	(((VAL) >> 16) & 0x0F)
#define IMX307_FETCH_MID_BYTE_EXP(VAL) (((VAL) >> 8) & 0xFF)
#define IMX307_FETCH_LOW_BYTE_EXP(VAL) ((VAL) & 0xFF)

#define IMX307_FETCH_HIGH_BYTE_VTS(VAL)	(((VAL) >> 16) & 0x03)
#define IMX307_FETCH_MID_BYTE_VTS(VAL) (((VAL) >> 8) & 0xFF)
#define IMX307_FETCH_LOW_BYTE_VTS(VAL) ((VAL) & 0xFF)

enum { TEST_PATTERN_DISABLED = 0,
		TEST_PATTERN_ALL_000H,
		TEST_PATTERN_SEQUENCE_1,
		TEST_PATTERN_HORIZONTAL_COLOR_BARS,
		TEST_PATTERN_VERTICAL_COLOR_BARS,
		TEST_PATTERN_SEQUENCE_2,
		TEST_PATTERN_GRADIATION_1,
		TEST_PATTERN_GRADIATION_2,
		TEST_PATTERN_VSP_50H,
};

static const char * const imx307_test_pattern_menu[] = {
	"Disabled",
	"All 000h Pattern",
	"Sequence Pattern 1",
	"Horizontal Color Bars",
	"Vertical Color Bars",
	"Sequence Pattern 2",
	"Gradiation pattern 1",
	"Gradiation pattern 2",
	"Vertical Stripe (555h / 000h)",
};

struct imx307_reg {
	u16 address;
	u8 val;
};

struct imx307_reg_list {
	u32 num_of_regs;
	const struct imx307_reg *regs;
};

struct imx307_mode {
	u32 width;
	u32 height;
	u32 code;
	u32 hts_def;
	u32 vts_def;
	u32 vts_min;
	u32 vts_max;
	u32 link_freq_idx;
	u32 lanes;
	u32 bpp;
	struct imx307_reg_list reg_list;
	struct v4l2_fract frame_interval;
};

struct imx307 {
	struct device *dev;
	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct gpio_desc *reset_gpio;
	struct clk *inclk;
	struct v4l2_ctrl_handler ctrl_handler;
	const struct imx307_mode *cur_mode;
	struct mutex mutex;
	bool streaming;
	u32 cur_vts;

	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *anal_gain;
	struct v4l2_ctrl *test_pattern;
	struct v4l2_ctrl *h_flip;
	struct v4l2_ctrl *v_flip;
};

/*
 * Xclk 37.125Mhz
 * max_framerate 30fps
 * mipi_datarate per lane 222.75Mbps 4 lane
 */
static const struct imx307_reg imx307_linear_1920x1080_mipi_regs[] = {
	{0x3005, 0x01}, /* ADBIT: 1 (12 bit) */
	{0x3007, 0x00}, /* VREVERSE: 0 (0 vertical flip),  HREVERSE: 0 (no horizontal flip) */
	{0x3009, 0x02}, /* HCG mode */
	{0x3018, 0x65}, /* VMAX initial value 465 */
	{0x3019, 0x04}, /* VMAX initial value 465 */
	{0x301C, 0x30}, /* HMAX 1130 / 14A0 */
	{0x301D, 0x11}, /* HMAX 1130 / 14A0 */
	{0x3046, 0x01}, /* ODBIT: 1 (12 bit) */
	{0x305C, 0x18}, /* INCKSEL1 01200318 (37.125 mhz) */
	{0x305D, 0x03}, /* INCKSEL2 01200318 (37.125 mhz) */
	{0x305E, 0x20}, /* INCKSEL3 01200318 (37.125 mhz) */
	{0x305F, 0x01}, /* INCKSEL4 01200318 (37.125 mhz) */
	{0x3129, 0x1d}, /* ADBIT1: 1D (10bit) */
	{0x315E, 0x1A}, /* INCKSEL4 1A (37.125 mhz) */
	{0x3164, 0x1A}, /* INCKSEL5 1A (37.125 mhz) */
	{0x317C, 0x00}, /* ADBIT2 0 (12bit) */
	{0x31EC, 0x0E}, /* ADBIT3 0E (12bit) */
	{0x3405, 0x20}, /* REPETITION */
	{0x3407, 0x03}, /* PHYSICAL LANE NUM: 3 (4 lanes) */
	{0x3418, 0x38}, /* Y_OUT_SIZE = 0x0438 = 1080 */
	{0x3419, 0x04}, /* Y_OUT_SIZE = 0x0438 = 1080 */
	{0x3441, 0x0c}, /* CSI_DTT_FMT: 0C0C (RAW12) */ 
	{0x3442, 0x0c}, /* CSI_DTT_FMT: 0C0C (RAW12) */ 
	{0x3443, 0x03}, /* CSI_LANE_MODE: 03 (4 lanes) */
	{0x3444, 0x20}, /* EXTCK_FREQ: 2520 (37.125 MHz) */
	{0x3445, 0x25}, /* EXTCK_FREQ: 2520 (37.125 MHz) */
	{0x3446, 0x47}, /* TCLKPOST: 47 (30/25 frame/s) */
	{0x3447, 0x00}, /* TCLKPOST: 47 (30/25 frame/s) */
	{0x3448, 0x1f}, /* THSZERO: 1F (30/25 frame/s) */
	{0x3449, 0x00}, /* THSZERO: 1F (30/25 frame/s) */
	{0x344A, 0x17}, /* THSPREPARE: 017 (30/25 frame/s) */
	{0x344B, 0x00}, /* THSPREPARE: 017 (30/25 frame/s) */
	{0x344C, 0x0F}, /* TCLKTRAIL: 0F (30/25 frame/s) */
	{0x344D, 0x00}, /* TCLKTRAIL: 0F (30/25 frame/s) */
	{0x344E, 0x17}, /* TTHSTRAIL: 017 (30/25 frame/s) */
	{0x344F, 0x00}, /* TTHSTRAIL: 017 (30/25 frame/s) */
	{0x3450, 0x47}, /* TCLKZERO: 047 (30/25 frame/s) */
	{0x3451, 0x00}, /* TCLKZERO: 047 (30/25 frame/s) */
	{0x3452, 0x0F}, /* TCLKPREPARE: 0F (30/25 frame/s) */
	{0x3453, 0x00}, /* TCLKPREPARE: 0F (30/25 frame/s) */
	{0x3454, 0x0f}, /* TLPX: 0F (30/25 frame/s) */
	{0x3455, 0x00}, /* TLPX: 0F (30/25 frame/s) */
	{0x3472, 0x80}, /* X_OUT_SIZE = 0x0780 = 1920 */
	{0x3473, 0x07},	/* X_OUT_SIZE = 0x0780 = 1920 */
	{0x3480, 0x49}, /* INCKSEL7: 49 (37.125 mhz) */
	{0x3002, 0x00}, /* INCKSEL7: 49 (37.125 mhz) */
};

static const s64 link_freq[] = {
	IMX307_LINK_FREQ_111M, IMX307_LINK_FREQ_222M,
};

static const struct imx307_mode supported_modes[] = {
	/* 1920x1080 @ 30fps, 4 lane, 12-bit */
	{
		.width = IMX307_WIDTH,
		.height = IMX307_HEIGHT,
		.hts_def = IMX307_HTS_DEF,
		.vts_def = IMX307_VTS_DEF,
		.vts_min = IMX307_VTS_DEF,
		.vts_max = IMX307_VTS_MAX,
		.link_freq_idx = 1,  /* 222.75 MHz */
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.lanes = IMX307_LANES,
		.bpp = IMX307_BITS_PER_PIXEL,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(imx307_linear_1920x1080_mipi_regs),
			.regs = imx307_linear_1920x1080_mipi_regs,
		},
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		},
	}
};

static inline struct imx307 *to_imx307(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct imx307, sd);
}

/**
 * imx307_read_reg() - Read registers.
 * @imx307: pointer to imx307 device
 * @reg: register address
 * @len: length of bytes to read. Max supported bytes is 4
 * @val: pointer to register value to be filled.
 *
 * Big endian register addresses with little endian values.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx307_read_reg(struct imx307 *imx307, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx307->sd);
	struct i2c_msg msgs[2] = { { 0 } };
	u8 addr_buf[2] = { 0 };
	u8 data_buf[4] = { 0 };
	int ret;

	if (WARN_ON(len > 4))
		return -EINVAL;

	put_unaligned_be16(reg, addr_buf);

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = ARRAY_SIZE(addr_buf);
	msgs[0].buf = addr_buf;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data_buf;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));

	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = get_unaligned_le32(data_buf);

	return 0;
}

/**
 * imx307_write_reg() - Write register
 * @imx307: pointer to imx307 device
 * @reg: register address
 * @len: length of bytes. Max supported bytes is 4
 * @val: register value
 *
 * Big endian register addresses with little endian values.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx307_write_reg(struct imx307 *imx307, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx307->sd);
	u8 buf[6] = { 0 };
	int res = 0;

	if (WARN_ON(len > 4))
	{
		return -EINVAL;
	}

	put_unaligned_be16(reg, buf);
	put_unaligned_le32(val, buf + 2);
	res = i2c_master_send(client, buf, len + 2);
	if (res != len + 2) {
		return -EIO;
	}

	return 0;
}

static int imx307_write_regs(struct imx307 *imx307,
				const struct imx307_reg *regs, u32 len)
{
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = imx307_write_reg(imx307, regs[i].address, IMX307_REG_VALUE_08BIT, regs[i].val);
		if (ret) {
			dev_err(imx307->dev, "Failed to write reg[%u] 0x%04x = 0x%02x: %d\n",
				i, regs[i].address, regs[i].val, ret);
			return ret;
		}
	}

	return 0;
}

static int imx307_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx307 *sensor = to_imx307(sd);
	int ret;

	/* Hold XCLR (reset) low to apply system clear. 
	* This ensures all internal registers are cleared and undefined states are avoided.
	* Minimum 500 ns according to spec — use 1 µs for safe margin. */
	if (sensor->reset_gpio)
		gpiod_set_value_cansleep(sensor->reset_gpio, 0);
	udelay(1);  // 1 µs = 1000 ns

	/* Start master clock */
	ret = clk_prepare_enable(sensor->inclk);
	if (ret) {
		dev_err(dev, "Failed to enable clock: %d\n", ret);
		return ret;
	}

	/* Release XCLR (set high) to complete the system clear. */
	if (sensor->reset_gpio)
		gpiod_set_value_cansleep(sensor->reset_gpio, 1);

	/*
	 * Wait for clock and sensor stabilization after XCLR goes high.
	 * Spec requires ≥20 µs before using the sensor
	 */
	usleep_range(20, 25);

	return 0;
}

static int imx307_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx307 *sensor = to_imx307(sd);

	if (sensor->reset_gpio)
		gpiod_set_value_cansleep(sensor->reset_gpio, 0);

	clk_disable_unprepare(sensor->inclk);
	return 0;
}

static int imx307_start_streaming(struct imx307 *imx307)
{
	int ret;

	/* Save all writable control values before handler setup may overwrite them */
	struct hailo_ctrl_snapshot snap;

	hailo_ctrl_snapshot_save(&imx307->ctrl_handler, &snap);

	/* Write sensor mode registers */
	ret = imx307_write_regs(imx307, imx307->cur_mode->reg_list.regs,
				imx307->cur_mode->reg_list.num_of_regs);
	if (ret) {
		dev_err(imx307->dev, "Failed to write mode registers: %d\n", ret);
		return ret;
	}

	/* Setup handler: pushes all cur.val to hardware via s_ctrl callbacks */
	ret = __v4l2_ctrl_handler_setup(&imx307->ctrl_handler);
	if (ret) {
		dev_err(imx307->dev, "Failed to apply v4l2 controls: %d\n", ret);
		return ret;
	}

	/* Restore any control values corrupted by handler_setup side effects
	 * (e.g. VBLANK s_ctrl calling __v4l2_ctrl_modify_range on exposure) */
	hailo_ctrl_snapshot_restore(&snap);

	/* Start streaming - set STANDBY to 0 */
	ret = imx307_write_reg(imx307, IMX307_REG_CTRL_MODE, IMX307_REG_VALUE_08BIT, IMX307_MODE_STREAMING);
	if (ret) {
		dev_err(imx307->dev, "Failed to start streaming (set STANDBY to 0): %d\n", ret);
		return ret;
	}

	dev_info(imx307->dev, "imx307: stream started");
	return 0;
}

static int imx307_stop_streaming(struct imx307 *imx307)
{
	int ret;

	/* Set STANDBY register to Standby (1) */
	ret = imx307_write_reg(imx307, IMX307_REG_CTRL_MODE, IMX307_REG_VALUE_08BIT, IMX307_MODE_STANDBY);

	if (ret) {
		dev_err(imx307->dev, "Failed to stop stream (set STANDBY to 1): %d\n", ret);
		return ret;
	}

	dev_info(imx307->dev, "imx307: stream stopped");
	return 0;
}

static int imx307_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx307 *imx307 = to_imx307(sd);
	struct i2c_client *client = imx307->client;
	int ret = 0;

	mutex_lock(&imx307->mutex);
	if (imx307->streaming == enable)
		goto unlock_and_return;

	if (enable) {
		ret = pm_runtime_resume_and_get(&client->dev);
		if (ret < 0)
			goto unlock_and_return;

		ret = imx307_start_streaming(imx307);
		if (ret) {
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		imx307_stop_streaming(imx307);
		pm_runtime_put(&client->dev);
	}

	imx307->streaming = enable;
unlock_and_return:
	mutex_unlock(&imx307->mutex);

	return ret;

}

static int imx307_g_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *fi)
{
	struct imx307 *imx307 = to_imx307(sd);

	mutex_lock(&imx307->mutex);
	fi->interval = imx307->cur_mode->frame_interval;
	mutex_unlock(&imx307->mutex);

	return 0;
}

static int imx307_s_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *fi)
{
	struct imx307 *imx307 = to_imx307(sd);

	mutex_lock(&imx307->mutex);
	/* Only 30fps supported for now */
	fi->interval = imx307->cur_mode->frame_interval;
	mutex_unlock(&imx307->mutex);

	return 0;
}

static const struct v4l2_subdev_video_ops imx307_video_ops = {
	.s_stream = imx307_set_stream,
	.g_frame_interval = imx307_g_frame_interval,
	.s_frame_interval = imx307_s_frame_interval,
};

static int imx307_detect(struct imx307 *imx307)
{
	int ret;
	u32 id_1, id_2;

	ret = imx307_read_reg(imx307, IMX307_REG_CHIP_ID_1, IMX307_REG_VALUE_08BIT, &id_1);
	if (ret) {
	    dev_err(imx307->dev, "failed to read sensor id_1 register, ret %d\n",
			ret);
	    return ret;
	}

	ret = imx307_read_reg(imx307, IMX307_REG_CHIP_ID_2, IMX307_REG_VALUE_08BIT, &id_2);
	if (ret) {
        dev_err(imx307->dev, "failed to read sensor id_2 register, ret %d\n",
            ret);
        return ret;
	}

	if(id_1 != SENSOR_ID_IMX307 ||
		id_2 != IMX307_SENSOR_ID_VAL) {
		dev_info(imx307->dev,
			"Sensor ID wrong (could be corrupted or sensor is not connected) expected 0x%02x%02x, found 0x%02x%02x\n",
			SENSOR_ID_IMX307, IMX307_SENSOR_ID_VAL, id_1, id_2);
		return -ENXIO;
	}

	return 0;
}

static const struct imx307_reg imx307_tpg_enable_regs[] = {
	{ 0x300a, 0x00 },
	{ 0x300e, 0x00 },
};

static const struct imx307_reg imx307_tpg_disable_regs[] = {
	{ 0x300a, 0x3c },
	{ 0x300e, 0x01 },
};

static int imx307_set_test_pattern(struct imx307 *imx307, int val)
{
	u32 reg_val;
	int ret;

	ret = imx307_read_reg(imx307,
			IMX307_REG_TEST_PATTERN,
			IMX307_REG_VALUE_08BIT,
			&reg_val);
	if (ret) {
		dev_err(imx307->dev, "failed to read test pattern reg: %d\n", ret);
		return ret;
	}

	if (val == TEST_PATTERN_DISABLED) {
		ret = imx307_write_reg(imx307,
					IMX307_REG_TEST_PATTERN,
					IMX307_REG_VALUE_08BIT,
					0x00);
		if (ret)
			return ret;

		return imx307_write_regs(imx307,
						imx307_tpg_disable_regs,
						ARRAY_SIZE(imx307_tpg_disable_regs));
	}

	/* pattern enabled */
	reg_val = ((val - 1) << 4) | IMX307_TEST_PATTERN_ENABLE;

	ret = imx307_write_reg(imx307,
				IMX307_REG_TEST_PATTERN,
				IMX307_REG_VALUE_08BIT,
				reg_val);
	if (ret)
		return ret;

	return imx307_write_regs(imx307,
					imx307_tpg_enable_regs,
					ARRAY_SIZE(imx307_tpg_enable_regs));
}

static int imx307_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx307 *imx307 = container_of(ctrl->handler, struct imx307, ctrl_handler);
	struct i2c_client *client = imx307->client;
	s64 max;
	int ret = 0;
	u32 shs1 = 0;
	u32 vts = 0;
	u32 val = 0;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = imx307->cur_mode->height + ctrl->val - 2;
		__v4l2_ctrl_modify_range(imx307->exposure,
						imx307->exposure->minimum, max,
						imx307->exposure->step,
						imx307->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		shs1 = imx307->cur_vts - (ctrl->val + 1);
		ret = imx307_write_reg(imx307, IMX307_REG_SHS1_H, IMX307_REG_VALUE_08BIT, IMX307_FETCH_HIGH_BYTE_EXP(shs1));
		ret |= imx307_write_reg(imx307, IMX307_REG_SHS1_M, IMX307_REG_VALUE_08BIT, IMX307_FETCH_MID_BYTE_EXP(shs1));
		ret |= imx307_write_reg(imx307, IMX307_REG_SHS1_L, IMX307_REG_VALUE_08BIT, IMX307_FETCH_LOW_BYTE_EXP(shs1));
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		ret = imx307_write_reg(imx307, IMX307_REG_LF_GAIN, IMX307_REG_VALUE_08BIT, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		vts = ctrl->val + imx307->cur_mode->height;
		imx307->cur_vts = vts;
		ret = imx307_write_reg(imx307, IMX307_REG_VTS_H, IMX307_REG_VALUE_08BIT, IMX307_FETCH_HIGH_BYTE_VTS(vts));
		ret |= imx307_write_reg(imx307, IMX307_REG_VTS_M, IMX307_REG_VALUE_08BIT, IMX307_FETCH_MID_BYTE_VTS(vts));
		ret |= imx307_write_reg(imx307, IMX307_REG_VTS_L, IMX307_REG_VALUE_08BIT, IMX307_FETCH_LOW_BYTE_VTS(vts));
		break;
	case V4L2_CID_HFLIP:
		ret = imx307_read_reg(imx307, IMX307_FLIP_REG, IMX307_REG_VALUE_08BIT, &val);
		if (!ret){
			if (ctrl->val)
				val |= MIRROR_BIT_MASK;
			else
				val &= ~MIRROR_BIT_MASK;
			ret |= imx307_write_reg(imx307, IMX307_FLIP_REG, IMX307_REG_VALUE_08BIT, val);
		}
		break;
	case V4L2_CID_TEST_PATTERN:
		if (!pm_runtime_get_if_in_use(imx307->dev))
			return 0;

		ret = imx307_set_test_pattern(imx307, ctrl->val);

		pm_runtime_put(imx307->dev);
		break;
	case V4L2_CID_VFLIP:
		ret = imx307_read_reg(imx307, IMX307_FLIP_REG, IMX307_REG_VALUE_08BIT, &val);
		if (!ret){
			if (ctrl->val)
				val |= FLIP_BIT_MASK;
			else
				val &= ~FLIP_BIT_MASK;
			ret |= imx307_write_reg(imx307, IMX307_FLIP_REG, IMX307_REG_VALUE_08BIT, val);
		}
		break;
	case IMX307_CID_CUSTOM_RHS1:
		/* Stub: control accepted but not implemented for this sensor */
		ret = 0;
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
				__func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops imx307_ctrl_ops = {
	.s_ctrl = imx307_set_ctrl,
};

static int imx307_enum_mbus_code(struct v4l2_subdev *sd,
					struct v4l2_subdev_state *state,
					struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SRGGB12_1X12;
	return 0;
}

static int imx307_enum_frame_size(struct v4l2_subdev *sd,
					struct v4l2_subdev_state *state,
					struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SRGGB12_1X12)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static int imx307_get_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_format *fmt)
{
	struct imx307 *imx307 = to_imx307(sd);
	const struct imx307_mode *mode = imx307->cur_mode;

	mutex_lock(&imx307->mutex);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = mode->code;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;
	fmt->format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->format.quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->format.xfer_func = V4L2_XFER_FUNC_NONE;

	mutex_unlock(&imx307->mutex);

	return 0;
}

static int imx307_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_format *fmt)
{
	struct imx307 *imx307 = to_imx307(sd);

	/* For now, only support FHD SDR */
	const struct imx307_mode *mode = &supported_modes[0];

	mutex_lock(&imx307->mutex);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = mode->code;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;
	fmt->format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->format.quantization = V4L2_QUANTIZATION_DEFAULT;
	fmt->format.xfer_func = V4L2_XFER_FUNC_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, state, fmt->pad) = fmt->format;
	} else {
		imx307->cur_mode = mode;
		imx307->cur_vts = mode->vts_def;
	}

	mutex_unlock(&imx307->mutex);

	return 0;
}

static int imx307_init_cfg(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = 0,
	};

	return imx307_set_fmt(sd, state, &fmt);
}

static const struct v4l2_subdev_pad_ops imx307_pad_ops = {
	.init_cfg = imx307_init_cfg,
	.enum_mbus_code = imx307_enum_mbus_code,
	.enum_frame_size = imx307_enum_frame_size,
	.get_fmt = imx307_get_fmt,
	.set_fmt = imx307_set_fmt,
};

static const struct v4l2_subdev_ops imx307_subdev_ops = {
	.video = &imx307_video_ops,
	.pad = &imx307_pad_ops,
};

static int imx307_parse_hw_config(struct imx307 *sensor)
{
	struct fwnode_handle *fwnode = dev_fwnode(sensor->dev);
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY,
	};
	struct fwnode_handle *ep;
	unsigned long rate;
	int ret, i, j;

	if (!fwnode)
		return -ENXIO;

	/* Request optional reset pin */
	sensor->reset_gpio = devm_gpiod_get_optional(sensor->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(sensor->reset_gpio)) {
		dev_err(sensor->dev, "Failed to get reset GPIO\n");
		return PTR_ERR(sensor->reset_gpio);
	}

	/* Get sensor input clock */
	sensor->inclk = devm_clk_get(sensor->dev, NULL);
	if (IS_ERR(sensor->inclk)) {
		dev_err(sensor->dev, "Failed to get inclk\n");
		return PTR_ERR(sensor->inclk);
	}

	rate = clk_get_rate(sensor->inclk);
	if (rate != IMX307_INCLK_RATE) {
		dev_err(sensor->dev,
			"inclk mismatch: got %lu, expected %u\n",
			rate, IMX307_INCLK_RATE);
		return -EINVAL;
	}

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -ENXIO;

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &ep_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	/* Validate lane count */
	if (ep_cfg.bus.mipi_csi2.num_data_lanes != IMX307_LANES) {
		dev_err(sensor->dev,
			"unsupported lane count %u\n",
			ep_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto out_free;
	}

	/* Validate link frequencies */
	if (!ep_cfg.nr_of_link_frequencies) {
		dev_err(sensor->dev, "no link-frequencies defined\n");
		ret = -EINVAL;
		goto out_free;
	}

	for (i = 0; i < ARRAY_SIZE(link_freq); i++) {
		for (j = 0; j < ep_cfg.nr_of_link_frequencies; j++) {
			if (ep_cfg.link_frequencies[j] == link_freq[i])
				break;
		}
		if (j == ep_cfg.nr_of_link_frequencies) {
			dev_err(sensor->dev,
				"required link freq %lld missing in DT\n",
				link_freq[i]);
			ret = -EINVAL;
			goto out_free;
		}
    }

    ret = 0;

out_free:
	v4l2_fwnode_endpoint_free(&ep_cfg);
	return ret;
}

static int imx307_init_controls(struct imx307 *imx307)
{
	struct v4l2_ctrl_handler *ctrl_hdlr = &imx307->ctrl_handler;
	const struct imx307_mode *mode = imx307->cur_mode;
	s64 pixel_rate;
	u32 hblank, vblank_def;
	int ret;

	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 13);
	if (ret) {
		dev_err(imx307->dev, "failed to init control handler (%d)", ret);
		return ret;
	}

	ctrl_hdlr->lock = &imx307->mutex;

	/* Custom RHS1 stub (not implemented for this sensor) */
	{
		struct v4l2_ctrl_config custom_rhs1_cfg = {
			.ops = &imx307_ctrl_ops,
			.id = IMX307_CID_CUSTOM_RHS1,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_UPDATE,
			.name = "custom_rhs1",
			.step = 1,
			.min = 0,
			.max = 65535,
			.def = 0,
		};
		v4l2_ctrl_new_custom(ctrl_hdlr, &custom_rhs1_cfg, NULL);
	}

	/* Initialize exposure and gain */
	imx307->exposure = v4l2_ctrl_new_std(
		ctrl_hdlr, &imx307_ctrl_ops, V4L2_CID_EXPOSURE,
		IMX307_EXPOSURE_MIN,
		IMX307_EXPOSURE_MAX,
		IMX307_EXPOSURE_STEP,
		IMX307_EXPOSURE_DEFAULT);

	imx307->anal_gain = v4l2_ctrl_new_std(
		ctrl_hdlr, &imx307_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
		IMX307_MIN_GAIN,
		IMX307_MAX_GAIN,
		IMX307_GAIN_STEP,
		IMX307_GAIN_DEFAULT);

	vblank_def = mode->vts_def - mode->height;
	imx307->vblank = v4l2_ctrl_new_std(
		ctrl_hdlr, &imx307_ctrl_ops, V4L2_CID_VBLANK,
		mode->vts_min - mode->height,
		mode->vts_max - mode->height,
		1, vblank_def);

	/* Link frequency (read-only) */
	imx307->link_freq = v4l2_ctrl_new_int_menu(
		ctrl_hdlr, NULL, V4L2_CID_LINK_FREQ,
		ARRAY_SIZE(link_freq) - 1,
		mode->link_freq_idx,
		link_freq);
	if (imx307->link_freq)
		imx307->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx307->test_pattern = v4l2_ctrl_new_std_menu_items(ctrl_hdlr,
				&imx307_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(imx307_test_pattern_menu) - 1,
				0, 0, imx307_test_pattern_menu);


	/* pixel_rate = link_freq * 2 * lanes / bits_per_pixe */
	pixel_rate = link_freq[mode->link_freq_idx] * 2 * mode->lanes / mode->bpp;
	imx307->pixel_rate = v4l2_ctrl_new_std(
		ctrl_hdlr, NULL, V4L2_CID_PIXEL_RATE,
		pixel_rate, pixel_rate, 1, pixel_rate);
	if (imx307->pixel_rate)
		imx307->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* Horizontal blanking (read-only) */
	hblank = mode->hts_def - mode->width;
	imx307->hblank = v4l2_ctrl_new_std(
		ctrl_hdlr, NULL, V4L2_CID_HBLANK,
		hblank, hblank, 1, hblank);
	if (imx307->hblank)
		imx307->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx307->h_flip = v4l2_ctrl_new_std(ctrl_hdlr, &imx307_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);

	imx307->v_flip = v4l2_ctrl_new_std(ctrl_hdlr, &imx307_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(imx307->dev, "Control init failed: %d\n", ret);
		v4l2_ctrl_handler_free(ctrl_hdlr);
		return ret;
	}

	imx307->sd.ctrl_handler = ctrl_hdlr;
	return 0;
}

static int imx307_probe(struct i2c_client *client)
{
	struct imx307 *imx307;
	int ret;  

	imx307 = devm_kzalloc(&client->dev, sizeof(*imx307), GFP_KERNEL);
	if (!imx307)
		return -ENOMEM;

	imx307->dev = &client->dev;
	dev_info(imx307->dev, "probe started");

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&imx307->sd, client, &imx307_subdev_ops);

	ret = imx307_parse_hw_config(imx307);
	if (ret) {
		dev_err(imx307->dev, "HW configuration invalid\n");
		return ret;
	}

	imx307->client = client;

	mutex_init(&imx307->mutex);

	/* Power on and detect sensor */
	ret = imx307_power_on(imx307->dev);
	if (ret) {
		dev_err(imx307->dev, "Failed to power on\n");
		goto error_mutex;
	}

	/* Detect sensor id */
	ret = imx307_detect(imx307);
	if (ret) {
		dev_err(imx307->dev, "sensor detection failed: %d\n", ret);
		goto error_power;
	}

	/* Set default mode to FHD SDR */
	imx307->cur_mode = &supported_modes[0];
	imx307->cur_vts = imx307->cur_mode->vts_def;

	/* Initialize controls */
	ret = imx307_init_controls(imx307);
	if (ret) {
		dev_err(imx307->dev, "failed to init controls: %d", ret);
		goto error_power;
	}

	/* Setup media pad */
	imx307->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	imx307->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	imx307->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&imx307->sd.entity, 1, &imx307->pad);
	if (ret) {
		dev_err(imx307->dev, "Failed to init media pads\n");
		goto error_ctrl;
	}

	ret = v4l2_async_register_subdev_sensor(&imx307->sd);
	if (ret) {
		dev_err(imx307->dev, "Failed to register subdev\n");
		goto error_media;
	}

	pm_runtime_set_active(imx307->dev);
	pm_runtime_enable(imx307->dev);
	pm_runtime_idle(imx307->dev);

	dev_info(imx307->dev, "probe finished successfully");
	return 0;

error_media:
	media_entity_cleanup(&imx307->sd.entity);
error_ctrl:
	v4l2_ctrl_handler_free(&imx307->ctrl_handler);
error_power:
	imx307_power_off(imx307->dev);
error_mutex:
	mutex_destroy(&imx307->mutex);

	return ret;
}

static int imx307_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx307 *sensor = to_imx307(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx307_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	mutex_destroy(&sensor->mutex);

	return 0;
}

static const struct dev_pm_ops imx307_pm_ops = {
	SET_RUNTIME_PM_OPS(imx307_power_off, imx307_power_on, NULL)
};

static const struct of_device_id imx307_of_match[] = {
	{.compatible = "sony,imx307"},
	{},
};

MODULE_DEVICE_TABLE(of, imx307_of_match);

static struct i2c_driver imx307_driver = {
	.probe_new = imx307_probe,
	.remove = imx307_remove,
	.driver = {
		.name = "imx307",
		.pm = &imx307_pm_ops,
		.of_match_table = imx307_of_match,
	},
};

module_i2c_driver(imx307_driver);

MODULE_AUTHOR("Daniel Varennikov <danielv@hailo.ai>");
MODULE_DESCRIPTION("Sony imx307 sensor driver");
MODULE_LICENSE("GPL");