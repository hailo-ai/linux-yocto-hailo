// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sony imx678 sensor driver
 *
 * Copyright (C) 2021 Intel Corporation
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
#include <linux/kernel.h>

#define DEFAULT_MODE_IDX 0

/* Streaming Mode */
#define IMX678_REG_MODE_SELECT 0x3000
#define IMX678_MODE_STANDBY 0x01
#define IMX678_MODE_STREAMING 0x00

/* Lines per frame */
#define IMX678_REG_LPFR 0x3028 // VMAX

/* Chip ID */
#define IMX678_REG_ID 0x0
// #define IMX678_REG_ID		0x4d1c
#define IMX678_ID 0xa6 // from SupportPackage_E_Rev2.0 20p

/* Exposure control */
#define IMX678_REG_SHUTTER 0x3050
#define IMX678_EXPOSURE_MIN 1
#define IMX678_EXPOSURE_OFFSET 5
#define IMX678_EXPOSURE_STEP 1
#define IMX678_EXPOSURE_DEFAULT 0x0648

/* Analog gain control */
#define IMX678_REG_AGAIN 0x3070
#define IMX678_REG_AGAIN1 0x3072
#define IMX678_REG_AGAIN2 0x3074
#define IMX678_AGAIN_MIN 0
#define IMX678_AGAIN_MAX 240
#define IMX678_AGAIN_STEP 1
#define IMX678_AGAIN_DEFAULT 0

/* Wide Dynamic Range control */
#define IMX678_WDR_MIN 0
#define IMX678_WDR_MAX 1
#define IMX678_WDR_STEP 1
#define IMX678_WDR_DEFAULT 0

/* Group hold register */
#define IMX678_REG_HOLD 0x3001

/* Input clock rate */
#define IMX678_INCLK_RATE 24000000

/* CSI2 HW configuration */
#define IMX678_LINK_FREQ 891000000
#define IMX678_NUM_DATA_LANES 4

#define IMX678_REG_MIN 0x00
#define IMX678_REG_MAX 0xfffff

#define IMX678_TPG_EN_DUOUT 0x30e0 /* TEST PATTERN ENABLE */
#define IMX678_TPG_PATSEL_DUOUT 0x30e2 /*Patsel mode */
#define IMX678_TPG_COLOR_WIDTH 0x30e4 /*color width */

static int imx678_set_ctrl(struct v4l2_ctrl *ctrl);

/*
 * imx678 test pattern related structure
 */
enum { TEST_PATTERN_DISABLED = 0,
       TEST_PATTERN_ALL_000H,
       TEST_PATTERN_ALL_FFFH,
       TEST_PATTERN_ALL_555H,
       TEST_PATTERN_ALL_AAAH,
       TEST_PATTERN_VSP_5AH, /* VERTICAL STRIPE PATTERN 555H/AAAH */
       TEST_PATTERN_VSP_A5H, /* VERTICAL STRIPE PATTERN AAAH/555H */
       TEST_PATTERN_VSP_05H, /* VERTICAL STRIPE PATTERN 000H/555H */
       TEST_PATTERN_VSP_50H, /* VERTICAL STRIPE PATTERN 555H/000H */
       TEST_PATTERN_VSP_0FH, /* VERTICAL STRIPE PATTERN 000H/FFFH */
       TEST_PATTERN_VSP_F0H, /* VERTICAL STRIPE PATTERN FFFH/000H */
       TEST_PATTERN_H_COLOR_BARS,
       TEST_PATTERN_V_COLOR_BARS,
};

/**
 * enum imx678_test_pattern_menu - imx678 test pattern options
 */
static const char *const imx678_test_pattern_menu[] = {
	"Disabled",
	"All 000h Pattern",
	"All FFFh Pattern",
	"All 555h Pattern",
	"All AAAh Pattern",
	"Vertical Stripe (555h / AAAh)",
	"Vertical Stripe (AAAh / 555h)",
	"Vertical Stripe (000h / 555h)",
	"Vertical Stripe (555h / 000h)",
	"Vertical Stripe (000h / FFFh)",
	"Vertical Stripe (FFFh / 000h)",
	"Horizontal Color Bars",
	"Vertical Color Bars",
};

/* V4l2 subdevice control ops*/
static const struct v4l2_ctrl_ops imx678_ctrl_ops = {
	.s_ctrl = imx678_set_ctrl,
};

/**
 * struct imx678_reg - imx678 sensor register
 * @address: Register address
 * @val: Register value
 */
struct imx678_reg {
	u16 address;
	u8 val;
};

/**
 * struct imx678_reg_list - imx678 sensor register list
 * @num_of_regs: Number of registers in the list
 * @regs: Pointer to register list
 */
struct imx678_reg_list {
	u32 num_of_regs;
	const struct imx678_reg *regs;
};

/**
 * struct imx678_mode - imx678 sensor mode structure
 * @width: Frame width
 * @height: Frame height
 * @code: Format code
 * @hblank: Horizontal blanking in lines
 * @vblank: Vertical blanking in lines
 * @vblank_min: Minimal vertical blanking in lines
 * @vblank_max: Maximum vertical blanking in lines
 * @pclk: Sensor pixel clock
 * @link_freq_idx: Link frequency index
 * @reg_list: Register list for sensor mode
 */
struct imx678_mode {
	u32 width;
	u32 height;
	u32 code;
	u32 hblank;
	u32 vblank;
	u32 vblank_min;
	u32 vblank_max;
	u64 pclk;
	u32 link_freq_idx;
	struct imx678_reg_list reg_list;
	struct v4l2_fract frame_interval;
};

/**
 * struct imx678 - imx678 sensor device structure
 * @dev: Pointer to generic device
 * @client: Pointer to i2c client
 * @sd: V4L2 sub-device
 * @pad: Media pad. Only one pad supported
 * @reset_gpio: Sensor reset gpio
 * @inclk: Sensor input clock
 * @ctrl_handler: V4L2 control handler
 * @link_freq_ctrl: Pointer to link frequency control
 * @pclk_ctrl: Pointer to pixel clock control
 * @hblank_ctrl: Pointer to horizontal blanking control
 * @vblank_ctrl: Pointer to vertical blanking control
 * @test_pattern_ctrl: pointer to test pattern control
 * @mode_sel_ctrl: pointer to mode select control
 * @exp_ctrl: Pointer to exposure control
 * @again_ctrl: Pointer to analog gain control
 * @vblank: Vertical blanking in lines
 * @cur_mode: Pointer to current selected sensor mode
 * @mutex: Mutex for serializing sensor controls
 * @streaming: Flag indicating streaming state
 */
struct imx678 {
	struct device *dev;
	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct gpio_desc *reset_gpio;
	struct clk *inclk;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq_ctrl;
	struct v4l2_ctrl *pclk_ctrl;
	struct v4l2_ctrl *hblank_ctrl;
	struct v4l2_ctrl *vblank_ctrl;
	struct v4l2_ctrl *test_pattern_ctrl;
	struct v4l2_ctrl *mode_sel_ctrl;
	struct {
		struct v4l2_ctrl *exp_ctrl;
		struct v4l2_ctrl *again_ctrl;
	};
	u32 vblank;
	const struct imx678_mode *cur_mode;
	struct mutex mutex;
	bool streaming;
	bool hdr_enabled;
	struct v4l2_subdev_format curr_fmt;
};

static const s64 link_freq[] = {
	IMX678_LINK_FREQ,
};

