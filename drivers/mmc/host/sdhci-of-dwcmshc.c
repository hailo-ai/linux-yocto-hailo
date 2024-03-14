// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Synopsys DesignWare Cores Mobile Storage Host Controller
 *
 * Copyright (C) 2018 Synaptics Incorporated
 *
 * Author: Jisheng Zhang <jszhang@kernel.org>
 */

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/sizes.h>
#include <linux/of_reserved_mem.h>
#include <linux/reset.h>
#include "sdhci-pltfm.h"
#include <linux/gpio/consumer.h>
#include <linux/delay.h>

#define DWCMSHC_EMMC_CTRL_R  			0x0000052C
#define DWCMSHC_EMMC_CTRL_R__CARD_IS_EMMC            BIT(0)
#define DWCMSHC_EMMC_CTRL_R__DISABLE_DATA_CRC_CHK    BIT(1)
#define DWCMSHC_EMMC_CTRL_R__EMMC_RST_N		     BIT(2)
#define DWCMSHC_EMMC_CTRL_R__EMMC_RST_N_OE	     BIT(3)

#define DWCMSHC_CMDPAD_CNFG  			0x00000304 
#define DWCMSHC_CMDPAD_CNFG__RXSEL GENMASK(2, 0)
#define DWCMSHC_CMDPAD_CNFG__WEAKPULL_EN GENMASK(4, 3)
#define DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_P GENMASK(8,5)
#define DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_N GENMASK(12,9)

#define DWCMSHC_DATPAD_CNFG 			0x00000306 
#define DWCMSHC_DATPAD_CNFG__RXSEL GENMASK(2, 0)
#define DWCMSHC_DATPAD_CNFG__WEAKPULL_EN GENMASK (4, 3)
#define DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_P GENMASK(8,5)
#define DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_N GENMASK(12,9)

#define DWCMSHC_RSTNPAD_CNFG			0x0000030C 
#define DWCMSHC_RSTNPAD_CNFG__RXSEL GENMASK(2, 0)
#define DWCMSHC_RSTNPAD_CNFG__WEAKPULL_EN GENMASK (4, 3)
#define DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_P GENMASK(8,5)
#define DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_N GENMASK(12,9)

#define DWCMSHC_CLKPAD_CNFG			0x00000308 
#define DWCMSHC_CLKPAD_CNFG__RXSEL GENMASK(2, 0)
#define DWCMSHC_CLKPAD_CNFG__WEAKPULL_EN GENMASK(4, 3)
#define DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_P GENMASK(8,5)
#define DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_N GENMASK(12,9)

#define DWCMSHC_PHY_CNFG			0x00000300
#define DWCMSHC_PHY_CNFG__PHY_RSTN BIT(0)
#define DWCMSHC_PHY_CNFG__PHY_PWRGOOD BIT(1)
#define DWCMSHC_PHY_CNFG__PAD_SP GENMASK(19,16)
#define DWCMSHC_PHY_CNFG__PAD_SN GENMASK(23,20)

#define DWCMSHC_CLK_CTRL_R			0x0000002C
#define DWCMSHC_CLK_CTRL_R__PLL_ENABLE	BIT(3)

#define DWCMSHC_SDCLKDL_CNFG			0x0000031D
#define DWCMSHC_SDCLKDL_CNFG__EXTDLY_EN BIT(0)

#define DWCMSHC_SDCLKDL_DC			0x0000031E
#define DWCMSHC_SDCLKDL_DC__CCKDL_DC GENMASK(6,0)

#define DWCMSHC_BLOCKSIZE_R			0x00000004
#define DWCMSHC_BLOCKSIZE_R__XFER_BLOCK_SIZE GENMASK(11,0)

#define DWCMSHC_BLOCKCOUNT_R			0x00000006

#define DWCMSHC_XFER_MODE_R			0x0000000C
#define DWCMSHC_XFER_MODE_R__BLOCK_COUNT_ENABLE BIT(1)
#define DWCMSHC_XFER_MODE_R__DATA_XFER_DIR BIT(4)
#define DWCMSHC_XFER_MODE_R__MULTI_BLK_SEL BIT(5)

#define DWCMSHC_AT_CTRL_R			0x00000540
#define DWCMSHC_AT_CTRL_R__SWIN_TH_EN BIT(2)
#define DWCMSHC_AT_CTRL_R__SWIN_TH_VAL GENMASK(30, 24)


#define SDHCI_DWCMSHC_ARG2_STUFF	GENMASK(31, 16)

/* DWCMSHC specific Mode Select value */
#define DWCMSHC_CTRL_HS400		0x7

/* DWC IP vendor area 1 pointer */
#define DWCMSHC_P_VENDOR_AREA1		0xe8
#define DWCMSHC_AREA1_MASK		GENMASK(11, 0)
/* Offset inside the  vendor area 1 */
#define DWCMSHC_HOST_CTRL3		0x8
#define DWCMSHC_EMMC_CONTROL		0x2c
#define DWCMSHC_ENHANCED_STROBE		BIT(8)
#define DWCMSHC_EMMC_ATCTRL		0x40

/* Rockchip specific Registers */
#define DWCMSHC_EMMC_DLL_CTRL		0x800
#define DWCMSHC_EMMC_DLL_RXCLK		0x804
#define DWCMSHC_EMMC_DLL_TXCLK		0x808
#define DWCMSHC_EMMC_DLL_STRBIN		0x80c
#define DLL_STRBIN_TAPNUM_FROM_SW	BIT(24)
#define DWCMSHC_EMMC_DLL_STATUS0	0x840
#define DWCMSHC_EMMC_DLL_START		BIT(0)
#define DWCMSHC_EMMC_DLL_LOCKED		BIT(8)
#define DWCMSHC_EMMC_DLL_TIMEOUT	BIT(9)
#define DWCMSHC_EMMC_DLL_RXCLK_SRCSEL	29
#define DWCMSHC_EMMC_DLL_START_POINT	16
#define DWCMSHC_EMMC_DLL_INC		8
#define DWCMSHC_EMMC_DLL_DLYENA		BIT(27)
#define DLL_TXCLK_TAPNUM_DEFAULT	0x8
#define DLL_STRBIN_TAPNUM_DEFAULT	0x8
#define DLL_TXCLK_TAPNUM_FROM_SW	BIT(24)
#define DLL_RXCLK_NO_INVERTER		1
#define DLL_RXCLK_INVERTER		0
#define DLL_LOCK_WO_TMOUT(x) \
	((((x) & DWCMSHC_EMMC_DLL_LOCKED) == DWCMSHC_EMMC_DLL_LOCKED) && \
	(((x) & DWCMSHC_EMMC_DLL_TIMEOUT) == 0))
