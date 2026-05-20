// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sony imx662 sensor driver
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

#define IMX662_MIN_GAIN 0
#define IMX662_MAX_GAIN_DEC 240
#define IMX662_MAX_GAIN (IMX662_MAX_GAIN_DEC)
#define IMX662_MAX_GAIN_DB 72
#define IMX662_GAIN_STEP 1
#define IMX662_GAIN_DEFAULT 0
#define IMX662_GAIN_LOW 0x3070
#define IMX662_GAIN_HIGH 0x3071

#define IMX662_EXPOSURE_STEP 1
#define IMX662_EXPOSURE_DEFAULT 10000

#define IMX662_INCLK_RATE 37125000
#define IMX662_REG_VALUE_08BIT 1

/* Wide Dynamic Range (SDR/HDR mode select) */
#define IMX662_WDR_MIN 0
#define IMX662_WDR_MAX 1
#define IMX662_WDR_STEP 1
#define IMX662_WDR_DEFAULT 0

/* HDR DOL defaults */
#define IMX662_2DOL_RHS1 0x7F  /* Max 127: ISP can buffer at most 63 pure L lines = (RHS1-1)/2 */
#define IMX662_DEFAULT_RHS2 0xAA
#define IMX662_2DOL_HMAX_VALUE 990

/* HDR gap constants */
#define IMX662_2DOL_SMALL_GAP 2
#define IMX662_2DOL_LARGE_GAP 5
#define IMX662_2DOL_SHR0_RHS1_GAP   IMX662_2DOL_LARGE_GAP
#define IMX662_2DOL_SHR0_FSC_GAP    IMX662_MIN_SHR0_LENGTH
#define IMX662_2DOL_SHR1_MIN_GAP    IMX662_2DOL_LARGE_GAP
#define IMX662_2DOL_SHR1_RHS1_GAP   IMX662_2DOL_SMALL_GAP

/* Analog gain SEF1 */
#define IMX662_REG_AGAIN_SHORT 0x3072

/* HCG registers */
#define IMX662_REG_HCG 0x3030
#define IMX662_REG_HCG_SEF1 0x3031
#define IMX662_HCG_MIN 0
#define IMX662_HCG_MAX 1
#define IMX662_HCG_STEP 1
#define IMX662_HCG_DEFAULT 0

/* Exposure SEF1 */
#define IMX662_EXPOSURE_SHORT_STEP 1

#define IMX662_LINK_FREQ_594M 594000000 /* 594 Mbps data rate */

/* IMX662 Configuration - 12-bit, 37.125MHz, 30fps, 1920x1080 */
#define IMX662_BITS_PER_PIXEL 12

#define IMX662_LANES_DEFAULT  4
#define IMX662_LANES_2        2
#define IMX662_REG_LANEMODE   0x3040
#define IMX662_LANEMODE_2LANE 0x01
#define IMX662_LANEMODE_4LANE 0x03
#define IMX662_WIDTH 1920
#define IMX662_HEIGHT 1080

#define IMX662_MIN_SHR0_LENGTH 4
#define IMX662_SHR0_LOW 0x3050
#define IMX662_SHR0_MID 0x3051
#define IMX662_SHR0_HIGH 0x3052
#define IMX662_SHR1_LOW 0x3054
#define IMX662_SHR1_MID 0x3055
#define IMX662_SHR1_HIGH 0x3056
#define IMX662_SHR2_LOW 0x3058
#define IMX662_SHR2_MID 0x3059
#define IMX662_SHR2_HIGH 0x305A
#define IMX662_RHS1_LOW 0x3060
#define IMX662_RHS1_MID 0x3061
#define IMX662_RHS1_HIGH 0x3062
#define IMX662_RHS2_LOW 0x3064
#define IMX662_RHS2_MID 0x3065
#define IMX662_RHS2_HIGH 0x3066

/* VMAX (Lines Per Frame) registers */
#define IMX662_REG_LPFR 0x3028
#define IMX662_VMAX_DEFAULT 1250
#define IMX662_VMAX_15FPS 2500
#define IMX662_VMAX_MAX 0xFFFFF   /* 20-bit max */

#define IMX662_HMAX_DEFAULT 1980

#define IMX662_REG_HMAX_LOW 0x302C
#define IMX662_REG_HMAX_HIGH 0x302D

/* Test patterns */
#define IMX662_TPG_EN_DUOUT 0x30E0
#define IMX662_TPG_PATSEL_DUOUT 0x30E2
#define IMX662_TPG_COLORWIDTH 0x30E4
#define IMX662_BLKLEVEL_LOW 0x30DC
#define IMX662_TESTCLKEN 0x4900

#define IMX662_REG_STANDBY 0x3000
#define IMX662_REGHOLD 0x3001
#define IMX662_REG_XMSTA 0x3002
#define IMX662_MODE_STANDBY 0x01
#define IMX662_MODE_STREAMING 0x00

#define IMX662_REG_CHIP_ID_1 0x3cb6
#define IMX662_REG_CHIP_ID_2 0x3cc4

/* Custom controls */
#define IMX662_CID_BASE (V4L2_CID_USER_BASE + 0x2000)
#define IMX662_CID_EXPOSURE_SHORT (IMX662_CID_BASE + 1)
#define IMX662_CID_ANALOGUE_GAIN_SHORT (IMX662_CID_BASE + 3)
#define IMX662_CID_RHS1 (IMX662_CID_BASE + 5)
#define IMX662_CID_RHS2 (IMX662_CID_BASE + 6)
#define IMX662_CID_SHR0 (IMX662_CID_BASE + 7)
#define IMX662_CID_SHR1 (IMX662_CID_BASE + 8)
#define IMX662_CID_SHR2 (IMX662_CID_BASE + 9)
#define IMX662_CID_VMAX (IMX662_CID_BASE + 10)
#define IMX662_CID_HMAX (IMX662_CID_BASE + 11)
#define IMX662_CID_HCG (IMX662_CID_BASE + 12)
#define IMX662_CID_CUSTOM_RHS1 (IMX662_CID_BASE + 13)
#define IMX662_CID_WDR_PRIMING (IMX662_CID_BASE + 14)
#define IMX662_CID_CUSTOM_RHS1_PRIMING (IMX662_CID_BASE + 15)
#define IMX662_CID_HCG_LEF  (IMX662_CID_BASE + 16)
#define IMX662_CID_HCG_SEF1 (IMX662_CID_BASE + 17)

/* Priming defaults */
#define IMX662_CUSTOM_RHS1_PRIMING_MIN -1
#define IMX662_CUSTOM_RHS1_PRIMING_DEF -1
#define IMX662_CUSTOM_RHS1_PRIMING_MAX 65535

#define NON_NEGATIVE(val) ((val) < 0 ? 0 : (val))
#define MAX_VAL(val1, val2) ((val1) < (val2) ? (val2) : (val1))

static u32 imx662_reg_shutter[] = {IMX662_SHR0_LOW, IMX662_SHR1_LOW};
static u32 imx662_reg_again[] = {IMX662_GAIN_LOW, IMX662_REG_AGAIN_SHORT};

enum imx662_exposure_type {
	LEF,
	SEF1,
};

#define IMX662_TO_LOW_BYTE(x) (x & 0xFF)
#define IMX662_TO_MID_BYTE(x) ((x >> 8) & 0xFF)
#define IMX662_TO_HIGH_BYTE(x) ((x >> 16) & 0xFF)

/* Cropping */
#define PIX_HST_LOW 0x303C
#define PIX_HST_HIGH 0x303D
#define PIX_HWIDTH_LOW 0x303E
#define PIX_HWIDTH_HIGH 0x303F
#define PIX_VST_LOW 0x3044
#define PIX_VST_HIGH 0x3045
#define PIX_VWIDTH_LOW 0x3046
#define PIX_VWIDTH_HIGH 0x3047

static const char * const imx662_test_pattern_menu[] = {
	[0] = "No pattern",
	[1] = "000h Pattern",
	[2] = "3FF(FFFh) Pattern",
	[3] = "155(555h) Pattern",
	[4] = "2AA(AAAh) Pattern",
	[5] = "555/AAAh Pattern",
	[6] = "AAA/555h Pattern",
	[7] = "000/555h Pattern",
	[8] = "555/000h Pattern",
	[9] = "000/FFFh Pattern",
	[10] = "FFF/000h Pattern",
	[11] = "H Color-bar",
	[12] = "V Color-bar",
};

struct imx662_reg {
	u16 address;
	u8 val;
};
struct imx662_reg_list {
	u32 num_of_regs;
	const struct imx662_reg *regs;
};

struct imx662_mode {
	u32 width;
	u32 height;
	u32 code;
	u32 link_freq_idx;
	u32 lanes;
	u32 bpp;
	s64 pclk;
	u32 vblank;
	u32 vblank_min;
	u32 vblank_max;
	u32 dol;
	u32 rhs1;
	u32 rhs2;
	struct imx662_reg_list reg_list;
	struct v4l2_fract frame_interval;
};

struct exp_gain_ctrl_cluster {
	struct v4l2_ctrl *exp_ctrl;
	struct v4l2_ctrl *again_ctrl;
};
struct imx662 {
	struct device *dev;
	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct gpio_desc *reset_gpio;
	struct clk *inclk;
	struct v4l2_ctrl_handler ctrl_handler;
	const struct imx662_mode *cur_mode;
	struct mutex mutex;
	bool streaming;
	bool hdr_enabled;
	u32 vblank;

	/* Exposure/gain control clusters */
	struct exp_gain_ctrl_cluster lef;
	struct exp_gain_ctrl_cluster sef1;

	/* V4L2 Controls */
	struct v4l2_ctrl *link_freq_ctrl;
	struct v4l2_ctrl *pclk_ctrl;
	struct v4l2_ctrl *vblank_ctrl;
	struct v4l2_ctrl *hblank_ctrl;
	struct v4l2_ctrl *test_pattern;
	struct v4l2_ctrl *mode_sel_ctrl;
	/* HCG control cluster — must be contiguous for v4l2_ctrl_cluster */
	struct v4l2_ctrl *hcg_ctrl;
	struct v4l2_ctrl *hcg_lef_ctrl;
	struct v4l2_ctrl *hcg_sef1_ctrl;

	/* Read-only timing readback controls */
	struct v4l2_ctrl *rhs1_ctrl;
	struct v4l2_ctrl *rhs2_ctrl;
	struct v4l2_ctrl *shr0_ctrl;
	struct v4l2_ctrl *shr1_ctrl;
	struct v4l2_ctrl *shr2_ctrl;
	struct v4l2_ctrl *vmax_ctrl;
	struct v4l2_ctrl *hmax_ctrl;

