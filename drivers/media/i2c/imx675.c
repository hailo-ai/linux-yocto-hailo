// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sony imx675 sensor driver
 *
 * Copyright (C) 2021 Intel Corporation
 */
#include "imx675.h"
#include <asm/unaligned.h>
#include <linux/videodev2.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <linux/kernel.h>
#include "sensor_id.h"


#define DEFAULT_MODE_IDX 0

/* Streaming Mode */
#define IMX675_REG_MODE_SELECT 0x3000
#define IMX675_MODE_STANDBY 0x01
#define IMX675_MODE_STREAMING 0x00

/* Lines per frame */
#define IMX675_REG_LPFR 0x3028 // VMAX
#define IMX675_REG_LPFR_BITS 20
#define IMX675_SDR_VMAX_VALUE 0x898
#define IMX675_2DOL_VMAX_VALUE 0x804

#define IMX675_SDR_HMAX_VALUE 0x465
#define IMX675_2DOL_HMAX_VALUE 0x25A

// Native sensor resolution (effective pixels): 2608x1964
// Using recommended resolution:
#define RES_5MP_WIDTH 2592
#define RES_5MP_HEIGHT 1944

#define IMX675_VMAX_MAX ((1 << IMX675_REG_LPFR_BITS) - 2) // max even value of unsigned IMX675_REG_LPFR_BITS

#define IMX675_SDR_MAX_VBLANK_FHD (IMX675_VMAX_MAX - 1080)
#define IMX675_SDR_MIN_VBLANK_FHD (IMX675_SDR_VMAX_VALUE - 1080)

#define IMX675_MAX_VBLANK_5MP (IMX675_VMAX_MAX - RES_5MP_HEIGHT)
#define IMX675_SDR_MIN_VBLANK_5MP (IMX675_SDR_VMAX_VALUE - RES_5MP_HEIGHT)
#define IMX675_2DOL_MIN_VBLANK_5MP (IMX675_2DOL_VMAX_VALUE - RES_5MP_HEIGHT)

#define LOW_U8_OF_U16(val) ((u8)(val & 0xff))
#define HIGH_U8_OF_U16(val) ((u8)((val >> 8) & 0xff))

#define IMX675_REG_HMAX 0x302C

/* defaults */
#define IMX675_2DOL_RHS1 0x7D
#define IMX675_DEFAULT_RHS2 0xaa
#define IMX675_EXPOSURE_DEFAULT 0x0648

/* gaps */
#define IMX675_2DOL_SMALL_GAP 2
#define IMX675_2DOL_LARGE_GAP 5
#define IMX675_3DOL_SMALL_GAP 3
#define IMX675_3DOL_LARGE_GAP 7

#define IMX675_2DOL_SHR0_RHS1_GAP   IMX675_2DOL_LARGE_GAP
#define IMX675_2DOL_SHR0_FSC_GAP    IMX675_2DOL_SMALL_GAP
#define IMX675_2DOL_SHR1_MIN_GAP    IMX675_2DOL_LARGE_GAP
#define IMX675_2DOL_SHR1_RHS1_GAP   IMX675_2DOL_SMALL_GAP

#define IMX675_3DOL_SHR0_RHS2_GAP   IMX675_3DOL_LARGE_GAP
#define IMX675_3DOL_SHR0_FSC_GAP    IMX675_3DOL_SMALL_GAP
#define IMX675_3DOL_SHR1_MIN_GAP    IMX675_3DOL_LARGE_GAP
#define IMX675_3DOL_SHR1_RHS1_GAP   IMX675_3DOL_SMALL_GAP
#define IMX675_3DOL_SHR2_RHS1_GAP   IMX675_3DOL_LARGE_GAP
#define IMX675_3DOL_SHR2_RHS2_GAP   IMX675_3DOL_SMALL_GAP

#define IMX675_INTEGER_STEP 1

/* Exposure control LEF */
#define IMX675_REG_SHUTTER 0x3050
#define IMX675_EXPOSURE_STEP 1

/* Exposure control SEF1 */
#define IMX675_REG_SHUTTER_SHORT 0x3054
#define IMX675_EXPOSURE_SHORT_STEP 1

/* Exposure control SEF2 */
#define IMX675_REG_SHUTTER_VERY_SHORT 0x3058
#define IMX675_EXPOSURE_VERY_SHORT_STEP 1

/* Analog gain control */
#define IMX675_REG_AGAIN 0x3070
#define IMX675_REG_AGAIN_SHORT 0x3072
#define IMX675_REG_AGAIN_VERY_SHORT 0x3074
#define IMX675_AGAIN_MIN 0
#define IMX675_AGAIN_MAX 240
#define IMX675_AGAIN_STEP 1
#define IMX675_AGAIN_DEFAULT 0

/* Hcg control */
#define IMX675_REG_HCG 0x3030
#define IMX675_HCG_MIN 0
#define IMX675_HCG_MAX 1
#define IMX675_HCG_STEP 1
#define IMX675_HCG_DEFAULT 0

/* Wide Dynamic Range control */
#define IMX675_WDR_MIN 0
#define IMX675_WDR_MAX 1
#define IMX675_WDR_STEP 1
#define IMX675_WDR_DEFAULT 0

/* Group hold register */
#define IMX675_REG_HOLD 0x3001

/* Input clock rate */
enum imx675_input_clk_rate_code {
	INPUT_CLK_74_25_MHZ = 0,
	INPUT_CLK_37_125_MHZ,
	INPUT_CLK_72_MHZ,
	INPUT_CLK_27_MHZ,
	INPUT_CLK_24_MHZ,
	INPUT_CLK_36_MHZ,
	INPUT_CLK_18_MHZ,
	INPUT_CLK_13_5_MHZ,
};

#define IMX675_INCLK_CODE INPUT_CLK_27_MHZ
/* TODO: This should actually be 27 Mhz, but this value only exists
 * for validation with the device tree, and the device tree always returns
 * 24 MHz at the moment. */
#define IMX675_INCLK_RATE 24000000

/* CSI2 HW configuration */
#define IMX675_NUM_DATA_LANES 4

#define IMX675_REG_MIN 0x00
#define IMX675_REG_MAX 0xfffff

#define IMX675_TPG_EN_DUOUT 0x30e0 /* TEST PATTERN ENABLE */
#define IMX675_TPG_PATSEL_DUOUT 0x30e2 /*Patsel mode */
#define IMX675_TPG_COLOR_WIDTH 0x30e4 /*color width */

#define NON_NEGATIVE(val) ((val) < 0 ? 0 : (val))
#define MAX(val1, val2) ((val1) < val2 ? val2 : (val1))

static u32 imx675_reg_shutter[3] = {IMX675_REG_SHUTTER, IMX675_REG_SHUTTER_SHORT, IMX675_REG_SHUTTER_VERY_SHORT};
static u32 imx675_reg_again[3] = {IMX675_REG_AGAIN, IMX675_REG_AGAIN_SHORT, IMX675_REG_AGAIN_VERY_SHORT};

enum imx675_exposure_type {
	LEF,
	SEF1,
	SEF2,
};

static int imx675_set_ctrl(struct v4l2_ctrl *ctrl);
static int imx675_get_ctrl(struct v4l2_ctrl *ctrl);

/*
 * imx675 test pattern related structure
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
 * enum imx675_test_pattern_menu - imx675 test pattern options
 */