#define RK3568_MAX_CLKS 3

#define BOUNDARY_OK(addr, len) \
	((addr | (SZ_128M - 1)) == ((addr + len - 1) | (SZ_128M - 1)))

struct rk3568_priv {
	/* Rockchip specified optional clocks */
	struct clk_bulk_data rockchip_clks[RK3568_MAX_CLKS];
	u8 txclk_tapnum;
};

enum pad_config {
	TXSLEW_CTRL_N = 0,
	TXSLEW_CTRL_P = 1,
	WEAKPULL_EN = 2,
	RXSEL = 3,
	PAD_CONFIG_MAX = 4
};
enum clk_delay_config {
	EXTDLY_EN = 0,
        CCKDL_DC = 1,
	CLK_DELAY_CONFIG_MAX = 2
};
enum drive_strength_config {
	PAD_SP = 0,
	PAD_SN = 1,
	DS_CONFIG_MAX = 2
};

typedef struct hailo15_phy_config {
	u32 card_is_emmc;
	u32 cmd_pad[PAD_CONFIG_MAX];
	u32 dat_pad[PAD_CONFIG_MAX];
	u32 rst_pad[PAD_CONFIG_MAX];
	u32 clk_pad[PAD_CONFIG_MAX];
	u32 clk_delay[CLK_DELAY_CONFIG_MAX];
	u32 drive_strength[DS_CONFIG_MAX];
} hailo15_phy_config;
struct hailo_priv {
	/* hailo specified optional clocks */
	struct clk *div_clk_bypass;
	bool is_clk_divider_bypass;
	hailo15_phy_config sdio_phy_config;
	u64 data_error_count;
	struct gpio_desc *vdd_gpio;
	bool is_sd_vsel_polarity_high;
};
struct dwcmshc_priv {
	struct clk	*bus_clk;
	int vendor_specific_area1; /* P_VENDOR_SPECIFIC_AREA reg */
	void *priv; /* pointer to SoC private stuff */
};
static void dwcmshc_hailo15_phy_config(struct sdhci_host *host);
/*
 * If DMA addr spans 128MB boundary, we split the DMA transfer into two
 * so that each DMA transfer doesn't exceed the boundary.
 */
static void dwcmshc_adma_write_desc(struct sdhci_host *host, void **desc,
				    dma_addr_t addr, int len, unsigned int cmd)
{
	int tmplen, offset;

	if (likely(!len || BOUNDARY_OK(addr, len))) {
		sdhci_adma_write_desc(host, desc, addr, len, cmd);
		return;
	}

	offset = addr & (SZ_128M - 1);
	tmplen = SZ_128M - offset;
	sdhci_adma_write_desc(host, desc, addr, tmplen, cmd);

	addr += tmplen;
	len -= tmplen;
	sdhci_adma_write_desc(host, desc, addr, len, cmd);
}

static unsigned int dwcmshc_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	if (pltfm_host->clk)
		return sdhci_pltfm_clk_get_max_clock(host);
	else
		return pltfm_host->clock;
}

static void dwcmshc_check_auto_cmd23(struct mmc_host *mmc,
				     struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);

	/*
	 * No matter V4 is enabled or not, ARGUMENT2 register is 32-bit
	 * block count register which doesn't support stuff bits of
	 * CMD23 argument on dwcmsch host controller.
	 */
	if (mrq->sbc && (mrq->sbc->arg & SDHCI_DWCMSHC_ARG2_STUFF))
		host->flags &= ~SDHCI_AUTO_CMD23;
	else
		host->flags |= SDHCI_AUTO_CMD23;
}

static void dwcmshc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	dwcmshc_check_auto_cmd23(mmc, mrq);

	sdhci_request(mmc, mrq);
}

static void dwcmshc_set_uhs_signaling(struct sdhci_host *host,
				      unsigned int timing)
{
	u16 ctrl_2;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	if ((timing == MMC_TIMING_MMC_HS200) ||
	    (timing == MMC_TIMING_UHS_SDR104))
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
	else if (timing == MMC_TIMING_UHS_SDR12)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
	else if ((timing == MMC_TIMING_UHS_SDR25) ||
		 (timing == MMC_TIMING_MMC_HS))
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
	else if (timing == MMC_TIMING_UHS_SDR50)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
	else if ((timing == MMC_TIMING_UHS_DDR50) ||
		 (timing == MMC_TIMING_MMC_DDR52))
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
	else if (timing == MMC_TIMING_MMC_HS400)
		ctrl_2 |= DWCMSHC_CTRL_HS400;
	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);
}

static void dwcmshc_hs400_enhanced_strobe(struct mmc_host *mmc,
					  struct mmc_ios *ios)
{
	u32 vendor;
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int reg = priv->vendor_specific_area1 + DWCMSHC_EMMC_CONTROL;

	vendor = sdhci_readl(host, reg);
	if (ios->enhanced_strobe)
		vendor |= DWCMSHC_ENHANCED_STROBE;
	else
		vendor &= ~DWCMSHC_ENHANCED_STROBE;