	/* Priming controls */
	struct v4l2_ctrl *custom_rhs1_ctrl;
	struct v4l2_ctrl *custom_rhs1_priming_ctrl;
	struct v4l2_ctrl *wdr_priming_ctrl;
	int wdr_priming_val;
	int custom_rhs1_priming_val;
	struct v4l2_subdev_format curr_fmt;
	enum fast_toggle_state fast_toggle_state;

	/* Active MIPI data lane count parsed from DT endpoint (2 or 4) */
	u32 lanes;
};

/*
 * Xclk 37.125Mhz
 * max_framerate 30fps
 * 1920x1080 4-lane MIPI, 12-bit, SDR
 */
static const struct imx662_reg imx662_linear_1920x1080_mipi_regs[] = {
	{0x3014, 0x01}, /* INCK_SEL: 0x01 = 37.125 MHz */
	{0x3015, 0x07}, /* DATARATE_SEL 594 Mbps */
	{0x3018, 0x04}, /* WINMODE Crop mode */
	{0x301b, 0x00}, /* ADDMODE non-binning */
	{0x3022, 0x01}, /* ADBIT 12 bit */
	{0x3023, 0x01}, /* MDBIT 12 bit*/
	{0x3040, 0x03}, /* LANEMODE 4 lanes */

	/* HMAX = 1980 (0x07BC) for 30fps */
	{IMX662_REG_HMAX_LOW,  IMX662_TO_LOW_BYTE(IMX662_HMAX_DEFAULT)},
	{IMX662_REG_HMAX_HIGH, IMX662_TO_MID_BYTE(IMX662_HMAX_DEFAULT)},

	/* Crop to 1920x1080 centered in 1936x1108 */
	/* Horizontal: start at (1936-1920)/2 = 8, width = 1920 */
	{PIX_HST_HIGH,    IMX662_TO_MID_BYTE(8)},
	{PIX_HST_LOW,     IMX662_TO_LOW_BYTE(8)},
    {PIX_HWIDTH_HIGH, IMX662_TO_MID_BYTE(1920)},
	{PIX_HWIDTH_LOW,  IMX662_TO_LOW_BYTE(1920)},

	/* Vertical: start at (1108-1080)/2 = 14, height = 1080 */
	{PIX_VST_HIGH,    IMX662_TO_MID_BYTE(14)},   /* PIX_VST = 14 */
	{PIX_VST_LOW,     IMX662_TO_LOW_BYTE(14)},
	{PIX_VWIDTH_HIGH, IMX662_TO_MID_BYTE(1080)}, /* PIX_VWIDTH = 1080 */
	{PIX_VWIDTH_LOW,  IMX662_TO_LOW_BYTE(1080)},

	{0x3050, 0x04}, /* SHR0 Normal mode */
	{0x30A6, 0x00}, /* XVS_DRV_XHS_DRV XVS outtput VHS output*/
	{0x3070, 0x00}, /* GAIN 0 */
	{0x3444, 0xAC}, /* Set to 0xAC */
	{0x3A50, 0xFF}, /* Normal mode AD12bit */
	{0x3A51, 0x03}, /* Normal mode AD12bit */
	{0x3A52, 0x00}, /* AD12bit */
	{0x3460, 0x21}, /* Normal mode (non HDR) */
	{0x3492, 0x08}, /* Set to 0x08 */
	{0x3B00, 0x39}, /* Set to 0x39 */
	{0x3B23, 0x2D}, /* Set to 0x2D */
	{0x3B45, 0x04}, /* Set to 0x04 */
	{0x3C0A, 0x1F}, /* Set to 0x1F */
	{0x3C0B, 0x1E}, /* Set to 0x1E */
	{0x3C38, 0x21}, /* Set to 0x21 */
	{0x3C44, 0x00}, /* Set to 0x00 */
	{0x3CB6, 0xD8}, /* Set to 0xD8 */
	{0x3CC4, 0xDA}, /* Set to 0xDA */
	{0x3E24, 0x79}, /* Set to 0x79 */
	{0x3E2C, 0x15}, /* Set to 0x15 */
	{0x3EDC, 0x2D}, /* Set to 0x2D */
	{0x4498, 0x05}, /* Set to 0x05 */
	{0x449C, 0x19}, /* Set to 0x19 */
	{0x449D, 0x00}, /* Set to 0x00 */
	{0x449E, 0x32}, /* Set to 0x32 */
	{0x449F, 0x01}, /* Set to 0x01 */
	{0x44A0, 0x92}, /* Set to 0x92 */
	{0x44A2, 0x91}, /* Set to 0x91 */
	{0x44A4, 0x8C}, /* Set to 0x8C */
	{0x44A6, 0x87}, /* Set to 0x87 */
	{0x44A8, 0x82}, /* Set to 0x82 */
	{0x44AA, 0x78}, /* Set to 0x78 */
	{0x44AC, 0x6E}, /* Set to 0x6E */
	{0x44AE, 0x69}, /* Set to 0x69 */
	{0x44B0, 0x92}, /* Set to 0x92 */
	{0x44B2, 0x91}, /* Set to 0x91 */
	{0x44B4, 0x8C}, /* Set to 0x8C */
	{0x44B6, 0x87}, /* Set to 0x87 */
	{0x44B8, 0x82}, /* Set to 0x82 */
	{0x44BA, 0x78}, /* Set to 0x78 */
	{0x44BC, 0x6E}, /* Set to 0x6E */
	{0x44BE, 0x69}, /* Set to 0x69 */
	{0x44C0, 0x7F}, /* Set to 0x7F */
	{0x44C1, 0x01}, /* Set to 0x01 */
	{0x44C2, 0x7F}, /* Set to 0x7F */
	{0x44C3, 0x01}, /* Set to 0x01 */
	{0x44C4, 0x7A}, /* Set to 0x7A */
	{0x44C5, 0x01}, /* Set to 0x01 */
	{0x44C6, 0x7A}, /* Set to 0x7A */
	{0x44C7, 0x01}, /* Set to 0x01 */
	{0x44C8, 0x70}, /* Set to 0x70 */
	{0x44C9, 0x01}, /* Set to 0x01 */
	{0x44CA, 0x6B}, /* Set to 0x6B */
	{0x44CB, 0x01}, /* Set to 0x01 */
	{0x44CC, 0x6B}, /* Set to 0x6B */
	{0x44CD, 0x01}, /* Set to 0x01 */
	{0x44CE, 0x5C}, /* Set to 0x5C */
	{0x44CF, 0x01}, /* Set to 0x01 */
	{0x44D0, 0x7F}, /* Set to 0x7F */
	{0x44D1, 0x01}, /* Set to 0x01 */
	{0x44D2, 0x7F}, /* Set to 0x7F */
	{0x44D3, 0x01}, /* Set to 0x01 */
	{0x44D4, 0x7A}, /* Set to 0x7A */
	{0x44D5, 0x01}, /* Set to 0x01 */
	{0x44D6, 0x7A}, /* Set to 0x7A */
	{0x44D7, 0x01}, /* Set to 0x01 */
	{0x44D8, 0x70}, /* Set to 0x70 */
	{0x44D9, 0x01}, /* Set to 0x01 */
	{0x44DA, 0x6B}, /* Set to 0x6B */
	{0x44DB, 0x01}, /* Set to 0x01 */
	{0x44DC, 0x6B}, /* Set to 0x6B */
	{0x44DD, 0x01}, /* Set to 0x01 */
	{0x44DE, 0x5C}, /* Set to 0x5C */
	{0x44DF, 0x01}, /* Set to 0x01 */
	{0x4534, 0x1C}, /* Set to 0x1C */
	{0x4535, 0x03}, /* Set to 0x03 */
	{0x4538, 0x1C}, /* Set to 0x1C */
	{0x4539, 0x1C}, /* Set to 0x1C */
	{0x453A, 0x1C}, /* Set to 0x1C */
	{0x453B, 0x1C}, /* Set to 0x1C */
	{0x453C, 0x1C}, /* Set to 0x1C */
	{0x453D, 0x1C}, /* Set to 0x1C */
	{0x453E, 0x1C}, /* Set to 0x1C */
	{0x453F, 0x1C}, /* Set to 0x1C */
	{0x4540, 0x1C}, /* Set to 0x1C */
	{0x4541, 0x03}, /* Set to 0x03 */
	{0x4542, 0x03}, /* Set to 0x03 */
	{0x4543, 0x03}, /* Set to 0x03 */
	{0x4544, 0x03}, /* Set to 0x03 */
	{0x4545, 0x03}, /* Set to 0x03 */
	{0x4546, 0x03}, /* Set to 0x03 */
	{0x4547, 0x03}, /* Set to 0x03 */
	{0x4548, 0x03}, /* Set to 0x03 */
	{0x4549, 0x03}, /* Set to 0x03 */
};

/*
 * Xclk 37.125Mhz
 * max_framerate 30fps
 * 1920x1080 4-lane MIPI, 12-bit, DOL 2-frame HDR
 */