/* Sensor mode registers */
static const struct imx678_reg mode_3840x2160_regs[] = {
	{0x3000, 0x01}, // STANDBY					*imx678
	{0x3002, 0x01}, // XMSTA					*imx678
	{0x3018, 0x04}, // WINMODE					*imx678
	//{0x37b0, 0x36}, // ?
	//{0x304c, 0x00}, // OPB_SIZE_V				*no in imx678
	//{0x300c, 0x3b}, // BC_WAIT_TIME			*no in imx678
	//{0x300d, 0x2a}, // CPWAIT_TIME			*no in imx678
	{0x302c, 0x26}, // HMAX						*imx678
	{0x302d, 0x02}, // HMAX						*imx678
	{0x3014, 0x04}, // INCK_SEL					*imx678
	{0x3040, 0x03}, // DATARATE_SEL=891Mbps		*imx678
	//{0x301a, 0x00}, // WDMODE=Normal			*imx678
	//{0x3022, 0x01}, // ADBIT=12Bit			*imx678
	{0x303c, 0x00}, // PIX_HST=00				*imx678
	{0x303e, 0x00}, // PIX_HWIDTH_LSB=3840		*imx678
	{0x303f, 0x0f}, // PIX_HWIDTH_MSB			*imx678
	{0x3044, 0x00}, // PIX_VST=00				*imx678
	{0x3046, 0x70}, // PIX_VWIDTH_LSB=2160		*imx678
	{0x3047, 0x08}, // PIX_VWIDTH_MSB			*imx678
	{0x30a6, 0x00}, // XVS_DRV, XHS_DRV			*imx678
	{0x3460, 0x22},
	{0x355A, 0x64},
	{0x3A02, 0x7A},
	{0x3A10, 0xEC},
	{0x3A12, 0x71},
	{0x3A14, 0xDE},
	{0x3A20, 0x2B},
	{0x3A24, 0x22},
	{0x3A25, 0x25},
	{0x3A26, 0x2A},
	{0x3A27, 0x2C},
	{0x3A28, 0x39},
	{0x3A29, 0x38},
	{0x3A30, 0x04},
	{0x3A31, 0x04},
	{0x3A32, 0x03},
	{0x3A33, 0x03},
	{0x3A34, 0x09},
	{0x3A35, 0x06},
	{0x3A38, 0xCD},
	{0x3A3A, 0x4C},
	{0x3A3C, 0xB9},
	{0x3A3E, 0x30},
	{0x3A40, 0x2C},
	{0x3A42, 0x39},
	{0x3A4E, 0x00},
	{0x3A52, 0x00},
	{0x3A56, 0x00},
	{0x3A5A, 0x00},
	{0x3A5E, 0x00},
	{0x3A62, 0x00},
	{0x3A6E, 0xA0},
	{0x3A70, 0x50},
	{0x3A8C, 0x04},
	{0x3A8D, 0x03},
	{0x3A8E, 0x09},
	{0x3A90, 0x38},
	{0x3A91, 0x42},
	{0x3A92, 0x3C},
	{0x3B0E, 0xF3},
	{0x3B12, 0xE5},
	{0x3B27, 0xC0},
	{0x3B2E, 0xEF},
	{0x3B30, 0x6A},
	{0x3B32, 0xF6},
	{0x3B36, 0xE1},
	{0x3B3A, 0xE8},
	{0x3B5A, 0x17},
	{0x3B5E, 0xEF},
	{0x3B60, 0x6A},
	{0x3B62, 0xF6},
	{0x3B66, 0xE1},
	{0x3B6A, 0xE8},
	{0x3B88, 0xEC},
	{0x3B8A, 0xED},
	{0x3B94, 0x71},
	{0x3B96, 0x72},
	{0x3B98, 0xDE},
	{0x3B9A, 0xDF},
	{0x3C0F, 0x06},
	{0x3C10, 0x06},
	{0x3C11, 0x06},
	{0x3C12, 0x06},
	{0x3C13, 0x06},
	{0x3C18, 0x20},
	{0x3C3A, 0x7A},
	{0x3C40, 0xF4},
	{0x3C48, 0xE6},
	{0x3C54, 0xCE},
	{0x3C56, 0xD0},
	{0x3C6C, 0x53},
	{0x3C6E, 0x55},
	{0x3C70, 0xC0},
	{0x3C72, 0xC2},
	{0x3C7E, 0xCE},
	{0x3C8C, 0xCF},
	{0x3C8E, 0xEB},
	{0x3C98, 0x54},
	{0x3C9A, 0x70},
	{0x3C9C, 0xC1},
	{0x3C9E, 0xDD},
	{0x3CB0, 0x7A},
	{0x3CB2, 0xBA},
	{0x3CC8, 0xBC},
	{0x3CCA, 0x7C},
	{0x3CD4, 0xEA},
	{0x3CD5, 0x01},
	{0x3CD6, 0x4A},
	{0x3CD8, 0x00},
	{0x3CD9, 0x00},
	{0x3CDA, 0xFF},
	{0x3CDB, 0x03},
	{0x3CDC, 0x00},
	{0x3CDD, 0x00},
	{0x3CDE, 0xFF},
	{0x3CDF, 0x03},
	{0x3CE4, 0x4C},
	{0x3CE6, 0xEC},
	{0x3CE7, 0x01},
	{0x3CE8, 0xFF},
	{0x3CE9, 0x03},
	{0x3CEA, 0x00},
	{0x3CEB, 0x00},
	{0x3CEC, 0xFF},
	{0x3CED, 0x03},
	{0x3CEE, 0x00},
	{0x3CEF, 0x00},
	{0x3E28, 0x82},
	{0x3E2A, 0x80},
	{0x3E30, 0x85},
	{0x3E32, 0x7D},
	{0x3E5C, 0xCE},
	{0x3E5E, 0xD3},
	{0x3E70, 0x53},
	{0x3E72, 0x58},
	{0x3E74, 0xC0},
	{0x3E76, 0xC5},
	{0x3E78, 0xC0},
	{0x3E79, 0x01},
	{0x3E7A, 0xD4},
	{0x3E7B, 0x01},
	{0x3EB4, 0x0B},
	{0x3EB5, 0x02},
	{0x3EB6, 0x4D},
	{0x3EEC, 0xF3},
	{0x3EEE, 0xE7},
	{0x3F01, 0x01},
	{0x3F24, 0x10},
	{0x3F28, 0x2D},
	{0x3F2A, 0x2D},
	{0x3F2C, 0x2D},
	{0x3F2E, 0x2D},
	{0x3F30, 0x23},
	{0x3F38, 0x2D},
	{0x3F3A, 0x2D},
	{0x3F3C, 0x2D},
	{0x3F3E, 0x28},
	{0x3F40, 0x1E},
	{0x3F48, 0x2D},
	{0x3F4A, 0x2D},
	{0x4004, 0xE4},
	{0x4006, 0xFF},
	{0x4018, 0x69},
	{0x401A, 0x84},
	{0x401C, 0xD6},
	{0x401E, 0xF1},
	{0x4038, 0xDE},
	{0x403A, 0x00},
	{0x403B, 0x01},
	{0x404C, 0x63},
	{0x404E, 0x85},
	{0x4050, 0xD0},
	{0x4052, 0xF2},
	{0x4108, 0xDD},
	{0x410A, 0xF7},
	{0x411C, 0x62},
	{0x411E, 0x7C},
	{0x4120, 0xCF},
	{0x4122, 0xE9},
	{0x4138, 0xE6},
	{0x413A, 0xF1},
	{0x414C, 0x6B},
	{0x414E, 0x76},
	{0x4150, 0xD8},
	{0x4152, 0xE3},
	{0x417E, 0x03},
	{0x417F, 0x01},
	{0x4186, 0xE0},
	{0x4190, 0xF3},
	{0x4192, 0xF7},
	{0x419C, 0x78},
	{0x419E, 0x7C},
	{0x41A0, 0xE5},
	{0x41A2, 0xE9},
	{0x41C8, 0xE2},
	{0x41CA, 0xFD},
	{0x41DC, 0x67},
	{0x41DE, 0x82},
	{0x41E0, 0xD4},
	{0x41E2, 0xEF},
	{0x4200, 0xDE},
	{0x4202, 0xDA},
	{0x4218, 0x63},
	{0x421A, 0x5F},
	{0x421C, 0xD0},
	{0x421E, 0xCC},
	{0x425A, 0x82},
	{0x425C, 0xEF},
	{0x4348, 0xFE},
	{0x4349, 0x06},
	{0x4352, 0xCE},
	{0x4420, 0x0B},
	{0x4421, 0x02},
	{0x4422, 0x4D},
	{0x4426, 0xF5},
	{0x442A, 0xE7},
	{0x4432, 0xF5},
	{0x4436, 0xE7},
	{0x4466, 0xB4},
	{0x446E, 0x32},
	{0x449F, 0x1C},
	{0x44A4, 0x2C},
	{0x44A6, 0x2C},
	{0x44A8, 0x2C},
	{0x44AA, 0x2C},
	{0x44B4, 0x2C},
	{0x44B6, 0x2C},
	{0x44B8, 0x2C},
	{0x44BA, 0x2C},
	{0x44C4, 0x2C},
	{0x44C6, 0x2C},
	{0x44C8, 0x2C},
	{0x4506, 0xF3},
	{0x450E, 0xE5},
	{0x4516, 0xF3},
	{0x4522, 0xE5},
	{0x4524, 0xF3},
	{0x452C, 0xE5},
	{0x453C, 0x22},
	{0x453D, 0x1B},
	{0x453E, 0x1B},
	{0x453F, 0x15},
	{0x4540, 0x15},
	{0x4541, 0x15},
	{0x4542, 0x15},
	{0x4543, 0x15},
	{0x4544, 0x15},
	{0x4548, 0x00},
	{0x4549, 0x01},
	{0x454A, 0x01},
	{0x454B, 0x06},
	{0x454C, 0x06},
	{0x454D, 0x06},
	{0x454E, 0x06},
	{0x454F, 0x06},
	{0x4550, 0x06},
	{0x4554, 0x55},
	{0x4555, 0x02},
	{0x4556, 0x42},
	{0x4557, 0x05},
	{0x4558, 0xFD},
	{0x4559, 0x05},
	{0x455A, 0x94},
	{0x455B, 0x06},
	{0x455D, 0x06},
	{0x455E, 0x49},
	{0x455F, 0x07},
	{0x4560, 0x7F},
	{0x4561, 0x07},
	{0x4562, 0xA5},
	{0x4564, 0x55},
	{0x4565, 0x02},
	{0x4566, 0x42},
	{0x4567, 0x05},
	{0x4568, 0xFD},
	{0x4569, 0x05},
	{0x456A, 0x94},
	{0x456B, 0x06},
	{0x456D, 0x06},
	{0x456E, 0x49},
	{0x456F, 0x07},
	{0x4572, 0xA5},
	{0x460C, 0x7D},
	{0x460E, 0xB1},
	{0x4614, 0xA8},
	{0x4616, 0xB2},
	{0x461C, 0x7E},
	{0x461E, 0xA7},
	{0x4624, 0xA8},
	{0x4626, 0xB2},
	{0x462C, 0x7E},
	{0x462E, 0x8A},
	{0x4630, 0x94},
	{0x4632, 0xA7},
	{0x4634, 0xFB},
	{0x4636, 0x2F},
	{0x4638, 0x81},
	{0x4639, 0x01},
	{0x463A, 0xB5},
	{0x463B, 0x01},
	{0x463C, 0x26},
	{0x463E, 0x30},
	{0x4640, 0xAC},
	{0x4641, 0x01},
	{0x4642, 0xB6},
	{0x4643, 0x01},
	{0x4644, 0xFC},
	{0x4646, 0x25},
	{0x4648, 0x82},
	{0x4649, 0x01},
	{0x464A, 0xAB},
	{0x464B, 0x01},
	{0x464C, 0x26},
	{0x464E, 0x30},
	{0x4654, 0xFC},
	{0x4656, 0x08},
	{0x4658, 0x12},
	{0x465A, 0x25},
	{0x4662, 0xFC},
	{0x46A2, 0xFB},
	{0x46D6, 0xF3},
	{0x46E6, 0x00},
	{0x46E8, 0xFF},
	{0x46E9, 0x03},
	{0x46EC, 0x7A},
	{0x46EE, 0xE5},
	{0x46F4, 0xEE},
	{0x46F6, 0xF2},
	{0x470C, 0xFF},
	{0x470D, 0x03},
	{0x470E, 0x00},
	{0x4714, 0xE0},
	{0x4716, 0xE4},
	{0x471E, 0xED},
	{0x472E, 0x00},
	{0x4730, 0xFF},
	{0x4731, 0x03},
	{0x4734, 0x7B},
	{0x4736, 0xDF},
	{0x4754, 0x7D},
	{0x4756, 0x8B},
	{0x4758, 0x93},
	{0x475A, 0xB1},
	{0x475C, 0xFB},
	{0x475E, 0x09},
	{0x4760, 0x11},
	{0x4762, 0x2F},
	{0x4766, 0xCC},
	{0x4776, 0xCB},
	{0x477E, 0x4A},
	{0x478E, 0x49},
	{0x4794, 0x7C},
	{0x4796, 0x8F},
	{0x4798, 0xB3},
	{0x4799, 0x00},
	{0x479A, 0xCC},
	{0x479C, 0xC1},
	{0x479E, 0xCB},
	{0x47A4, 0x7D},
	{0x47A6, 0x8E},
	{0x47A8, 0xB4},
	{0x47A9, 0x00},
	{0x47AA, 0xC0},
	{0x47AC, 0xFA},
	{0x47AE, 0x0D},
	{0x47B0, 0x31},
	{0x47B1, 0x01},
	{0x47B2, 0x4A},
	{0x47B3, 0x01},
	{0x47B4, 0x3F},
	{0x47B6, 0x49},
	{0x47BC, 0xFB},
	{0x47BE, 0x0C},
	{0x47C0, 0x32},
	{0x47C1, 0x01},
	{0x47C2, 0x3E},
	{0x47C3, 0x01},
};