	sdhci_writel(host, vendor, reg);
}
static int dwcmshc_hailo15_set_clock_divider_bypass(struct sdhci_host *host, bool is_bypass)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct hailo_priv *priv = dwc_priv->priv;
	struct device *dev = mmc_dev(host->mmc);
	int ret = 0;

	if ((is_bypass) & (!priv->is_clk_divider_bypass)) {
		ret = clk_prepare_enable(priv->div_clk_bypass);
    		if (ret) {
			dev_err(dev, "clk_divider bypass enable failed: ret[%d]\n", ret);
			return ret;
		}
		priv->is_clk_divider_bypass = true;
	} else if ((!is_bypass) & (priv->is_clk_divider_bypass)) {
		clk_disable_unprepare(priv->div_clk_bypass);
		priv->is_clk_divider_bypass = false;
	}

	return 0;
}
static void dwcmshc_rk3568_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct rk3568_priv *priv = dwc_priv->priv;
	u8 txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT;
	u32 extra, reg;
	int err;

	host->mmc->actual_clock = 0;

	/*
	 * DO NOT TOUCH THIS SETTING. RX clk inverter unit is enabled
	 * by default, but it shouldn't be enabled. We should anyway
	 * disable it before issuing any cmds.
	 */
	extra = DWCMSHC_EMMC_DLL_DLYENA |
		DLL_RXCLK_NO_INVERTER << DWCMSHC_EMMC_DLL_RXCLK_SRCSEL;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_RXCLK);

	if (clock == 0)
		return;

	/* Rockchip platform only support 375KHz for identify mode */
	if (clock <= 400000)
		clock = 375000;

	err = clk_set_rate(pltfm_host->clk, clock);
	if (err)
		dev_err(mmc_dev(host->mmc), "fail to set clock %d", clock);

	sdhci_set_clock(host, clock);

	/* Disable cmd conflict check */
	reg = dwc_priv->vendor_specific_area1 + DWCMSHC_HOST_CTRL3;
	extra = sdhci_readl(host, reg);
	extra &= ~BIT(0);
	sdhci_writel(host, extra, reg);

	if (clock <= 400000) {
		/* Disable DLL to reset sample clock */
		sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_CTRL);
		return;
	}

	/* Reset DLL */
	sdhci_writel(host, BIT(1), DWCMSHC_EMMC_DLL_CTRL);
	udelay(1);
	sdhci_writel(host, 0x0, DWCMSHC_EMMC_DLL_CTRL);

	/* Init DLL settings */
	extra = 0x5 << DWCMSHC_EMMC_DLL_START_POINT |
		0x2 << DWCMSHC_EMMC_DLL_INC |
		DWCMSHC_EMMC_DLL_START;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_CTRL);
	err = readl_poll_timeout(host->ioaddr + DWCMSHC_EMMC_DLL_STATUS0,
				 extra, DLL_LOCK_WO_TMOUT(extra), 1,
				 500 * USEC_PER_MSEC);
	if (err) {
		dev_err(mmc_dev(host->mmc), "DLL lock timeout!\n");
		return;
	}

	extra = 0x1 << 16 | /* tune clock stop en */
		0x2 << 17 | /* pre-change delay */
		0x3 << 19;  /* post-change delay */
	sdhci_writel(host, extra, dwc_priv->vendor_specific_area1 + DWCMSHC_EMMC_ATCTRL);

	if (host->mmc->ios.timing == MMC_TIMING_MMC_HS200 ||
	    host->mmc->ios.timing == MMC_TIMING_MMC_HS400)
		txclk_tapnum = priv->txclk_tapnum;

	extra = DWCMSHC_EMMC_DLL_DLYENA |
		DLL_TXCLK_TAPNUM_FROM_SW |
		txclk_tapnum;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_TXCLK);

	extra = DWCMSHC_EMMC_DLL_DLYENA |
		DLL_STRBIN_TAPNUM_DEFAULT |
		DLL_STRBIN_TAPNUM_FROM_SW;
	sdhci_writel(host, extra, DWCMSHC_EMMC_DLL_STRBIN);
}

static u16 sdhci_hailo15_calc_clk(struct sdhci_host *host, unsigned int clock,
				   unsigned int *actual_clock)
{
	u16 clk = 0;
	int div = 2;
	struct device *dev = mmc_dev(host->mmc);
	bool is_bypass = (host->max_clk <= clock) ? true : false;
	int ret;

	ret = dwcmshc_hailo15_set_clock_divider_bypass(host, is_bypass);
	if (ret) {
		dev_err(dev, "set clock divider bypass failed ret[%d]\n", ret);
		return 0;
	}
	if (is_bypass)
		div = 1;
	else {
		for (div = 2; div < 1023; div++) {
			if ((host->max_clk / div) <= clock)
				break;

		}
	}
	*actual_clock = host->max_clk / div;
	clk |= (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
	clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
		<< SDHCI_DIVIDER_HI_SHIFT;

	pr_debug("[%s] 'requested clock %d actual clock: %d is_div_bypass %s\n", mmc_hostname(host->mmc), clock, *actual_clock, is_bypass ? "True ": "False");
	return clk;
}

static void dwcmshc_hailo15_set_clock(struct sdhci_host *host, unsigned int clock)
{
	u16 clk;

	host->mmc->actual_clock = 0;

	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	if (clock == 0)
		return;

	clk = sdhci_hailo15_calc_clk(host, clock, &host->mmc->actual_clock);
	if (clk == 0) {
		dev_err(mmc_dev(host->mmc), "%s: set clk failed\n", __func__);
		return;
	}

	sdhci_enable_clk(host, clk);
}
static int dwcmshc_hailo15_get_phy_config_from_dts(struct device *dev, struct hailo_priv *hailo_priv)
{
	struct device_node *phy_config_node;
	phy_config_node = of_find_node_by_name(dev->of_node, "phy-config");
	if (!phy_config_node){
		dev_err(dev, "%s: No phy configuration property found.\n", __func__);
		return EINVAL;
	}
	
	of_property_read_u32(phy_config_node, "card-is-emmc", &hailo_priv->sdio_phy_config.card_is_emmc);
	of_property_read_u32_array(phy_config_node, "cmd-pad-values", hailo_priv->sdio_phy_config.cmd_pad, PAD_CONFIG_MAX);
	of_property_read_u32_array(phy_config_node, "dat-pad-values", hailo_priv->sdio_phy_config.dat_pad, PAD_CONFIG_MAX);
	of_property_read_u32_array(phy_config_node, "rst-pad-values", hailo_priv->sdio_phy_config.rst_pad, PAD_CONFIG_MAX);
	of_property_read_u32_array(phy_config_node, "clk-pad-values", hailo_priv->sdio_phy_config.clk_pad, PAD_CONFIG_MAX);
	of_property_read_u32_array(phy_config_node, "sdclkdl-cnfg", hailo_priv->sdio_phy_config.clk_delay, CLK_DELAY_CONFIG_MAX);
	of_property_read_u32_array(phy_config_node, "drive-strength", hailo_priv->sdio_phy_config.drive_strength, DS_CONFIG_MAX);
	of_node_put(phy_config_node);
	return 0;
}

static u64 hailo15_dwcmshc_get_data_error_count(struct sdhci_host *host){
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct hailo_priv *hailo_priv = dwc_priv->priv;
	
	return hailo_priv->data_error_count;
}

static void hailo15_dwcmshc_increase_data_error_count(struct sdhci_host *host){
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);

	struct hailo_priv *hailo_priv = dwc_priv->priv;
	hailo_priv->data_error_count++;
}