static const char *const imx675_test_pattern_menu[] = {
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
static const struct v4l2_ctrl_ops imx675_ctrl_ops = {
	.s_ctrl = imx675_set_ctrl,
};

/* V4l2 subdevice control ops*/
static const struct v4l2_ctrl_ops imx675_get_ctrl_ops = {
	.g_volatile_ctrl = imx675_get_ctrl,
};

/**
 * struct imx675_reg - imx675 sensor register
 * @address: Register address
 * @val: Register value
 */
struct imx675_reg {
	u16 address;
	u8 val;
};

/**
 * struct imx675_reg_list - imx675 sensor register list
 * @num_of_regs: Number of registers in the list
 * @regs: Pointer to register list
 */
struct imx675_reg_list {
	u32 num_of_regs;
	const struct imx675_reg *regs;
};

u32 _get_mode_reg_val_by_address(const struct imx675_reg_list *reg_list, u16 reg_address, int num_bytes) {
	u32 left = 0;
	u32 right = reg_list->num_of_regs - 1;
	u32 val = 0;
	int i = 0;
	while (left <= right) {
		u32 mid = left + (right - left) / 2;
		if (reg_list->regs[mid].address == reg_address) {
			val = 0;
			for (i = 0; i < num_bytes; i++) {
				if (reg_list->regs[mid + i].address == reg_address + i) {
					val |= (reg_list->regs[mid + i].val & 0xff) << (8 * i);
				}
			}
			return val;
		}
		if (reg_list->regs[mid].address < reg_address) {
			left = mid + 1;
		} else {
			right = mid - 1;
		}
	}
	return 0;
}

/**
 * struct imx675_mode - imx675 sensor mode structure
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
struct imx675_mode {
	u32 width;
	u32 height;
	u32 code;
	u32 hblank;
	u32 vblank;
	u32 vblank_min;
	u32 vblank_max;
	u64 pclk;
	u32 link_freq_idx;
	u32 rhs1;
	u32 rhs2;
	u8 dol;
	struct imx675_reg_list reg_list;
	struct v4l2_fract frame_interval;
};

int compare_imx675_mode(const struct imx675_mode *mode1, const struct imx675_mode *mode2) {
	bool not_equal = (mode1->width != mode2->width) ||
		   (mode1->height != mode2->height) ||
		   (mode1->code != mode2->code) ||
		   (mode1->hblank != mode2->hblank) ||
		   (mode1->vblank != mode2->vblank) ||
		   (mode1->vblank_min != mode2->vblank_min) ||
		   (mode1->vblank_max != mode2->vblank_max) ||
		   (mode1->pclk != mode2->pclk);
	return not_equal;
}

struct exp_gain_ctrl_cluster {
	struct v4l2_ctrl *exp_ctrl;
	struct v4l2_ctrl *again_ctrl;
};

/**
 * struct imx675 - imx675 sensor device structure
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
struct imx675 {
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
	struct v4l2_ctrl *rhs1_ctrl;
	struct v4l2_ctrl *rhs2_ctrl;
	struct v4l2_ctrl *shr0_ctrl;
	struct v4l2_ctrl *shr1_ctrl;
	struct v4l2_ctrl *shr2_ctrl;
	struct v4l2_ctrl *vmax_ctrl;
	struct v4l2_ctrl *hmax_ctrl;
	struct v4l2_ctrl *test_pattern_ctrl;
	struct v4l2_ctrl *mode_sel_ctrl;
	struct v4l2_ctrl *hcg_ctrl;
	struct exp_gain_ctrl_cluster lef;
	struct exp_gain_ctrl_cluster sef1;
	struct exp_gain_ctrl_cluster sef2;
	u32 vblank;
	const struct imx675_mode *cur_mode;
	struct mutex mutex;
	bool streaming;
	bool hdr_enabled;
	struct v4l2_subdev_format curr_fmt;
};

static const s64 link_freq[] = {
	720000000, 1188000000,
};

static const struct imx675_reg mode_native_5mp_all_pixel_30fps[] = { // Config #16
	/* Using default value for 0x3000 (STANDBY, 0x01) */
	/* Using default value for 0x3001 (REGHOLD, 0x00) */
	/* Using default value for 0x3002 (XMSTA, 0x01) */
	{ 0x3014, IMX675_INCLK_CODE }, // MANUAL (default(0x00) -> our clock's value)
	{ 0x3015, 0x06 }, /* DATARATE_SEL [3:0] */
	{ 0x3018, 0x04 }, // MANUAL (default(0x00) -> recommended resolution)
	/* Using default value for 0x3019 (CFMODE, 0x00) */
	/* Using default value for 0x301A (WDMODE [7:0], 0x00) */
	/* Using default value for 0x301B (ADDMODE [1:0], 0x00) */
	/* Using default value for 0x301C (THIN_V_EN [7:0], 0x00) */
	/* Using default value for 0x301E (VCMODE [7:0], 0x01) */
	/* Using default value for 0x3020 (HREVERSE, 0x00) */
	/* Using default value for 0x3021 (VREVERSE, 0x00) */
	{ 0x3022, 0x01 }, /* ADBIT [1:0] */
	/* Using default value for 0x3023 (MDBIT, 0x01) */
	{ 0x3028, 0x98 }, /* VMAX [19:0] */
	{ 0x3029, 0x08 },
	/* Using default value for 0x302A (0x00) */
	{ 0x302C, 0x65 }, /* HMAX [15:0] */
	/* Using default value for 0x302D (0x04) */
	/* Using default value for 0x3030 (FDG_SEL0 [1:0], 0x00) */
	/* Using default value for 0x3031 (FDG_SEL1 [1:0], 0x00) */
	/* Using default value for 0x3032 (FDG_SEL2 [1:0], 0x00) */
	/* Using default value for 0x303C (PIX_HST [12:0], 0x00) */
	/* Using default value for 0x303D (0x00) */
	{ 0x303E, LOW_U8_OF_U16(RES_5MP_WIDTH) }, // MANUAL (default(0x30) -> recommended resolution)
	{ 0x303F, HIGH_U8_OF_U16(RES_5MP_WIDTH) }, // MANUAL (default(0x0A) -> recommended resolution)
	/* Using default value for 0x3040 (LANEMODE [2:0], 0x03) */
	/* Using default value for 0x3044 (PIX_VST [11:0], 0x00) */
	/* Using default value for 0x3045 (0x00) */
	{ 0x3046, LOW_U8_OF_U16(RES_5MP_HEIGHT) }, // MANUAL (default(0xAC) -> recommended resolution)
	{ 0x3047, HIGH_U8_OF_U16(RES_5MP_HEIGHT) }, // MANUAL (default(0x07) -> recommended resolution)
	/* Using default value for 0x304C (GAIN_HG0 [10:0], 0x00) */
	/* Using default value for 0x304D (0x00) */
	{ 0x3050, 0x04 }, /* SHR0 [19:0] */
	/* Using default value for 0x3051 (0x00) */
	/* Using default value for 0x3052 (0x00) */
	/* Using default value for 0x3054 (SHR1 [19:0], 0x93) */
	/* Using default value for 0x3055 (0x00) */
	/* Using default value for 0x3056 (0x00) */
	/* Using default value for 0x3058 (SHR2 [19:0], 0x53) */
	/* Using default value for 0x3059 (0x00) */
	/* Using default value for 0x305A (0x00) */
	/* Using default value for 0x3060 (RHS1 [19:0], 0x95) */
	/* Using default value for 0x3061 (0x00) */
	/* Using default value for 0x3062 (0x00) */
	/* Using default value for 0x3064 (RHS2 [19:0], 0x56) */
	/* Using default value for 0x3065 (0x00) */
	/* Using default value for 0x3066 (0x00) */
	/* Using default value for 0x3070 (GAIN_0 [10:0], 0x00) */
	/* Using default value for 0x3071 (0x00) */
	/* Using default value for 0x3072 (GAIN_1 [10:0], 0x00) */
	/* Using default value for 0x3073 (0x00) */
	/* Using default value for 0x3074 (GAIN_2 [10:0], 0x00) */
	/* Using default value for 0x3075 (0x00) */
	/* Using default value for 0x30A4 (XVSOUTSEL [1:0], 0xAA) */
	{ 0x30A6, 0x00 }, /* XVS_DRV [1:0] */
	/* Using default value for 0x30CC (0x00) */
	/* Using default value for 0x30CD (0x00) */
	{ 0x30CE, 0x02 },
	/* Using default value for 0x30DC (BLKLEVEL [9:0], 0x32) */
	/* Using default value for 0x30DD (0x40) */
	/* Using default value for 0x310C (0x01) */
	/* Using default value for 0x3130 (0x01) */
	{ 0x3148, 0x00 },
	/* Using default value for 0x315E (0x10) */
	{ 0x3400, 0x00 }, /* GAIN_PGC_FIDMD - 0: set individual exposure gains*/
	{ 0x3460, 0x22 },
	{ 0x347B, 0x02 },
	{ 0x3492, 0x08 },
	/* Using default value for 0x3890 (HFR_EN [3:0], 0x08) */
	/* Using default value for 0x3891 (0x00) */
	/* Using default value for 0x3893 (0x00) */
	{ 0x3B1D, 0x17 },
	{ 0x3B44, 0x3F },
	{ 0x3B60, 0x03 },
	{ 0x3C03, 0x04 },
	{ 0x3C04, 0x04 },
	{ 0x3C0A, 0x1F },
	{ 0x3C0B, 0x1F },
	{ 0x3C0C, 0x1F },
	{ 0x3C0D, 0x1F },
	{ 0x3C0E, 0x1F },
	{ 0x3C0F, 0x1F },
	{ 0x3C30, 0x73 },
	{ 0x3C3C, 0x20 },
	/* Using default value for 0x3C44 (0x06) */
	{ 0x3C7C, 0xB9 },
	{ 0x3C7D, 0x01 },
	{ 0x3C7E, 0xB7 },
	{ 0x3C7F, 0x01 },
	{ 0x3CB0, 0x00 },
	{ 0x3CB2, 0xFF },
	{ 0x3CB3, 0x03 },
	{ 0x3CB4, 0xFF },
	{ 0x3CB5, 0x03 },
	{ 0x3CBA, 0xFF },
	{ 0x3CBB, 0x03 },
	{ 0x3CC0, 0xFF },
	{ 0x3CC1, 0x03 },
	{ 0x3CC2, 0x00 },
	{ 0x3CC6, 0xFF },
	{ 0x3CC7, 0x03 },
	{ 0x3CC8, 0xFF },
	{ 0x3CC9, 0x03 },
	{ 0x3E00, 0x1E },
	{ 0x3E02, 0x04 },
	{ 0x3E03, 0x00 },
	{ 0x3E20, 0x04 },
	{ 0x3E21, 0x00 },
	{ 0x3E22, 0x1E },
	{ 0x3E24, 0xBA },
	{ 0x3E72, 0x85 },
	{ 0x3E76, 0x0C },
	{ 0x3E77, 0x01 },
	{ 0x3E7A, 0x85 },
	{ 0x3E7E, 0x1F },
	{ 0x3E82, 0xA6 },
	{ 0x3E86, 0x2D },
	{ 0x3EE2, 0x33 },
	{ 0x3EE3, 0x03 },
	{ 0x4490, 0x07 },
	{ 0x4494, 0x19 },
	{ 0x4495, 0x00 },
	{ 0x4496, 0xBB },
	{ 0x4497, 0x00 },
	{ 0x4498, 0x55 },
	{ 0x449A, 0x50 },
	{ 0x449C, 0x50 },
	{ 0x449E, 0x50 },
	{ 0x44A0, 0x3C },
	{ 0x44A2, 0x19 },
	{ 0x44A4, 0x19 },
	{ 0x44A6, 0x19 },
	{ 0x44A8, 0x4B },
	{ 0x44AA, 0x4B },
	{ 0x44AC, 0x4B },
	{ 0x44AE, 0x4B },
	{ 0x44B0, 0x3C },
	{ 0x44B2, 0x19 },
	{ 0x44B4, 0x19 },
	{ 0x44B6, 0x19 },
	{ 0x44B8, 0x4B },
	{ 0x44BA, 0x4B },
	{ 0x44BC, 0x4B },
	{ 0x44BE, 0x4B },
	{ 0x44C0, 0x3C },
	{ 0x44C2, 0x19 },
	{ 0x44C4, 0x19 },
	{ 0x44C6, 0x19 },
	{ 0x44C8, 0xF0 },
	{ 0x44CA, 0xEB },
	{ 0x44CC, 0xEB },
	{ 0x44CE, 0xE6 },
	{ 0x44D0, 0xE6 },
	{ 0x44D2, 0xBB },
	{ 0x44D4, 0xBB },
	{ 0x44D6, 0xBB },
	{ 0x44D8, 0xE6 },
	{ 0x44DA, 0xE6 },
	{ 0x44DC, 0xE6 },
	{ 0x44DE, 0xE6 },
	{ 0x44E0, 0xE6 },
	{ 0x44E2, 0xBB },
	{ 0x44E4, 0xBB },
	{ 0x44E6, 0xBB },
	{ 0x44E8, 0xE6 },
	{ 0x44EA, 0xE6 },
	{ 0x44EC, 0xE6 },
	{ 0x44EE, 0xE6 },
	{ 0x44F0, 0xE6 },
	{ 0x44F2, 0xBB },
	{ 0x44F4, 0xBB },
	{ 0x44F6, 0xBB },
	{ 0x4538, 0x15 },
	{ 0x4539, 0x15 },
	{ 0x453A, 0x15 },
	{ 0x4544, 0x15 },
	{ 0x4545, 0x15 },
	{ 0x4546, 0x15 },
	{ 0x4550, 0x10 },
	{ 0x4551, 0x10 },
	{ 0x4552, 0x10 },
	{ 0x4553, 0x10 },
	{ 0x4554, 0x10 },
	{ 0x4555, 0x10 },
	{ 0x4556, 0x10 },
	{ 0x4557, 0x10 },
	{ 0x4558, 0x10 },
	{ 0x455C, 0x10 },
	{ 0x455D, 0x10 },
	{ 0x455E, 0x10 },
	{ 0x455F, 0x10 },
	{ 0x4560, 0x10 },
	{ 0x4561, 0x10 },
	{ 0x4562, 0x10 },
	{ 0x4563, 0x10 },
	{ 0x4564, 0x10 },
	/* Using default value for 0x4569 (0x01) */
	/* Using default value for 0x456A (0x01) */
	/* Using default value for 0x456B (0x06) */
	/* Using default value for 0x456C (0x06) */
	/* Using default value for 0x456D (0x06) */
	/* Using default value for 0x456E (0x06) */
	/* Using default value for 0x456F (0x06) */
	/* Using default value for 0x4570 (0x06) */
};