static const struct imx678_reg mode_1920x1080_sdr_binning_regs[] = {
    { 0x3000, 0x01 }, // STANDBY                    *imx678
    { 0x3002, 0x00 }, // XMSTA                  *imx678
    { 0x3014, 0x04 }, // INCK_SEL                   *imx678
    { 0x3018, 0x04 }, // WINMODE - crop                 *imx678
    { 0x301A, 0x00 }, //WDMODE[7:0] - sdr
    { 0x301B, 0x01 }, //ADDMODE[1:0] - binning
    // { 0x301C, 0x01 }, //THIN_V_EN[7:0] - subsampling
    { 0x3022, 0x00 }, //ADBIT[1:0] // 10 bit
    { 0x3023, 0x01 }, //MDBIT[1:0] // raw12 IMPORTANT
    { 0x3028, 0x5e }, //VMAX[15:0]
    { 0x3029, 0x1a }, //VMAX[15:0] // 6750
    { 0x302C, 0x26 }, //HMAX[15:0] // 550
    { 0x302D, 0x02 },
    { 0x303c, 0x00 }, // PIX_HST=00             *imx678
    { 0x303e, 0x00 }, // PIX_HWIDTH_LSB=3840        *imx678
    { 0x303f, 0x0f }, // PIX_HWIDTH_MSB         *imx678
    { 0x3044, 0x00 }, // PIX_VST=00             *imx678
    { 0x3046, 0x70 }, // PIX_VWIDTH_LSB=2160        *imx678
    { 0x3047, 0x08 }, // PIX_VWIDTH_MSB         *imx678
    { 0x3050, 0x18 }, //SHR0[19:0]
    { 0x3051, 0x15 }, 
    // { 0x3054, 0x07 }, //SHR1[19:0]
    // { 0x3058, 0x4A }, //SHR2[19:0]
    // { 0x3059, 0x00 }, 
    // { 0x3060, 0x40 }, //RHS1[19:0]
    // { 0x3061, 0x00 }, 
    // { 0x3064, 0x53 }, //RHS2[19:0]
    { 0x3065, 0x00 }, { 0x30A6, 0x00 }, //XVS_DRV[1:0]
    { 0x3460, 0x22 }, { 0x355A, 0x64 }, { 0x3A02, 0x7A }, { 0x3A10, 0xEC },
    { 0x3A12, 0x71 }, { 0x3A14, 0xDE }, { 0x3A20, 0x2B }, { 0x3A24, 0x22 },
    { 0x3A25, 0x25 }, { 0x3A26, 0x2A }, { 0x3A27, 0x2C }, { 0x3A28, 0x39 },
    { 0x3A29, 0x38 }, { 0x3A30, 0x04 }, { 0x3A31, 0x04 }, { 0x3A32, 0x03 },
    { 0x3A33, 0x03 }, { 0x3A34, 0x09 }, { 0x3A35, 0x06 }, { 0x3A38, 0xCD },
    { 0x3A3A, 0x4C }, { 0x3A3C, 0xB9 }, { 0x3A3E, 0x30 }, { 0x3A40, 0x2C },
    { 0x3A42, 0x39 }, { 0x3A4E, 0x00 }, { 0x3A52, 0x00 }, { 0x3A56, 0x00 },
    { 0x3A5A, 0x00 }, { 0x3A5E, 0x00 }, { 0x3A62, 0x00 }, { 0x3A6E, 0xA0 },
    { 0x3A70, 0x50 }, { 0x3A8C, 0x04 }, { 0x3A8D, 0x03 }, { 0x3A8E, 0x09 },
    { 0x3A90, 0x38 }, { 0x3A91, 0x42 }, { 0x3A92, 0x3C }, { 0x3B0E, 0xF3 },
    { 0x3B12, 0xE5 }, { 0x3B27, 0xC0 }, { 0x3B2E, 0xEF }, { 0x3B30, 0x6A },
    { 0x3B32, 0xF6 }, { 0x3B36, 0xE1 }, { 0x3B3A, 0xE8 }, { 0x3B5A, 0x17 },
    { 0x3B5E, 0xEF }, { 0x3B60, 0x6A }, { 0x3B62, 0xF6 }, { 0x3B66, 0xE1 },
    { 0x3B6A, 0xE8 }, { 0x3B88, 0xEC }, { 0x3B8A, 0xED }, { 0x3B94, 0x71 },
    { 0x3B96, 0x72 }, { 0x3B98, 0xDE }, { 0x3B9A, 0xDF }, { 0x3C0F, 0x06 },
    { 0x3C10, 0x06 }, { 0x3C11, 0x06 }, { 0x3C12, 0x06 }, { 0x3C13, 0x06 },
    { 0x3C18, 0x20 }, { 0x3C3A, 0x7A }, { 0x3C40, 0xF4 }, { 0x3C48, 0xE6 },
    { 0x3C54, 0xCE }, { 0x3C56, 0xD0 }, { 0x3C6C, 0x53 }, { 0x3C6E, 0x55 },
    { 0x3C70, 0xC0 }, { 0x3C72, 0xC2 }, { 0x3C7E, 0xCE }, { 0x3C8C, 0xCF },
    { 0x3C8E, 0xEB }, { 0x3C98, 0x54 }, { 0x3C9A, 0x70 }, { 0x3C9C, 0xC1 },
    { 0x3C9E, 0xDD }, { 0x3CB0, 0x7A }, { 0x3CB2, 0xBA }, { 0x3CC8, 0xBC },
    { 0x3CCA, 0x7C }, { 0x3CD4, 0xEA }, { 0x3CD5, 0x01 }, { 0x3CD6, 0x4A },
    { 0x3CD8, 0x00 }, { 0x3CD9, 0x00 }, { 0x3CDA, 0xFF }, { 0x3CDB, 0x03 },
    { 0x3CDC, 0x00 }, { 0x3CDD, 0x00 }, { 0x3CDE, 0xFF }, { 0x3CDF, 0x03 },
    { 0x3CE4, 0x4C }, { 0x3CE6, 0xEC }, { 0x3CE7, 0x01 }, { 0x3CE8, 0xFF },
    { 0x3CE9, 0x03 }, { 0x3CEA, 0x00 }, { 0x3CEB, 0x00 }, { 0x3CEC, 0xFF },
    { 0x3CED, 0x03 }, { 0x3CEE, 0x00 }, { 0x3CEF, 0x00 }, { 0x3E28, 0x82 },
    { 0x3E2A, 0x80 }, { 0x3E30, 0x85 }, { 0x3E32, 0x7D }, { 0x3E5C, 0xCE },
    { 0x3E5E, 0xD3 }, { 0x3E70, 0x53 }, { 0x3E72, 0x58 }, { 0x3E74, 0xC0 },
    { 0x3E76, 0xC5 }, { 0x3E78, 0xC0 }, { 0x3E79, 0x01 }, { 0x3E7A, 0xD4 },
    { 0x3E7B, 0x01 }, { 0x3EB4, 0x0B }, { 0x3EB5, 0x02 }, { 0x3EB6, 0x4D },
    { 0x3EEC, 0xF3 }, { 0x3EEE, 0xE7 }, { 0x3F01, 0x01 }, { 0x3F24, 0x10 },
    { 0x3F28, 0x2D }, { 0x3F2A, 0x2D }, { 0x3F2C, 0x2D }, { 0x3F2E, 0x2D },
    { 0x3F30, 0x23 }, { 0x3F38, 0x2D }, { 0x3F3A, 0x2D }, { 0x3F3C, 0x2D },
    { 0x3F3E, 0x28 }, { 0x3F40, 0x1E }, { 0x3F48, 0x2D }, { 0x3F4A, 0x2D },
    { 0x4004, 0xE4 }, { 0x4006, 0xFF }, { 0x4018, 0x69 }, { 0x401A, 0x84 },
    { 0x401C, 0xD6 }, { 0x401E, 0xF1 }, { 0x4038, 0xDE }, { 0x403A, 0x00 },
    { 0x403B, 0x01 }, { 0x404C, 0x63 }, { 0x404E, 0x85 }, { 0x4050, 0xD0 },
    { 0x4052, 0xF2 }, { 0x4108, 0xDD }, { 0x410A, 0xF7 }, { 0x411C, 0x62 },
    { 0x411E, 0x7C }, { 0x4120, 0xCF }, { 0x4122, 0xE9 }, { 0x4138, 0xE6 },
    { 0x413A, 0xF1 }, { 0x414C, 0x6B }, { 0x414E, 0x76 }, { 0x4150, 0xD8 },
    { 0x4152, 0xE3 }, { 0x417E, 0x03 }, { 0x417F, 0x01 }, { 0x4186, 0xE0 },
    { 0x4190, 0xF3 }, { 0x4192, 0xF7 }, { 0x419C, 0x78 }, { 0x419E, 0x7C },
    { 0x41A0, 0xE5 }, { 0x41A2, 0xE9 }, { 0x41C8, 0xE2 }, { 0x41CA, 0xFD },
    { 0x41DC, 0x67 }, { 0x41DE, 0x82 }, { 0x41E0, 0xD4 }, { 0x41E2, 0xEF },
    { 0x4200, 0xDE }, { 0x4202, 0xDA }, { 0x4218, 0x63 }, { 0x421A, 0x5F },
    { 0x421C, 0xD0 }, { 0x421E, 0xCC }, { 0x425A, 0x82 }, { 0x425C, 0xEF },
    { 0x4348, 0xFE }, { 0x4349, 0x06 }, { 0x4352, 0xCE }, { 0x4420, 0x0B },
    { 0x4421, 0x02 }, { 0x4422, 0x4D }, { 0x4426, 0xF5 }, { 0x442A, 0xE7 },
    { 0x4432, 0xF5 }, { 0x4436, 0xE7 }, { 0x4466, 0xB4 }, { 0x446E, 0x32 },
    { 0x449F, 0x1C }, { 0x44A4, 0x2C }, { 0x44A6, 0x2C }, { 0x44A8, 0x2C },
    { 0x44AA, 0x2C }, { 0x44B4, 0x2C }, { 0x44B6, 0x2C }, { 0x44B8, 0x2C },
    { 0x44BA, 0x2C }, { 0x44C4, 0x2C }, { 0x44C6, 0x2C }, { 0x44C8, 0x2C },
    { 0x4506, 0xF3 }, { 0x450E, 0xE5 }, { 0x4516, 0xF3 }, { 0x4522, 0xE5 },
    { 0x4524, 0xF3 }, { 0x452C, 0xE5 }, { 0x453C, 0x22 }, { 0x453D, 0x1B },
    { 0x453E, 0x1B }, { 0x453F, 0x15 }, { 0x4540, 0x15 }, { 0x4541, 0x15 },
    { 0x4542, 0x15 }, { 0x4543, 0x15 }, { 0x4544, 0x15 }, { 0x4548, 0x00 },
    { 0x4549, 0x01 }, { 0x454A, 0x01 }, { 0x454B, 0x06 }, { 0x454C, 0x06 },
    { 0x454D, 0x06 }, { 0x454E, 0x06 }, { 0x454F, 0x06 }, { 0x4550, 0x06 },
    { 0x4554, 0x55 }, { 0x4555, 0x02 }, { 0x4556, 0x42 }, { 0x4557, 0x05 },
    { 0x4558, 0xFD }, { 0x4559, 0x05 }, { 0x455A, 0x94 }, { 0x455B, 0x06 },
    { 0x455D, 0x06 }, { 0x455E, 0x49 }, { 0x455F, 0x07 }, { 0x4560, 0x7F },
    { 0x4561, 0x07 }, { 0x4562, 0xA5 }, { 0x4564, 0x55 }, { 0x4565, 0x02 },
    { 0x4566, 0x42 }, { 0x4567, 0x05 }, { 0x4568, 0xFD }, { 0x4569, 0x05 },
    { 0x456A, 0x94 }, { 0x456B, 0x06 }, { 0x456D, 0x06 }, { 0x456E, 0x49 },
    { 0x456F, 0x07 }, { 0x4572, 0xA5 }, { 0x460C, 0x7D }, { 0x460E, 0xB1 },
    { 0x4614, 0xA8 }, { 0x4616, 0xB2 }, { 0x461C, 0x7E }, { 0x461E, 0xA7 },
    { 0x4624, 0xA8 }, { 0x4626, 0xB2 }, { 0x462C, 0x7E }, { 0x462E, 0x8A },
    { 0x4630, 0x94 }, { 0x4632, 0xA7 }, { 0x4634, 0xFB }, { 0x4636, 0x2F },
    { 0x4638, 0x81 }, { 0x4639, 0x01 }, { 0x463A, 0xB5 }, { 0x463B, 0x01 },
    { 0x463C, 0x26 }, { 0x463E, 0x30 }, { 0x4640, 0xAC }, { 0x4641, 0x01 },
    { 0x4642, 0xB6 }, { 0x4643, 0x01 }, { 0x4644, 0xFC }, { 0x4646, 0x25 },
    { 0x4648, 0x82 }, { 0x4649, 0x01 }, { 0x464A, 0xAB }, { 0x464B, 0x01 },
    { 0x464C, 0x26 }, { 0x464E, 0x30 }, { 0x4654, 0xFC }, { 0x4656, 0x08 },
    { 0x4658, 0x12 }, { 0x465A, 0x25 }, { 0x4662, 0xFC }, { 0x46A2, 0xFB },
    { 0x46D6, 0xF3 }, { 0x46E6, 0x00 }, { 0x46E8, 0xFF }, { 0x46E9, 0x03 },
    { 0x46EC, 0x7A }, { 0x46EE, 0xE5 }, { 0x46F4, 0xEE }, { 0x46F6, 0xF2 },
    { 0x470C, 0xFF }, { 0x470D, 0x03 }, { 0x470E, 0x00 }, { 0x4714, 0xE0 },
    { 0x4716, 0xE4 }, { 0x471E, 0xED }, { 0x472E, 0x00 }, { 0x4730, 0xFF },
    { 0x4731, 0x03 }, { 0x4734, 0x7B }, { 0x4736, 0xDF }, { 0x4754, 0x7D },
    { 0x4756, 0x8B }, { 0x4758, 0x93 }, { 0x475A, 0xB1 }, { 0x475C, 0xFB },
    { 0x475E, 0x09 }, { 0x4760, 0x11 }, { 0x4762, 0x2F }, { 0x4766, 0xCC },
    { 0x4776, 0xCB }, { 0x477E, 0x4A }, { 0x478E, 0x49 }, { 0x4794, 0x7C },
    { 0x4796, 0x8F }, { 0x4798, 0xB3 }, { 0x4799, 0x00 }, { 0x479A, 0xCC },
    { 0x479C, 0xC1 }, { 0x479E, 0xCB }, { 0x47A4, 0x7D }, { 0x47A6, 0x8E },
    { 0x47A8, 0xB4 }, { 0x47A9, 0x00 }, { 0x47AA, 0xC0 }, { 0x47AC, 0xFA },
    { 0x47AE, 0x0D }, { 0x47B0, 0x31 }, { 0x47B1, 0x01 }, { 0x47B2, 0x4A },
    { 0x47B3, 0x01 }, { 0x47B4, 0x3F }, { 0x47B6, 0x49 }, { 0x47BC, 0xFB },
    { 0x47BE, 0x0C }, { 0x47C0, 0x32 }, { 0x47C1, 0x01 }, { 0x47C2, 0x3E },
    { 0x47C3, 0x01 }, { 0x4E3C, 0x07 }
};