static const struct imx662_reg imx662_2dol_1920x1080_30fps_regs[] = {
	{0x3014, 0x01}, /* INCK_SEL: 0x01 = 37.125 MHz */
	{0x3015, 0x07}, /* DATARATE_SEL 594 Mbps */
	{0x3018, 0x04}, /* WINMODE Crop mode */
	{0x301A, 0x01}, /* WDMODE DOL 2-frame */
	{0x301B, 0x00}, /* ADDMODE non-binning */
	{0x301C, 0x01}, /* THIN_V_EN Enable (DOL subsampling) */
	{0x3022, 0x01}, /* ADBIT 12 bit */
	{0x3023, 0x01}, /* MDBIT 12 bit */
	{0x3040, 0x03}, /* LANEMODE 4 lanes */

	/* HMAX = 990 (0x03DE) for 30fps DOL 2-frame */
	{IMX662_REG_HMAX_LOW,  IMX662_TO_LOW_BYTE(IMX662_2DOL_HMAX_VALUE)},
	{IMX662_REG_HMAX_HIGH, IMX662_TO_MID_BYTE(IMX662_2DOL_HMAX_VALUE)},

	/* Crop to 1920x1080 centered in 1936x1108 */
	{PIX_HST_HIGH,    IMX662_TO_MID_BYTE(8)},
	{PIX_HST_LOW,     IMX662_TO_LOW_BYTE(8)},
	{PIX_HWIDTH_HIGH, IMX662_TO_MID_BYTE(1920)},
	{PIX_HWIDTH_LOW,  IMX662_TO_LOW_BYTE(1920)},
	{PIX_VST_HIGH,    IMX662_TO_MID_BYTE(14)},
	{PIX_VST_LOW,     IMX662_TO_LOW_BYTE(14)},
	{PIX_VWIDTH_HIGH, IMX662_TO_MID_BYTE(1080)},
	{PIX_VWIDTH_LOW,  IMX662_TO_LOW_BYTE(1080)},

	/* DOL timing registers - SHR0/SHR1/RHS1 bytes must be consecutive */
	{IMX662_SHR0_LOW,  0xE8}, /* SHR0 = 0x03E8 (1000) */
	{IMX662_SHR0_MID,  0x03},
	{IMX662_SHR0_HIGH, 0x00},
	{IMX662_SHR1_LOW,  0x05}, /* SHR1 = 5 */
	{IMX662_SHR1_MID,  0x00},
	{IMX662_SHR1_HIGH, 0x00},
	{IMX662_RHS1_LOW,  IMX662_2DOL_RHS1},
	{IMX662_RHS1_MID,  0x00},
	{IMX662_RHS1_HIGH, 0x00},

	{0x3070, 0x00}, /* GAIN 0 */
	{0x30A6, 0x00}, /* XVS_DRV_XHS_DRV Master mode */
	{0x3400, 0x00}, /* GAIN_PGC_FIDMD per-frame gain enable */

	/* Calibration registers - identical for all modes */
	{0x3444, 0xAC}, /* Set to 0xAC */
	{0x3A50, 0xFF}, /* Normal mode AD12bit */
	{0x3A51, 0x03}, /* Normal mode AD12bit */
	{0x3A52, 0x00}, /* AD12bit */
	{0x3460, 0x21}, /* Normal mode (DOL HDR) */
	{0x3492, 0x08}, /* Set to 0x08 */
	{0x3B00, 0x39}, /* Set to 0x39 */
	{0x3B23, 0x2D}, /* Set to 0x2D */
	{0x3B45, 0x04}, /* Set to 0x04 */
	{0x3C0A, 0x1F}, /* Set to 0x1F */
	{0x3C0B, 0x1E}, /* Set to 0x1E */
	{0x3C38, 0x21}, /* Set to 0x21 */
	{0x3C44, 0x00}, /* Set to 0x00 */
	{0x3CB6, 0xD8}, /* Set to 0xD8 */
	{0x3CC4, 0xDA}, /* Set to 0xDA */
	{0x3E24, 0x79}, /* Set to 0x79 */
	{0x3E2C, 0x15}, /* Set to 0x15 */
	{0x3EDC, 0x2D}, /* Set to 0x2D */
	{0x4498, 0x05}, /* Set to 0x05 */
	{0x449C, 0x19}, /* Set to 0x19 */
	{0x449D, 0x00}, /* Set to 0x00 */
	{0x449E, 0x32}, /* Set to 0x32 */
	{0x449F, 0x01}, /* Set to 0x01 */
	{0x44A0, 0x92}, /* Set to 0x92 */
	{0x44A2, 0x91}, /* Set to 0x91 */
	{0x44A4, 0x8C}, /* Set to 0x8C */
	{0x44A6, 0x87}, /* Set to 0x87 */
	{0x44A8, 0x82}, /* Set to 0x82 */
	{0x44AA, 0x78}, /* Set to 0x78 */
	{0x44AC, 0x6E}, /* Set to 0x6E */
	{0x44AE, 0x69}, /* Set to 0x69 */
	{0x44B0, 0x92}, /* Set to 0x92 */
	{0x44B2, 0x91}, /* Set to 0x91 */
	{0x44B4, 0x8C}, /* Set to 0x8C */
	{0x44B6, 0x87}, /* Set to 0x87 */
	{0x44B8, 0x82}, /* Set to 0x82 */
	{0x44BA, 0x78}, /* Set to 0x78 */
	{0x44BC, 0x6E}, /* Set to 0x6E */
	{0x44BE, 0x69}, /* Set to 0x69 */
	{0x44C0, 0x7F}, /* Set to 0x7F */
	{0x44C1, 0x01}, /* Set to 0x01 */
	{0x44C2, 0x7F}, /* Set to 0x7F */
	{0x44C3, 0x01}, /* Set to 0x01 */
	{0x44C4, 0x7A}, /* Set to 0x7A */
	{0x44C5, 0x01}, /* Set to 0x01 */
	{0x44C6, 0x7A}, /* Set to 0x7A */
	{0x44C7, 0x01}, /* Set to 0x01 */
	{0x44C8, 0x70}, /* Set to 0x70 */
	{0x44C9, 0x01}, /* Set to 0x01 */
	{0x44CA, 0x6B}, /* Set to 0x6B */
	{0x44CB, 0x01}, /* Set to 0x01 */
	{0x44CC, 0x6B}, /* Set to 0x6B */
	{0x44CD, 0x01}, /* Set to 0x01 */
	{0x44CE, 0x5C}, /* Set to 0x5C */
	{0x44CF, 0x01}, /* Set to 0x01 */
	{0x44D0, 0x7F}, /* Set to 0x7F */
	{0x44D1, 0x01}, /* Set to 0x01 */
	{0x44D2, 0x7F}, /* Set to 0x7F */
	{0x44D3, 0x01}, /* Set to 0x01 */
	{0x44D4, 0x7A}, /* Set to 0x7A */
	{0x44D5, 0x01}, /* Set to 0x01 */
	{0x44D6, 0x7A}, /* Set to 0x7A */
	{0x44D7, 0x01}, /* Set to 0x01 */
	{0x44D8, 0x70}, /* Set to 0x70 */
	{0x44D9, 0x01}, /* Set to 0x01 */
	{0x44DA, 0x6B}, /* Set to 0x6B */
	{0x44DB, 0x01}, /* Set to 0x01 */
	{0x44DC, 0x6B}, /* Set to 0x6B */
	{0x44DD, 0x01}, /* Set to 0x01 */
	{0x44DE, 0x5C}, /* Set to 0x5C */
	{0x44DF, 0x01}, /* Set to 0x01 */
	{0x4534, 0x1C}, /* Set to 0x1C */
	{0x4535, 0x03}, /* Set to 0x03 */
	{0x4538, 0x1C}, /* Set to 0x1C */
	{0x4539, 0x1C}, /* Set to 0x1C */
	{0x453A, 0x1C}, /* Set to 0x1C */
	{0x453B, 0x1C}, /* Set to 0x1C */
	{0x453C, 0x1C}, /* Set to 0x1C */
	{0x453D, 0x1C}, /* Set to 0x1C */
	{0x453E, 0x1C}, /* Set to 0x1C */
	{0x453F, 0x1C}, /* Set to 0x1C */
	{0x4540, 0x1C}, /* Set to 0x1C */
	{0x4541, 0x03}, /* Set to 0x03 */
	{0x4542, 0x03}, /* Set to 0x03 */
	{0x4543, 0x03}, /* Set to 0x03 */
	{0x4544, 0x03}, /* Set to 0x03 */
	{0x4545, 0x03}, /* Set to 0x03 */
	{0x4546, 0x03}, /* Set to 0x03 */
	{0x4547, 0x03}, /* Set to 0x03 */
	{0x4548, 0x03}, /* Set to 0x03 */
	{0x4549, 0x03}, /* Set to 0x03 */
};

struct imx662_exp_limits {
	u32 lpfr;
	u32 min_lpfr;
	u32 max_lpfr;
	u32 shr0_min;
	u32 shr0_max;
	u32 exp_lef_min;
	u32 exp_lef_max;
	u32 exp_lef_default;

	u32 shr1_min;
	u32 shr1_max;
	u32 exp_sef1_min;
	u32 exp_sef1_max;
	u32 exp_sef1_default;
};

static const s64 link_freq[] = {
	IMX662_LINK_FREQ_594M,
};

static const struct imx662_mode supported_sdr_modes[] = {
	/* 1920x1080 @ 30fps, 4 lane, 12-bit, SDR */
	{
		.width = IMX662_WIDTH,
		.height = IMX662_HEIGHT,
		.link_freq_idx = 0,  /* 594 Mbps */
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.lanes = IMX662_LANES_DEFAULT,
		.bpp = IMX662_BITS_PER_PIXEL,
		.pclk = IMX662_LINK_FREQ_594M,
		.vblank = IMX662_VMAX_DEFAULT - IMX662_HEIGHT,
		.vblank_min = 4,
		.vblank_max = IMX662_VMAX_MAX - IMX662_HEIGHT,
		.dol = 1,
		.rhs1 = 0,
		.rhs2 = 0,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(imx662_linear_1920x1080_mipi_regs),
			.regs = imx662_linear_1920x1080_mipi_regs,
		},
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		},
	},
	/* 1920x1080 @ 15fps SDR — VMAX-only scaling per IMX662 SRM Rev5.0.
	 * Reuses the 30fps reg list; VBLANK ctrl writes LPFR=2500 at stream start.
	 */
	{
		.width = IMX662_WIDTH,
		.height = IMX662_HEIGHT,
		.link_freq_idx = 0,
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.lanes = IMX662_LANES_DEFAULT,
		.bpp = IMX662_BITS_PER_PIXEL,
		.pclk = IMX662_LINK_FREQ_594M,
		.vblank = IMX662_VMAX_15FPS - IMX662_HEIGHT,
		.vblank_min = 4,
		.vblank_max = IMX662_VMAX_MAX - IMX662_HEIGHT,
		.dol = 1,
		.rhs1 = 0,
		.rhs2 = 0,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(imx662_linear_1920x1080_mipi_regs),
			.regs = imx662_linear_1920x1080_mipi_regs,
		},
		.frame_interval = {
			.denominator = 15,
			.numerator = 1,
		},
	},
};

static const struct imx662_mode supported_hdr_modes[] = {
	/* 1920x1080 @ 30fps, 4 lane, 12-bit, DOL 2-frame HDR */
	{
		.width = IMX662_WIDTH,
		.height = IMX662_HEIGHT,
		.link_freq_idx = 0,  /* 594 Mbps */
		.code = MEDIA_BUS_FMT_SRGGB12_2X12,
		.lanes = IMX662_LANES_DEFAULT,
		.bpp = IMX662_BITS_PER_PIXEL,
		.pclk = IMX662_LINK_FREQ_594M,
		.vblank = IMX662_VMAX_DEFAULT - IMX662_HEIGHT,
		.vblank_min = 4,
		.vblank_max = IMX662_VMAX_MAX - IMX662_HEIGHT,
		.dol = 2,
		.rhs1 = IMX662_2DOL_RHS1,
		.rhs2 = 0,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(imx662_2dol_1920x1080_30fps_regs),
			.regs = imx662_2dol_1920x1080_30fps_regs,
		},
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		},
	}
};