static const struct imx675_reg mode_fhd_crop_30fps[] = {
	/* Using default value for 0x3000 (STANDBY, 0x01) */
	/* Using default value for 0x3001 (REGHOLD, 0x00) */
	/* Using default value for 0x3002 (XMSTA, 0x01) */
	{ 0x3014, IMX675_INCLK_CODE }, // MANUAL (default(0x00) -> our clock's value)
	{ 0x3015, 0x06 }, /* DATARATE_SEL [3:0] */
	{ 0x3018, 0x04 }, // MANUAL (default(0x00) -> crop to FHD)
	/* Using default value for 0x3019 (CFMODE, 0x00) */
	/* Using default value for 0x301A (WDMODE [7:0], 0x00) */
	/* Using default value for 0x301B (ADDMODE [1:0], 0x00) */
	/* Using default value for 0x301C (THIN_V_EN [7:0], 0x00) */
	/* Using default value for 0x301E (VCMODE [7:0], 0x01) */
	/* Using default value for 0x3020 (HREVERSE, 0x00) */
	/* Using default value for 0x3021 (VREVERSE, 0x00) */
	{ 0x3022, 0x01 }, /* ADBIT [1:0] */
	/* Using default value for 0x3023 (MDBIT, 0x01) */
	{ 0x3028, 0x98 }, /* VMAX [19:0] */
	{ 0x3029, 0x08 },
	/* Using default value for 0x302A (0x00) */
	{ 0x302C, 0x65 }, /* HMAX [15:0] */
	/* Using default value for 0x302D (0x04) */
	/* Using default value for 0x3030 (FDG_SEL0 [1:0], 0x00) */
	/* Using default value for 0x3031 (FDG_SEL1 [1:0], 0x00) */
	/* Using default value for 0x3032 (FDG_SEL2 [1:0], 0x00) */
	/* Using default value for 0x303C (PIX_HST [12:0], 0x00) */
	/* Using default value for 0x303D (0x00) */
	{ 0x303E, LOW_U8_OF_U16(1920) }, // MANUAL (default(0x30) -> crop to FHD)
    { 0x303F, HIGH_U8_OF_U16(1920) }, // MANUAL (default(0x0A) -> crop to FHD)
	/* Using default value for 0x3040 (LANEMODE [2:0], 0x03) */
	/* Using default value for 0x3044 (PIX_VST [11:0], 0x00) */
	/* Using default value for 0x3045 (0x00) */
	{ 0x3046, LOW_U8_OF_U16(1080) }, // MANUAL (default(0xAC) -> crop to FHD)
	{ 0x3047, HIGH_U8_OF_U16(1080) }, // MANUAL (default(0x07) -> crop to FHD)
	/* Using default value for 0x304C (GAIN_HG0 [10:0], 0x00) */
	/* Using default value for 0x304D (0x00) */
	{ 0x3050, 0x04 }, /* SHR0 [19:0] */
	/* Using default value for 0x3051 (0x00) */
	/* Using default value for 0x3052 (0x00) */
	/* Using default value for 0x3054 (SHR1 [19:0], 0x93) */
	/* Using default value for 0x3055 (0x00) */
	/* Using default value for 0x3056 (0x00) */
	/* Using default value for 0x3058 (SHR2 [19:0], 0x53) */
	/* Using default value for 0x3059 (0x00) */
	/* Using default value for 0x305A (0x00) */
	/* Using default value for 0x3060 (RHS1 [19:0], 0x95) */
	/* Using default value for 0x3061 (0x00) */
	/* Using default value for 0x3062 (0x00) */
	/* Using default value for 0x3064 (RHS2 [19:0], 0x56) */
	/* Using default value for 0x3065 (0x00) */
	/* Using default value for 0x3066 (0x00) */
	/* Using default value for 0x3070 (GAIN_0 [10:0], 0x00) */
	/* Using default value for 0x3071 (0x00) */
	/* Using default value for 0x3072 (GAIN_1 [10:0], 0x00) */
	/* Using default value for 0x3073 (0x00) */
	/* Using default value for 0x3074 (GAIN_2 [10:0], 0x00) */
	/* Using default value for 0x3075 (0x00) */
	/* Using default value for 0x30A4 (XVSOUTSEL [1:0], 0xAA) */
	{ 0x30A6, 0x00 }, /* XVS_DRV [1:0] */
	/* Using default value for 0x30CC (0x00) */
	/* Using default value for 0x30CD (0x00) */
	{ 0x30CE, 0x02 },
	/* Using default value for 0x30DC (BLKLEVEL [9:0], 0x32) */
	/* Using default value for 0x30DD (0x40) */
	/* Using default value for 0x310C (0x01) */
	/* Using default value for 0x3130 (0x01) */
	{ 0x3148, 0x00 },
	/* Using default value for 0x315E (0x10) */
	{ 0x3400, 0x00 }, /* GAIN_PGC_FIDMD - 0: set individual exposure gains*/
	{ 0x3460, 0x22 },
	{ 0x347B, 0x02 },
	{ 0x3492, 0x08 },
	/* Using default value for 0x3890 (HFR_EN [3:0], 0x08) */
	/* Using default value for 0x3891 (0x00) */
	/* Using default value for 0x3893 (0x00) */
	{ 0x3B1D, 0x17 },
	{ 0x3B44, 0x3F },
	{ 0x3B60, 0x03 },
	{ 0x3C03, 0x04 },
	{ 0x3C04, 0x04 },
	{ 0x3C0A, 0x1F },
	{ 0x3C0B, 0x1F },
	{ 0x3C0C, 0x1F },
	{ 0x3C0D, 0x1F },
	{ 0x3C0E, 0x1F },
	{ 0x3C0F, 0x1F },
	{ 0x3C30, 0x73 },
	{ 0x3C3C, 0x20 },
	/* Using default value for 0x3C44 (0x06) */
	{ 0x3C7C, 0xB9 },
	{ 0x3C7D, 0x01 },
	{ 0x3C7E, 0xB7 },
	{ 0x3C7F, 0x01 },
	{ 0x3CB0, 0x00 },
	{ 0x3CB2, 0xFF },
	{ 0x3CB3, 0x03 },
	{ 0x3CB4, 0xFF },
	{ 0x3CB5, 0x03 },
	{ 0x3CBA, 0xFF },
	{ 0x3CBB, 0x03 },
	{ 0x3CC0, 0xFF },
	{ 0x3CC1, 0x03 },
	{ 0x3CC2, 0x00 },
	{ 0x3CC6, 0xFF },
	{ 0x3CC7, 0x03 },
	{ 0x3CC8, 0xFF },
	{ 0x3CC9, 0x03 },
	{ 0x3E00, 0x1E },
	{ 0x3E02, 0x04 },
	{ 0x3E03, 0x00 },
	{ 0x3E20, 0x04 },
	{ 0x3E21, 0x00 },
	{ 0x3E22, 0x1E },
	{ 0x3E24, 0xBA },
	{ 0x3E72, 0x85 },
	{ 0x3E76, 0x0C },
	{ 0x3E77, 0x01 },
	{ 0x3E7A, 0x85 },
	{ 0x3E7E, 0x1F },
	{ 0x3E82, 0xA6 },
	{ 0x3E86, 0x2D },
	{ 0x3EE2, 0x33 },
	{ 0x3EE3, 0x03 },
	{ 0x4490, 0x07 },
	{ 0x4494, 0x19 },
	{ 0x4495, 0x00 },
	{ 0x4496, 0xBB },
	{ 0x4497, 0x00 },
	{ 0x4498, 0x55 },
	{ 0x449A, 0x50 },
	{ 0x449C, 0x50 },
	{ 0x449E, 0x50 },
	{ 0x44A0, 0x3C },
	{ 0x44A2, 0x19 },
	{ 0x44A4, 0x19 },
	{ 0x44A6, 0x19 },
	{ 0x44A8, 0x4B },
	{ 0x44AA, 0x4B },
	{ 0x44AC, 0x4B },
	{ 0x44AE, 0x4B },
	{ 0x44B0, 0x3C },
	{ 0x44B2, 0x19 },
	{ 0x44B4, 0x19 },
	{ 0x44B6, 0x19 },
	{ 0x44B8, 0x4B },
	{ 0x44BA, 0x4B },
	{ 0x44BC, 0x4B },
	{ 0x44BE, 0x4B },
	{ 0x44C0, 0x3C },
	{ 0x44C2, 0x19 },
	{ 0x44C4, 0x19 },
	{ 0x44C6, 0x19 },
	{ 0x44C8, 0xF0 },
	{ 0x44CA, 0xEB },
	{ 0x44CC, 0xEB },
	{ 0x44CE, 0xE6 },
	{ 0x44D0, 0xE6 },
	{ 0x44D2, 0xBB },
	{ 0x44D4, 0xBB },
	{ 0x44D6, 0xBB },
	{ 0x44D8, 0xE6 },
	{ 0x44DA, 0xE6 },
	{ 0x44DC, 0xE6 },
	{ 0x44DE, 0xE6 },
	{ 0x44E0, 0xE6 },
	{ 0x44E2, 0xBB },
	{ 0x44E4, 0xBB },
	{ 0x44E6, 0xBB },
	{ 0x44E8, 0xE6 },
	{ 0x44EA, 0xE6 },
	{ 0x44EC, 0xE6 },
	{ 0x44EE, 0xE6 },
	{ 0x44F0, 0xE6 },
	{ 0x44F2, 0xBB },
	{ 0x44F4, 0xBB },
	{ 0x44F6, 0xBB },
	{ 0x4538, 0x15 },
	{ 0x4539, 0x15 },
	{ 0x453A, 0x15 },
	{ 0x4544, 0x15 },
	{ 0x4545, 0x15 },
	{ 0x4546, 0x15 },
	{ 0x4550, 0x10 },
	{ 0x4551, 0x10 },
	{ 0x4552, 0x10 },
	{ 0x4553, 0x10 },
	{ 0x4554, 0x10 },
	{ 0x4555, 0x10 },
	{ 0x4556, 0x10 },
	{ 0x4557, 0x10 },
	{ 0x4558, 0x10 },
	{ 0x455C, 0x10 },
	{ 0x455D, 0x10 },
	{ 0x455E, 0x10 },
	{ 0x455F, 0x10 },
	{ 0x4560, 0x10 },
	{ 0x4561, 0x10 },
	{ 0x4562, 0x10 },
	{ 0x4563, 0x10 },
	{ 0x4564, 0x10 },
	/* Using default value for 0x4569 (0x01) */
	/* Using default value for 0x456A (0x01) */
	/* Using default value for 0x456B (0x06) */
	/* Using default value for 0x456C (0x06) */
	/* Using default value for 0x456D (0x06) */
	/* Using default value for 0x456E (0x06) */
	/* Using default value for 0x456F (0x06) */
	/* Using default value for 0x4570 (0x06) */
};

static const struct imx675_reg imx675_tpg_en_regs[] = {
	//TPG config
	{ 0x3042, 0x00 }, //XSIZE_OVERLAP
	{ 0x30e0, 0x01 }, //TPG_EN_DUOUT
	{ 0x30e4, 0x13 }, //TPG_COLORWIDTH
};