static void hailo15_dwcmshc_set_vdd_gpio(struct sdhci_host *host, bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct hailo_priv *hailo_priv = dwc_priv->priv;
	int gpio_1_8_val = hailo_priv->is_sd_vsel_polarity_high ? 1 : 0;
	int gpio_3_3_val = hailo_priv->is_sd_vsel_polarity_high ? 0 : 1;

	if (hailo_priv->vdd_gpio == NULL)
		return;

	if (enable) {
		/* Set to 1.8v */
		gpiod_set_value(hailo_priv->vdd_gpio, gpio_1_8_val);
		pr_debug("%s hailo15_dwcmshc_voltage_switch to 1.8 vdd polarity %d\n", mmc_hostname(host->mmc), gpio_1_8_val);
	} else {
		/* Set to 3.3v */
		gpiod_set_value(hailo_priv->vdd_gpio, gpio_3_3_val);
		pr_debug("%s hailo15_dwcmshc_voltage_switch to 3.3 vdd vdd polarity %d\n", mmc_hostname(host->mmc), gpio_3_3_val);
	}

	usleep_range(2500, 3000);
}

static void dwcmshc_hailo15_get_sd_vsel_from_dts(struct device *dev, struct sdhci_host *host, struct hailo_priv *hailo_priv)
{
		hailo_priv->is_sd_vsel_polarity_high = false;
		hailo_priv->vdd_gpio = devm_gpiod_get_optional(dev, "sd-vsel", GPIOD_OUT_LOW);
		if (hailo_priv->vdd_gpio == NULL)
			return;
		if (of_property_read_bool(dev->of_node, "sd-vsel-polarity-high"))
			hailo_priv->is_sd_vsel_polarity_high = true;

		hailo15_dwcmshc_set_vdd_gpio(host, false);
}

static void hailo15_dwcmshc_hw_reset(struct sdhci_host *host)
{
	dwcmshc_hailo15_phy_config(host);
	hailo15_dwcmshc_set_vdd_gpio(host, false);
}

static void hailo15_dwcmshc_voltage_switch(struct sdhci_host *host)
{
	hailo15_dwcmshc_set_vdd_gpio(host, true);
}

static unsigned int hailo15_dwcmshc_get_min_clock(struct sdhci_host *host)
{
	/* TODO:
		we should return dwcmshc_get_max_clock(host) / 1023
		we need to investigate why we get unwanted responses in lower frequency
		see MSW-2498
	*/
	return 800000;
}

static int hailo15_dwcmshc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct sdhci_host *host = mmc_priv(mmc);
	int err;

	err = sdhci_execute_tuning(mmc, opcode);
	if (!err && !host->tuning_err)
		pr_info("%s Tuning Success!\n", mmc_hostname(host->mmc));

	return err;
}

static const struct sdhci_ops sdhci_dwcmshc_ops = {
	.set_clock		= sdhci_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= dwcmshc_get_max_clock,
	.reset			= sdhci_reset,
	.adma_write_desc	= dwcmshc_adma_write_desc,
};

static const struct sdhci_ops sdhci_dwcmshc_rk3568_ops = {
	.set_clock		= dwcmshc_rk3568_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= sdhci_pltfm_clk_get_max_clock,
	.reset			= sdhci_reset,
	.adma_write_desc	= dwcmshc_adma_write_desc,	
};

static const struct sdhci_ops hailo15_dwcmshc_ops = {
	.set_clock		= dwcmshc_hailo15_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.set_uhs_signaling	= dwcmshc_set_uhs_signaling,
	.get_max_clock		= dwcmshc_get_max_clock,
	.get_min_clock		= hailo15_dwcmshc_get_min_clock,
	.reset			= sdhci_reset,
	.adma_write_desc	= dwcmshc_adma_write_desc,
	.hw_reset = hailo15_dwcmshc_hw_reset,
	.voltage_switch = hailo15_dwcmshc_voltage_switch,
	.get_data_error_count = hailo15_dwcmshc_get_data_error_count,
	.increase_data_error_count = hailo15_dwcmshc_increase_data_error_count,
};