static inline struct imx662 *to_imx662(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct imx662, sd);
}

/**
 * imx662_read_reg() - Read registers.
 * @imx662: pointer to imx662 device
 * @reg: register address
 * @len: length of bytes to read. Max supported bytes is 4
 * @val: pointer to register value to be filled.
 *
 * Big endian register addresses with little endian values.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx662_read_reg(struct imx662 *imx662, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx662->sd);
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
 * imx662_write_reg() - Write register
 * @imx662: pointer to imx662 device
 * @reg: register address
 * @len: length of bytes. Max supported bytes is 4
 * @val: register value
 *
 * Big endian register addresses with little endian values.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx662_write_reg(struct imx662 *imx662, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx662->sd);
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

static int imx662_write_regs(struct imx662 *imx662,
				const struct imx662_reg *regs, u32 len)
{
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = imx662_write_reg(imx662, regs[i].address, IMX662_REG_VALUE_08BIT, regs[i].val);
		if (ret) {
			dev_err(imx662->dev, "Failed to write reg[%u] 0x%04x = 0x%02x: %d\n",
				i, regs[i].address, regs[i].val, ret);
			return ret;
		}
	}

	return 0;
}

static int imx662_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx662 *sensor = to_imx662(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	/* Hold i2c bus across the power-on transition so userspace 3A i2c
	 * cannot interleave a write to a sensor that is mid-reset. */
	i2c_lock_bus(client->adapter, I2C_LOCK_SEGMENT);

	/* 1) Hold XCLR low for >= 500 ns (use 1 µs margin) */
	if (sensor->reset_gpio)
		gpiod_set_value_cansleep(sensor->reset_gpio, 0);
	udelay(1); /* 1 µs */

	/* 2) Release XCLR (set high) -> system clear applied here */
	if (sensor->reset_gpio)
		gpiod_set_value_cansleep(sensor->reset_gpio, 1);

	/* XCLR high to INCK start must be >= 1us */
	udelay(2);

	/* 3) Now enable master clock (clock must come after XCLR high) */
	ret = clk_prepare_enable(sensor->inclk);
	if (ret) {
		dev_err(dev, "failed to enable master clock: %d\n", ret);
		/* attempt to leave XCLR low for safety */
		if (sensor->reset_gpio)
			gpiod_set_value_cansleep(sensor->reset_gpio, 0);
		i2c_unlock_bus(client->adapter, I2C_LOCK_SEGMENT);
		return ret;
	}

	/* 4) Wait >= 20 µs for internal stabilization */
	usleep_range(20, 25);

	i2c_unlock_bus(client->adapter, I2C_LOCK_SEGMENT);
	return 0;
}

static int imx662_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx662 *sensor = to_imx662(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	/* Hold i2c bus across the power-off transition so userspace 3A i2c
	 * cannot interleave a write to a sensor that is mid-reset. */
	i2c_lock_bus(client->adapter, I2C_LOCK_SEGMENT);

	/* 1) Disable master clock first (stop INCK) */
	clk_disable_unprepare(sensor->inclk);

	/* 2) Assert XCLR low (make sure input is 0V before OVDD falls) */
	if (sensor->reset_gpio)
		gpiod_set_value_cansleep(sensor->reset_gpio, 0);

	i2c_unlock_bus(client->adapter, I2C_LOCK_SEGMENT);

	return 0;
}

static int imx662_start_streaming(struct imx662 *imx662)
{
	int ret;

	/* Save all writable control values before handler setup may overwrite them */
	struct hailo_ctrl_snapshot snap;

	hailo_ctrl_snapshot_save(&imx662->ctrl_handler, &snap);

	/* Write sensor mode registers */
	ret = imx662_write_regs(imx662, imx662->cur_mode->reg_list.regs,
				imx662->cur_mode->reg_list.num_of_regs);
	if (ret) {
		dev_err(imx662->dev, "Failed to write mode registers: %d\n", ret);
		return ret;
	}

	/* Override LANEMODE if DT declared 2-lane (boards where the SoC only
	 * routes 2 data lanes to the CSI bridge, e.g. H15L SBC CSI1). The
	 * mode register tables hardcode 4-lane (0x03); patch back to 0x01
	 * for 2-lane configs.
	 */
	if (imx662->lanes == IMX662_LANES_2) {
		ret = imx662_write_reg(imx662, IMX662_REG_LANEMODE,
				       IMX662_REG_VALUE_08BIT,
				       IMX662_LANEMODE_2LANE);
		if (ret) {
			dev_err(imx662->dev,
				"Failed to set 2-lane LANEMODE: %d\n", ret);
			return ret;
		}
	}

	/* Setup handler: pushes all cur.val to hardware via s_ctrl callbacks */
	ret = __v4l2_ctrl_handler_setup(&imx662->ctrl_handler);
	if (ret) {
		dev_err(imx662->dev, "Failed to apply v4l2 controls: %d\n", ret);
		return ret;
	}

	/* Restore any control values corrupted by handler_setup side effects
	 * (e.g. VBLANK s_ctrl calling __v4l2_ctrl_modify_range on exposure) */
	hailo_ctrl_snapshot_restore(&snap);

	/* Standby cancel */
	ret = imx662_write_reg(imx662, IMX662_REG_STANDBY, IMX662_REG_VALUE_08BIT, IMX662_MODE_STREAMING);
	if (ret) {
		dev_err(imx662->dev, "Failed to cancel standby on stream start: %d\n", ret);
		return ret;
	}

	/* Wait 24ms for internal regulator stabilization */
	usleep_range(24000, 25000);

	/* Master mode start */
	ret = imx662_write_reg(imx662, IMX662_REG_XMSTA, IMX662_REG_VALUE_08BIT, IMX662_MODE_STREAMING);
	if (ret) {
		dev_err(imx662->dev, "Failed to start master mode on stream start: %d\n", ret);
		return ret;
	}

	dev_info(imx662->dev, "imx662: stream started");
	return 0;
}

static int imx662_stop_streaming(struct imx662 *imx662)
{
	int ret;

	/* STANDBY=1 then XMSTA=1 per datasheet stop sequence */
	ret = imx662_write_reg(imx662, IMX662_REG_STANDBY, IMX662_REG_VALUE_08BIT, IMX662_MODE_STANDBY);
	if (ret) {
		dev_err(imx662->dev, "Failed to set standby on stream stop: %d\n", ret);
		return ret;
	}

	ret = imx662_write_reg(imx662, IMX662_REG_XMSTA, IMX662_REG_VALUE_08BIT, IMX662_MODE_STANDBY);
	if (ret) {
		dev_err(imx662->dev, "Failed to stop master mode on stream stop: %d\n", ret);
		return ret;
	}

	dev_info(imx662->dev, "imx662: stream stopped");
	return 0;
}

static int imx662_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx662 *imx662 = to_imx662(sd);
	struct i2c_client *client = imx662->client;
	int ret = 0;

	mutex_lock(&imx662->mutex);
	if (imx662->streaming == enable)
		goto unlock_and_return;

	if (enable) {
		ret = pm_runtime_resume_and_get(&client->dev);
		if (ret < 0)
			goto unlock_and_return;

		ret = imx662_start_streaming(imx662);
		if (ret) {
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		ret = imx662_stop_streaming(imx662);
		if (ret) {
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
		pm_runtime_put(&client->dev);
	}

	imx662->streaming = enable;
unlock_and_return:
	mutex_unlock(&imx662->mutex);

	return ret;
}

static int imx662_g_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *fi)
{
	struct imx662 *imx662 = to_imx662(sd);

	mutex_lock(&imx662->mutex);
	fi->interval = imx662->cur_mode->frame_interval;
	mutex_unlock(&imx662->mutex);

	return 0;
}

static void imx662_set_mode(struct imx662 *imx662, const struct imx662_mode *mode);
static int imx662_update_exp_vblank_controls(struct imx662 *imx662);
static void imx662_set_exp_activity(struct imx662 *imx662);

static int imx662_s_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *fi)
{
	struct imx662 *imx662 = to_imx662(sd);
	const struct imx662_mode *modes;
	const struct imx662_mode *match = NULL;
	const struct imx662_mode *prev_mode;
	size_t mode_count;
	size_t i;
	int ret;
	int revert_ret;

	mutex_lock(&imx662->mutex);

	dev_dbg(imx662->dev, "s_frame_interval: requested %u/%u (fps=%u)\n",
		fi->interval.numerator, fi->interval.denominator,
		fi->interval.numerator ? (fi->interval.denominator / fi->interval.numerator) : 0);

	if (imx662->streaming) {
		fi->interval = imx662->cur_mode->frame_interval;
		mutex_unlock(&imx662->mutex);
		return -EBUSY;
	}

	if (fi->interval.numerator == 0 || fi->interval.denominator == 0) {
		fi->interval = imx662->cur_mode->frame_interval;
		mutex_unlock(&imx662->mutex);
		return -EINVAL;
	}

	if (imx662->hdr_enabled) {
		modes = supported_hdr_modes;
		mode_count = ARRAY_SIZE(supported_hdr_modes);
	} else {
		modes = supported_sdr_modes;
		mode_count = ARRAY_SIZE(supported_sdr_modes);
	}

	/* Cross-product equality for rational comparison: match if
	 *   mode.num * fi.den == mode.den * fi.num
	 * so equivalent rationals (e.g. 1/30 ≡ 2/60) all match.
	 * Mirrors the spirit of imx715_find_nearest_frame_interval_mode().
	 */
	for (i = 0; i < mode_count; i++) {
		u64 lhs = (u64)modes[i].frame_interval.numerator * fi->interval.denominator;
		u64 rhs = (u64)modes[i].frame_interval.denominator * fi->interval.numerator;

		if (lhs == rhs) {
			match = &modes[i];
			break;
		}
	}

	if (!match) {
		fi->interval = imx662->cur_mode->frame_interval;
		mutex_unlock(&imx662->mutex);
		return -EINVAL;
	}

	if (match != imx662->cur_mode) {
		/* Mirror the HDR-toggle pattern (see imx662_set_ctrl WDR path):
		 * change cur_mode, refresh exposure + vblank ctrl ranges, revert
		 * on failure so userspace observes a consistent state.
		 */
		prev_mode = imx662->cur_mode;
		imx662_set_mode(imx662, match);

		ret = imx662_update_exp_vblank_controls(imx662);
		if (ret) {
			dev_warn(imx662->dev,
				 "Failed to update exp/vblank controls for %ufps, reverting\n",
				 fi->interval.denominator);
			imx662_set_mode(imx662, prev_mode);
			revert_ret = imx662_update_exp_vblank_controls(imx662);
			if (revert_ret)
				dev_err(imx662->dev,
					"Failed to revert to previous mode\n");
			fi->interval = imx662->cur_mode->frame_interval;
			mutex_unlock(&imx662->mutex);
			return ret;
		}

		imx662_set_exp_activity(imx662);
	}

	fi->interval = imx662->cur_mode->frame_interval;
	mutex_unlock(&imx662->mutex);
	return 0;
}