static const struct imx675_reg mode_native_5mp_2dol_all_pixel_30fps[] = {  // Config #27
	/* Using default value for 0x3000 (STANDBY, 0x01) */
	/* Using default value for 0x3001 (REGHOLD, 0x00) */
	/* Using default value for 0x3002 (XMSTA, 0x01) */
	{ 0x3014, IMX675_INCLK_CODE }, // MANUAL (default(0x00) -> our clock's value)
	/* Using default value for 0x3015 (DATARATE_SEL [3:0], 0x04) */
	{ 0x3018, 0x04 }, // MANUAL (default(0x00) -> recommended resolution)
	/* Using default value for 0x3019 (CFMODE, 0x00) */
	{ 0x301A, 0x01 }, /* WDMODE [7:0] */
	/* Using default value for 0x301B (ADDMODE [1:0], 0x00) */
	{ 0x301C, 0x01 }, /* THIN_V_EN [7:0] */
	/* Using default value for 0x301E (VCMODE [7:0], 0x01) */
	/* Using default value for 0x3020 (HREVERSE, 0x00) */
	/* Using default value for 0x3021 (VREVERSE, 0x00) */
	{ 0x3022, 0x01 }, /* ADBIT [1:0] */
	/* Using default value for 0x3023 (MDBIT, 0x01) */
	{ 0x3028, 0x04 }, /* VMAX [19:0] */ // decimal 2052
	{ 0x3029, 0x08 },
	/* Using default value for 0x302A (0x00) */
	{ 0x302C, 0x5a }, /* HMAX [15:0] */ // decimal 602
	{ 0x302D, 0x02 },
	/* Using default value for 0x3030 (FDG_SEL0 [1:0], 0x00) */
	/* Using default value for 0x3031 (FDG_SEL1 [1:0], 0x00) */
	/* Using default value for 0x3032 (FDG_SEL2 [1:0], 0x00) */
	/* Using default value for 0x303C (PIX_HST [12:0], 0x00) */
	/* Using default value for 0x303D (0x00) */
	{ 0x303E, LOW_U8_OF_U16(RES_5MP_WIDTH) }, // MANUAL (default(0x30) -> recommended resolution)
	{ 0x303F, HIGH_U8_OF_U16(RES_5MP_WIDTH) }, // MANUAL (default(0x0A) -> recommended resolution)
	/* Using default value for 0x3040 (LANEMODE [2:0], 0x03) */
	/* Using default value for 0x3044 (PIX_VST [11:0], 0x00) */
	/* Using default value for 0x3045 (0x00) */
	{ 0x3046, LOW_U8_OF_U16(RES_5MP_HEIGHT) }, // MANUAL (default(0xAC) -> recommended resolution)
	{ 0x3047, HIGH_U8_OF_U16(RES_5MP_HEIGHT) }, // MANUAL (default(0x07) -> recommended resolution)
	/* Using default value for 0x304C (GAIN_HG0 [10:0], 0x00) */
	/* Using default value for 0x304D (0x00) */
	{ 0x3050, 0xf4 }, /* SHR0 [19:0] */ // decimal 1268
	{ 0x3051, 0x04 },
	/* Using default value for 0x3052 (0x00) */
	{ 0x3054, 0x05 }, /* SHR1 [19:0] */ // decimal 5
	/* Using default value for 0x3055 (0x00) */
	/* Using default value for 0x3056 (0x00) */
	/* Using default value for 0x3058 (SHR2 [19:0], 0x53) */
	/* Using default value for 0x3059 (0x00) */
	/* Using default value for 0x305A (0x00) */
	{ 0x3060, IMX675_2DOL_RHS1 }, // MANUAL (0x81 -> ISP's limit)
	/* Using default value for 0x3061 (0x00) */
	/* Using default value for 0x3062 (0x00) */
	/* Using default value for 0x3064 (RHS2 [19:0], 0x56) */
	/* Using default value for 0x3065 (0x00) */
	/* Using default value for 0x3066 (0x00) */
	/* Using default value for 0x3070 (GAIN_0 [10:0], 0x00) */
	/* Using default value for 0x3071 (0x00) */
	/* Using default value for 0x3072 (GAIN_1 [10:0], 0x00) */
	/* Using default value for 0x3073 (0x00) */
	/* Using default value for 0x3074 (GAIN_2 [10:0], 0x00) */
	/* Using default value for 0x3075 (0x00) */
	/* Using default value for 0x30A4 (XVSOUTSEL [1:0], 0xAA) */
	{ 0x30A6, 0x00 }, /* XVS_DRV [1:0] */
	/* Using default value for 0x30CC (0x00) */
	/* Using default value for 0x30CD (0x00) */
	{ 0x30CE, 0x02 },
	/* Using default value for 0x30DC (BLKLEVEL [9:0], 0x32) */
	/* Using default value for 0x30DD (0x40) */
	/* Using default value for 0x310C (0x01) */
	/* Using default value for 0x3130 (0x01) */
	{ 0x3148, 0x00 },
	/* Using default value for 0x315E (0x10) */
	{ 0x3400, 0x00 }, /* GAIN_PGC_FIDMD - 0: set individual exposure gains*/
	{ 0x3460, 0x22 },
	{ 0x347B, 0x02 },
	{ 0x3492, 0x08 },
	/* Using default value for 0x3890 (HFR_EN [3:0], 0x08) */
	/* Using default value for 0x3891 (0x00) */
	/* Using default value for 0x3893 (0x00) */
	{ 0x3B1D, 0x17 },
	{ 0x3B44, 0x3F },
	{ 0x3B60, 0x03 },
	{ 0x3C03, 0x04 },
	{ 0x3C04, 0x04 },
	{ 0x3C0A, 0x1f },
	{ 0x3C0B, 0x1f },
	{ 0x3C0C, 0x1f },
	{ 0x3C0D, 0x1f },
	{ 0x3C0E, 0x1f },
	{ 0x3C0F, 0x1f },
	{ 0x3C30, 0x73 },
	{ 0x3C3C, 0x20 },
	/* Using default value for 0x3C44 (0x06) */
	{ 0x3C7C, 0xB9 },
	{ 0x3C7D, 0x01 },
	{ 0x3C7E, 0xB7 },
	{ 0x3C7F, 0x01 },
	{ 0x3CB0, 0x00 },
	{ 0x3CB2, 0xFF },
	{ 0x3CB3, 0x03 },
	{ 0x3CB4, 0xFF },
	{ 0x3CB5, 0x03 },
	{ 0x3CBA, 0xFF },
	{ 0x3CBB, 0x03 },
	{ 0x3CC0, 0xFF },
	{ 0x3CC1, 0x03 },
	{ 0x3CC2, 0x00 },
	{ 0x3CC6, 0xFF },
	{ 0x3CC7, 0x03 },
	{ 0x3CC8, 0xFF },
	{ 0x3CC9, 0x03 },
	{ 0x3E00, 0x1E },
	{ 0x3E02, 0x04 },
	{ 0x3E03, 0x00 },
	{ 0x3E20, 0x04 },
	{ 0x3E21, 0x00 },
	{ 0x3E22, 0x1E },
	{ 0x3E24, 0xBA },
	{ 0x3E72, 0x85 },
	{ 0x3E76, 0x0C },
	{ 0x3E77, 0x01 },
	{ 0x3E7A, 0x85 },
	{ 0x3E7E, 0x1F },
	{ 0x3E82, 0xA6 },
	{ 0x3E86, 0x2D },
	{ 0x3EE2, 0x33 },
	{ 0x3EE3, 0x03 },
	{ 0x4490, 0x07 },
	{ 0x4494, 0x19 },
	{ 0x4495, 0x00 },
	{ 0x4496, 0xBB },
	{ 0x4497, 0x00 },
	{ 0x4498, 0x55 },
	{ 0x449A, 0x50 },
	{ 0x449C, 0x50 },
	{ 0x449E, 0x50 },
	{ 0x44A0, 0x3C },
	{ 0x44A2, 0x19 },
	{ 0x44A4, 0x19 },
	{ 0x44A6, 0x19 },
	{ 0x44A8, 0x4B },
	{ 0x44AA, 0x4B },
	{ 0x44AC, 0x4B },
	{ 0x44AE, 0x4B },
	{ 0x44B0, 0x3C },
	{ 0x44B2, 0x19 },
	{ 0x44B4, 0x19 },
	{ 0x44B6, 0x19 },
	{ 0x44B8, 0x4B },
	{ 0x44BA, 0x4B },
	{ 0x44BC, 0x4B },
	{ 0x44BE, 0x4B },
	{ 0x44C0, 0x3C },
	{ 0x44C2, 0x19 },
	{ 0x44C4, 0x19 },
	{ 0x44C6, 0x19 },
	{ 0x44C8, 0xF0 },
	{ 0x44CA, 0xEB },
	{ 0x44CC, 0xEB },
	{ 0x44CE, 0xE6 },
	{ 0x44D0, 0xE6 },
	{ 0x44D2, 0xBB },
	{ 0x44D4, 0xBB },
	{ 0x44D6, 0xBB },
	{ 0x44D8, 0xE6 },
	{ 0x44DA, 0xE6 },
	{ 0x44DC, 0xE6 },
	{ 0x44DE, 0xE6 },
	{ 0x44E0, 0xE6 },
	{ 0x44E2, 0xBB },
	{ 0x44E4, 0xBB },
	{ 0x44E6, 0xBB },
	{ 0x44E8, 0xE6 },
	{ 0x44EA, 0xE6 },
	{ 0x44EC, 0xE6 },
	{ 0x44EE, 0xE6 },
	{ 0x44F0, 0xE6 },
	{ 0x44F2, 0xBB },
	{ 0x44F4, 0xBB },
	{ 0x44F6, 0xBB },
	{ 0x4538, 0x15 },
	{ 0x4539, 0x15 },
	{ 0x453A, 0x15 },
	{ 0x4544, 0x15 },
	{ 0x4545, 0x15 },
	{ 0x4546, 0x15 },
	{ 0x4550, 0x10 },
	{ 0x4551, 0x10 },
	{ 0x4552, 0x10 },
	{ 0x4553, 0x10 },
	{ 0x4554, 0x10 },
	{ 0x4555, 0x10 },
	{ 0x4556, 0x10 },
	{ 0x4557, 0x10 },
	{ 0x4558, 0x10 },
	{ 0x455C, 0x10 },
	{ 0x455D, 0x10 },
	{ 0x455E, 0x10 },
	{ 0x455F, 0x10 },
	{ 0x4560, 0x10 },
	{ 0x4561, 0x10 },
	{ 0x4562, 0x10 },
	{ 0x4563, 0x10 },
	{ 0x4564, 0x10 },
	/* Using default value for 0x4569 (0x01) */
	/* Using default value for 0x456A (0x01) */
	/* Using default value for 0x456B (0x06) */
	/* Using default value for 0x456C (0x06) */
	/* Using default value for 0x456D (0x06) */
	/* Using default value for 0x456E (0x06) */
	/* Using default value for 0x456F (0x06) */
	/* Using default value for 0x4570 (0x06) */
};

/* Supported sensor mode configurations */
static const struct imx675_mode supported_sdr_modes[] = {
	// Native 5MP Resolution ("Effective Pixels")
	{
		.width = RES_5MP_WIDTH,
		.height = RES_5MP_HEIGHT,
		.hblank = IMX675_SDR_HMAX_VALUE,
		.vblank = IMX675_SDR_MIN_VBLANK_5MP,
		.vblank_min = IMX675_SDR_MIN_VBLANK_5MP,
		.vblank_max = IMX675_MAX_VBLANK_5MP,
		.rhs1 = 0x0,
		.rhs2 = 0x0,
		.link_freq_idx = 0,
		.pclk = link_freq[0],
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.dol = 1,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_native_5mp_all_pixel_30fps),
			.regs = mode_native_5mp_all_pixel_30fps,
		},
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		},
	},
	// FHD Crop
	{
		.width = 1920,
		.height = 1080,
		.hblank = IMX675_SDR_HMAX_VALUE,
		.vblank = IMX675_SDR_MIN_VBLANK_FHD,
		.vblank_min = IMX675_SDR_MIN_VBLANK_FHD,
		.vblank_max = IMX675_SDR_MAX_VBLANK_FHD,
		.rhs1 = 0x0,
		.rhs2 = 0x0,
		.link_freq_idx = 0,
		.pclk = link_freq[0],
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.dol = 1,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_fhd_crop_30fps),
			.regs = mode_fhd_crop_30fps,
		},
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		},
	}
};

static const struct imx675_mode supported_hdr_modes[] = {
	// Native 5MP Resolution ("Recommended Pixels") 2DOL 12 bit
	{
		.width = RES_5MP_WIDTH,
		.height = RES_5MP_HEIGHT,
		.hblank = IMX675_2DOL_HMAX_VALUE,
		.vblank = IMX675_2DOL_MIN_VBLANK_5MP,
		.vblank_min = IMX675_2DOL_MIN_VBLANK_5MP,
		.vblank_max = IMX675_MAX_VBLANK_5MP,
		.rhs1 = IMX675_2DOL_RHS1,
		.rhs2 = 0x0,
		.link_freq_idx = 1,
		.pclk = link_freq[1],
		.code = MEDIA_BUS_FMT_SRGGB12_2X12,
		.dol = 2,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_native_5mp_2dol_all_pixel_30fps),
			.regs = mode_native_5mp_2dol_all_pixel_30fps,
		},
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		}
	}
};

struct v4l2_ctrl_config imx675_custom_ctrls[] = {
	{
		.ops = &imx675_ctrl_ops,
		.id = IMX675_CID_ANALOGUE_GAIN_SHORT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.name = "analogue_gain_short",
		.step = IMX675_AGAIN_STEP,
		.min = IMX675_AGAIN_MIN,
		.max = IMX675_AGAIN_MAX,
		.def = IMX675_AGAIN_DEFAULT,
	},
	{
		.ops = &imx675_ctrl_ops,
		.id = IMX675_CID_ANALOGUE_GAIN_VERY_SHORT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.name = "analogue_gain_very_short",
		.step = IMX675_AGAIN_STEP,
		.min = IMX675_AGAIN_MIN,
		.max = IMX675_AGAIN_MAX,
		.def = IMX675_AGAIN_DEFAULT,
	},
	{
		.ops = &imx675_ctrl_ops,
		.id = IMX675_CID_EXPOSURE_SHORT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.name = "exposure_short",
		.step = IMX675_EXPOSURE_SHORT_STEP,
	},
	{
		.ops = &imx675_ctrl_ops,
		.id = IMX675_CID_EXPOSURE_VERY_SHORT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.name = "exposure_very_short",
		.step = IMX675_EXPOSURE_VERY_SHORT_STEP,
	},
	{
		.ops = &imx675_ctrl_ops,
		.id = IMX675_CID_HCG,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_UPDATE,
		.name = "hcg",
		.step = IMX675_HCG_STEP,
		.min = IMX675_HCG_MIN,
		.max = IMX675_HCG_MAX,
		.def = IMX675_HCG_DEFAULT,
	},
	{
		.ops = &imx675_get_ctrl_ops,
		.id = IMX675_CID_RHS1,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
		.name = "readout_timing_short",
		.step = IMX675_INTEGER_STEP,
	},
	{
		.ops = &imx675_get_ctrl_ops,
		.id = IMX675_CID_RHS2,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
		.name = "readout_timing_very_short",
		.step = IMX675_INTEGER_STEP,
	},
	{
		.ops = &imx675_get_ctrl_ops,
		.id = IMX675_CID_SHR0,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
		.name = "shutter_timing_long",
		.step = IMX675_INTEGER_STEP,
	},
	{
		.ops = &imx675_get_ctrl_ops,
		.id = IMX675_CID_SHR1,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
		.name = "shutter_timing_short",
		.step = IMX675_EXPOSURE_SHORT_STEP,
	},
	{
		.ops = &imx675_get_ctrl_ops,
		.id = IMX675_CID_SHR2,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
		.name = "shutter_timing_very_short",
		.step = IMX675_EXPOSURE_VERY_SHORT_STEP,
	},
	{
		.ops = &imx675_get_ctrl_ops,
		.id = IMX675_CID_VMAX,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
		.name = "vertical_span",
		.step = IMX675_INTEGER_STEP,
	},
	{
		.ops = &imx675_get_ctrl_ops,
		.id = IMX675_CID_HMAX,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
		.name = "horizontal_span",
		.step = IMX675_INTEGER_STEP,
	},	
};