static const struct imx678_reg imx678_tpg_en_regs[] = {
	//TPG config
	{ 0x3042, 0x00 }, //XSIZE_OVERLAP
	{ 0x30e0, 0x01 }, //TPG_EN_DUOUT
	{ 0x30e4, 0x13 }, //TPG_COLORWIDTH
};

static const struct imx678_reg mode_1920x1080_3dol_binning_regs[] = {
	/* 0x3000: Using default value (STANDBY) */
	/* 0x3001: Using default value (REGHOLD) */
	{ 0x3002, 0x01 }, /* XMSTA */
	{ 0x3014, 0x04 }, /* INCK_SEL[3:0] */
	{ 0x3015, 0x06 }, /* DATARATE_SEL[3:0] */
	{ 0x3018, 0x04 }, /* WINMODE[4:0] */  // MANUAL (crop 3856x2180 to 3840x2160)
	/* 0x3019: Using default value (CFMODE[1:0]) */
	{ 0x301A, 0x02 }, /* WDMODE[7:0] */
	{ 0x301B, 0x01 }, /* ADDMODE[1:0] */
	{ 0x301C, 0x01 }, /* THIN_V_EN[7:0] */
	/* 0x301E: Using default value (VCMODE[7:0]) */
	/* 0x3020: Using default value (HREVERSE) */
	/* 0x3021: Using default value (VREVERSE) */
	{ 0x3022, 0x00 }, /* ADBIT[1:0] */
	{ 0x3023, 0x01 }, /* MDBIT */
	/* 0x3028: Using default value (VMAX[19:0]) */
	{ 0x3028, 0xca }, /* VMAX[19:0] */
	{ 0x3029, 0x08 },
	/* 0x302A: Using default value */
	{ 0x302C, 0x28 }, /* HMAX[15:0] */
	{ 0x302D, 0x05 },
	/* 0x3030: Using default value (FDG_SEL0[1:0]) */
	/* 0x3031: Using default value (FDG_SEL1[1:0]) */
	/* 0x3032: Using default value (FDG_SEL2[1:0]) */
	/* 0x303C: Using default value (PIX_HST[12:0]) */
	/* 0x303D: Using default value */
	{ 0x303E, 0x00 }, /* PIX_HWIDTH[12:0] */  // MANUAL (crop 3856 to 3840)
	{ 0x303F, 0x0F }, // MANUAL (crop 3856 to 3840)
	{ 0x3040, 0x03 }, /* LANEMODE[2:0] */
	/* 0x3042: Using default value (XSIZE_OVERLAP[10:0]) */
	/* 0x3043: Using default value */
	/* 0x3044: Using default value (PIX_VST[11:0]) */
	/* 0x3045: Using default value */
	{ 0x3046, 0x70 }, /* PIX_VWIDTH[11:0] */  // MANUAL (crop 2180 to 2160)
	{ 0x3047, 0x08 }, // MANUAL (crop 2180 to 2160)
	{ 0x3050, 0x18 }, /* SHR0[19:0] */
	{ 0x3051, 0x15 },
	/* 0x3052: Using default value */
	{ 0x3054, 0x07 }, /* SHR1[19:0] */
	/* 0x3055: Using default value */
	/* 0x3056: Using default value */
	{ 0x3058, 0x4A }, /* SHR2[19:0] */
	{ 0x3059, 0x00 },
	/* 0x305A: Using default value */
	{ 0x3060, 0x43 }, /* RHS1[19:0] */
	{ 0x3061, 0x00 },
	/* 0x3062: Using default value */
	{ 0x3064, 0x56 }, /* RHS2[19:0] */
	{ 0x3065, 0x00 },
	/* 0x3066: Using default value */
	/* 0x3069: Using default value (CHDR_GAIN_EN[7:0]) */
	/* 0x306B: Using default value */
	/* 0x3070: Using default value (GAIN[10:0]) */
	/* 0x3071: Using default value */
	/* 0x3072: Using default value (GAIN_1[10:0]) */
	/* 0x3073: Using default value */
	/* 0x3074: Using default value (GAIN_2[10:0]) */
	/* 0x3075: Using default value */
	/* 0x3081: Using default value (EXP_GAIN[7:0]) */
	/* 0x308C: Using default value (CHDR_DGAIN0_HG[15:0]) */
	/* 0x308D: Using default value */
	/* 0x3094: Using default value (CHDR_AGAIN0_LG[10:0]) */
	/* 0x3095: Using default value */
	/* 0x309C: Using default value (CHDR_AGAIN0_HG[10:0]) */
	/* 0x309D: Using default value */
	/* 0x30A4: Using default value (XVSOUTSEL[1:0]) */
	{ 0x30A6, 0x00 }, /* XVS_DRV[1:0] */
	/* 0x30CC: Using default value */
	/* 0x30CD: Using default value */
	/* 0x30DC: Using default value (BLKLEVEL[11:0]) */
	/* 0x30DD: Using default value */
	/* 0x3400: Using default value (GAIN_PGC_FIDMD) */
	{ 0x3460, 0x22 },
	{ 0x355A, 0x64 },
	{ 0x3A02, 0x7A },
	{ 0x3A10, 0xEC },
	{ 0x3A12, 0x71 },
	{ 0x3A14, 0xDE },
	{ 0x3A20, 0x2B },
	{ 0x3A24, 0x22 },
	{ 0x3A25, 0x25 },
	{ 0x3A26, 0x2A },
	{ 0x3A27, 0x2C },
	{ 0x3A28, 0x39 },
	{ 0x3A29, 0x38 },
	{ 0x3A30, 0x04 },
	{ 0x3A31, 0x04 },
	{ 0x3A32, 0x03 },
	{ 0x3A33, 0x03 },
	{ 0x3A34, 0x09 },
	{ 0x3A35, 0x06 },
	{ 0x3A38, 0xCD },
	{ 0x3A3A, 0x4C },
	{ 0x3A3C, 0xB9 },
	{ 0x3A3E, 0x30 },
	{ 0x3A40, 0x2C },
	{ 0x3A42, 0x39 },
	{ 0x3A4E, 0x00 },
	{ 0x3A52, 0x00 },
	{ 0x3A56, 0x00 },
	{ 0x3A5A, 0x00 },
	{ 0x3A5E, 0x00 },
	{ 0x3A62, 0x00 },
	/* 0x3A64: Using default value */
	{ 0x3A6E, 0xA0 },
	{ 0x3A70, 0x50 },
	{ 0x3A8C, 0x04 },
	{ 0x3A8D, 0x03 },
	{ 0x3A8E, 0x09 },
	{ 0x3A90, 0x38 },
	{ 0x3A91, 0x42 },
	{ 0x3A92, 0x3C },
	{ 0x3B0E, 0xF3 },
	{ 0x3B12, 0xE5 },
	{ 0x3B27, 0xC0 },
	{ 0x3B2E, 0xEF },
	{ 0x3B30, 0x6A },
	{ 0x3B32, 0xF6 },
	{ 0x3B36, 0xE1 },
	{ 0x3B3A, 0xE8 },
	{ 0x3B5A, 0x17 },
	{ 0x3B5E, 0xEF },
	{ 0x3B60, 0x6A },
	{ 0x3B62, 0xF6 },
	{ 0x3B66, 0xE1 },
	{ 0x3B6A, 0xE8 },
	{ 0x3B88, 0xEC },
	{ 0x3B8A, 0xED },
	{ 0x3B94, 0x71 },
	{ 0x3B96, 0x72 },
	{ 0x3B98, 0xDE },
	{ 0x3B9A, 0xDF },
	{ 0x3C0F, 0x06 },
	{ 0x3C10, 0x06 },
	{ 0x3C11, 0x06 },
	{ 0x3C12, 0x06 },
	{ 0x3C13, 0x06 },
	{ 0x3C18, 0x20 },
	/* 0x3C37: Using default value */
	{ 0x3C3A, 0x7A },
	{ 0x3C40, 0xF4 },
	{ 0x3C48, 0xE6 },
	{ 0x3C54, 0xCE },
	{ 0x3C56, 0xD0 },
	{ 0x3C6C, 0x53 },
	{ 0x3C6E, 0x55 },
	{ 0x3C70, 0xC0 },
	{ 0x3C72, 0xC2 },
	{ 0x3C7E, 0xCE },
	{ 0x3C8C, 0xCF },
	{ 0x3C8E, 0xEB },
	{ 0x3C98, 0x54 },
	{ 0x3C9A, 0x70 },
	{ 0x3C9C, 0xC1 },
	{ 0x3C9E, 0xDD },
	{ 0x3CB0, 0x7A },
	{ 0x3CB2, 0xBA },
	{ 0x3CC8, 0xBC },
	{ 0x3CCA, 0x7C },
	{ 0x3CD4, 0xEA },
	{ 0x3CD5, 0x01 },
	{ 0x3CD6, 0x4A },
	{ 0x3CD8, 0x00 },
	{ 0x3CD9, 0x00 },
	{ 0x3CDA, 0xFF },
	{ 0x3CDB, 0x03 },
	{ 0x3CDC, 0x00 },
	{ 0x3CDD, 0x00 },
	{ 0x3CDE, 0xFF },
	{ 0x3CDF, 0x03 },
	{ 0x3CE4, 0x4C },
	{ 0x3CE6, 0xEC },
	{ 0x3CE7, 0x01 },
	{ 0x3CE8, 0xFF },
	{ 0x3CE9, 0x03 },
	{ 0x3CEA, 0x00 },
	{ 0x3CEB, 0x00 },
	{ 0x3CEC, 0xFF },
	{ 0x3CED, 0x03 },
	{ 0x3CEE, 0x00 },
	{ 0x3CEF, 0x00 },
	/* 0x3CF2: Using default value */
	/* 0x3CF3: Using default value */
	/* 0x3CF4: Using default value */
	{ 0x3E28, 0x82 },
	{ 0x3E2A, 0x80 },
	{ 0x3E30, 0x85 },
	{ 0x3E32, 0x7D },
	{ 0x3E5C, 0xCE },
	{ 0x3E5E, 0xD3 },
	{ 0x3E70, 0x53 },
	{ 0x3E72, 0x58 },
	{ 0x3E74, 0xC0 },
	{ 0x3E76, 0xC5 },
	{ 0x3E78, 0xC0 },
	{ 0x3E79, 0x01 },
	{ 0x3E7A, 0xD4 },
	{ 0x3E7B, 0x01 },
	{ 0x3EB4, 0x0B },
	{ 0x3EB5, 0x02 },
	{ 0x3EB6, 0x4D },
	/* 0x3EB7: Using default value */
	{ 0x3EEC, 0xF3 },
	{ 0x3EEE, 0xE7 },
	{ 0x3F01, 0x01 },
	{ 0x3F24, 0x10 },
	{ 0x3F28, 0x2D },
	{ 0x3F2A, 0x2D },
	{ 0x3F2C, 0x2D },
	{ 0x3F2E, 0x2D },
	{ 0x3F30, 0x23 },
	{ 0x3F38, 0x2D },
	{ 0x3F3A, 0x2D },
	{ 0x3F3C, 0x2D },
	{ 0x3F3E, 0x28 },
	{ 0x3F40, 0x1E },
	{ 0x3F48, 0x2D },
	{ 0x3F4A, 0x2D },
	/* 0x3F4C: Using default value */
	{ 0x4004, 0xE4 },
	{ 0x4006, 0xFF },
	{ 0x4018, 0x69 },
	{ 0x401A, 0x84 },
	{ 0x401C, 0xD6 },
	{ 0x401E, 0xF1 },
	{ 0x4038, 0xDE },
	{ 0x403A, 0x00 },
	{ 0x403B, 0x01 },
	{ 0x404C, 0x63 },
	{ 0x404E, 0x85 },
	{ 0x4050, 0xD0 },
	{ 0x4052, 0xF2 },
	{ 0x4108, 0xDD },
	{ 0x410A, 0xF7 },
	{ 0x411C, 0x62 },
	{ 0x411E, 0x7C },
	{ 0x4120, 0xCF },
	{ 0x4122, 0xE9 },
	{ 0x4138, 0xE6 },
	{ 0x413A, 0xF1 },
	{ 0x414C, 0x6B },
	{ 0x414E, 0x76 },
	{ 0x4150, 0xD8 },
	{ 0x4152, 0xE3 },
	{ 0x417E, 0x03 },
	{ 0x417F, 0x01 },
	{ 0x4186, 0xE0 },
	{ 0x4190, 0xF3 },
	{ 0x4192, 0xF7 },
	{ 0x419C, 0x78 },
	{ 0x419E, 0x7C },
	{ 0x41A0, 0xE5 },
	{ 0x41A2, 0xE9 },
	{ 0x41C8, 0xE2 },
	{ 0x41CA, 0xFD },
	{ 0x41DC, 0x67 },
	{ 0x41DE, 0x82 },
	{ 0x41E0, 0xD4 },
	{ 0x41E2, 0xEF },
	{ 0x4200, 0xDE },
	{ 0x4202, 0xDA },
	{ 0x4218, 0x63 },
	{ 0x421A, 0x5F },
	{ 0x421C, 0xD0 },
	{ 0x421E, 0xCC },
	{ 0x425A, 0x82 },
	{ 0x425C, 0xEF },
	{ 0x4348, 0xFE },
	{ 0x4349, 0x06 },
	{ 0x4352, 0xCE },
	{ 0x4420, 0x0B },
	{ 0x4421, 0x02 },
	{ 0x4422, 0x4D },
	/* 0x4423: Using default value */
	{ 0x4426, 0xF5 },
	{ 0x442A, 0xE7 },
	{ 0x4432, 0xF5 },
	{ 0x4436, 0xE7 },
	{ 0x4466, 0xB4 },
	{ 0x446E, 0x32 },
	{ 0x449F, 0x1C },
	{ 0x44A4, 0x2C },
	{ 0x44A6, 0x2C },
	{ 0x44A8, 0x2C },
	{ 0x44AA, 0x2C },
	{ 0x44B4, 0x2C },
	{ 0x44B6, 0x2C },
	{ 0x44B8, 0x2C },
	{ 0x44BA, 0x2C },
	{ 0x44C4, 0x2C },
	{ 0x44C6, 0x2C },
	{ 0x44C8, 0x2C },
	{ 0x4506, 0xF3 },
	{ 0x450E, 0xE5 },
	{ 0x4516, 0xF3 },
	{ 0x4522, 0xE5 },
	{ 0x4524, 0xF3 },
	{ 0x452C, 0xE5 },
	{ 0x453C, 0x22 },
	{ 0x453D, 0x1B },
	{ 0x453E, 0x1B },
	{ 0x453F, 0x15 },
	{ 0x4540, 0x15 },
	{ 0x4541, 0x15 },
	{ 0x4542, 0x15 },
	{ 0x4543, 0x15 },
	{ 0x4544, 0x15 },
	{ 0x4548, 0x00 },
	{ 0x4549, 0x01 },
	{ 0x454A, 0x01 },
	{ 0x454B, 0x06 },
	{ 0x454C, 0x06 },
	{ 0x454D, 0x06 },
	{ 0x454E, 0x06 },
	{ 0x454F, 0x06 },
	{ 0x4550, 0x06 },
	{ 0x4554, 0x55 },
	{ 0x4555, 0x02 },
	{ 0x4556, 0x42 },
	{ 0x4557, 0x05 },
	{ 0x4558, 0xFD },
	{ 0x4559, 0x05 },
	{ 0x455A, 0x94 },
	{ 0x455B, 0x06 },
	{ 0x455D, 0x06 },
	{ 0x455E, 0x49 },
	{ 0x455F, 0x07 },
	{ 0x4560, 0x7F },
	{ 0x4561, 0x07 },
	{ 0x4562, 0xA5 },
	{ 0x4564, 0x55 },
	{ 0x4565, 0x02 },
	{ 0x4566, 0x42 },
	{ 0x4567, 0x05 },
	{ 0x4568, 0xFD },
	{ 0x4569, 0x05 },
	{ 0x456A, 0x94 },
	{ 0x456B, 0x06 },
	{ 0x456D, 0x06 },
	{ 0x456E, 0x49 },
	{ 0x456F, 0x07 },
	{ 0x4572, 0xA5 },
	{ 0x460C, 0x7D },
	{ 0x460E, 0xB1 },
	{ 0x4614, 0xA8 },
	{ 0x4616, 0xB2 },
	{ 0x461C, 0x7E },
	{ 0x461E, 0xA7 },
	{ 0x4624, 0xA8 },
	{ 0x4626, 0xB2 },
	{ 0x462C, 0x7E },
	{ 0x462E, 0x8A },
	{ 0x4630, 0x94 },
	{ 0x4632, 0xA7 },
	{ 0x4634, 0xFB },
	{ 0x4636, 0x2F },
	{ 0x4638, 0x81 },
	{ 0x4639, 0x01 },
	{ 0x463A, 0xB5 },
	{ 0x463B, 0x01 },
	{ 0x463C, 0x26 },
	{ 0x463E, 0x30 },
	{ 0x4640, 0xAC },
	{ 0x4641, 0x01 },
	{ 0x4642, 0xB6 },
	{ 0x4643, 0x01 },
	{ 0x4644, 0xFC },
	{ 0x4646, 0x25 },
	{ 0x4648, 0x82 },
	{ 0x4649, 0x01 },
	{ 0x464A, 0xAB },
	{ 0x464B, 0x01 },
	{ 0x464C, 0x26 },
	{ 0x464E, 0x30 },
	{ 0x4654, 0xFC },
	{ 0x4656, 0x08 },
	{ 0x4658, 0x12 },
	{ 0x465A, 0x25 },
	{ 0x4662, 0xFC },
	{ 0x46A2, 0xFB },
	{ 0x46D6, 0xF3 },
	{ 0x46E6, 0x00 },
	{ 0x46E8, 0xFF },
	{ 0x46E9, 0x03 },
	{ 0x46EC, 0x7A },
	{ 0x46EE, 0xE5 },
	{ 0x46F4, 0xEE },
	{ 0x46F6, 0xF2 },
	{ 0x470C, 0xFF },
	{ 0x470D, 0x03 },
	{ 0x470E, 0x00 },
	{ 0x4714, 0xE0 },
	{ 0x4716, 0xE4 },
	{ 0x471E, 0xED },
	{ 0x472E, 0x00 },
	{ 0x4730, 0xFF },
	{ 0x4731, 0x03 },
	{ 0x4734, 0x7B },
	{ 0x4736, 0xDF },
	{ 0x4754, 0x7D },
	{ 0x4756, 0x8B },
	{ 0x4758, 0x93 },
	{ 0x475A, 0xB1 },
	{ 0x475C, 0xFB },
	{ 0x475E, 0x09 },
	{ 0x4760, 0x11 },
	{ 0x4762, 0x2F },
	{ 0x4766, 0xCC },
	{ 0x4776, 0xCB },
	{ 0x477E, 0x4A },
	{ 0x478E, 0x49 },
	{ 0x4794, 0x7C },
	{ 0x4796, 0x8F },
	{ 0x4798, 0xB3 },
	{ 0x4799, 0x00 },
	{ 0x479A, 0xCC },
	{ 0x479C, 0xC1 },
	{ 0x479E, 0xCB },
	{ 0x47A4, 0x7D },
	{ 0x47A6, 0x8E },
	{ 0x47A8, 0xB4 },
	{ 0x47A9, 0x00 },
	{ 0x47AA, 0xC0 },
	{ 0x47AC, 0xFA },
	{ 0x47AE, 0x0D },
	{ 0x47B0, 0x31 },
	{ 0x47B1, 0x01 },
	{ 0x47B2, 0x4A },
	{ 0x47B3, 0x01 },
	{ 0x47B4, 0x3F },
	{ 0x47B6, 0x49 },
	{ 0x47BC, 0xFB },
	{ 0x47BE, 0x0C },
	{ 0x47C0, 0x32 },
	{ 0x47C1, 0x01 },
	{ 0x47C2, 0x3E },
	{ 0x47C3, 0x01 },
	{ 0x4E3C, 0x07 }
};