static const struct sdhci_pltfm_data sdhci_dwcmshc_pdata = {
	.ops = &sdhci_dwcmshc_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

static const struct sdhci_pltfm_data hailo_sdhci_dwcmshc_pdata = {
	.ops = &hailo15_dwcmshc_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN |
		SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		SDHCI_QUIRK_BROKEN_CARD_DETECTION,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
};

static const struct sdhci_pltfm_data sdhci_dwcmshc_rk3568_pdata = {
	.ops = &sdhci_dwcmshc_rk3568_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN |
		  SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		   SDHCI_QUIRK2_CLOCK_DIV_ZERO_BROKEN,
};

static int dwcmshc_rk3568_init(struct sdhci_host *host, struct dwcmshc_priv *dwc_priv)
{
	int err;
	struct rk3568_priv *priv = dwc_priv->priv;

	priv->rockchip_clks[0].id = "axi";
	priv->rockchip_clks[1].id = "block";
	priv->rockchip_clks[2].id = "timer";
	err = devm_clk_bulk_get_optional(mmc_dev(host->mmc), RK3568_MAX_CLKS,
					 priv->rockchip_clks);
	if (err) {
		dev_err(mmc_dev(host->mmc), "failed to get clocks %d\n", err);
		return err;
	}

	err = clk_bulk_prepare_enable(RK3568_MAX_CLKS, priv->rockchip_clks);
	if (err) {
		dev_err(mmc_dev(host->mmc), "failed to enable clocks %d\n", err);
		return err;
	}

	if (of_property_read_u8(mmc_dev(host->mmc)->of_node, "rockchip,txclk-tapnum",
				&priv->txclk_tapnum))
		priv->txclk_tapnum = DLL_TXCLK_TAPNUM_DEFAULT;

	/* Disable cmd conflict check */
	sdhci_writel(host, 0x0, dwc_priv->vendor_specific_area1 + DWCMSHC_HOST_CTRL3);
	/* Reset previous settings */
	sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_TXCLK);
	sdhci_writel(host, 0, DWCMSHC_EMMC_DLL_STRBIN);

	return 0;
}

static const struct of_device_id sdhci_dwcmshc_dt_ids[] = {
	{
		.compatible = "rockchip,rk3568-dwcmshc",
		.data = &sdhci_dwcmshc_rk3568_pdata,
	},
	{
		.compatible = "snps,dwcmshc-sdhci",
		.data = &sdhci_dwcmshc_pdata,
	},
	{
		.compatible = "hailo,dwcmshc-sdhci-0",
		.data = &hailo_sdhci_dwcmshc_pdata,
	},
	{
		.compatible = "hailo,dwcmshc-sdhci-1",
		.data = &hailo_sdhci_dwcmshc_pdata,
	},
	{},
};
MODULE_DEVICE_TABLE(of, sdhci_dwcmshc_dt_ids);

#ifdef CONFIG_ACPI
static const struct acpi_device_id sdhci_dwcmshc_acpi_ids[] = {
	{ .id = "MLNXBF30" },
	{}
};
#endif