static struct v4l2_ctrl_config *get_custom_ctrl_by_id(u32 id) {
	int i;

	for (i = 0; i < ARRAY_SIZE(imx675_custom_ctrls); i++) {
		if (imx675_custom_ctrls[i].id == id)
			return &imx675_custom_ctrls[i];
	}

	pr_err("Invalid control id: %d\n", id);
	return NULL;
}

static void imx675_setup_custom_ctrl(struct imx675 *imx675, struct v4l2_ctrl **ctrl, u32 id) {
	struct v4l2_ctrl_config *config = get_custom_ctrl_by_id(id);

	if (!config) {
		dev_err(imx675->dev, "Setup invalid custom control id: %d\n", id);
		return;
	}

	*ctrl = v4l2_ctrl_new_custom(&imx675->ctrl_handler, config, NULL);
}

static void imx675_setup_custom_ctrl_limits(
	struct imx675 *imx675, struct v4l2_ctrl **ctrl, u32 id,
	s64 min, s64 max, s64 def)
{
	struct v4l2_ctrl_config *config = get_custom_ctrl_by_id(id);

	if (!config) {
		dev_err(imx675->dev, "Setup invalid custom control id: %d\n", id);
		return;
	}

	config->min = min;
	config->max = max;
	config->def = def;
	
	*ctrl = v4l2_ctrl_new_custom(&imx675->ctrl_handler, config, NULL);
}

/**
 * to_imx675() - imv675 V4L2 sub-device to imx675 device.
 * @subdev: pointer to imx675 V4L2 sub-device
 *
 * Return: pointer to imx675 device
 */
static inline struct imx675 *to_imx675(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct imx675, sd);
}

/**
 * imx675_read_reg() - Read registers.
 * @imx675: pointer to imx675 device
 * @reg: register address
 * @len: length of bytes to read. Max supported bytes is 4
 * @val: pointer to register value to be filled.
 *
 * Big endian register addresses with little endian values.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_read_reg(struct imx675 *imx675, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx675->sd);
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
 * imx675_write_reg() - Write register
 * @imx675: pointer to imx675 device
 * @reg: register address
 * @len: length of bytes. Max supported bytes is 4
 * @val: register value
 *
 * Big endian register addresses with little endian values.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_write_reg(struct imx675 *imx675, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx675->sd);
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

/**
 * imx675_write_regs() - Write a list of registers
 * @imx675: pointer to imx675 device
 * @regs: list of registers to be written
 * @len: length of registers array
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_write_regs(struct imx675 *imx675,
				 const struct imx675_reg *regs, u32 len)
{
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = imx675_write_reg(imx675, regs[i].address, 1, regs[i].val);
		if (ret) {
			return ret;
		}
			
	}

	return 0;
}

typedef struct ExposureLimits_t {
	u32 lpfr;
	u32 min_lpfr;
	u32 max_lpfr;
	u32 lef_reg;
	u32 shr0_min;
	u32 shr0_max;
	u32 exp_lef_min;
	u32 exp_lef_max;
	u32 exp_lef_default;

	u32 sef1_reg;
	u32 shr1_min;
	u32 shr1_max;
	u32 exp_sef1_min;
	u32 exp_sef1_max;
	u32 exp_sef1_default;

	u32 sef2_reg;
	u32 shr2_min;
	u32 shr2_max;
	u32 exp_sef2_min;
	u32 exp_sef2_max;
	u32 exp_sef2_default;
} * ExposureLimits;

void calculate_exposure_limits(struct imx675* imx675, ExposureLimits limits) {
	/* TODO: here assumed hdr is only of type 2dol, since this is the mode that was ported.
	If using 3dol, change limits accordingly. */
	const int rhs1 = imx675->cur_mode->rhs1 > 0 ? imx675->cur_mode->rhs1 : IMX675_2DOL_RHS1;
	const int rhs2 = imx675->cur_mode->rhs2 > 0 ? imx675->cur_mode->rhs2 : IMX675_DEFAULT_RHS2;
	u32 shr0, shr1, shr2;

	limits->lpfr = imx675->cur_mode->dol * (imx675->vblank + imx675->cur_mode->height);
	limits->min_lpfr = imx675->cur_mode->dol * (imx675->cur_mode->vblank_min + imx675->cur_mode->height);
	limits->max_lpfr = imx675->cur_mode->dol * (imx675->cur_mode->vblank_max + imx675->cur_mode->height);

	limits->lef_reg = IMX675_REG_SHUTTER;
	limits->shr0_min = imx675->hdr_enabled ? imx675->cur_mode->rhs1 + IMX675_2DOL_SHR0_RHS1_GAP : IMX675_2DOL_SHR0_FSC_GAP;
	limits->shr0_max = NON_NEGATIVE(limits->max_lpfr - IMX675_2DOL_SHR0_FSC_GAP);
	limits->exp_lef_min = IMX675_2DOL_SHR0_FSC_GAP;
	limits->exp_lef_max = NON_NEGATIVE(limits->max_lpfr - limits->shr0_min);
	shr0 = _get_mode_reg_val_by_address(&imx675->cur_mode->reg_list, limits->lef_reg, 3);
	limits->exp_lef_default = MAX(limits->exp_lef_min, NON_NEGATIVE((int)limits->lpfr - (int)shr0));

	limits->sef1_reg = IMX675_REG_SHUTTER_SHORT;
	limits->shr1_min = IMX675_2DOL_SHR1_MIN_GAP;
	limits->shr1_max = NON_NEGATIVE(rhs1 - IMX675_2DOL_SHR1_RHS1_GAP);
	limits->exp_sef1_min = NON_NEGATIVE(rhs1 - limits->shr1_max);
	limits->exp_sef1_max = NON_NEGATIVE(rhs1 - limits->shr1_min);
	shr1 = MAX(limits->shr1_min, _get_mode_reg_val_by_address(&imx675->cur_mode->reg_list, limits->sef1_reg, 3));
	limits->exp_sef1_default = MAX(limits->exp_sef1_min, NON_NEGATIVE((int)rhs1 - (int)shr1));

    // sef2/shr2 are irrelevant for 2dol, using 3dol constants
	limits->sef2_reg = IMX675_REG_SHUTTER_VERY_SHORT;
	limits->shr2_min = rhs1 + IMX675_3DOL_SHR2_RHS1_GAP;
	limits->shr2_max = NON_NEGATIVE(rhs2 - IMX675_3DOL_SHR2_RHS2_GAP);
	limits->exp_sef2_min = NON_NEGATIVE(rhs2 - limits->shr2_max);
	limits->exp_sef2_max = NON_NEGATIVE(rhs2 - limits->shr2_min);
	shr2 = MAX(limits->shr2_min, _get_mode_reg_val_by_address(&imx675->cur_mode->reg_list, limits->sef2_reg, 3));
	limits->exp_sef2_default = MAX(limits->exp_sef2_min, NON_NEGATIVE((int)rhs2 - (int)shr2));
}

static int imx675_set_ctrl_range_and_value(struct imx675 *imx675,
		struct v4l2_ctrl *ctrl, u32 min, u32 max, u32 step, u32 def)
{
	int ret;

	ret = __v4l2_ctrl_modify_range(ctrl, min, max, step, def);
	if (ret) {
		dev_err(imx675->dev, "Failed to modify control %s range. "
			"ret=%d. min=%d, max=%d, default=%d",
			ctrl->name, ret, min, max, def);
		return ret;
	}

	ret = __v4l2_ctrl_s_ctrl(ctrl, def);
	if (ret) {
		dev_err(imx675->dev, "Failed to set control %s to default %d. ret=%d",
			ctrl->name, def, ret);
		return ret;
	}

	return 0;
}

/**
 * imx675_update_exp_vblank_controls() - Update control ranges based on streaming mode
 * @imx675: pointer to imx675 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_update_exp_vblank_controls(struct imx675* imx675)
{
	struct ExposureLimits_t limits;
	const struct imx675_mode *mode = imx675->cur_mode;
	int ret;

	memset(&limits, 0, sizeof(struct ExposureLimits_t));
	calculate_exposure_limits(imx675, &limits);

	ret = imx675_set_ctrl_range_and_value(imx675, imx675->lef.exp_ctrl, limits.exp_lef_min,
		limits.exp_lef_max, IMX675_EXPOSURE_STEP, limits.exp_lef_default);
	if (ret) {
		dev_err(imx675->dev, "Failed to update LEF exposure range and value\n");
		return ret;
	}

	if (imx675->cur_mode->dol >= 2) {
		ret = imx675_set_ctrl_range_and_value(imx675, imx675->sef1.exp_ctrl, limits.exp_sef1_min,
			limits.exp_sef1_max, IMX675_EXPOSURE_SHORT_STEP, limits.exp_sef1_default);
		if (ret) {
			dev_err(imx675->dev, "Failed to update SEF1 exposure range and value\n");
			return ret;
		}
	}

	if (imx675->cur_mode->dol >= 3) {
		ret = imx675_set_ctrl_range_and_value(imx675, imx675->sef2.exp_ctrl, limits.exp_sef2_min,
			limits.exp_sef2_max, IMX675_EXPOSURE_VERY_SHORT_STEP, limits.exp_sef2_default);
		if (ret) {
			dev_err(imx675->dev, "Failed to update SEF2 exposure range and value\n");
			return ret;
		}
	}

	ret = imx675_set_ctrl_range_and_value(imx675, imx675->vblank_ctrl, mode->vblank_min,
		mode->vblank_max, 1, imx675->vblank);
	if (ret) {
		dev_err(imx675->dev, "Failed to update vblank range and value\n");
		return ret;
	}

	return 0;
}

static int imx675_set_hcg_mode(struct imx675 *imx675, u32 hcg)
{
	int ret;
	ret = imx675_write_reg(imx675, IMX675_REG_HCG, 1, hcg);
	if (ret) {
		dev_err(imx675->dev, "Failed to write HCG register: %d\n", ret);
		return ret;
	}

	dev_dbg(imx675->dev, "HCG mode set to %s\n", hcg ? "enabled" : "disabled");

	return 0;
}

static int search_mode(const struct imx675_mode *mode, bool *o_hdr)
{
	// First search in HDR modes, then SDR modes
	if (mode >= supported_hdr_modes && mode < supported_hdr_modes + ARRAY_SIZE(supported_hdr_modes)) {
		if (o_hdr) *o_hdr = true;
		return mode - supported_hdr_modes;
	} else if (mode >= supported_sdr_modes && mode < supported_sdr_modes + ARRAY_SIZE(supported_sdr_modes)) {
		if (o_hdr) *o_hdr = false;
		return mode - supported_sdr_modes;
	}

	pr_err("Error. selected mode was not found!\n");
	return -1;
}

static const char *imx675_get_mode_name(struct imx675 *imx675)
{
	static char mode_str[32];
	int fps;
	int idx;

	fps = imx675->cur_mode->frame_interval.denominator / 
		imx675->cur_mode->frame_interval.numerator;

	switch (imx675->cur_mode->dol) {
    case 1:
        if (imx675->hdr_enabled) {
            dev_err(imx675->dev, "Invalid HDR mode with dol=%d\n", imx675->cur_mode->dol);
            return NULL;
        }
        idx = search_mode(imx675->cur_mode, NULL);
        snprintf(mode_str, sizeof(mode_str), "SDR #%d %dfps", idx, fps);
        return mode_str;
	case 2:
	case 3:
        if (!imx675->hdr_enabled) {
            dev_err(imx675->dev, "Invalid SDR mode with dol=%d\n", imx675->cur_mode->dol);
            return NULL;
        }
		idx = search_mode(imx675->cur_mode, NULL);
		snprintf(mode_str, sizeof(mode_str), "HDR #%d, %dDOL %dfps", 
			idx, imx675->cur_mode->dol, fps);
		return mode_str;
	default:
		dev_err(imx675->dev, "Invalid mode with dol=%d\n", imx675->cur_mode->dol);
		return NULL;
	}
}

/**
 * imx675_update_exp_gain() - Set updated exposure and gain
 * @imx675: pointer to imx675 device
 * @exposure: updated exposure value
 * @gain: updated analog gain value
 * @exposure_type: exposure type (0: LEF, 1: SEF1, 2: SEF2)
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_update_exp_gain(struct imx675 *imx675, u32 exposure, u32 gain, enum imx675_exposure_type exposure_type)
{
	u32 lpfr, shutter;
	int ret;
	int gap;

	switch (exposure_type) {
		case (LEF): {
            gap = imx675->hdr_enabled ? imx675->cur_mode->rhs1 + IMX675_2DOL_SHR0_RHS1_GAP : IMX675_2DOL_SHR0_FSC_GAP;

			// If the vblank is too small to fit the requested exposure, increase vblank
			if (exposure > imx675->cur_mode->dol * (imx675->vblank + imx675->cur_mode->height) - gap) {
				imx675->vblank = exposure - imx675->cur_mode->dol * (imx675->cur_mode->height) + gap;
				__v4l2_ctrl_s_ctrl(imx675->vblank_ctrl, imx675->vblank);
			}

			lpfr = imx675->vblank + imx675->cur_mode->height;
			lpfr += lpfr % 2; // LPFR must be even
			shutter = NON_NEGATIVE(imx675->cur_mode->dol * (int)lpfr - (int)exposure);
			break;
		}
		case (SEF1): {
			shutter = NON_NEGATIVE((int)imx675->cur_mode->rhs1 - (int)exposure);
			break;
		}
		case (SEF2): {
			shutter = NON_NEGATIVE((int)imx675->cur_mode->rhs2 - (int)exposure);
			break;
		}
	}

	dev_dbg(imx675->dev, "Set long exp %u analog gain %u sh%d %u lpfr %u",
		 exposure, gain, exposure_type, shutter, lpfr);

	ret = imx675_write_reg(imx675, IMX675_REG_HOLD, 1, 1);
	if (ret)
		return ret;

	if (exposure_type == LEF) {
		ret = imx675_write_reg(imx675, IMX675_REG_LPFR, 3, lpfr);
		if (ret)
			goto error_release_group_hold;
	}

	ret = imx675_write_reg(imx675, imx675_reg_shutter[exposure_type], 3, shutter);
	if (ret)
		goto error_release_group_hold;

	ret = imx675_write_reg(imx675, imx675_reg_again[exposure_type], 2, gain);

error_release_group_hold:
	imx675_write_reg(imx675, IMX675_REG_HOLD, 1, 0);

	return ret;
}

/*
 * imx675_set_test_pattern - Function called when setting test pattern
 * @priv: Pointer to device structure
 * @val: Variable for test pattern
 *
 * Set to different test patterns based on input value.
 *
 * Return: 0 on success
*/
static int imx675_set_test_pattern(struct imx675 *imx675, int val)
{
	int ret = 0;

	if (TEST_PATTERN_DISABLED == val)
		ret = imx675_write_reg(imx675, IMX675_TPG_EN_DUOUT, 1, val);
	else {
		ret = imx675_write_reg(imx675, IMX675_TPG_PATSEL_DUOUT, 1,
					   val - 1);
		if (!ret) {
			ret = imx675_write_regs(imx675, imx675_tpg_en_regs,
						ARRAY_SIZE(imx675_tpg_en_regs));
		}
	}
	return ret;
}