static const struct v4l2_subdev_video_ops imx662_video_ops = {
	.s_stream = imx662_set_stream,
	.g_frame_interval = imx662_g_frame_interval,
	.s_frame_interval = imx662_s_frame_interval,
};

static int imx662_detect(struct imx662 *imx662)
{
	int ret;
	u32 id_1, id_2;

	ret = imx662_read_reg(imx662, IMX662_REG_CHIP_ID_1, IMX662_REG_VALUE_08BIT, &id_1);
	if (ret) {
		dev_err(imx662->dev, "failed to read sensor id_1 register, ret %d\n",
			ret);
		return ret;
	}

	ret = imx662_read_reg(imx662, IMX662_REG_CHIP_ID_2, IMX662_REG_VALUE_08BIT, &id_2);
	if (ret) {
    	dev_err(imx662->dev, "failed to read sensor id_2 register, ret %d\n",
			ret);
		return ret;
	}

	if(id_1 != SENSOR_ID_IMX662 ||
		id_2 != IMX662_SENSOR_ID_VAL) {
		dev_info(imx662->dev,
			"Sensor ID wrong (could be corrupted or sensor is not connected) expected 0x%02x%02x, found 0x%02x%02x\n",
			SENSOR_ID_IMX662, IMX662_SENSOR_ID_VAL, id_1, id_2);
		return -ENXIO;
	}

	return 0;
}

static struct imx662_reg mode_enable_pattern_generator[] = {
	{IMX662_BLKLEVEL_LOW, 0x00},
	{IMX662_TPG_EN_DUOUT, 0x01},
	{IMX662_TPG_COLORWIDTH, 0x00},
	{IMX662_TESTCLKEN, 0x0A},
};

static struct imx662_reg mode_disable_pattern_generator[] = {
	{IMX662_BLKLEVEL_LOW, 0x32},
	{IMX662_TPG_EN_DUOUT, 0x00},
	{IMX662_TPG_COLORWIDTH, 0x00},
	{IMX662_TESTCLKEN, 0x02},
};

static int imx662_set_test_pattern(struct imx662 *imx662, int val)
{
	int ret;

	if (val > 0 && val < ARRAY_SIZE(imx662_test_pattern_menu)) {
		ret = imx662_write_regs(imx662, mode_enable_pattern_generator,
					ARRAY_SIZE(mode_enable_pattern_generator));
		if (ret < 0) {
			dev_err(imx662->dev, "%s:imx662_write_reg_array error\n", __func__);
			return -EINVAL;
		}
		ret = imx662_write_reg(imx662, IMX662_TPG_PATSEL_DUOUT, IMX662_REG_VALUE_08BIT, val - 1);
	} else {
		ret = imx662_write_regs(imx662, mode_disable_pattern_generator,
			ARRAY_SIZE(mode_disable_pattern_generator));
		if (ret < 0) {
			dev_err(imx662->dev, "%s:imx662_write_reg_array disable error\n", __func__);
			return -EINVAL;
		}
	}
	return ret;
}

/**
 * _get_mode_reg_val_by_address() - Get register value from mode's register list
 * @reg_list: pointer to the register list
 * @reg_address: base register address to find
 * @num_bytes: number of consecutive bytes to read
 *
 * Searches the register list for the given address and constructs a multi-byte
 * value from consecutive register entries.
 *
 * Return: register value, or 0 if not found.
 */
static u32 _get_mode_reg_val_by_address(const struct imx662_reg_list *reg_list,
					u16 reg_address, int num_bytes)
{
	u32 val = 0;
	u32 i, j;

	for (i = 0; i < reg_list->num_of_regs; i++) {
		if (reg_list->regs[i].address == reg_address) {
			for (j = 0; j < num_bytes && (i + j) < reg_list->num_of_regs; j++) {
				if (reg_list->regs[i + j].address == reg_address + j)
					val |= (reg_list->regs[i + j].val & 0xff) << (8 * j);
			}
			return val;
		}
	}
	return 0;
}

static void imx662_calculate_exposure_limits(struct imx662 *imx662, struct imx662_exp_limits *limits)
{
	const int rhs1 = imx662->cur_mode->rhs1 > 0 ? imx662->cur_mode->rhs1 : IMX662_2DOL_RHS1;
	u32 shr0, shr1;

	limits->lpfr = imx662->cur_mode->dol * (imx662->vblank + imx662->cur_mode->height);
	limits->min_lpfr = imx662->cur_mode->dol * (imx662->cur_mode->vblank_min + imx662->cur_mode->height);
	limits->max_lpfr = imx662->cur_mode->dol * (imx662->cur_mode->vblank_max + imx662->cur_mode->height);

	/* LEF (SHR0) limits */
	limits->shr0_min = imx662->hdr_enabled ? rhs1 + IMX662_2DOL_SHR0_RHS1_GAP : IMX662_2DOL_SHR0_FSC_GAP;
	limits->shr0_max = NON_NEGATIVE((int)limits->max_lpfr - IMX662_2DOL_SHR0_FSC_GAP);
	limits->exp_lef_min = IMX662_2DOL_SHR0_FSC_GAP;
	limits->exp_lef_max = NON_NEGATIVE((int)limits->max_lpfr - (int)limits->shr0_min);
	shr0 = _get_mode_reg_val_by_address(&imx662->cur_mode->reg_list, IMX662_SHR0_LOW, 3);
	limits->exp_lef_default = MAX_VAL(limits->exp_lef_min, NON_NEGATIVE((int)limits->lpfr - (int)shr0));

	if (imx662->cur_mode->dol < 2)
		return;

	limits->shr1_min = IMX662_2DOL_SHR1_MIN_GAP;
	limits->shr1_max = NON_NEGATIVE(rhs1 - IMX662_2DOL_SHR1_RHS1_GAP);
	limits->exp_sef1_min = NON_NEGATIVE(rhs1 - (int)limits->shr1_max);
	limits->exp_sef1_max = NON_NEGATIVE(rhs1 - (int)limits->shr1_min);
	shr1 = MAX_VAL(limits->shr1_min, _get_mode_reg_val_by_address(&imx662->cur_mode->reg_list, IMX662_SHR1_LOW, 3));
	limits->exp_sef1_default = MAX_VAL(limits->exp_sef1_min, NON_NEGATIVE((int)rhs1 - (int)shr1));
}

/**
 * imx662_update_exp_gain() - Set exposure and gain for the specified frame type
 * @imx662: pointer to imx662 device
 * @exposure: exposure value in lines
 * @gain: analog gain value
 * @exposure_type: LEF or SEF1
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx662_update_exp_gain(struct imx662 *imx662, u32 exposure, u32 gain,
				  enum imx662_exposure_type exposure_type)
{
	u32 lpfr, shutter;
	int ret;
	int gap;

	switch (exposure_type) {
	case LEF:
		gap = imx662->hdr_enabled ?
			imx662->cur_mode->rhs1 + IMX662_2DOL_SHR0_RHS1_GAP :
			IMX662_2DOL_SHR0_FSC_GAP;

		/* If vblank is too small for the requested exposure, increase it */
		if (exposure > imx662->cur_mode->dol * (imx662->vblank + imx662->cur_mode->height) - gap) {
			imx662->vblank = exposure - imx662->cur_mode->dol * imx662->cur_mode->height + gap;
			__v4l2_ctrl_s_ctrl(imx662->vblank_ctrl, imx662->vblank);
		}

		lpfr = imx662->vblank + imx662->cur_mode->height;
		shutter = NON_NEGATIVE((int)imx662->cur_mode->dol * (int)lpfr - (int)exposure);
		break;
	case SEF1:
		lpfr = imx662->vblank + imx662->cur_mode->height;
		shutter = NON_NEGATIVE((int)imx662->cur_mode->rhs1 - (int)exposure);
		break;
	}

	dev_dbg(imx662->dev, "Set exp type=%d exp=%u gain=%u shutter=%u lpfr=%u",
		exposure_type, exposure, gain, shutter, lpfr);

	ret = imx662_write_reg(imx662, IMX662_REGHOLD, 1, 1);
	if (ret)
		return ret;

	if (exposure_type == LEF) {
		ret = imx662_write_reg(imx662, IMX662_REG_LPFR, 3, lpfr);
		if (ret)
			goto error_release_group_hold;
	}

	ret = imx662_write_reg(imx662, imx662_reg_shutter[exposure_type], 3, shutter);
	if (ret)
		goto error_release_group_hold;

	ret = imx662_write_reg(imx662, imx662_reg_again[exposure_type], 2, gain);

error_release_group_hold:
	imx662_write_reg(imx662, IMX662_REGHOLD, 1, 0);

	return ret;
}