static void dwcmshc_hailo15_phy_config(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *dwc_priv = sdhci_pltfm_priv(pltfm_host);
	struct hailo_priv *hailo_priv = dwc_priv->priv;
	hailo15_phy_config *sdio_phy_config = &hailo_priv->sdio_phy_config;
	u32       reg32 = 0;
	u16       reg16 = 0;
	u8	  reg8 = 0;

	reg32 = sdhci_readl(host, DWCMSHC_EMMC_CTRL_R);
	reg32 &= ~DWCMSHC_EMMC_CTRL_R__CARD_IS_EMMC;
	if (sdio_phy_config->card_is_emmc) {
		reg32 |= DWCMSHC_EMMC_CTRL_R__CARD_IS_EMMC;
	}
	sdhci_writel(host, reg32, DWCMSHC_EMMC_CTRL_R);
	
	/* CMD PAD configuration */
	reg16 = sdhci_readw(host, DWCMSHC_CMDPAD_CNFG);
	reg16 &= ~DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_N;
	reg16 |= FIELD_PREP(DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_N, sdio_phy_config->cmd_pad[TXSLEW_CTRL_N]);
	reg16 &= ~DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_P;
	reg16 |= FIELD_PREP(DWCMSHC_CMDPAD_CNFG__TXSLEW_CTRL_P, sdio_phy_config->cmd_pad[TXSLEW_CTRL_P]);
	reg16 &= ~DWCMSHC_CMDPAD_CNFG__WEAKPULL_EN;
	reg16 |= FIELD_PREP(DWCMSHC_CMDPAD_CNFG__WEAKPULL_EN, sdio_phy_config->cmd_pad[WEAKPULL_EN]);
	reg16 &= ~DWCMSHC_CMDPAD_CNFG__RXSEL;
	reg16 |= FIELD_PREP(DWCMSHC_CMDPAD_CNFG__RXSEL, sdio_phy_config->cmd_pad[RXSEL]);
	sdhci_writew(host, reg16, DWCMSHC_CMDPAD_CNFG);
	
	/* DAT PAD configuration */
	reg16 = sdhci_readw(host, DWCMSHC_DATPAD_CNFG);
	reg16 &= ~DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_N;
	reg16 |= FIELD_PREP(DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_N, sdio_phy_config->dat_pad[TXSLEW_CTRL_N]);
	reg16 &= ~DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_P;
	reg16 |= FIELD_PREP(DWCMSHC_DATPAD_CNFG__TXSLEW_CTRL_P, sdio_phy_config->dat_pad[TXSLEW_CTRL_P]);
	reg16 &= ~DWCMSHC_DATPAD_CNFG__WEAKPULL_EN;
	reg16 |= FIELD_PREP(DWCMSHC_DATPAD_CNFG__WEAKPULL_EN, sdio_phy_config->dat_pad[WEAKPULL_EN]);
	reg16 &= ~DWCMSHC_DATPAD_CNFG__RXSEL;
	reg16 |= FIELD_PREP(DWCMSHC_DATPAD_CNFG__RXSEL, sdio_phy_config->dat_pad[RXSEL]);
	sdhci_writew(host, reg16, DWCMSHC_DATPAD_CNFG);
	
	/* RST PAD configuration */
	reg16 = sdhci_readw(host, DWCMSHC_RSTNPAD_CNFG);
	reg16 &= ~DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_N;
	reg16 |= FIELD_PREP(DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_N, sdio_phy_config->rst_pad[TXSLEW_CTRL_N]);
	reg16 &= ~DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_P;
	reg16 |= FIELD_PREP(DWCMSHC_RSTNPAD_CNFG__TXSLEW_CTRL_P, sdio_phy_config->rst_pad[TXSLEW_CTRL_P]);
	reg16 &= ~DWCMSHC_RSTNPAD_CNFG__WEAKPULL_EN;
	reg16 |= FIELD_PREP(DWCMSHC_RSTNPAD_CNFG__WEAKPULL_EN, sdio_phy_config->rst_pad[WEAKPULL_EN]);
	reg16 &= ~DWCMSHC_RSTNPAD_CNFG__RXSEL;
	reg16 |= FIELD_PREP(DWCMSHC_RSTNPAD_CNFG__RXSEL, sdio_phy_config->rst_pad[RXSEL]);
	sdhci_writew(host, reg16, DWCMSHC_RSTNPAD_CNFG);

	/* CLK PAD configuration */
	reg16 = sdhci_readw(host, DWCMSHC_CLKPAD_CNFG);
	reg16 &= ~DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_N;
	reg16 |= FIELD_PREP(DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_N, sdio_phy_config->clk_pad[TXSLEW_CTRL_N]);
	reg16 &= ~DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_P;
	reg16 |= FIELD_PREP(DWCMSHC_CLKPAD_CNFG__TXSLEW_CTRL_P, sdio_phy_config->clk_pad[TXSLEW_CTRL_P]);
	reg16 &= ~DWCMSHC_CLKPAD_CNFG__WEAKPULL_EN;
	reg16 |= FIELD_PREP(DWCMSHC_CLKPAD_CNFG__WEAKPULL_EN, sdio_phy_config->clk_pad[WEAKPULL_EN]);
	reg16 &= ~DWCMSHC_CLKPAD_CNFG__RXSEL;
	reg16 |= FIELD_PREP(DWCMSHC_CLKPAD_CNFG__RXSEL, sdio_phy_config->clk_pad[RXSEL]);
	sdhci_writew(host, reg16, DWCMSHC_CLKPAD_CNFG);
	/* wait for phy power good */
	while (!(sdhci_readw(host, DWCMSHC_PHY_CNFG) & DWCMSHC_PHY_CNFG__PHY_PWRGOOD));
	
	/* de-assert phy reset */
	reg32 = sdhci_readl(host, DWCMSHC_PHY_CNFG);
	reg32 &= ~DWCMSHC_PHY_CNFG__PHY_RSTN;
	reg32 |= DWCMSHC_PHY_CNFG__PHY_RSTN;
	sdhci_writel(host, reg32, DWCMSHC_PHY_CNFG);
	
	/* enable pll */
	reg16 = sdhci_readw(host, DWCMSHC_CLK_CTRL_R);
	reg16 &= ~DWCMSHC_CLK_CTRL_R__PLL_ENABLE;
	reg16 |= DWCMSHC_CLK_CTRL_R__PLL_ENABLE;
	sdhci_writew(host, reg16, DWCMSHC_CLK_CTRL_R);

	/* Adding Clock delay for SD */
	reg8 = sdhci_readb(host, DWCMSHC_SDCLKDL_CNFG);
	reg8 &= ~DWCMSHC_SDCLKDL_CNFG__EXTDLY_EN;
	if (sdio_phy_config->clk_delay[EXTDLY_EN]) {
		reg8 |= DWCMSHC_SDCLKDL_CNFG__EXTDLY_EN;
	}
	sdhci_writeb(host, reg8, DWCMSHC_SDCLKDL_CNFG);
	reg16 = sdhci_readw(host, DWCMSHC_SDCLKDL_DC);
	reg16 &= ~DWCMSHC_SDCLKDL_DC__CCKDL_DC;
	reg16 |= FIELD_PREP(DWCMSHC_SDCLKDL_DC__CCKDL_DC, sdio_phy_config->clk_delay[CCKDL_DC]);
	sdhci_writew(host, reg16, DWCMSHC_SDCLKDL_DC); 

	/* PHY configuration - NMOS, PMOS TX drive strength control */
	reg32 = sdhci_readl(host, DWCMSHC_PHY_CNFG);
	reg32 &= ~DWCMSHC_PHY_CNFG__PAD_SP;
	reg32 |= FIELD_PREP(DWCMSHC_PHY_CNFG__PAD_SP, sdio_phy_config->drive_strength[PAD_SP]);
	reg32 &= ~DWCMSHC_PHY_CNFG__PAD_SN;
	reg32 |= FIELD_PREP(DWCMSHC_PHY_CNFG__PAD_SN, sdio_phy_config->drive_strength[PAD_SN]);
	sdhci_writel(host, reg32, DWCMSHC_PHY_CNFG);

	/* In case CMD19(tuning) will be sent - BLOCKSIZE_R, BLOCKCOUNT_R BLOCK_COUNT_ENABLE must be set */	
	reg16 = sdhci_readw(host, DWCMSHC_BLOCKSIZE_R);
	reg16 &= ~DWCMSHC_BLOCKSIZE_R__XFER_BLOCK_SIZE;
	reg16 |= FIELD_PREP(DWCMSHC_BLOCKSIZE_R__XFER_BLOCK_SIZE, 0x200);
	sdhci_writew(host, reg16, DWCMSHC_BLOCKSIZE_R);

	sdhci_writew(host, (u16)0x1, DWCMSHC_BLOCKCOUNT_R);
	
	reg16 = sdhci_readw(host, DWCMSHC_XFER_MODE_R);
	reg16 &= ~DWCMSHC_XFER_MODE_R__BLOCK_COUNT_ENABLE;
	reg16 |= DWCMSHC_XFER_MODE_R__BLOCK_COUNT_ENABLE;
	reg16 &= ~DWCMSHC_XFER_MODE_R__DATA_XFER_DIR;
	reg16 |= DWCMSHC_XFER_MODE_R__DATA_XFER_DIR;
	reg16 &= ~DWCMSHC_XFER_MODE_R__MULTI_BLK_SEL;
	reg16 |= DWCMSHC_XFER_MODE_R__MULTI_BLK_SEL;
	sdhci_writew(host, reg16, DWCMSHC_XFER_MODE_R);

	/* Sampling window threshold value is 31 out of 128 taps */
	reg32 = sdhci_readl(host, DWCMSHC_AT_CTRL_R);
	reg32 &= ~DWCMSHC_AT_CTRL_R__SWIN_TH_EN;
	reg32 |= DWCMSHC_AT_CTRL_R__SWIN_TH_EN;
	reg32 &= ~DWCMSHC_AT_CTRL_R__SWIN_TH_VAL;
	reg32 |= FIELD_PREP(DWCMSHC_AT_CTRL_R__SWIN_TH_VAL,0x1f);
	sdhci_writel(host, reg32, DWCMSHC_AT_CTRL_R);

	pr_debug("%s phy configuration for %s mode done\n", mmc_hostname(host->mmc), sdio_phy_config->card_is_emmc ? "EMMC ": "SD");
}