static void imx675_set_mode(struct imx675 *imx675, const struct imx675_mode *mode)
{
	int ret;
	imx675->cur_mode = mode;
	imx675->vblank = mode->vblank;

	/* set the link freq index and the pixel rate controls */
	if (imx675->link_freq_ctrl) {
		ret = __v4l2_ctrl_s_ctrl(imx675->link_freq_ctrl, mode->link_freq_idx);
		if (ret)
			dev_err(imx675->dev, "Failed to set link freq index to %d.", mode->link_freq_idx);
	}
	if (imx675->pclk_ctrl) {
		ret = __v4l2_ctrl_s_ctrl_int64(imx675->pclk_ctrl, mode->pclk);
		if (ret)
			dev_err(imx675->dev, "Failed to set pixel rate to %lld.", mode->pclk);
	}

	if (imx675->hdr_enabled) {
		if (mode->dol <= 1)
			dev_err(imx675->dev, "Set to invalid HDR mode with DOL %d", mode->dol);
	} else {
		if (mode->dol > 1)
			dev_err(imx675->dev, "Set to invalid SDR mode with DOL %d", mode->dol);
	}
}

static void imx675_set_exp_activity(struct imx675 *imx675)
{
	int dol = imx675->cur_mode->dol;
	bool sef1, sef2;

	sef1 = dol >= 2;
	sef2 = dol >= 3;

	v4l2_ctrl_activate(imx675->sef1.again_ctrl, sef1);
	v4l2_ctrl_activate(imx675->sef1.exp_ctrl, sef1);

	v4l2_ctrl_activate(imx675->sef2.again_ctrl, sef2);
	v4l2_ctrl_activate(imx675->sef2.exp_ctrl, sef2);
}

static int imx675_set_hdr_mode(struct imx675 *imx675, bool enable)
{
	const struct imx675_mode *prev_mode = NULL;
	int ret, revert_ret;

	ret = 0;
	if (imx675->hdr_enabled != enable) {
		imx675->hdr_enabled = enable;
		prev_mode = imx675->cur_mode;

		imx675_set_mode(imx675, imx675->hdr_enabled ? 
								&supported_hdr_modes[DEFAULT_MODE_IDX] :
								&supported_sdr_modes[DEFAULT_MODE_IDX]);

		ret = imx675_update_exp_vblank_controls(imx675);
		if (ret) {
			dev_warn(imx675->dev, "Failed to update exp controls, trying to revert to previous mode\n");

			imx675->hdr_enabled = !imx675->hdr_enabled;
			imx675_set_mode(imx675, prev_mode);

			revert_ret = imx675_update_exp_vblank_controls(imx675);
			if (revert_ret)
				dev_err(imx675->dev, "Failed to revert to previous mode (hdr_enabled back to %d, ret=%d)\n",
					 imx675->hdr_enabled, revert_ret);
		}

		dev_dbg(imx675->dev, "Set HDR mode to %d", imx675->hdr_enabled);
		imx675_set_exp_activity(imx675);
	}

	return ret;
}

/**
 * imx675_get_ctrl() - Get subdevice control
 * @ctrl: pointer to v4l2_ctrl structure
 *
 * Supported controls:
 * - IMX675_CID_RHS1
 * - IMX675_CID_RHS2
 * - IMX675_CID_SHR0
 * - IMX675_CID_SHR1
 * - IMX675_CID_SHR2
 * - IMX675_CID_VMAX
 * - IMX675_CID_HMAX
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx675 *imx675 = container_of(ctrl->handler, struct imx675, ctrl_handler);
	u16 reg = 0;
	u32 len = 0;
	int ret = 0;

	switch (ctrl->id) {
	case IMX675_CID_RHS1:
		ctrl->val = imx675->cur_mode->rhs1;
		break;
	case IMX675_CID_RHS2:
		ctrl->val = imx675->cur_mode->rhs2;
		break;
	case IMX675_CID_SHR0:
		reg = IMX675_REG_SHUTTER;
		len = 3;
		break;
	case IMX675_CID_SHR1:
		reg = IMX675_REG_SHUTTER_SHORT;
		len = 3;
		break;
	case IMX675_CID_SHR2:
		reg = IMX675_REG_SHUTTER_VERY_SHORT;
		len = 3;
		break;
	case IMX675_CID_VMAX:
		reg = IMX675_REG_LPFR;
		len = 3;
		break;
	case IMX675_CID_HMAX:
		reg = IMX675_REG_HMAX;
		len = 2;
		break;
	default:
		dev_err(imx675->dev, "Invalid control %d", ctrl->id);
		return -EINVAL;
	}

	if (reg && len) {
		if (!imx675->streaming) {
			dev_warn(imx675->dev, "Cannot read register 0x%x from sensor while not streaming\n", reg);
			return -EBUSY;
		}

		ret = imx675_read_reg(imx675, reg, len, &ctrl->val);
		if (ret)
			dev_err(imx675->dev, "Failed to read register %d", reg);
	}

	return ret;
}

/**
 * imx675_set_ctrl() - Set subdevice control
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
static int imx675_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx675 *imx675 =
		container_of(ctrl->handler, struct imx675, ctrl_handler);
	u32 analog_gain, exposure, lpfr, max_lpfr;
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		imx675->vblank = imx675->vblank_ctrl->val;
		max_lpfr = (imx675->cur_mode->vblank_max + imx675->cur_mode->height) * imx675->cur_mode->dol;
		lpfr = (imx675->vblank + imx675->cur_mode->height) * imx675->cur_mode->dol;

		dev_dbg(imx675->dev, "Received vblank %u, new lpfr %u",
			imx675->vblank, lpfr);
		ret = __v4l2_ctrl_modify_range(
			imx675->lef.exp_ctrl, IMX675_2DOL_SHR0_FSC_GAP,
			max_lpfr - imx675->cur_mode->rhs1 - IMX675_2DOL_SHR0_RHS1_GAP,
			IMX675_EXPOSURE_STEP, lpfr - imx675->cur_mode->rhs1 - IMX675_2DOL_SHR0_RHS1_GAP);
		break;
	case V4L2_CID_EXPOSURE:

		/* Set controls only if sensor is in power on state */
		if (!pm_runtime_get_if_in_use(imx675->dev))
			return 0;

		exposure = ctrl->val;
		analog_gain = imx675->lef.again_ctrl->val;

		dev_dbg(imx675->dev, "Received exp %u analog gain %u", exposure,
			analog_gain);

		ret = imx675_update_exp_gain(imx675, exposure, analog_gain, LEF);

		pm_runtime_put(imx675->dev);

		break;
	case IMX675_CID_EXPOSURE_SHORT:
		if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
			return 0;

		/* Set controls only if sensor is in power on state */
		if (!pm_runtime_get_if_in_use(imx675->dev))
			return 0;

		exposure = ctrl->val;
		analog_gain = imx675->sef1.again_ctrl->val;

		dev_dbg(imx675->dev, "Received exp(sef1) %u analog gain %u", exposure,
			analog_gain);

		ret = imx675_update_exp_gain(imx675, exposure, analog_gain, SEF1);

		pm_runtime_put(imx675->dev);

		break;
	case IMX675_CID_EXPOSURE_VERY_SHORT:
		if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
			return 0;

		/* Set controls only if sensor is in power on state */
		if (!pm_runtime_get_if_in_use(imx675->dev))
			return 0;

		exposure = ctrl->val;
		analog_gain = imx675->sef2.again_ctrl->val;

		dev_dbg(imx675->dev, "Received exp(sef2) %u analog gain %u", exposure,
			analog_gain);

		ret = imx675_update_exp_gain(imx675, exposure, analog_gain, SEF2);

		pm_runtime_put(imx675->dev);

		break;
	case V4L2_CID_TEST_PATTERN:
		if (!pm_runtime_get_if_in_use(imx675->dev))
			return 0;
		ret = imx675_set_test_pattern(imx675, ctrl->val);

		pm_runtime_put(imx675->dev);

		break;
	case IMX675_CID_HCG:
		/* Set controls only if sensor is in power on state */
		if (!pm_runtime_get_if_in_use(imx675->dev))
			return 0;
		
		dev_dbg(imx675->dev, "Setting HCG to %u\n", ctrl->val);

		ret = imx675_set_hcg_mode(imx675, ctrl->val);
		if (ret) {
			dev_err(imx675->dev, "Failed to set HCG mode: %d\n", ret);
		}
		pm_runtime_put(imx675->dev);
		break;

	case V4L2_CID_WIDE_DYNAMIC_RANGE:
		if (imx675->streaming) {
			dev_warn(imx675->dev,
				"Cannot set WDR mode while streaming\n");
			return -EBUSY;
		}

		ret = imx675_set_hdr_mode(imx675, ctrl->val);
		break;		
	case V4L2_CID_LINK_FREQ:
	case V4L2_CID_PIXEL_RATE:
		ret = 0;
		break;	
	default:
		dev_err(imx675->dev, "Invalid control %d", ctrl->id);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * imx675_enum_mbus_code() - Enumerate V4L2 sub-device mbus codes
 * @sd: pointer to imx675 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 * @code: V4L2 sub-device code enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx675_mode *supported_modes;
	struct imx675 *imx675 = to_imx675(sd);

	if (code->index > 0)
		return -EINVAL;

	if (imx675->hdr_enabled)
		supported_modes = (struct imx675_mode *)supported_hdr_modes;
	else
		supported_modes = (struct imx675_mode *)supported_sdr_modes;
	
	mutex_lock(&imx675->mutex);
	code->code = supported_modes[DEFAULT_MODE_IDX].code;
	mutex_unlock(&imx675->mutex);
	return 0;
}