/* Supported sensor mode configurations */
static const struct imx678_mode supported_sdr_modes[] = {
	{
	.width = 3840,
	.height = 2160,
	.hblank = 560,
	.vblank = 2340,
	.vblank_min = 90,
	.vblank_max = 132840,
	.pclk = 594000000,
	.link_freq_idx = 0,
	.code = MEDIA_BUS_FMT_SRGGB12_1X12,
	.reg_list = {
		.num_of_regs = ARRAY_SIZE(mode_3840x2160_regs),
		.regs = mode_3840x2160_regs,
	},
	.frame_interval = {
		.denominator = 30,
		.numerator = 1,
	},
	},
	{
	.width = 3840,
	.height = 2160,
	.hblank = 560,
	.vblank = 90,
	.vblank_min = 90,
	.vblank_max = 132840,
	.pclk = 594000000,
	.link_freq_idx = 0,
	.code = MEDIA_BUS_FMT_SRGGB12_1X12,
	.reg_list = {
		.num_of_regs = ARRAY_SIZE(mode_3840x2160_regs),
		.regs = mode_3840x2160_regs,
	},
	.frame_interval = {
		.denominator = 60,
		.numerator = 1,
	},
	},
	{
	.width = 1920,
	.height = 1080,
	.hblank = 550,
	.vblank = 3420,
	.vblank_min = 90,
	.vblank_max = 132840,
	.pclk = 594000000,
	.link_freq_idx = 0,
	.code = MEDIA_BUS_FMT_SRGGB12_1X12,
	.reg_list = {
		.num_of_regs = ARRAY_SIZE(mode_1920x1080_sdr_binning_regs),
		.regs = mode_1920x1080_sdr_binning_regs,
	},
	.frame_interval = {
		.denominator = 30,
		.numerator = 1,
	},
	},
};