/**
 * imx662_set_hcg_mode() - Set High Conversion Gain mode
 * @imx662: pointer to imx662 device
 * @hcg: 0 = LCG, 1 = HCG
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx662_set_hcg_mode(struct imx662 *imx662, u32 hcg)
{
	int ret;

	ret = imx662_write_reg(imx662, IMX662_REG_HCG, 1, hcg);
	if (ret) {
		dev_err(imx662->dev, "Failed to write HCG register: %d\n", ret);
		return ret;
	}

	if (imx662->cur_mode->dol >= 2) {
		ret = imx662_write_reg(imx662, IMX662_REG_HCG_SEF1, 1, hcg);
		if (ret) {
			imx662_write_reg(imx662, IMX662_REG_HCG, 1, !hcg);
			dev_err(imx662->dev, "Failed to write HCG SEF1 register: %d\n", ret);
			return ret;
		}
	}

	dev_dbg(imx662->dev, "HCG mode set to %s, dol=%d\n",
		hcg ? "enabled" : "disabled", imx662->cur_mode->dol);

	return 0;
}

static int imx662_set_hcg_lef(struct imx662 *imx662, u32 hcg)
{
	return imx662_write_reg(imx662, IMX662_REG_HCG, 1, hcg);
}

static int imx662_set_hcg_sef1(struct imx662 *imx662, u32 hcg)
{
	return imx662_write_reg(imx662, IMX662_REG_HCG_SEF1, 1, hcg);
}

static void imx662_set_mode(struct imx662 *imx662, const struct imx662_mode *mode)
{
	int ret;

	imx662->cur_mode = mode;
	imx662->vblank = mode->vblank;

	if (imx662->link_freq_ctrl) {
		ret = __v4l2_ctrl_s_ctrl(imx662->link_freq_ctrl, mode->link_freq_idx);
		if (ret)
			dev_err(imx662->dev, "Failed to set link freq index to %d.", mode->link_freq_idx);
	}
	if (imx662->pclk_ctrl) {
		ret = __v4l2_ctrl_s_ctrl_int64(imx662->pclk_ctrl, mode->pclk);
		if (ret)
			dev_err(imx662->dev, "Failed to set pixel rate to %lld.", mode->pclk);
	}

	if (imx662->hdr_enabled) {
		if (mode->dol <= 1)
			dev_err(imx662->dev, "Set to invalid HDR mode with DOL %d", mode->dol);
	} else {
		if (mode->dol > 1)
			dev_err(imx662->dev, "Set to invalid SDR mode with DOL %d", mode->dol);
	}
}

static void imx662_set_exp_activity(struct imx662 *imx662)
{
	int dol = imx662->cur_mode->dol;
	bool sef1 = dol >= 2;

	v4l2_ctrl_activate(imx662->sef1.again_ctrl, sef1);
	v4l2_ctrl_activate(imx662->sef1.exp_ctrl, sef1);
	v4l2_ctrl_activate(imx662->hcg_sef1_ctrl, sef1);
}

static int imx662_update_exp_vblank_controls(struct imx662 *imx662)
{
	struct imx662_exp_limits limits;
	const struct imx662_mode *mode = imx662->cur_mode;
	int ret;

	memset(&limits, 0, sizeof(struct imx662_exp_limits));
	imx662_calculate_exposure_limits(imx662, &limits);

	ret = __v4l2_ctrl_modify_range(imx662->lef.exp_ctrl,
		limits.exp_lef_min, limits.exp_lef_max,
		IMX662_EXPOSURE_STEP, limits.exp_lef_default);
	if (ret) {
		dev_err(imx662->dev, "Failed to modify LEF exposure range\n");
		return ret;
	}

	if (imx662->cur_mode->dol >= 2) {
		ret = __v4l2_ctrl_modify_range(imx662->sef1.exp_ctrl,
			limits.exp_sef1_min, limits.exp_sef1_max,
			IMX662_EXPOSURE_SHORT_STEP, limits.exp_sef1_default);
		if (ret) {
			dev_err(imx662->dev, "Failed to modify SEF1 exposure range\n");
			return ret;
		}
	}

	ret = __v4l2_ctrl_modify_range(imx662->vblank_ctrl,
		mode->vblank_min, mode->vblank_max, 1, imx662->vblank);
	if (ret) {
		dev_err(imx662->dev, "Failed to modify vblank range\n");
		return ret;
	}

	ret = __v4l2_ctrl_s_ctrl(imx662->vblank_ctrl, imx662->vblank);
	if (ret) {
		dev_err(imx662->dev, "Failed to set vblank to %d. ret=%d", imx662->vblank, ret);
		return ret;
	}

	return 0;
}

static int imx662_set_hdr_mode(struct imx662 *imx662, bool enable)
{
	const struct imx662_mode *prev_mode = NULL;
	int ret, revert_ret;

	ret = 0;
	if (imx662->hdr_enabled != enable) {
		imx662->hdr_enabled = enable;
		prev_mode = imx662->cur_mode;

		imx662_set_mode(imx662, imx662->hdr_enabled ?
				&supported_hdr_modes[0] :
				&supported_sdr_modes[0]);

		ret = imx662_update_exp_vblank_controls(imx662);
		if (ret) {
			dev_warn(imx662->dev, "Failed to update exp controls, reverting\n");

			imx662->hdr_enabled = !imx662->hdr_enabled;
			imx662_set_mode(imx662, prev_mode);

			revert_ret = imx662_update_exp_vblank_controls(imx662);
			if (revert_ret)
				dev_err(imx662->dev, "Failed to revert to previous mode\n");
		}

		dev_dbg(imx662->dev, "Set HDR mode to %d", imx662->hdr_enabled);
		imx662_set_exp_activity(imx662);
	}

	return ret;
}

static int imx662_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx662 *imx662 = container_of(ctrl->handler, struct imx662, ctrl_handler);
	struct i2c_client *client = imx662->client;
	u32 exposure, analog_gain, lpfr, max_lpfr;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		imx662->vblank = ctrl->val;
		max_lpfr = (imx662->cur_mode->vblank_max + imx662->cur_mode->height) * imx662->cur_mode->dol;
		lpfr = (imx662->vblank + imx662->cur_mode->height) * imx662->cur_mode->dol;

		dev_dbg(imx662->dev, "Received vblank %u, new lpfr %u",
			imx662->vblank, lpfr);
		ret = __v4l2_ctrl_modify_range(
			imx662->lef.exp_ctrl, IMX662_2DOL_SHR0_FSC_GAP,
			max_lpfr - imx662->cur_mode->rhs1 - IMX662_2DOL_SHR0_RHS1_GAP,
			IMX662_EXPOSURE_STEP, lpfr - imx662->cur_mode->rhs1 - IMX662_2DOL_SHR0_RHS1_GAP);
		break;

	case V4L2_CID_EXPOSURE:
		if (!pm_runtime_get_if_in_use(&client->dev))
			return 0;

		exposure = ctrl->val;
		analog_gain = imx662->lef.again_ctrl->val;

		dev_dbg(&client->dev, "Set LEF exp=%u gain=%u", exposure, analog_gain);
		ret = imx662_update_exp_gain(imx662, exposure, analog_gain, LEF);

		pm_runtime_put(&client->dev);
		break;

	case IMX662_CID_EXPOSURE_SHORT:
		if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
			return 0;

		if (!pm_runtime_get_if_in_use(&client->dev))
			return 0;

		exposure = ctrl->val;
		analog_gain = imx662->sef1.again_ctrl->val;

		dev_dbg(&client->dev, "Set SEF1 exp=%u gain=%u", exposure, analog_gain);
		ret = imx662_update_exp_gain(imx662, exposure, analog_gain, SEF1);

		pm_runtime_put(&client->dev);
		break;

	case V4L2_CID_TEST_PATTERN:
		if (!pm_runtime_get_if_in_use(&client->dev))
			return 0;
		ret = imx662_set_test_pattern(imx662, ctrl->val);
		pm_runtime_put(&client->dev);
		break;

	case IMX662_CID_HCG:
		/* Global HCG: sync per-exposure cached values unconditionally,
		 * write registers only if sensor is powered on */
		/* Controls are independent (no cluster) so direct cur.val update is safe */
		imx662->hcg_lef_ctrl->cur.val = ctrl->val;
		if (imx662->cur_mode->dol >= 2)
			imx662->hcg_sef1_ctrl->cur.val = ctrl->val;

		if (!pm_runtime_get_if_in_use(&client->dev))
			return 0;

		dev_dbg(&client->dev, "Setting HCG (global) to %u\n", ctrl->val);
		ret = imx662_set_hcg_mode(imx662, ctrl->val);
		if (ret)
			dev_err(&client->dev, "Failed to set HCG mode: %d\n", ret);
		pm_runtime_put(&client->dev);
		break;

	case IMX662_CID_HCG_LEF:
		if (!pm_runtime_get_if_in_use(&client->dev))
			return 0;
		dev_dbg(&client->dev, "Setting HCG LEF to %u\n", ctrl->val);
		ret = imx662_set_hcg_lef(imx662, ctrl->val);
		pm_runtime_put(&client->dev);
		break;

	case IMX662_CID_HCG_SEF1:
		if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
			return 0;
		if (!pm_runtime_get_if_in_use(&client->dev))
			return 0;
		dev_dbg(&client->dev, "Setting HCG SEF1 to %u\n", ctrl->val);
		ret = imx662_set_hcg_sef1(imx662, ctrl->val);
		pm_runtime_put(&client->dev);
		break;

	case V4L2_CID_WIDE_DYNAMIC_RANGE:
		if (imx662->fast_toggle_state > FAST_TOGGLE_NONE && imx662->fast_toggle_state < FAST_TOGGLE_STATE_MAX) {
			/* If currently toggling, ignore this v4l control */
			return 0;
		}

		if (imx662->streaming) {
			dev_warn(&client->dev,
				"Cannot set WDR mode while streaming\n");
			return -EBUSY;
		}

		ret = imx662_set_hdr_mode(imx662, ctrl->val);
		break;

	case IMX662_CID_WDR_PRIMING:
		imx662->wdr_priming_val = ctrl->val;
		ret = 0;
		break;

	case IMX662_CID_CUSTOM_RHS1_PRIMING:
		imx662->custom_rhs1_priming_val = ctrl->val;
		ret = 0;
		break;

	case IMX662_CID_CUSTOM_RHS1:
		/* Stub: control accepted but not implemented */
		ret = 0;
		break;

	case V4L2_CID_LINK_FREQ:
	case V4L2_CID_PIXEL_RATE:
		ret = 0;
		break;

	default:
		dev_dbg(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
				__func__, ctrl->id, ctrl->val);
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops imx662_ctrl_ops = {
	.s_ctrl = imx662_set_ctrl,
};

static int imx662_get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx662 *imx662 = container_of(ctrl->handler, struct imx662, ctrl_handler);
	u16 reg = 0;
	u32 len = 0;
	int ret = 0;

	switch (ctrl->id) {
	case IMX662_CID_RHS1:
		ctrl->val = imx662->cur_mode->rhs1;
		break;
	case IMX662_CID_RHS2:
		ctrl->val = imx662->cur_mode->rhs2;
		break;
	case IMX662_CID_SHR0:
		reg = IMX662_SHR0_LOW;
		len = 3;
		break;
	case IMX662_CID_SHR1:
		reg = IMX662_SHR1_LOW;
		len = 3;
		break;
	case IMX662_CID_SHR2:
		reg = IMX662_SHR2_LOW;
		len = 3;
		break;
	case IMX662_CID_VMAX:
		if (imx662->streaming) {
			reg = IMX662_REG_LPFR;
			len = 3;
		} else {
			ctrl->val = (imx662->vblank + imx662->cur_mode->height);
		}
		break;
	case IMX662_CID_HMAX:
		reg = IMX662_REG_HMAX_LOW;
		len = 2;
		break;
	default:
		dev_err(imx662->dev, "Invalid get_ctrl id %d", ctrl->id);
		return -EINVAL;
	}

	if (reg && len) {
		if (!imx662->streaming) {
			dev_warn(imx662->dev, "Cannot read register 0x%x while not streaming\n", reg);
			return -EBUSY;
		}

		ret = imx662_read_reg(imx662, reg, len, (u32 *)&ctrl->val);
		if (ret)
			dev_err(imx662->dev, "Failed to read register 0x%x", reg);
	}

	return ret;
}