/**
 * imx675_enum_frame_size() - Enumerate V4L2 sub-device frame sizes
 * @sd: pointer to imx675 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 * @fsize: V4L2 sub-device size enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fsize)
{
	struct imx675_mode *supported_modes;
	struct imx675 *imx675 = to_imx675(sd);
	int mode_count, i = 0;
	int min_width = INT_MAX;
	int min_height = INT_MAX;
	int max_width = 0;
	int max_height = 0;

	if (fsize->index > 0)
		return -EINVAL;

	if(imx675->hdr_enabled){
		supported_modes = (struct imx675_mode *)supported_hdr_modes;
		mode_count = sizeof(supported_hdr_modes) / sizeof(struct imx675_mode);
	}
	else {
		supported_modes = (struct imx675_mode *)supported_sdr_modes;
		mode_count = sizeof(supported_sdr_modes) / sizeof(struct imx675_mode);
	}

	mutex_lock(&imx675->mutex);
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
		mutex_unlock(&imx675->mutex);
		return -EINVAL;
	}

	fsize->min_width = min_width;
	fsize->max_width = max_width;
	fsize->min_height = min_height;
	fsize->max_height = max_height;
	mutex_unlock(&imx675->mutex);

	return 0;
}

/**
 * imx675_fill_pad_format() - Fill subdevice pad format
 *                            from selected sensor mode
 * @imx675: pointer to imx675 device
 * @mode: pointer to imx675_mode sensor mode
 * @fmt: V4L2 sub-device format need to be filled
 */
static void imx675_fill_pad_format(struct imx675 *imx675,
				   const struct imx675_mode *mode,
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
 * imx675_get_pad_format() - Get subdevice pad format
 * @sd: pointer to imx675 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx675 *imx675 = to_imx675(sd);

	mutex_lock(&imx675->mutex);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *framefmt;

		framefmt = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
		fmt->format = *framefmt;
	} else {
		imx675_fill_pad_format(imx675, imx675->cur_mode, fmt);
	}

	mutex_unlock(&imx675->mutex);

	return 0;
}

static int imx675_get_fmt_mode(struct imx675* imx675, struct v4l2_subdev_format* fmt, const struct imx675_mode** mode){
	struct imx675_mode *supported_modes;
	int mode_count = 0;
	int index = 0;

	if(!imx675 || !fmt || !mode)
		return -EINVAL;

	if(imx675->hdr_enabled){
		supported_modes = (struct imx675_mode *)supported_hdr_modes;
		mode_count = sizeof(supported_hdr_modes) / sizeof(struct imx675_mode);
	}
	else {
		supported_modes = (struct imx675_mode *)supported_sdr_modes;
		mode_count = sizeof(supported_sdr_modes) / sizeof(struct imx675_mode);
	}
	
	for (index = 0; index < mode_count; ++index) {
		if (supported_modes[index].width == fmt->format.width && 
			supported_modes[index].height == fmt->format.height) {
			*mode = &supported_modes[index];
			return 0;
		}
	}

	return -EINVAL;
}

/**
 * imx675_set_pad_format() - Set subdevice pad format
 * @sd: pointer to imx675 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx675 *imx675 = to_imx675(sd);
	const struct imx675_mode *mode;
	int ret = 0;
	
	mutex_lock(&imx675->mutex);
	
	ret = imx675_get_fmt_mode(imx675, fmt, &mode);
	if(ret){
		pr_err("%s - get_fmt failed with %d (h=%d, w=%d, code: %d)\n", __func__,
			ret, fmt->format.width, fmt->format.height, fmt->format.code);
		goto out;
	}

	imx675_fill_pad_format(imx675, mode, fmt);

	// even if which is V4L2_SUBDEV_FORMAT_TRY, update current format for tuning case
	memcpy(&imx675->curr_fmt, fmt, sizeof(struct v4l2_subdev_format));
	if (compare_imx675_mode(mode, imx675->cur_mode)) {
		imx675_set_mode(imx675, mode);
		ret = imx675_update_exp_vblank_controls(imx675);
	}

out:
	mutex_unlock(&imx675->mutex);
	return ret;
}

/**
 * imx675_init_pad_cfg() - Initialize sub-device pad configuration
 * @sd: pointer to imx675 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device state
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_init_pad_cfg(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state)
{
	struct imx675_mode *supported_modes;
	struct imx675 *imx675 = to_imx675(sd);
	struct v4l2_subdev_format fmt = { 0 };

	supported_modes = (struct imx675_mode *)(imx675->hdr_enabled ? supported_hdr_modes : supported_sdr_modes);
	fmt.which = sd_state ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;

	imx675_fill_pad_format(imx675, &supported_modes[DEFAULT_MODE_IDX], &fmt);
	return imx675_set_pad_format(sd, sd_state, &fmt);
}


/**
 * imx675_start_streaming() - Start sensor stream
 * @imx675: pointer to imx675 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_start_streaming(struct imx675 *imx675)
{
	const struct imx675_reg_list *reg_list;
	int ret;

	/* Write sensor mode registers */
	pr_debug("%s - hdr_enabled: %d\n", __func__, imx675->hdr_enabled);
	reg_list = &imx675->cur_mode->reg_list;
	ret = imx675_write_regs(imx675, reg_list->regs, reg_list->num_of_regs);
	if (ret) {
		dev_err(imx675->dev, "fail to write initial registers (ret=%d)", ret);
		return ret;
	}

	/* Setup handler will write actual exposure and gain */
	ret = __v4l2_ctrl_handler_setup(imx675->sd.ctrl_handler);
	if (ret) {
		dev_err(imx675->dev, "fail to setup handler (%d)", ret);
		return ret;
	}

	/* Start streaming */
	ret = imx675_write_reg(imx675, IMX675_REG_MODE_SELECT, 1,
				   IMX675_MODE_STREAMING);
	if (ret) {
		dev_err(imx675->dev, "fail to start streaming");
		return ret;
	}
	/* Start streaming */
	ret = imx675_write_reg(imx675, 0x3002, 1, 0);
	if (ret) {
		dev_err(imx675->dev, "fail to start streaming");
		return ret;
	}

	dev_info(imx675->dev, "imx675: start_streaming successful (%s)", imx675_get_mode_name(imx675));
	return 0;
}

/**
 * imx675_stop_streaming() - Stop sensor stream
 * @imx675: pointer to imx675 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_stop_streaming(struct imx675 *imx675)
{
	return imx675_write_reg(imx675, IMX675_REG_MODE_SELECT, 1,
				IMX675_MODE_STANDBY);
}

/**
 * imx675_set_stream() - Enable sensor streaming
 * @sd: pointer to imx675 subdevice
 * @enable: set to enable sensor streaming
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx675 *imx675 = to_imx675(sd);
	int ret;

	mutex_lock(&imx675->mutex);

	if (imx675->streaming == enable) {
		mutex_unlock(&imx675->mutex);
		return 0;
	}

	if (enable) {
		ret = pm_runtime_resume_and_get(imx675->dev);
		if (ret < 0)
			goto error_unlock;

		ret = imx675_start_streaming(imx675);
		if (ret)
			goto error_power_off;
	} else {
		imx675_stop_streaming(imx675);
		pm_runtime_put(imx675->dev);
	}

	imx675->streaming = enable;

	mutex_unlock(&imx675->mutex);

	return 0;

error_power_off:
	pm_runtime_put(imx675->dev);
error_unlock:
	mutex_unlock(&imx675->mutex);

	return ret;
}

static int
imx675_find_nearest_frame_interval_mode(struct imx675 *imx675,
					struct v4l2_subdev_frame_interval *fi,
					struct imx675_mode const **mode)
{
	struct imx675_mode const* curr_mode;
	struct v4l2_mbus_framefmt* framefmt;
	struct imx675_mode *supported_modes;

	int min_diff = INT_MAX;
	int curr_diff;
	int i, found = 0, mode_count = 0;

	if (!imx675 || !fi) {
		return -EINVAL;
	}

	if(imx675->hdr_enabled){
		supported_modes = (struct imx675_mode *)supported_hdr_modes;
		mode_count = sizeof(supported_hdr_modes) / sizeof(struct imx675_mode);
	}
	else {
		supported_modes = (struct imx675_mode *)supported_sdr_modes;
		mode_count = sizeof(supported_sdr_modes) / sizeof(struct imx675_mode);
	}

	framefmt = &imx675->curr_fmt.format;

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
 * imx675_s_frame_interval - Set the frame interval
 * @sd: Pointer to V4L2 Sub device structure
 * @fi: Pointer to V4l2 Sub device frame interval structure
 *
 * This function is used to set the frame intervavl.
 *
 * Return: 0 on success
 */
static int imx675_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx675 *imx675 = to_imx675(sd);
	struct imx675_mode const *mode;
	int ret;

	ret = pm_runtime_resume_and_get(imx675->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&imx675->mutex);

	ret = imx675_find_nearest_frame_interval_mode(imx675, fi, &mode);

	if (ret == 0) {
		fi->interval = mode->frame_interval;
		if (compare_imx675_mode(mode, imx675->cur_mode)) {
			imx675_set_mode(imx675, mode);
			ret = imx675_update_exp_vblank_controls(imx675);
		}
	}

	mutex_unlock(&imx675->mutex);
	pm_runtime_put(imx675->dev);

	return ret;
}

static int imx675_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct imx675 *imx675 = to_imx675(sd);

	mutex_lock(&imx675->mutex);
	fi->interval = imx675->cur_mode->frame_interval;
	mutex_unlock(&imx675->mutex);

	return 0;
}
/**
 * check_sensor_id() - Check sensor ID
 * @imx675: pointer to imx675 device
 * @id_reg: register address to read sensor ID
 * @sensor_id: expected sensor ID value
 *
 * Return: 0 if successful, -ENXIO if sensor ID does not match
 */
static int check_sensor_id(struct imx675 *imx675, u16 id_reg, u8 sensor_id)
{
	int ret;
	u32 id;

	ret = imx675_read_reg(imx675, id_reg, 1, &id);
	if (ret) {
		dev_err(imx675->dev,
			"failed to read sensor id register 0x%x, ret %d\n",
			id_reg, ret);
		return ret;
	}

	if (id != sensor_id) {
		dev_info(imx675->dev,
			"sensor is not connected: (reg %x, expected %x, found %x)",
			id_reg, sensor_id, id);
		return -ENXIO;
	}

	return 0;
}

/**
 * imx675_detect() - Detect imx675 sensor
 * @imx675: pointer to imx675 device
 *
 * Return: 0 if successful, -EIO if sensor id does not match
 */
static int imx675_detect(struct imx675 *imx675)
{
	int ret;
	
	ret = check_sensor_id(imx675, GENERIC_SENSOR_ID_REG, SENSOR_ID_IMX675);
	if (ret)
		return ret;
	
	ret = check_sensor_id(imx675, GENERIC_SENSOR_ID_REG3, IMX675_SENSOR_ID_VAL);
	if (ret)
		return ret;
	
	return 0;
}