static const struct imx678_mode supported_hdr_modes[] = {
	{
	.width = 1920,
	.height = 1080,
	.hblank = 1320,
	.vblank = 2250,
	.vblank_min = 90,
	.vblank_max = 132840,
	.pclk = 594000000,
	.link_freq_idx = 0,
	.code = MEDIA_BUS_FMT_SRGGB12_1X12,
	.reg_list = {
		.num_of_regs = ARRAY_SIZE(mode_1920x1080_3dol_binning_regs),
		.regs = mode_1920x1080_3dol_binning_regs,
	},
	.frame_interval = {
		.denominator = 8,
		.numerator = 1,
	},
	},

};

/**
 * to_imx678() - imv678 V4L2 sub-device to imx678 device.
 * @subdev: pointer to imx678 V4L2 sub-device
 *
 * Return: pointer to imx678 device
 */
static inline struct imx678 *to_imx678(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct imx678, sd);
}

/**
 * imx678_read_reg() - Read registers.
 * @imx678: pointer to imx678 device
 * @reg: register address
 * @len: length of bytes to read. Max supported bytes is 4
 * @val: pointer to register value to be filled.
 *
 * Big endian register addresses with little endian values.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_read_reg(struct imx678 *imx678, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx678->sd);
	struct i2c_msg msgs[2] = { 0 };
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
 * imx678_write_reg() - Write register
 * @imx678: pointer to imx678 device
 * @reg: register address
 * @len: length of bytes. Max supported bytes is 4
 * @val: register value
 *
 * Big endian register addresses with little endian values.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_write_reg(struct imx678 *imx678, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx678->sd);
	u8 buf[6] = { 0 };

	if (WARN_ON(len > 4))
		return -EINVAL;

	put_unaligned_be16(reg, buf);
	put_unaligned_le32(val, buf + 2);
	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

/**
 * imx678_write_regs() - Write a list of registers
 * @imx678: pointer to imx678 device
 * @regs: list of registers to be written
 * @len: length of registers array
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_write_regs(struct imx678 *imx678,
			     const struct imx678_reg *regs, u32 len)
{
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = imx678_write_reg(imx678, regs[i].address, 1, regs[i].val);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * imx678_update_controls() - Update control ranges based on streaming mode
 * @imx678: pointer to imx678 device
 * @mode: pointer to imx678_mode sensor mode
 *
 * Return: 0 if successful, error code otherwise.
 */
/*
static int imx678_update_controls(struct imx678* imx678,
	const struct imx678_mode* mode)
{
	int ret;

	ret = __v4l2_ctrl_s_ctrl(imx678->link_freq_ctrl, mode->link_freq_idx);
	if (ret)
		return ret;

	ret = __v4l2_ctrl_s_ctrl(imx678->hblank_ctrl, mode->hblank);
	if (ret)
		return ret;

	return __v4l2_ctrl_modify_range(imx678->vblank_ctrl, mode->vblank_min,
		mode->vblank_max, 1, mode->vblank);
}
*/
/**
 * imx678_update_exp_gain() - Set updated exposure and gain
 * @imx678: pointer to imx678 device
 * @exposure: updated exposure value
 * @gain: updated analog gain value
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_update_exp_gain(struct imx678 *imx678, u32 exposure, u32 gain)
{
	u32 lpfr, shutter;
	int ret;

	lpfr = imx678->vblank + imx678->cur_mode->height;
	shutter = lpfr - exposure;

	dev_dbg(imx678->dev, "Set long exp %u analog gain %u sh0 %u lpfr %u",
		 exposure, gain, shutter, lpfr);

	ret = imx678_write_reg(imx678, IMX678_REG_HOLD, 1, 1);
	if (ret)
		return ret;

	ret = imx678_write_reg(imx678, IMX678_REG_LPFR, 3, lpfr);
	if (ret)
		goto error_release_group_hold;

	ret = imx678_write_reg(imx678, IMX678_REG_SHUTTER, 3, shutter);
	if (ret)
		goto error_release_group_hold;

	ret = imx678_write_reg(imx678, IMX678_REG_AGAIN, 1, gain);

error_release_group_hold:
	imx678_write_reg(imx678, IMX678_REG_HOLD, 1, 0);

	return ret;
}

/*
 * imx678_set_test_pattern - Function called when setting test pattern
 * @priv: Pointer to device structure
 * @val: Variable for test pattern
 *
 * Set to different test patterns based on input value.
 *
 * Return: 0 on success
*/
static int imx678_set_test_pattern(struct imx678 *imx678, int val)
{
	int ret = 0;

	if (TEST_PATTERN_DISABLED == val)
		ret = imx678_write_reg(imx678, IMX678_TPG_EN_DUOUT, 1, val);
	else {
		ret = imx678_write_reg(imx678, IMX678_TPG_PATSEL_DUOUT, 1,
				       val - 1);
		if (!ret) {
			ret = imx678_write_regs(imx678, imx678_tpg_en_regs,
						ARRAY_SIZE(imx678_tpg_en_regs));
		}
	}
	return ret;
}

/**
 * imx678_set_ctrl() - Set subdevice control
 * @ctrl: pointer to v4l2_ctrl structure
 *
 * Supported controls:
 * - V4L2_CID_VBLANK
 * - V4L2_CID_TEST_PATTERN
 * - cluster controls:
 *   - V4L2_CID_ANALOGUE_GAIN
 *   - V4L2_CID_EXPOSURE
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx678 *imx678 =
		container_of(ctrl->handler, struct imx678, ctrl_handler);
	u32 analog_gain;
	u32 exposure;
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		imx678->vblank = imx678->vblank_ctrl->val;

		dev_dbg(imx678->dev, "Received vblank %u, new lpfr %u",
			imx678->vblank,
			imx678->vblank + imx678->cur_mode->height);

		ret = __v4l2_ctrl_modify_range(
			imx678->exp_ctrl, IMX678_EXPOSURE_MIN,
			imx678->vblank + imx678->cur_mode->height -
				IMX678_EXPOSURE_OFFSET,
			1, IMX678_EXPOSURE_DEFAULT);
		break;
	case V4L2_CID_EXPOSURE:

		/* Set controls only if sensor is in power on state */
		if (!pm_runtime_get_if_in_use(imx678->dev))
			return 0;

		exposure = ctrl->val;
		analog_gain = imx678->again_ctrl->val;

		dev_dbg(imx678->dev, "Received exp %u analog gain %u", exposure,
			analog_gain);

		ret = imx678_update_exp_gain(imx678, exposure, analog_gain);

		pm_runtime_put(imx678->dev);

		break;
	case V4L2_CID_TEST_PATTERN:
		if (!pm_runtime_get_if_in_use(imx678->dev))
			return 0;
		ret = imx678_set_test_pattern(imx678, ctrl->val);

		pm_runtime_put(imx678->dev);

		break;
	case V4L2_CID_WIDE_DYNAMIC_RANGE:
		if (imx678->streaming) {
			dev_warn(imx678->dev,
				"Cannot set WDR mode while streaming\n");
			return 0;
		}

		imx678->hdr_enabled = ctrl->val;
		dev_dbg(imx678->dev, "hdr enable set to %d\n", imx678->hdr_enabled);
		if (imx678->hdr_enabled)
			imx678->cur_mode = &supported_hdr_modes[0];
		else
			imx678->cur_mode = &supported_sdr_modes[0];
		ret = 0;
		break;
	default:
		dev_err(imx678->dev, "Invalid control %d", ctrl->id);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * imx678_enum_mbus_code() - Enumerate V4L2 sub-device mbus codes
 * @sd: pointer to imx678 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 * @code: V4L2 sub-device code enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx678_mode *supported_modes;
	struct imx678 *imx678 = to_imx678(sd);

	if (code->index > 0)
		return -EINVAL;

	if(imx678->hdr_enabled)
		supported_modes = (struct imx678_mode *)supported_hdr_modes;
	else
		supported_modes = (struct imx678_mode *)supported_sdr_modes;

	mutex_lock(&imx678->mutex);
	code->code = supported_modes[DEFAULT_MODE_IDX].code;
	mutex_unlock(&imx678->mutex);
	return 0;
}