static const struct v4l2_ctrl_ops imx662_get_ctrl_ops = {
	.g_volatile_ctrl = imx662_get_ctrl,
};

static int imx662_enum_mbus_code(struct v4l2_subdev *sd,
					struct v4l2_subdev_state *state,
					struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx662 *imx662 = to_imx662(sd);
	const struct imx662_mode *modes;

	if (code->index > 0)
		return -EINVAL;

	modes = imx662->hdr_enabled ? supported_hdr_modes : supported_sdr_modes;

	mutex_lock(&imx662->mutex);
	code->code = modes[0].code;
	mutex_unlock(&imx662->mutex);
	return 0;
}

static int imx662_enum_frame_size(struct v4l2_subdev *sd,
					struct v4l2_subdev_state *state,
					struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx662 *imx662 = to_imx662(sd);
	const struct imx662_mode *modes;
	int mode_count;

	if (fse->index > 0)
		return -EINVAL;

	if (imx662->hdr_enabled) {
		modes = supported_hdr_modes;
		mode_count = ARRAY_SIZE(supported_hdr_modes);
	} else {
		modes = supported_sdr_modes;
		mode_count = ARRAY_SIZE(supported_sdr_modes);
	}

	if (fse->code != modes[0].code)
		return -EINVAL;

	fse->min_width = modes[0].width;
	fse->max_width = fse->min_width;
	fse->min_height = modes[0].height;
	fse->max_height = fse->min_height;

	return 0;
}

static int imx662_get_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_format *fmt)
{
	struct imx662 *imx662 = to_imx662(sd);
	const struct imx662_mode *mode = imx662->cur_mode ?: &supported_sdr_modes[0];

	mutex_lock(&imx662->mutex);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = mode->code;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;
	fmt->format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->format.quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->format.xfer_func = V4L2_XFER_FUNC_NONE;

	mutex_unlock(&imx662->mutex);

	return 0;
}

static int imx662_set_fmt(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_format *fmt)
{
	struct imx662 *imx662 = to_imx662(sd);

	/* Select mode based on current HDR state */
	const struct imx662_mode *mode = imx662->hdr_enabled ?
		&supported_hdr_modes[0] : &supported_sdr_modes[0];

	mutex_lock(&imx662->mutex);

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = mode->code;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;
	fmt->format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->format.quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->format.xfer_func = V4L2_XFER_FUNC_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_get_try_format(sd, state, fmt->pad) = fmt->format;
	} else {
		imx662->cur_mode = mode;
	}

	mutex_unlock(&imx662->mutex);

	return 0;
}

static int imx662_init_cfg(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = 0,
	};

	return imx662_set_fmt(sd, state, &fmt);
}

static const struct v4l2_subdev_pad_ops imx662_pad_ops = {
	.init_cfg = imx662_init_cfg,
	.enum_mbus_code = imx662_enum_mbus_code,
	.enum_frame_size = imx662_enum_frame_size,
	.get_fmt = imx662_get_fmt,
	.set_fmt = imx662_set_fmt,
};

/* toggle_type param might be useful in the future, we don't need it now though */
static int imx662_priming_apply(struct imx662 *imx662, int toggle_type)
{
	int ret;

	ret = imx662_set_hdr_mode(imx662, imx662->wdr_priming_val);
	if (ret) {
		dev_err(imx662->dev, "Failed to set HDR mode (%d) for priming: %d", imx662->wdr_priming_val, ret);
		return ret;
	}

	/* Custom RHS1 priming apply (if it's -1, it means no custom value was set, so skip) */
	/* Note: custom RHS1 write not yet implemented for this sensor */

	return 0;
}

static int imx662_fast_toggle_set_state(struct imx662 *imx662, int toggle_state)
{
	if (toggle_state < 0 || toggle_state >= FAST_TOGGLE_STATE_MAX) {
		dev_err(imx662->dev, "Invalid fast toggle state %d\n", toggle_state);
		return -EINVAL;
	}

	imx662->fast_toggle_state = toggle_state;

	switch (toggle_state) {
	case FAST_TOGGLE_APPLY_PRIMING:
		return imx662_priming_apply(imx662, toggle_state);
	default:
		/* No action needed for other states */
		break;
	}

	return 0;
}

static long imx662_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct imx662 *imx662 = to_imx662(sd);
	int toggle_state;
	long ret;

	mutex_lock(&imx662->mutex);
	switch (cmd) {
	case HAILO15_INTERNAL_SENSOR_FAST_TOGGLE_SET_STATUS:
		if (arg == NULL) {
			ret = -EINVAL;
			break;
		}
		toggle_state = *((int *)arg);
		ret = imx662_fast_toggle_set_state(imx662, toggle_state);
		break;
	default:
		ret = -ENOTTY;
	}
	mutex_unlock(&imx662->mutex);
	return ret;
}

static const struct v4l2_subdev_core_ops imx662_core_ops = {
	.ioctl = imx662_ioctl,
};

static const struct v4l2_subdev_ops imx662_subdev_ops = {
	.core = &imx662_core_ops,
	.video = &imx662_video_ops,
	.pad = &imx662_pad_ops,
};