/**
 * imx675_parse_hw_config() - Parse HW configuration and check if supported
 * @imx675: pointer to imx675 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_parse_hw_config(struct imx675 *imx675)
{
	struct fwnode_handle *fwnode = dev_fwnode(imx675->dev);
	struct v4l2_fwnode_endpoint bus_cfg = { .bus_type =
							V4L2_MBUS_CSI2_DPHY };
	struct fwnode_handle *ep;
	unsigned long rate;
	int ret;
	int i, j;

	if (!fwnode)
		return -ENXIO;

	/* Request optional reset pin */
	imx675->reset_gpio =
		devm_gpiod_get_optional(imx675->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(imx675->reset_gpio)) {
		dev_err(imx675->dev, "failed to get reset gpio %ld",
			PTR_ERR(imx675->reset_gpio));
		return PTR_ERR(imx675->reset_gpio);
	}

	/* Get sensor input clock */
	imx675->inclk = devm_clk_get(imx675->dev, NULL);
	if (IS_ERR(imx675->inclk)) {
		dev_err(imx675->dev, "could not get inclk");
		return PTR_ERR(imx675->inclk);
	}

	rate = clk_get_rate(imx675->inclk);
	if (rate != IMX675_INCLK_RATE) {
		dev_err(imx675->dev, "inclk frequency mismatch");
		return -EINVAL;
	}

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -ENXIO;

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != IMX675_NUM_DATA_LANES) {
		dev_err(imx675->dev,
			"number of CSI2 data lanes %d is not supported",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(imx675->dev, "no link frequencies defined");
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	/* check if all the required frequencies are provided in the device tree */
	for (i = 0; i < ARRAY_SIZE(link_freq); i++) {
		for (j = 0; j < bus_cfg.nr_of_link_frequencies; j++) {
			if (bus_cfg.link_frequencies[j] ==
				link_freq[i]) {
				break;
			}
		}
		if (j == bus_cfg.nr_of_link_frequencies) {
			dev_err(imx675->dev,
				"required link frequency %lld not supported in device tree",
				link_freq[i]);
			ret = -EINVAL;
			goto done_endpoint_free;
		}
	}

	ret = 0;

done_endpoint_free:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

/* V4l2 subdevice ops */
static const struct v4l2_subdev_video_ops imx675_video_ops = {
	.s_stream = imx675_set_stream,
	.s_frame_interval = imx675_s_frame_interval,
	.g_frame_interval = imx675_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx675_pad_ops = {
	.init_cfg = imx675_init_pad_cfg,
	.enum_mbus_code = imx675_enum_mbus_code,
	.enum_frame_size = imx675_enum_frame_size,
	.get_fmt = imx675_get_pad_format,
	.set_fmt = imx675_set_pad_format,
};

static const struct v4l2_subdev_ops imx675_subdev_ops = {
	.video = &imx675_video_ops,
	.pad = &imx675_pad_ops,
};

/**
 * imx675_power_on() - Sensor power on sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx675 *imx675 = to_imx675(sd);
	int ret;

	gpiod_set_value_cansleep(imx675->reset_gpio, 1);

	ret = clk_prepare_enable(imx675->inclk);
	if (ret) {
		dev_err(imx675->dev, "fail to enable inclk");
		goto error_reset;
	}

	usleep_range(18000, 20000);

	return 0;

error_reset:
	gpiod_set_value_cansleep(imx675->reset_gpio, 0);

	return ret;
}

/**
 * imx675_power_off() - Sensor power off sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx675 *imx675 = to_imx675(sd);

	gpiod_set_value_cansleep(imx675->reset_gpio, 0);

	clk_disable_unprepare(imx675->inclk);

	return 0;
}

/**
 * imx675_init_controls() - Initialize sensor subdevice controls
 * @imx675: pointer to imx675 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_init_controls(struct imx675 *imx675)
{
	struct v4l2_ctrl_handler *ctrl_hdlr = &imx675->ctrl_handler;
	const struct imx675_mode *mode = imx675->cur_mode;
	struct ExposureLimits_t limits;
	int ret;

	const int num_ctrls = 12;

	ret = v4l2_ctrl_handler_init(ctrl_hdlr, num_ctrls);
	if (ret)
		return ret;

	memset(&limits, 0, sizeof(struct ExposureLimits_t));
	calculate_exposure_limits(imx675, &limits);

	/* Serialize controls with sensor device */
	ctrl_hdlr->lock = &imx675->mutex;

	/* Initialize exposure and gain LEF */
	imx675->lef.exp_ctrl = v4l2_ctrl_new_std(
		ctrl_hdlr, &imx675_ctrl_ops, V4L2_CID_EXPOSURE,
		limits.exp_lef_min, limits.exp_lef_max,
		IMX675_EXPOSURE_STEP, limits.exp_lef_default);

	imx675->lef.again_ctrl =
		v4l2_ctrl_new_std(ctrl_hdlr, &imx675_ctrl_ops,
				  V4L2_CID_ANALOGUE_GAIN, IMX675_AGAIN_MIN,
				  IMX675_AGAIN_MAX, IMX675_AGAIN_STEP,
				  IMX675_AGAIN_DEFAULT);
	
	v4l2_ctrl_cluster(2, &imx675->lef.exp_ctrl);

	/* Initialize exposure and gain SEF1 */
	imx675_setup_custom_ctrl_limits(imx675, &imx675->sef1.exp_ctrl, IMX675_CID_EXPOSURE_SHORT,
		limits.exp_sef1_min, limits.exp_sef1_max, limits.exp_sef1_default);
	imx675_setup_custom_ctrl(imx675, &imx675->sef1.again_ctrl, IMX675_CID_ANALOGUE_GAIN_SHORT);
	v4l2_ctrl_cluster(2, &imx675->sef1.exp_ctrl);

	/* Initialize exposure and gain SEF2 */
	imx675_setup_custom_ctrl_limits(imx675, &imx675->sef2.exp_ctrl, IMX675_CID_EXPOSURE_VERY_SHORT,
		limits.exp_sef2_min, limits.exp_sef2_max, limits.exp_sef2_default);
	imx675_setup_custom_ctrl(imx675, &imx675->sef2.again_ctrl, IMX675_CID_ANALOGUE_GAIN_VERY_SHORT);
	v4l2_ctrl_cluster(2, &imx675->sef2.exp_ctrl);

	imx675->vblank_ctrl =
		v4l2_ctrl_new_std(ctrl_hdlr, &imx675_ctrl_ops, V4L2_CID_VBLANK,
				  mode->vblank_min, mode->vblank_max, 1,
				  mode->vblank);

	imx675->test_pattern_ctrl = v4l2_ctrl_new_std_menu_items(
		ctrl_hdlr, &imx675_ctrl_ops, V4L2_CID_TEST_PATTERN,
		ARRAY_SIZE(imx675_test_pattern_menu) - 1, 0, 0,
		imx675_test_pattern_menu);

	/* Read only HDR custom controls */
	imx675_setup_custom_ctrl(imx675, &imx675->rhs1_ctrl, IMX675_CID_RHS1);
	imx675_setup_custom_ctrl(imx675, &imx675->rhs2_ctrl, IMX675_CID_RHS2);
	imx675_setup_custom_ctrl(imx675, &imx675->shr0_ctrl, IMX675_CID_SHR0);
	imx675_setup_custom_ctrl(imx675, &imx675->shr1_ctrl, IMX675_CID_SHR1);
	imx675_setup_custom_ctrl(imx675, &imx675->shr2_ctrl, IMX675_CID_SHR2);

	/* Other read only custom controls */
	imx675_setup_custom_ctrl(imx675, &imx675->vmax_ctrl, IMX675_CID_VMAX);
	imx675_setup_custom_ctrl(imx675, &imx675->hmax_ctrl, IMX675_CID_HMAX);
	/* Initialize HCG control */
	imx675_setup_custom_ctrl(imx675, &imx675->hcg_ctrl, IMX675_CID_HCG);

	imx675->mode_sel_ctrl = v4l2_ctrl_new_std(ctrl_hdlr, &imx675_ctrl_ops,
				V4L2_CID_WIDE_DYNAMIC_RANGE, IMX675_WDR_MIN,
				IMX675_WDR_MAX, IMX675_WDR_STEP,
				IMX675_WDR_DEFAULT);

	/* Read only controls */
	imx675->pclk_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
						&imx675_ctrl_ops,
						V4L2_CID_PIXEL_RATE,
						link_freq[0],
						link_freq[ARRAY_SIZE(link_freq) - 1],
						1,
						mode->pclk);
	if (imx675->pclk_ctrl)
		imx675->pclk_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx675->link_freq_ctrl = v4l2_ctrl_new_int_menu(
		ctrl_hdlr, &imx675_ctrl_ops, V4L2_CID_LINK_FREQ,
		ARRAY_SIZE(link_freq) - 1, mode->link_freq_idx, link_freq);
	if (imx675->link_freq_ctrl)
		imx675->link_freq_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx675->hblank_ctrl =
		v4l2_ctrl_new_std(ctrl_hdlr, &imx675_ctrl_ops, V4L2_CID_HBLANK,
				  IMX675_REG_MIN, IMX675_REG_MAX, 1,
				  mode->hblank);
	if (imx675->hblank_ctrl)
		imx675->hblank_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (ctrl_hdlr->error) {
		dev_err(imx675->dev, "control init failed: %d", ctrl_hdlr->error);
		v4l2_ctrl_handler_free(ctrl_hdlr);
		return ctrl_hdlr->error;
	}

	imx675->sd.ctrl_handler = ctrl_hdlr;
	return 0;
}

/**
 * imx675_probe() - I2C client device binding
 * @client: pointer to i2c client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_probe(struct i2c_client *client)
{
	struct imx675 *imx675;
	int ret;
	imx675 = devm_kzalloc(&client->dev, sizeof(*imx675), GFP_KERNEL);
	if (!imx675)
		return -ENOMEM;

	imx675->dev = &client->dev;
	dev_info(imx675->dev, "probe started");

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&imx675->sd, client, &imx675_subdev_ops);

	ret = imx675_parse_hw_config(imx675);
	if (ret) {
		dev_err(imx675->dev, "HW configuration is not supported");
		return ret;
	}

	mutex_init(&imx675->mutex);

	ret = imx675_power_on(imx675->dev);
	if (ret) {
		dev_err(imx675->dev, "failed to power-on the sensor");
		goto error_mutex_destroy;
	}

	/* Check module identity */
	ret = imx675_detect(imx675);
	if (ret == -ENXIO) {
		// imx675 is not connected, but another sensor might be
		goto error_power_off;
	} else if (ret) {
		dev_err(imx675->dev, "failed to find sensor: %d", ret);
		goto error_power_off;
	}

	/* Set default mode to max resolution sdr */
	imx675_set_mode(imx675, &supported_sdr_modes[DEFAULT_MODE_IDX]);

	ret = imx675_init_controls(imx675);
	if (ret) {
		dev_err(imx675->dev, "failed to init controls: %d", ret);
		goto error_power_off;
	}

	/* Initialize subdev */
	imx675->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	imx675->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	imx675->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&imx675->sd.entity, 1, &imx675->pad);
	if (ret) {
		dev_err(imx675->dev, "failed to init entity pads: %d", ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&imx675->sd);
	if (ret < 0) {
		dev_err(imx675->dev, "failed to register async subdev: %d", ret);
		goto error_media_entity;
	}

	pm_runtime_set_active(imx675->dev);
	pm_runtime_enable(imx675->dev);
	pm_runtime_idle(imx675->dev);

	dev_info(imx675->dev, "probe finished successfully");
	return 0;

error_media_entity:
	media_entity_cleanup(&imx675->sd.entity);
error_handler_free:
	v4l2_ctrl_handler_free(imx675->sd.ctrl_handler);
error_power_off:
	imx675_power_off(imx675->dev);
error_mutex_destroy:
	mutex_destroy(&imx675->mutex);

	if (ret == -ENXIO) {
		dev_info(imx675->dev, "exit probe, sensor not connected");
	} else {
		dev_err(imx675->dev, "probe failed with %d", ret);
	}
	return ret;
}

/**
 * imx675_remove() - I2C client device unbinding
 * @client: pointer to I2C client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx675_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx675 *imx675 = to_imx675(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx675_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	mutex_destroy(&imx675->mutex);

	return 0;
}

static const struct dev_pm_ops imx675_pm_ops = { SET_RUNTIME_PM_OPS(
	imx675_power_off, imx675_power_on, NULL) };

static const struct of_device_id imx675_of_match[] = {
	{ .compatible = "sony,imx675" },
	{}
};

MODULE_DEVICE_TABLE(of, imx675_of_match);

static struct i2c_driver imx675_driver = {
	.probe_new = imx675_probe,
	.remove = imx675_remove,
	.driver = {
		.name = "imx675",
		.pm = &imx675_pm_ops,
		.of_match_table = imx675_of_match,
	},
};

module_i2c_driver(imx675_driver);

MODULE_DESCRIPTION("Sony imx675 sensor driver");
MODULE_LICENSE("GPL");