/**
 * imx678_enum_frame_size() - Enumerate V4L2 sub-device frame sizes
 * @sd: pointer to imx678 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 * @fsize: V4L2 sub-device size enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fsize)
{
	struct imx678_mode *supported_modes;
	struct imx678 *imx678 = to_imx678(sd);
	int mode_count, i = 0;
	int min_width = INT_MAX;
	int min_height = INT_MAX;
	int max_width = 0;
	int max_height = 0;

	if (fsize->index > 0)
		return -EINVAL;

	if(imx678->hdr_enabled){
		supported_modes = (struct imx678_mode *)supported_hdr_modes;
		mode_count = sizeof(supported_hdr_modes) / sizeof(struct imx678_mode);
	}
	else {
		supported_modes = (struct imx678_mode *)supported_sdr_modes;
		mode_count = sizeof(supported_sdr_modes) / sizeof(struct imx678_mode);
	}

	mutex_lock(&imx678->mutex);
	for(i = 0; i < mode_count; i++){
		if (fsize->code == supported_modes[i].code) {
			if(supported_modes[i].width > max_width)
				max_width = supported_modes[i].width;
			else if(supported_modes[i].width < min_width)
				min_width = supported_modes[i].width;
			else if(supported_modes[i].height > max_height)
				max_height = supported_modes[i].height;
			else if(supported_modes[i].height < min_height)
				min_height = supported_modes[i].height;
		}
	}

	if (max_width == 0 || max_height == 0 ||
		min_width == INT_MAX || min_height == INT_MAX) {
		pr_debug("%s: Invalid code %d\n", __func__, fsize->code);
		mutex_unlock(&imx678->mutex);
		return -EINVAL;
	}

	fsize->min_width = min_width;
	fsize->max_width = max_width;
	fsize->min_height = min_height;
	fsize->max_height = max_height;
	mutex_unlock(&imx678->mutex);

	return 0;
}

/**
 * imx678_fill_pad_format() - Fill subdevice pad format
 *                            from selected sensor mode
 * @imx678: pointer to imx678 device
 * @mode: pointer to imx678_mode sensor mode
 * @fmt: V4L2 sub-device format need to be filled
 */
static void imx678_fill_pad_format(struct imx678 *imx678,
				   const struct imx678_mode *mode,
				   struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = mode->code;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;
	fmt->format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->format.quantization = V4L2_QUANTIZATION_DEFAULT;
	fmt->format.xfer_func = V4L2_XFER_FUNC_NONE;
}

/**
 * imx678_get_pad_format() - Get subdevice pad format
 * @sd: pointer to imx678 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx678 *imx678 = to_imx678(sd);

	mutex_lock(&imx678->mutex);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *framefmt;

		framefmt = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
		fmt->format = *framefmt;
	} else {
		imx678_fill_pad_format(imx678, imx678->cur_mode, fmt);
	}

	mutex_unlock(&imx678->mutex);

	return 0;
}

static int imx678_get_fmt_mode(struct imx678* imx678, struct v4l2_subdev_format* fmt, const struct imx678_mode** mode){
	struct imx678_mode *supported_modes;
	int mode_count = 0;
	int index = 0;

	if(!imx678 || !fmt || !mode)
		return -EINVAL;

	if(imx678->hdr_enabled){
		supported_modes = (struct imx678_mode *)supported_hdr_modes;
		mode_count = sizeof(supported_hdr_modes) / sizeof(struct imx678_mode);
	}
	else {
		supported_modes = (struct imx678_mode *)supported_sdr_modes;
		mode_count = sizeof(supported_sdr_modes) / sizeof(struct imx678_mode);
	}
	
	for (index = 0; index < mode_count; ++index) {
		if(supported_modes[index].width == fmt->format.width && supported_modes[index].height == fmt->format.height){
			*mode = &supported_modes[index];
			return 0;
		}
	}

	return -EINVAL;
}

/**
 * imx678_set_pad_format() - Set subdevice pad format
 * @sd: pointer to imx678 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx678 *imx678 = to_imx678(sd);
	const struct imx678_mode *mode;
	int ret = 0;
	mutex_lock(&imx678->mutex);
	
	ret = imx678_get_fmt_mode(imx678, fmt, &mode);
	if(ret){
		pr_err("%s - get_fmt failed with %d\n", __func__, ret);
		goto out;
	}

	imx678_fill_pad_format(imx678, mode, fmt);

	// even if which is V4L2_SUBDEV_FORMAT_TRY, update current format for tuning case
	memcpy(&imx678->curr_fmt, fmt, sizeof(struct v4l2_subdev_format));
	imx678->cur_mode = mode;
#ifdef IMX678_UPDATE_CONTROLS_TRY_FMT
		ret = imx678_update_controls(imx687, mode);
#endif

out:
	mutex_unlock(&imx678->mutex);
	return ret;
}

/**
 * imx678_init_pad_cfg() - Initialize sub-device pad configuration
 * @sd: pointer to imx678 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_init_pad_cfg(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state)
{
	struct imx678_mode *supported_modes;
	struct imx678 *imx678 = to_imx678(sd);
	struct v4l2_subdev_format fmt = { 0 };

	if(imx678->hdr_enabled)
		supported_modes = (struct imx678_mode *)supported_hdr_modes;
	else
		supported_modes = (struct imx678_mode *)supported_sdr_modes;

	fmt.which =
		sd_state ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	imx678_fill_pad_format(imx678, &supported_modes[DEFAULT_MODE_IDX],
			       &fmt);

	return imx678_set_pad_format(sd, sd_state, &fmt);
}

/**
 * imx678_start_streaming() - Start sensor stream
 * @imx678: pointer to imx678 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_start_streaming(struct imx678 *imx678)
{
	const struct imx678_reg_list *reg_list;
	int ret;

	/* Write sensor mode registers */
	pr_debug("%s - hdr_enabled: %d\n", __func__, imx678->hdr_enabled);
	reg_list = &imx678->cur_mode->reg_list;
	ret = imx678_write_regs(imx678, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(imx678->dev, "fail to write initial registers");
		return ret;
	}

	/* Setup handler will write actual exposure and gain */
	ret = __v4l2_ctrl_handler_setup(imx678->sd.ctrl_handler);
	if (ret) {
		dev_err(imx678->dev, "fail to setup handler (%d)", ret);
		return ret;
	}

	/* Start streaming */
	ret = imx678_write_reg(imx678, IMX678_REG_MODE_SELECT, 1,
			       IMX678_MODE_STREAMING);
	if (ret) {
		dev_err(imx678->dev, "fail to start streaming");
		return ret;
	}
	/* Start streaming */
	ret = imx678_write_reg(imx678, 0x3002, 1, 0);
	if (ret) {
		dev_err(imx678->dev, "fail to start streaming");
		return ret;
	}
	pr_info("imx678: start_streaming successful\n");
	return 0;
}

/**
 * imx678_stop_streaming() - Stop sensor stream
 * @imx678: pointer to imx678 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_stop_streaming(struct imx678 *imx678)
{
	return imx678_write_reg(imx678, IMX678_REG_MODE_SELECT, 1,
				IMX678_MODE_STANDBY);
}

/**
 * imx678_set_stream() - Enable sensor streaming
 * @sd: pointer to imx678 subdevice
 * @enable: set to enable sensor streaming
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx678 *imx678 = to_imx678(sd);
	int ret;

	mutex_lock(&imx678->mutex);

	if (imx678->streaming == enable) {
		mutex_unlock(&imx678->mutex);
		return 0;
	}

	if (enable) {
		ret = pm_runtime_resume_and_get(imx678->dev);
		if (ret < 0)
			goto error_unlock;

		ret = imx678_start_streaming(imx678);
		if (ret)
			goto error_power_off;
	} else {
		imx678_stop_streaming(imx678);
		pm_runtime_put(imx678->dev);
	}

	imx678->streaming = enable;

	mutex_unlock(&imx678->mutex);

	return 0;

error_power_off:
	pm_runtime_put(imx678->dev);
error_unlock:
	mutex_unlock(&imx678->mutex);

	return ret;
}

static int
imx678_find_nearest_frame_interval_mode(struct imx678 *imx678,
					struct v4l2_subdev_frame_interval *fi,
					struct imx678_mode const **mode)
{
	struct imx678_mode const* curr_mode;
	struct v4l2_mbus_framefmt* framefmt;
	struct imx678_mode *supported_modes;

	int min_diff = INT_MAX;
	int curr_diff;
	int i, found = 0, mode_count = 0;

	if (!imx678 || !fi) {
		return -EINVAL;
	}

	if(imx678->hdr_enabled){
		supported_modes = (struct imx678_mode *)supported_hdr_modes;
		mode_count = sizeof(supported_hdr_modes) / sizeof(struct imx678_mode);
	}
	else {
		supported_modes = (struct imx678_mode *)supported_sdr_modes;
		mode_count = sizeof(supported_sdr_modes) / sizeof(struct imx678_mode);
	}

	framefmt = &imx678->curr_fmt.format;

	for (i = 0; i < mode_count; ++i) {
		curr_mode = &supported_modes[i];

		if(curr_mode->width != framefmt->width || curr_mode->height != framefmt->height)
			continue;
		found = 1;

		curr_diff = abs(curr_mode->frame_interval.denominator -
				(int)(fi->interval.denominator /
				      fi->interval.numerator));
		if (curr_diff == 0) {
			*mode = curr_mode;
			return 0;
		}
		if (curr_diff < min_diff) {
			min_diff = curr_diff;
			*mode = curr_mode;
		}
	}

	if(!found){
		return -ENOTSUPP;
	}

	return 0;
}

/**
 * imx678_s_frame_interval - Set the frame interval
 * @sd: Pointer to V4L2 Sub device structure
 * @fi: Pointer to V4l2 Sub device frame interval structure
 *
 * This function is used to set the frame intervavl.
 *
 * Return: 0 on success
 */
static int imx678_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx678 *imx678 = to_imx678(sd);
	struct imx678_mode const *mode;
	int ret;

	ret = pm_runtime_resume_and_get(imx678->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&imx678->mutex);

	ret = imx678_find_nearest_frame_interval_mode(imx678, fi, &mode);

	if (ret == 0) {
		fi->interval = mode->frame_interval;
		imx678->cur_mode = mode;
		ret = __v4l2_ctrl_s_ctrl(imx678->vblank_ctrl, mode->vblank);
	}

	mutex_unlock(&imx678->mutex);
	pm_runtime_put(imx678->dev);

	return ret;
}