static int dwcmshc_hailo_init(struct sdhci_host *host, struct dwcmshc_priv *dwc_priv)
{
	struct hailo_priv *priv = dwc_priv->priv;
	struct device *dev = mmc_dev(host->mmc);
	struct reset_control *sdio_cd_input = NULL;
	int err = 0;
	priv->is_clk_divider_bypass = false;

	/* In case card detection interrupt support - select card detect input pads_pinmux */
	if ((!(host->quirks & SDHCI_QUIRK_BROKEN_CARD_DETECTION)) &&
        (mmc_card_is_removable(host->mmc)) && (!mmc_can_gpio_cd(host->mmc))) {
		sdio_cd_input = devm_reset_control_get(dev,"sdio-cd-input");
		if (IS_ERR(sdio_cd_input)) {
			err = PTR_ERR(sdio_cd_input);
			dev_err_probe(dev, err, "failed to get cd input: %d\n", err);
			return err;
		}
		err = reset_control_deassert(sdio_cd_input);
		if (err < 0) {
			dev_err(dev, "Failed deasserting card detect input, err %d\n", err);
			return err;
		}

	}
	
	err = dwcmshc_hailo15_get_phy_config_from_dts(dev, priv);
	if (err)
		return err;
	

	dwcmshc_hailo15_get_sd_vsel_from_dts(dev, host, priv);

	priv->div_clk_bypass = devm_clk_get(dev, "clk_div_bypass");
    	if (IS_ERR(priv->div_clk_bypass)){
		dev_err(dev, "Failed to get div_clk_bypass\n");
		return PTR_ERR(priv->div_clk_bypass);
	}

	/* In order to enable low clock frequency at init */
	err = dwcmshc_hailo15_set_clock_divider_bypass(host, false);
	if (err) {
		dev_err(dev, "set clock divider bypass failed ret[%d]\n", err);
		return err;
	}

	/* SDIO delay line (DLL) has 128 taps */
	host->tuning_loop_count = 128;
	return 0;
}

static int set_sdio_8_bit_mux(struct device *dev)
{
	struct reset_control *sdio_reset = NULL;
	int err;
	u32 bus_width;
	
	device_property_read_u32(dev, "bus-width", &bus_width);
	
	sdio_reset = devm_reset_control_get(dev, "sdio1-8bit-mux");
	if (IS_ERR(sdio_reset)) {
		if (PTR_ERR(sdio_reset) != -EPROBE_DEFER) {
			dev_err(dev, "failed to get sdio reset, error = %pe.\n", sdio_reset);
		}
		return PTR_ERR(sdio_reset);
	}

	if (bus_width == 8){
		err = reset_control_deassert(sdio_reset);
    		if (err < 0) {
        		dev_err(dev, "Failed deasserting sdio 8 bit mux, err %d\n", err);
        		return err;
		}
    	} else {
		err = reset_control_assert(sdio_reset);
    		if (err < 0) {
        		dev_err(dev, "Failed asserting sdio 8 bit mux, err %d\n", err);
        		return err;
		}
	}
	return 0;
}
static int hailo_init_sdio_pins_and_muxes(struct device *dev)
{
	int err = 0;
	if (of_device_is_compatible(dev->of_node,"hailo,dwcmshc-sdhci-1")) {
		err = set_sdio_8_bit_mux(dev);
		if(err)
			return err;
	}
	return err;
}