static int imx662_parse_hw_config(struct imx662 *sensor)
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
	if (rate != IMX662_INCLK_RATE) {
		dev_err(sensor->dev,
			"inclk mismatch: got %lu, expected %u\n",
			rate, IMX662_INCLK_RATE);
		return -EINVAL;
	}

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -ENXIO;

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &ep_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	/* Validate lane count: accept 2 (e.g. H15L SBC CSI1, silicon-shared
	 * lanes) or 4 (default). The mode register init tables hardcode
	 * LANEMODE for 4 lanes; if 2-lane, imx662_start_streaming() patches
	 * register 0x3040 to IMX662_LANEMODE_2LANE after the table is written.
	 */
	if (ep_cfg.bus.mipi_csi2.num_data_lanes != IMX662_LANES_DEFAULT &&
	    ep_cfg.bus.mipi_csi2.num_data_lanes != IMX662_LANES_2) {
		dev_err(sensor->dev,
			"unsupported lane count %u (expected 2 or 4)\n",
			ep_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto out_free;
	}
	sensor->lanes = ep_cfg.bus.mipi_csi2.num_data_lanes;

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

static int imx662_init_controls(struct imx662 *imx662)
{
	struct v4l2_ctrl_handler *ctrl_hdlr = &imx662->ctrl_handler;
	const struct imx662_mode *mode = imx662->cur_mode;
	struct imx662_exp_limits limits;
	int ret;

	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 28);
	if (ret) {
		dev_err(imx662->dev, "failed to init control handler (%d)", ret);
		return ret;
	}

	ctrl_hdlr->lock = &imx662->mutex;

	/* Initialize vblank from mode default */
	imx662->vblank = mode->vblank;

	/* Calculate initial exposure limits */
	memset(&limits, 0, sizeof(struct imx662_exp_limits));
	imx662_calculate_exposure_limits(imx662, &limits);

	/* LEF exposure control */
	imx662->lef.exp_ctrl = v4l2_ctrl_new_std(
		ctrl_hdlr, &imx662_ctrl_ops, V4L2_CID_EXPOSURE,
		limits.exp_lef_min,
		limits.exp_lef_max,
		IMX662_EXPOSURE_STEP,
		limits.exp_lef_default);

	/* LEF analog gain control */
	imx662->lef.again_ctrl = v4l2_ctrl_new_std(
		ctrl_hdlr, &imx662_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
		IMX662_MIN_GAIN,
		IMX662_MAX_GAIN,
		IMX662_GAIN_STEP,
		IMX662_GAIN_DEFAULT);

	/* Cluster LEF exposure and gain */
	v4l2_ctrl_cluster(2, &imx662->lef.exp_ctrl);

	/* SEF1 exposure control (custom) */
	{
		struct v4l2_ctrl_config sef1_exp_cfg = {
			.ops = &imx662_ctrl_ops,
			.id = IMX662_CID_EXPOSURE_SHORT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_UPDATE,
			.name = "exposure_short",
			.step = IMX662_EXPOSURE_SHORT_STEP,
			.min = limits.exp_sef1_min,
			.max = limits.exp_sef1_max,
			.def = limits.exp_sef1_default,
		};
		imx662->sef1.exp_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &sef1_exp_cfg, NULL);
	}

	/* SEF1 analog gain control (custom) */
	{
		struct v4l2_ctrl_config sef1_gain_cfg = {
			.ops = &imx662_ctrl_ops,
			.id = IMX662_CID_ANALOGUE_GAIN_SHORT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_UPDATE,
			.name = "analogue_gain_short",
			.step = IMX662_GAIN_STEP,
			.min = IMX662_MIN_GAIN,
			.max = IMX662_MAX_GAIN,
			.def = IMX662_GAIN_DEFAULT,
		};
		imx662->sef1.again_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &sef1_gain_cfg, NULL);
	}

	/* Cluster SEF1 exposure and gain */
	v4l2_ctrl_cluster(2, &imx662->sef1.exp_ctrl);

	/* HCG control */
	{
		struct v4l2_ctrl_config hcg_cfg = {
			.ops = &imx662_ctrl_ops,
			.id = IMX662_CID_HCG,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.flags = V4L2_CTRL_FLAG_UPDATE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
			.name = "hcg",
			.step = IMX662_HCG_STEP,
			.min = IMX662_HCG_MIN,
			.max = IMX662_HCG_MAX,
			.def = IMX662_HCG_DEFAULT,
		};
		imx662->hcg_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hcg_cfg, NULL);
	}

	/* Per-exposure HCG LEF control */
	{
		struct v4l2_ctrl_config hcg_lef_cfg = {
			.ops = &imx662_ctrl_ops,
			.id = IMX662_CID_HCG_LEF,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.flags = V4L2_CTRL_FLAG_UPDATE,
			.name = "hcg_lef",
			.step = IMX662_HCG_STEP,
			.min = IMX662_HCG_MIN,
			.max = IMX662_HCG_MAX,
			.def = IMX662_HCG_DEFAULT,
		};
		imx662->hcg_lef_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hcg_lef_cfg, NULL);
	}

	/* Per-exposure HCG SEF1 control */
	{
		struct v4l2_ctrl_config hcg_sef1_cfg = {
			.ops = &imx662_ctrl_ops,
			.id = IMX662_CID_HCG_SEF1,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.flags = V4L2_CTRL_FLAG_UPDATE,
			.name = "hcg_sef1",
			.step = IMX662_HCG_STEP,
			.min = IMX662_HCG_MIN,
			.max = IMX662_HCG_MAX,
			.def = IMX662_HCG_DEFAULT,
		};
		imx662->hcg_sef1_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hcg_sef1_cfg, NULL);
	}



	/* Custom RHS1 stub */
	{
		struct v4l2_ctrl_config custom_rhs1_cfg = {
			.ops = &imx662_ctrl_ops,
			.id = IMX662_CID_CUSTOM_RHS1,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_UPDATE,
			.name = "custom_rhs1",
			.step = 1,
			.min = 0,
			.max = 65535,
			.def = 0,
		};
		imx662->custom_rhs1_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &custom_rhs1_cfg, NULL);
	}

	/* Custom RHS1 priming control */
	{
		struct v4l2_ctrl_config rhs1_priming_cfg = {
			.ops = &imx662_ctrl_ops,
			.id = IMX662_CID_CUSTOM_RHS1_PRIMING,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_UPDATE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
			.name = "custom_rhs1_priming",
			.step = 1,
			.min = IMX662_CUSTOM_RHS1_PRIMING_MIN,
			.max = IMX662_CUSTOM_RHS1_PRIMING_MAX,
			.def = IMX662_CUSTOM_RHS1_PRIMING_DEF,
		};
		imx662->custom_rhs1_priming_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &rhs1_priming_cfg, NULL);
	}

	/* WDR priming control */
	{
		struct v4l2_ctrl_config wdr_priming_cfg = {
			.ops = &imx662_ctrl_ops,
			.id = IMX662_CID_WDR_PRIMING,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.flags = V4L2_CTRL_FLAG_UPDATE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
			.name = "wdr_priming",
			.step = 1,
			.min = 0,
			.max = 1,
			.def = 0,
		};
		imx662->wdr_priming_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &wdr_priming_cfg, NULL);
	}

	/* Read-only timing readback controls */
	{
		struct v4l2_ctrl_config rhs1_ro_cfg = {
			.ops = &imx662_get_ctrl_ops,
			.id = IMX662_CID_RHS1,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
			.name = "readout_timing_short",
			.step = 1,
			.min = 0,
			.max = IMX662_VMAX_MAX,
			.def = 0,
		};
		imx662->rhs1_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &rhs1_ro_cfg, NULL);
	}
	{
		struct v4l2_ctrl_config rhs2_ro_cfg = {
			.ops = &imx662_get_ctrl_ops,
			.id = IMX662_CID_RHS2,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
			.name = "readout_timing_very_short",
			.step = 1,
			.min = 0,
			.max = IMX662_VMAX_MAX,
			.def = 0,
		};
		imx662->rhs2_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &rhs2_ro_cfg, NULL);
	}
	{
		struct v4l2_ctrl_config shr0_ro_cfg = {
			.ops = &imx662_get_ctrl_ops,
			.id = IMX662_CID_SHR0,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
			.name = "shutter_timing_long",
			.step = 1,
			.min = 0,
			.max = IMX662_VMAX_MAX,
			.def = 0,
		};
		imx662->shr0_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &shr0_ro_cfg, NULL);
	}
	{
		struct v4l2_ctrl_config shr1_ro_cfg = {
			.ops = &imx662_get_ctrl_ops,
			.id = IMX662_CID_SHR1,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
			.name = "shutter_timing_short",
			.step = IMX662_EXPOSURE_SHORT_STEP,
			.min = 0,
			.max = IMX662_VMAX_MAX,
			.def = 0,
		};
		imx662->shr1_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &shr1_ro_cfg, NULL);
	}
	{
		struct v4l2_ctrl_config shr2_ro_cfg = {
			.ops = &imx662_get_ctrl_ops,
			.id = IMX662_CID_SHR2,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
			.name = "shutter_timing_very_short",
			.step = 1,
			.min = 0,
			.max = IMX662_VMAX_MAX,
			.def = 0,
		};
		imx662->shr2_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &shr2_ro_cfg, NULL);
	}
	{
		struct v4l2_ctrl_config vmax_ro_cfg = {
			.ops = &imx662_get_ctrl_ops,
			.id = IMX662_CID_VMAX,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
			.name = "vertical_span",
			.step = 1,
			.min = 0,
			.max = IMX662_VMAX_MAX,
			.def = 0,
		};
		imx662->vmax_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &vmax_ro_cfg, NULL);
	}
	{
		struct v4l2_ctrl_config hmax_ro_cfg = {
			.ops = &imx662_get_ctrl_ops,
			.id = IMX662_CID_HMAX,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
			.name = "horizontal_span",
			.step = 1,
			.min = 0,
			.max = 0xFFFF,
			.def = 0,
		};
		imx662->hmax_ctrl = v4l2_ctrl_new_custom(ctrl_hdlr, &hmax_ro_cfg, NULL);
	}

	/* Vertical blanking control */
	imx662->vblank_ctrl = v4l2_ctrl_new_std(
		ctrl_hdlr, &imx662_ctrl_ops, V4L2_CID_VBLANK,
		mode->vblank_min,
		mode->vblank_max,
		1,
		mode->vblank);

	/* Horizontal blanking (read-only) */
	imx662->hblank_ctrl = v4l2_ctrl_new_std(
		ctrl_hdlr, NULL, V4L2_CID_HBLANK,
		IMX662_HMAX_DEFAULT - IMX662_WIDTH,
		IMX662_HMAX_DEFAULT - IMX662_WIDTH,
		1, IMX662_HMAX_DEFAULT - IMX662_WIDTH);
	if (imx662->hblank_ctrl)
		imx662->hblank_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx662->test_pattern = v4l2_ctrl_new_std_menu_items(ctrl_hdlr,
				&imx662_ctrl_ops, V4L2_CID_TEST_PATTERN,
				ARRAY_SIZE(imx662_test_pattern_menu) - 1,
				0, 0, imx662_test_pattern_menu);

	/* Wide Dynamic Range control (SDR/HDR mode) */
	imx662->mode_sel_ctrl = v4l2_ctrl_new_std(ctrl_hdlr, &imx662_ctrl_ops,
				V4L2_CID_WIDE_DYNAMIC_RANGE, IMX662_WDR_MIN,
				IMX662_WDR_MAX, IMX662_WDR_STEP,
				IMX662_WDR_DEFAULT);
	if (imx662->mode_sel_ctrl)
		imx662->mode_sel_ctrl->flags |= V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	/* pixel_rate from pre-computed mode value */
	imx662->pclk_ctrl = v4l2_ctrl_new_std(
		ctrl_hdlr, NULL, V4L2_CID_PIXEL_RATE,
		mode->pclk, mode->pclk, 1, mode->pclk);
	if (imx662->pclk_ctrl)
		imx662->pclk_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* Link frequency (read-only) */
	imx662->link_freq_ctrl = v4l2_ctrl_new_int_menu(
		ctrl_hdlr, NULL, V4L2_CID_LINK_FREQ,
		ARRAY_SIZE(link_freq) - 1,
		mode->link_freq_idx,
		link_freq);
	if (imx662->link_freq_ctrl)
		imx662->link_freq_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(imx662->dev, "Control init failed: %d\n", ret);
		v4l2_ctrl_handler_free(ctrl_hdlr);
		return ret;
	}

	/* Initially deactivate SEF1 controls (SDR mode) */
	imx662_set_exp_activity(imx662);

	imx662->sd.ctrl_handler = ctrl_hdlr;
	return 0;
}

static int imx662_probe(struct i2c_client *client)
{
	struct imx662 *imx662;
	int ret;  

	imx662 = devm_kzalloc(&client->dev, sizeof(*imx662), GFP_KERNEL);
	if (!imx662)
		return -ENOMEM;

	imx662->dev = &client->dev;
	dev_info(imx662->dev, "probe started");

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&imx662->sd, client, &imx662_subdev_ops);

	ret = imx662_parse_hw_config(imx662);
	if (ret) {
		dev_err(imx662->dev, "HW configuration invalid\n");
		return ret;
	}

	imx662->client = client;

	mutex_init(&imx662->mutex);

	/* Power on and detect sensor */
	ret = imx662_power_on(imx662->dev);
	if (ret) {
		dev_err(imx662->dev, "Failed to power on\n");
		goto error_mutex;
	}

	/* Detect sensor id */
	ret = imx662_detect(imx662);
	if (ret) {
		dev_err(imx662->dev, "sensor detection failed: %d\n", ret);
		goto error_power;
	}

	/* Set default mode to FHD SDR */
	imx662->cur_mode = &supported_sdr_modes[0];

	/* Initialize controls */
	ret = imx662_init_controls(imx662);
	if (ret) {
		dev_err(imx662->dev, "failed to init controls: %d", ret);
		goto error_power;
	}

	/* Setup media pad */
	imx662->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	imx662->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	imx662->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&imx662->sd.entity, 1, &imx662->pad);
	if (ret) {
		dev_err(imx662->dev, "Failed to init media pads\n");
		goto error_ctrl;
	}

	ret = v4l2_async_register_subdev_sensor(&imx662->sd);
	if (ret) {
		dev_err(imx662->dev, "Failed to register subdev\n");
		goto error_media;
	}

	pm_runtime_set_active(imx662->dev);
	pm_runtime_enable(imx662->dev);
	pm_runtime_idle(imx662->dev);

	dev_info(imx662->dev, "probe finished successfully");
	return 0;

error_media:
	media_entity_cleanup(&imx662->sd.entity);
error_ctrl:
	v4l2_ctrl_handler_free(&imx662->ctrl_handler);
error_power:
	imx662_power_off(imx662->dev);
error_mutex:
	mutex_destroy(&imx662->mutex);

	return ret;
}

static int imx662_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx662 *sensor = to_imx662(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx662_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	mutex_destroy(&sensor->mutex);

	return 0;
}

static const struct dev_pm_ops imx662_pm_ops = {
	SET_RUNTIME_PM_OPS(imx662_power_off, imx662_power_on, NULL)
};

static const struct of_device_id imx662_of_match[] = {
	{.compatible = "sony,imx662"},
	{},
};

MODULE_DEVICE_TABLE(of, imx662_of_match);

static struct i2c_driver imx662_driver = {
	.probe_new = imx662_probe,
	.remove = imx662_remove,
	.driver = {
		.name = "imx662",
		.pm = &imx662_pm_ops,
		.of_match_table = imx662_of_match,
	},
};

module_i2c_driver(imx662_driver);

MODULE_AUTHOR("Daniel Varennikov <danielv@hailo.ai>");
MODULE_DESCRIPTION("Sony imx662 sensor driver");
MODULE_LICENSE("GPL");