static int imx678_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx678 *imx678 = to_imx678(sd);

	mutex_lock(&imx678->mutex);
	fi->interval = imx678->cur_mode->frame_interval;
	mutex_unlock(&imx678->mutex);

	return 0;
}

/**
 * imx678_detect() - Detect imx678 sensor
 * @imx678: pointer to imx678 device
 *
 * Return: 0 if successful, -EIO if sensor id does not match
 */
static int imx678_detect(struct imx678 *imx678)
{
	int ret;
	u32 val;

	// IMX678_REG_ID that appeared in the SupportPackage value is 0 so check is irrelevant
	return 0;

	ret = imx678_read_reg(imx678, IMX678_REG_ID, 2, &val);
	if (ret)
		return ret;

	if (val != IMX678_ID) {
		dev_err(imx678->dev, "chip id mismatch: %x!=%x", IMX678_ID,
			val);
		return -ENXIO;
	}

	return 0;
}

/**
 * imx678_parse_hw_config() - Parse HW configuration and check if supported
 * @imx678: pointer to imx678 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_parse_hw_config(struct imx678 *imx678)
{
	struct fwnode_handle *fwnode = dev_fwnode(imx678->dev);
	struct v4l2_fwnode_endpoint bus_cfg = { .bus_type =
							V4L2_MBUS_CSI2_DPHY };
	struct fwnode_handle *ep;
	unsigned long rate;
	int ret;
	int i;

	if (!fwnode)
		return -ENXIO;

	/* Request optional reset pin */
	imx678->reset_gpio =
		devm_gpiod_get_optional(imx678->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(imx678->reset_gpio)) {
		dev_err(imx678->dev, "failed to get reset gpio %ld",
			PTR_ERR(imx678->reset_gpio));
		return PTR_ERR(imx678->reset_gpio);
	}

	/* Get sensor input clock */
	imx678->inclk = devm_clk_get(imx678->dev, NULL);
	if (IS_ERR(imx678->inclk)) {
		dev_err(imx678->dev, "could not get inclk");
		return PTR_ERR(imx678->inclk);
	}

	rate = clk_get_rate(imx678->inclk);
	if (rate != IMX678_INCLK_RATE) {
		dev_err(imx678->dev, "inclk frequency mismatch");
		return -EINVAL;
	}

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -ENXIO;

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != IMX678_NUM_DATA_LANES) {
		dev_err(imx678->dev,
			"number of CSI2 data lanes %d is not supported",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(imx678->dev, "no link frequencies defined");
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	for (i = 0; i < bus_cfg.nr_of_link_frequencies; i++)
		if (bus_cfg.link_frequencies[i] == IMX678_LINK_FREQ)
			goto done_endpoint_free;

	ret = -EINVAL;

done_endpoint_free:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

/* V4l2 subdevice ops */
static const struct v4l2_subdev_video_ops imx678_video_ops = {
	.s_stream = imx678_set_stream,
	.s_frame_interval = imx678_s_frame_interval,
	.g_frame_interval = imx678_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx678_pad_ops = {
	.init_cfg = imx678_init_pad_cfg,
	.enum_mbus_code = imx678_enum_mbus_code,
	.enum_frame_size = imx678_enum_frame_size,
	.get_fmt = imx678_get_pad_format,
	.set_fmt = imx678_set_pad_format,
};

static const struct v4l2_subdev_ops imx678_subdev_ops = {
	.video = &imx678_video_ops,
	.pad = &imx678_pad_ops,
};

/**
 * imx678_power_on() - Sensor power on sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx678 *imx678 = to_imx678(sd);
	int ret;

	gpiod_set_value_cansleep(imx678->reset_gpio, 1);

	ret = clk_prepare_enable(imx678->inclk);
	if (ret) {
		dev_err(imx678->dev, "fail to enable inclk");
		goto error_reset;
	}

	usleep_range(18000, 20000);

	return 0;

error_reset:
	gpiod_set_value_cansleep(imx678->reset_gpio, 0);

	return ret;
}

/**
 * imx678_power_off() - Sensor power off sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx678 *imx678 = to_imx678(sd);

	gpiod_set_value_cansleep(imx678->reset_gpio, 0);

	clk_disable_unprepare(imx678->inclk);

	return 0;
}

/**
 * imx678_init_controls() - Initialize sensor subdevice controls
 * @imx678: pointer to imx678 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_init_controls(struct imx678 *imx678)
{
	struct v4l2_ctrl_handler *ctrl_hdlr = &imx678->ctrl_handler;
	const struct imx678_mode *mode = imx678->cur_mode;
	u32 lpfr;
	int ret;

	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 7);
	if (ret)
		return ret;

	/* Serialize controls with sensor device */
	ctrl_hdlr->lock = &imx678->mutex;

	/* Initialize exposure and gain */
	lpfr = mode->vblank + mode->height;
	imx678->exp_ctrl = v4l2_ctrl_new_std(
		ctrl_hdlr, &imx678_ctrl_ops, V4L2_CID_EXPOSURE,
		IMX678_EXPOSURE_MIN, lpfr - IMX678_EXPOSURE_OFFSET,
		IMX678_EXPOSURE_STEP, IMX678_EXPOSURE_DEFAULT);

	imx678->again_ctrl =
		v4l2_ctrl_new_std(ctrl_hdlr, &imx678_ctrl_ops,
				  V4L2_CID_ANALOGUE_GAIN, IMX678_AGAIN_MIN,
				  IMX678_AGAIN_MAX, IMX678_AGAIN_STEP,
				  IMX678_AGAIN_DEFAULT);

	v4l2_ctrl_cluster(2, &imx678->exp_ctrl);

	imx678->vblank_ctrl =
		v4l2_ctrl_new_std(ctrl_hdlr, &imx678_ctrl_ops, V4L2_CID_VBLANK,
				  mode->vblank_min, mode->vblank_max, 1,
				  mode->vblank);

	imx678->test_pattern_ctrl = v4l2_ctrl_new_std_menu_items(
		ctrl_hdlr, &imx678_ctrl_ops, V4L2_CID_TEST_PATTERN,
		ARRAY_SIZE(imx678_test_pattern_menu) - 1, 0, 0,
		imx678_test_pattern_menu);
	
	imx678->mode_sel_ctrl = v4l2_ctrl_new_std(ctrl_hdlr, &imx678_ctrl_ops,
				V4L2_CID_WIDE_DYNAMIC_RANGE, IMX678_WDR_MIN,
				IMX678_WDR_MAX, IMX678_WDR_STEP,
				IMX678_WDR_DEFAULT);

	/* Read only controls */
	imx678->pclk_ctrl = v4l2_ctrl_new_std(ctrl_hdlr, &imx678_ctrl_ops,
					      V4L2_CID_PIXEL_RATE, mode->pclk,
					      mode->pclk, 1, mode->pclk);

	imx678->link_freq_ctrl = v4l2_ctrl_new_int_menu(
		ctrl_hdlr, &imx678_ctrl_ops, V4L2_CID_LINK_FREQ,
		ARRAY_SIZE(link_freq) - 1, mode->link_freq_idx, link_freq);
	if (imx678->link_freq_ctrl)
		imx678->link_freq_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx678->hblank_ctrl =
		v4l2_ctrl_new_std(ctrl_hdlr, &imx678_ctrl_ops, V4L2_CID_HBLANK,
				  IMX678_REG_MIN, IMX678_REG_MAX, 1,
				  mode->hblank);
	if (imx678->hblank_ctrl)
		imx678->hblank_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (ctrl_hdlr->error) {
		dev_err(imx678->dev, "control init failed: %d",
			ctrl_hdlr->error);
		v4l2_ctrl_handler_free(ctrl_hdlr);
		return ctrl_hdlr->error;
	}

	imx678->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

/**
 * imx678_probe() - I2C client device binding
 * @client: pointer to i2c client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_probe(struct i2c_client *client)
{
	struct imx678 *imx678;
	int ret;
	imx678 = devm_kzalloc(&client->dev, sizeof(*imx678), GFP_KERNEL);
	if (!imx678)
		return -ENOMEM;

	imx678->dev = &client->dev;

	dev_info(imx678->dev,
		 "imx678 probe start =-=-=-=-=-=-=-=-=-=-=-=-==-\n");

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&imx678->sd, client, &imx678_subdev_ops);

	ret = imx678_parse_hw_config(imx678);
	if (ret) {
		dev_err(imx678->dev, "HW configuration is not supported");
		return ret;
	}

	mutex_init(&imx678->mutex);

	ret = imx678_power_on(imx678->dev);
	if (ret) {
		dev_err(imx678->dev, "failed to power-on the sensor");
		goto error_mutex_destroy;
	}

	/* Check module identity */
	ret = imx678_detect(imx678);
	if (ret) {
		dev_err(imx678->dev, "failed to find sensor: %d", ret);
		goto error_power_off;
	}

	/* Set default mode to max resolution sdr */
	imx678->cur_mode = &supported_sdr_modes[DEFAULT_MODE_IDX];
	imx678->vblank = imx678->cur_mode->vblank;

	ret = imx678_init_controls(imx678);
	if (ret) {
		dev_err(imx678->dev, "failed to init controls: %d", ret);
		goto error_power_off;
	}

	/* Initialize subdev */
	imx678->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	imx678->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	imx678->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&imx678->sd.entity, 1, &imx678->pad);
	if (ret) {
		dev_err(imx678->dev, "failed to init entity pads: %d", ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&imx678->sd);
	if (ret < 0) {
		dev_err(imx678->dev, "failed to register async subdev: %d",
			ret);
		goto error_media_entity;
	}

	pm_runtime_set_active(imx678->dev);
	pm_runtime_enable(imx678->dev);
	pm_runtime_idle(imx678->dev);

	return 0;

error_media_entity:
	media_entity_cleanup(&imx678->sd.entity);
error_handler_free:
	v4l2_ctrl_handler_free(imx678->sd.ctrl_handler);
error_power_off:
	imx678_power_off(imx678->dev);
error_mutex_destroy:
	mutex_destroy(&imx678->mutex);

	return ret;
}

/**
 * imx678_remove() - I2C client device unbinding
 * @client: pointer to I2C client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx678_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx678 *imx678 = to_imx678(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	pm_runtime_disable(&client->dev);
	pm_runtime_suspended(&client->dev);

	mutex_destroy(&imx678->mutex);

	return 0;
}

static const struct dev_pm_ops imx678_pm_ops = { SET_RUNTIME_PM_OPS(
	imx678_power_off, imx678_power_on, NULL) };

static const struct of_device_id imx678_of_match[] = {
	{ .compatible = "sony,imx678" },
	{}
};

MODULE_DEVICE_TABLE(of, imx678_of_match);

static struct i2c_driver imx678_driver = {
	.probe_new = imx678_probe,
	.remove = imx678_remove,
	.driver = {
		.name = "imx678",
		.pm = &imx678_pm_ops,
		.of_match_table = imx678_of_match,
	},
};

module_i2c_driver(imx678_driver);

MODULE_DESCRIPTION("Sony imx678 sensor driver");
MODULE_AUTHOR("Muhyeon Kang, <muhyeon.kang@truen.co.kr>");
MODULE_LICENSE("GPL");