static int dwcmshc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_host *host;
	struct dwcmshc_priv *priv;
	struct rk3568_priv *rk_priv = NULL;
	struct hailo_priv *hailo_priv = NULL;
	const struct sdhci_pltfm_data *pltfm_data;
	int err;
	u32 extra;
	bool is_hailo_sdhci = false;

	pltfm_data = of_device_get_match_data(dev);
	if (!pltfm_data) {
		dev_err(dev, "Error: No device match data found\n");
		return -ENODEV;
	}

	if (of_device_is_compatible(dev->of_node,"hailo,dwcmshc-sdhci-0")||
	    of_device_is_compatible(dev->of_node,"hailo,dwcmshc-sdhci-1")) {
		is_hailo_sdhci = true;
		err = hailo_init_sdio_pins_and_muxes(dev);
		if(err)
			return err;
	}
	
	host = sdhci_pltfm_init(pdev, pltfm_data, sizeof(struct dwcmshc_priv));
	if (IS_ERR(host))
		return PTR_ERR(host);

	/*
	 * extra adma table cnt for cross 128M boundary handling.
	 */
	extra = DIV_ROUND_UP_ULL(dma_get_required_mask(dev), SZ_128M);
	if (extra > SDHCI_MAX_SEGS)
		extra = SDHCI_MAX_SEGS;
	host->adma_table_cnt += extra;
	pltfm_host = sdhci_priv(host);
	priv = sdhci_pltfm_priv(pltfm_host);

	if (dev->of_node) {
		pltfm_host->clk = devm_clk_get(dev, "core");
		if (IS_ERR(pltfm_host->clk)) {
			err = PTR_ERR(pltfm_host->clk);
			dev_err_probe(dev, err, "failed to get core clk: %d\n", err);
			goto free_pltfm;
		}
		err = clk_prepare_enable(pltfm_host->clk);
		if (err)
			goto free_pltfm;

		priv->bus_clk = devm_clk_get(dev, "bus");
		if (!IS_ERR(priv->bus_clk))
			clk_prepare_enable(priv->bus_clk);
	}
	err = mmc_of_parse(host->mmc);
	if (err)
		goto err_clk;

	sdhci_get_of_property(pdev);

	priv->vendor_specific_area1 =
	sdhci_readl(host, DWCMSHC_P_VENDOR_AREA1) & DWCMSHC_AREA1_MASK;
	host->mmc_host_ops.request = dwcmshc_request;
	host->mmc_host_ops.hs400_enhanced_strobe = dwcmshc_hs400_enhanced_strobe;
	
	if (pltfm_data == &sdhci_dwcmshc_rk3568_pdata) {
		rk_priv = devm_kzalloc(dev, sizeof(struct rk3568_priv), GFP_KERNEL);
		if (!rk_priv) {
			err = -ENOMEM;
			goto err_clk;
		}

		priv->priv = rk_priv;

		err = dwcmshc_rk3568_init(host, priv);
		if (err)
			goto err_clk;
	}
	if (is_hailo_sdhci) {
		hailo_priv = devm_kzalloc(dev, sizeof(struct hailo_priv), GFP_KERNEL);
		if (!hailo_priv) {
			dev_err(dev, "Error: devm_kzalloc fail for hailo_priv \n");
			err = -ENOMEM;
			goto err_clk;
		}

		priv->priv = hailo_priv;
		err = dwcmshc_hailo_init(host, priv);
		if (err) {
			dev_err_probe(dev, err, "dwcmshc_hailo_init failed ret[%d]\n", err);
			return err;
		}
		host->mmc_host_ops.execute_tuning = hailo15_dwcmshc_execute_tuning;
	}

	host->mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;
	host->timeout_clk = DIV_ROUND_UP(clk_get_rate(pltfm_host->clk), 1000);
	err = sdhci_add_host(host);
	if (err)
		goto err_clk;

	if (is_hailo_sdhci) {
		dwcmshc_hailo15_phy_config(host);
	}
	return 0;

err_clk:
	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(priv->bus_clk);
	if (rk_priv)
		clk_bulk_disable_unprepare(RK3568_MAX_CLKS,
					   rk_priv->rockchip_clks);
free_pltfm:
	sdhci_pltfm_free(pdev);
	return err;
}

static int dwcmshc_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct rk3568_priv *rk_priv = priv->priv;
	const struct sdhci_pltfm_data *pltfm_data;
	bool is_hailo = false;
	pltfm_data = of_device_get_match_data(&pdev->dev);
	if (!pltfm_data) {
		dev_err(&pdev->dev, "Error: No device match data found\n");
		return -ENODEV;
	}

	if (of_device_is_compatible(dev->of_node,"hailo,dwcmshc-sdhci-0")||
	    of_device_is_compatible(dev->of_node,"hailo,dwcmshc-sdhci-1")) {
		is_hailo = true;
		hailo15_dwcmshc_set_vdd_gpio(host, false);
	}
	sdhci_remove_host(host, 0);

	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(priv->bus_clk);
	if ((rk_priv) && (!is_hailo))
		clk_bulk_disable_unprepare(RK3568_MAX_CLKS,
					   rk_priv->rockchip_clks);
	sdhci_pltfm_free(pdev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwcmshc_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct rk3568_priv *rk_priv = priv->priv;
	int ret;

	ret = sdhci_suspend_host(host);
	if (ret)
		return ret;

	clk_disable_unprepare(pltfm_host->clk);
	if (!IS_ERR(priv->bus_clk))
		clk_disable_unprepare(priv->bus_clk);

	if (rk_priv)
		clk_bulk_disable_unprepare(RK3568_MAX_CLKS,
					   rk_priv->rockchip_clks);

	return ret;
}

static int dwcmshc_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct dwcmshc_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct rk3568_priv *rk_priv = priv->priv;
	int ret;

	ret = clk_prepare_enable(pltfm_host->clk);
	if (ret)
		return ret;

	if (!IS_ERR(priv->bus_clk)) {
		ret = clk_prepare_enable(priv->bus_clk);
		if (ret)
			return ret;
	}

	if (rk_priv) {
		ret = clk_bulk_prepare_enable(RK3568_MAX_CLKS,
					      rk_priv->rockchip_clks);
		if (ret)
			return ret;
	}

	return sdhci_resume_host(host);
}
#endif

static SIMPLE_DEV_PM_OPS(dwcmshc_pmops, dwcmshc_suspend, dwcmshc_resume);

static struct platform_driver sdhci_dwcmshc_driver = {
	.driver	= {
		.name	= "sdhci-dwcmshc",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = sdhci_dwcmshc_dt_ids,
		.acpi_match_table = ACPI_PTR(sdhci_dwcmshc_acpi_ids),
		.pm = &dwcmshc_pmops,
	},
	.probe	= dwcmshc_probe,
	.remove	= dwcmshc_remove,
};
module_platform_driver(sdhci_dwcmshc_driver);

MODULE_DESCRIPTION("SDHCI platform driver for Synopsys DWC MSHC");
MODULE_AUTHOR("Jisheng Zhang <jszhang@kernel.org>");
MODULE_LICENSE("GPL v2");
