/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Core pinctrl driver for Hailo pinctrl logic
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 */
 
#ifndef _PINCTRL_HAILO15_PROPERTIES_H
#define _PINCTRL_HAILO15_PROPERTIES_H

#include "pinctrl-hailo15.h"

#define H15_PINMUX_INVALID_FUNCTION_SELECTOR (ARRAY_SIZE(h15_pin_functions))
#define H15_PINMUX_GPIO_FUNCTION_SELECTOR (0)

static const struct pinctrl_pin_desc hailo15_pins[] = {
	PINCTRL_PIN(0, "GPIO_0"),
	PINCTRL_PIN(1, "GPIO_1"),
	PINCTRL_PIN(2, "GPIO_2"),
	PINCTRL_PIN(3, "GPIO_3"),
	PINCTRL_PIN(4, "GPIO_4"),
	PINCTRL_PIN(5, "GPIO_5"),
	PINCTRL_PIN(6, "GPIO_6"),
	PINCTRL_PIN(7, "GPIO_7"),
	PINCTRL_PIN(8, "GPIO_8"),
	PINCTRL_PIN(9, "GPIO_9"),
	PINCTRL_PIN(10, "GPIO_10"),
	PINCTRL_PIN(11, "GPIO_11"),
	PINCTRL_PIN(12, "GPIO_12"),
	PINCTRL_PIN(13, "GPIO_13"),
	PINCTRL_PIN(14, "GPIO_14"),
	PINCTRL_PIN(15, "GPIO_15"),
	PINCTRL_PIN(16, "GPIO_16"),
	PINCTRL_PIN(17, "GPIO_17"),
	PINCTRL_PIN(18, "GPIO_18"),
	PINCTRL_PIN(19, "GPIO_19"),
	PINCTRL_PIN(20, "GPIO_20"),
	PINCTRL_PIN(21, "GPIO_21"),
	PINCTRL_PIN(22, "GPIO_22"),
	PINCTRL_PIN(23, "GPIO_23"),
	PINCTRL_PIN(24, "GPIO_24"),
	PINCTRL_PIN(25, "GPIO_25"),
	PINCTRL_PIN(26, "GPIO_26"),
	PINCTRL_PIN(27, "GPIO_27"),
	PINCTRL_PIN(28, "GPIO_28"),
	PINCTRL_PIN(29, "GPIO_29"),
	PINCTRL_PIN(30, "GPIO_30"),
	PINCTRL_PIN(31, "GPIO_31"),
	PINCTRL_PIN(32, "uart0_txd"),
	PINCTRL_PIN(33, "uart0_rxd"),
	PINCTRL_PIN(34, "uart1_txd"),
	PINCTRL_PIN(35, "uart1_rxd"),
	PINCTRL_PIN(36, "eth_rgmii_tx_clk"),
	PINCTRL_PIN(37, "eth_rgmii_tx_ctl"),
	PINCTRL_PIN(38, "eth_rgmii_txd_0"),
	PINCTRL_PIN(39, "eth_rgmii_txd_1"),
	PINCTRL_PIN(40, "eth_rgmii_txd_2"),
	PINCTRL_PIN(41, "eth_rgmii_txd_3"),
	PINCTRL_PIN(42, "eth_rgmii_rxd_0"),
	PINCTRL_PIN(43, "eth_rgmii_rxd_1"),
	PINCTRL_PIN(44, "eth_rgmii_rxd_2"),
	PINCTRL_PIN(45, "eth_rgmii_rxd_3"),
	PINCTRL_PIN(46, "eth_rgmii_rx_clk"),
	PINCTRL_PIN(47, "eth_rgmii_rx_ctl"),
	PINCTRL_PIN(48, "eth_mdc"),
	PINCTRL_PIN(49, "eth_mdio"),
	PINCTRL_PIN(50, "i2c0_sda"),
	PINCTRL_PIN(51, "i2c0_scl"),
	PINCTRL_PIN(52, "i2c1_sda"),
	PINCTRL_PIN(53, "i2c1_scl"),
	PINCTRL_PIN(54, "jtag_tdi"),
	PINCTRL_PIN(55, "jtag_tdo"),
	PINCTRL_PIN(56, "jtag_tms"),
	PINCTRL_PIN(57, "jtag_tck"),
	PINCTRL_PIN(58, "jtag_trstn"),
	PINCTRL_PIN(59, "flash_sclk"),
	PINCTRL_PIN(60, "flash_cs_0"),
	PINCTRL_PIN(61, "flash_cs_1"),
	PINCTRL_PIN(62, "flash_reset"),
	PINCTRL_PIN(63, "flash_dq_0"),
	PINCTRL_PIN(64, "flash_dq_1"),
	PINCTRL_PIN(65, "flash_dq_2"),
	PINCTRL_PIN(66, "flash_dq_3"),
	PINCTRL_PIN(67, "i2s_sck"),
	PINCTRL_PIN(68, "i2s_ws"),
	PINCTRL_PIN(69, "i2s_sdo"),
	PINCTRL_PIN(70, "i2s_sdi"),
	PINCTRL_PIN(71, "pcie_clkreq"),
	PINCTRL_PIN(72, "pcie_wake"),
	PINCTRL_PIN(73, "safety_error"),
	PINCTRL_PIN(74, "safety_fatal"),
	PINCTRL_PIN(75, "parallel_pclk"),
};

static const unsigned initial_debug_bus_out_pins[] = { 0, 1, 2, 3, 4, 5 };
static const unsigned initial_debug_bus_out_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT, OUT, OUT, OUT, OUT, OUT);

static const unsigned gpio0_pins[] = { 0 };
static const unsigned gpio0_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c0_current_src_en_out_0_pins[] = { 0 };
static const unsigned i2c0_current_src_en_out_0_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned flash_cs_out_pad_2_pins[] = { 0 };
static const unsigned flash_cs_out_pad_2_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned pwm0_pins[] = { 0, 1 };
static const unsigned pwm0_pin_directions[] = H15_PIN_DIRECTIONS(OUT, OUT);

static const unsigned safety_out0_0_pins[] = { 0 };
static const unsigned safety_out0_0_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned sdio0_gp_out_0_pins[] = { 0 };
static const unsigned sdio0_gp_out_0_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned uart2_0_pins[] = { 6, 0 };
static const unsigned uart2_0_pin_directions[] = H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned i2c2_current_src_en_out_0_pins[] = { 0 };
static const unsigned i2c2_current_src_en_out_0_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio1_pins[] = { 1 };
static const unsigned gpio1_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c1_current_src_en_out_0_pins[] = { 1 };
static const unsigned i2c1_current_src_en_out_0_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned flash_cs_out_pad_3_pins[] = { 1 };
static const unsigned flash_cs_out_pad_3_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned safety_out1_0_pins[] = { 1 };
static const unsigned safety_out1_0_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned sdio1_gp_out_0_pins[] = { 1 };
static const unsigned sdio1_gp_out_0_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned uart3_0_pins[] = { 7, 1 };
static const unsigned uart3_0_pin_directions[] = H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned i2c3_current_src_en_out_0_pins[] = { 1 };
static const unsigned i2c3_current_src_en_out_0_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio2_pins[] = { 2 };
static const unsigned gpio2_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c0_current_src_en_out_1_pins[] = { 2 };
static const unsigned i2c0_current_src_en_out_1_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned isp_flash_trig_out_pins[] = { 2 };
static const unsigned isp_flash_trig_out_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned pwm1_pins[] = { 2, 3 };
static const unsigned pwm1_pin_directions[] = H15_PIN_DIRECTIONS(OUT, OUT);

static const unsigned safety_out0_1_pins[] = { 2 };
static const unsigned safety_out0_1_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned usb_drive_vbus_out_0_pins[] = { 2 };
static const unsigned usb_drive_vbus_out_0_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned uart2_1_pins[] = { 6, 2 };
static const unsigned uart2_1_pin_directions[] = H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned i2c2_current_src_en_out_1_pins[] = { 2 };
static const unsigned i2c2_current_src_en_out_1_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio3_pins[] = { 3 };
static const unsigned gpio3_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c1_current_src_en_out_1_pins[] = { 3 };
static const unsigned i2c1_current_src_en_out_1_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned isp_pre_flash_out_pins[] = { 3 };
static const unsigned isp_pre_flash_out_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned safety_out1_1_pins[] = { 3 };
static const unsigned safety_out1_1_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned sdio1_gp_out_1_pins[] = { 3 };
static const unsigned sdio1_gp_out_1_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned uart3_1_pins[] = { 7, 3 };
static const unsigned uart3_1_pin_directions[] = H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned i2c3_current_src_en_out_1_pins[] = { 3 };
static const unsigned i2c3_current_src_en_out_1_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio4_pins[] = { 4 };
static const unsigned gpio4_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned pwm2_pins[] = { 4, 5 };
static const unsigned pwm2_pin_directions[] = H15_PIN_DIRECTIONS(OUT, OUT);

static const unsigned safety_out0_2_pins[] = { 4 };
static const unsigned safety_out0_2_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned sdio0_gp_out_1_pins[] = { 4 };
static const unsigned sdio0_gp_out_1_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned uart2_2_pins[] = { 6, 4 };
static const unsigned uart2_2_pin_directions[] = H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned i2c2_current_src_en_out_2_pins[] = { 4 };
static const unsigned i2c2_current_src_en_out_2_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio5_pins[] = { 5 };
static const unsigned gpio5_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c3_current_src_en_out_2_pins[] = { 5 };
static const unsigned i2c3_current_src_en_out_2_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned I2S_SD_pins[] = { 5, 24 };
static const unsigned I2S_SD_pin_directions[] = H15_PIN_DIRECTIONS(OUT, IN);

static const unsigned safety_out1_2_pins[] = { 5 };
static const unsigned safety_out1_2_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned sdio1_gp_out_2_pins[] = { 5 };
static const unsigned sdio1_gp_out_2_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned uart3_2_pins[] = { 7, 5 };
static const unsigned uart3_2_pin_directions[] = H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned usb_drive_vbus_out_1_pins[] = { 5 };
static const unsigned usb_drive_vbus_out_1_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio6_pins[] = { 6 };
static const unsigned gpio6_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned uart1_cts_rts_0_pins[] = { 6, 7 };
static const unsigned uart1_cts_rts_0_pin_directions[] =
	H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned sdio0_wp_in_pins[] = { 6 };
static const unsigned sdio0_wp_in_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned parallel_in_16pins_pins[] = { 6,	7,  16, 17, 18, 19,
						    20, 21, 22, 23, 24, 25,
						    26, 27, 28, 29, 30, 31 };
static const unsigned parallel_in_16pins_pin_directions[] =
	H15_PIN_DIRECTIONS(IN, IN, IN, IN, IN, IN, IN, IN, IN, IN, IN, IN);

static const unsigned timer_ext_in2_pins[] = { 6 };
static const unsigned timer_ext_in2_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned i2c2_0_pins[] = { 6, 7 };
static const unsigned i2c2_0_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR, OUT);

static const unsigned i2c3_0_pins[] = { 6, 7 };
static const unsigned i2c3_0_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR, OUT);

static const unsigned uart3_cts_rts_pins[] = { 6, 7 };
static const unsigned uart3_cts_rts_pin_directions[] =
	H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned gpio7_pins[] = { 7 };
static const unsigned gpio7_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned sdio1_wp_in_pins[] = { 7 };
static const unsigned sdio1_wp_in_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned timer_ext_in3_pins[] = { 7 };
static const unsigned timer_ext_in3_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned parallel_in_24pins_pins[] = { 6,	7,  8,	9,  10, 11, 12,
						    13, 14, 15, 16, 17, 18, 19,
						    20, 21, 22, 23, 24, 25, 26,
						    27, 28, 29, 30, 31 };
static const unsigned parallel_in_24pins_pin_directions[] =
	H15_PIN_DIRECTIONS(IN, IN, IN, IN, IN, IN, IN, IN, IN, IN, IN, IN);

static const unsigned gpio8_pins[] = { 8 };
static const unsigned gpio8_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned uart2_3_pins[] = { 8, 9 };
static const unsigned uart2_3_pin_directions[] = H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned gpio9_pins[] = { 9 };
static const unsigned gpio9_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned sdio0_gp_in_pins[] = { 9 };
static const unsigned sdio0_gp_in_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned gpio10_pins[] = { 10 };
static const unsigned gpio10_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned sdio0_CD_in_pins[] = { 10 };
static const unsigned sdio0_CD_in_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned uart0_cts_rts_0_pins[] = { 10, 11 };
static const unsigned uart0_cts_rts_0_pin_directions[] =
	H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned gpio11_pins[] = { 11 };
static const unsigned gpio11_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned usb_overcurrent_in_pins[] = { 11 };
static const unsigned usb_overcurrent_in_pin_directions[] =
	H15_PIN_DIRECTIONS(IN);

static const unsigned gpio12_pins[] = { 12 };
static const unsigned gpio12_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned usb_drive_vbus_out_2_pins[] = { 12 };
static const unsigned usb_drive_vbus_out_2_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned safety_out1_3_pins[] = { 12 };
static const unsigned safety_out1_3_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned uart3_3_pins[] = { 12, 13 };
static const unsigned uart3_3_pin_directions[] = H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned gpio13_pins[] = { 13 };
static const unsigned gpio13_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned sdio1_gp_in_pins[] = { 13 };
static const unsigned sdio1_gp_in_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned i2c1_current_src_en_out_2_pins[] = { 13 };
static const unsigned i2c1_current_src_en_out_2_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio14_pins[] = { 14 };
static const unsigned gpio14_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned sdio1_CD_in_pins[] = { 14 };
static const unsigned sdio1_CD_in_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned safety_out0_3_pins[] = { 14 };
static const unsigned safety_out0_3_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio15_pins[] = { 15 };
static const unsigned gpio15_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned pcie_mperst_out_pins[] = { 15 };
static const unsigned pcie_mperst_out_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio16_pins[] = { 16 };
static const unsigned gpio16_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned uart2_4_pins[] = { 16, 17 };
static const unsigned uart2_4_pin_directions[] = H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned gpio17_pins[] = { 17 };
static const unsigned gpio17_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned gpio18_pins[] = { 18 };
static const unsigned gpio18_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned timer_ext_in0_pins[] = { 18 };
static const unsigned timer_ext_in0_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned gpio19_pins[] = { 19 };
static const unsigned gpio19_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned timer_ext_in1_pins[] = { 19 };
static const unsigned timer_ext_in1_pin_directions[] = H15_PIN_DIRECTIONS(IN);

static const unsigned gpio20_pins[] = { 20 };
static const unsigned gpio20_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned gpio21_pins[] = { 21 };
static const unsigned gpio21_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c3_1_pins[] = { 20, 21 };
static const unsigned i2c3_1_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR, OUT);

static const unsigned pwm3_pins[] = { 20, 21 };
static const unsigned pwm3_pin_directions[] = H15_PIN_DIRECTIONS(OUT, OUT);

static const unsigned gpio22_pins[] = { 22 };
static const unsigned gpio22_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned gpio23_pins[] = { 23 };
static const unsigned gpio23_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c2_1_pins[] = { 22, 23 };
static const unsigned i2c2_1_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR, OUT);

static const unsigned gpio24_pins[] = { 24 };
static const unsigned gpio24_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c2_current_src_en_out_3_pins[] = { 24 };
static const unsigned i2c2_current_src_en_out_3_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned i2c0_current_src_en_out_2_pins[] = { 24 };
static const unsigned i2c0_current_src_en_out_2_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio25_pins[] = { 25 };
static const unsigned gpio25_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c0_current_src_en_out_3_pins[] = { 25 };
static const unsigned i2c0_current_src_en_out_3_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned i2c1_current_src_en_out_3_pins[] = { 25 };
static const unsigned i2c1_current_src_en_out_3_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio26_pins[] = { 26 };
static const unsigned gpio26_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned i2c1_current_src_en_out_4_pins[] = { 26 };
static const unsigned i2c1_current_src_en_out_4_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio27_pins[] = { 27 };
static const unsigned gpio27_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned uart1_cts_rts_1_pins[] = { 26, 27 };
static const unsigned uart1_cts_rts_1_pin_directions[] =
	H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned sdio1_gp_out_3_pins[] = { 26 };
static const unsigned sdio1_gp_out_3_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio28_pins[] = { 28 };
static const unsigned gpio28_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned pwm4_pins[] = { 27, 28 };
static const unsigned pwm4_pin_directions[] = H15_PIN_DIRECTIONS(OUT, OUT);

static const unsigned cpu_trace_clk_pins[] = { 27 };
static const unsigned cpu_trace_clk_pin_directions[] = H15_PIN_DIRECTIONS(OUT);

static const unsigned gpio29_pins[] = { 29 };
static const unsigned gpio29_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned uart2_cts_rts_pins[] = { 28, 29 };
static const unsigned uart2_cts_rts_pin_directions[] =
	H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned gpio30_pins[] = { 30 };
static const unsigned gpio30_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned cpu_trace_data_pins[] = { 28, 29, 30, 31 };
static const unsigned cpu_trace_data_pin_directions[] =
	H15_PIN_DIRECTIONS(OUT, OUT, OUT, OUT);

static const unsigned gpio31_pins[] = { 31 };
static const unsigned gpio31_pin_directions[] = H15_PIN_DIRECTIONS(BIDIR);

static const unsigned uart0_cts_rts_1_pins[] = { 30, 31 };
static const unsigned uart0_cts_rts_1_pin_directions[] =
	H15_PIN_DIRECTIONS(IN, OUT);

static const unsigned debug_out_pins[] = { 0,  1,  2,  3,  4,  5,  6,  7,
					   8,  9,  10, 11, 12, 13, 14, 15,
					   16, 17, 18, 19, 20, 21, 22, 23,
					   24, 25, 26, 27, 28, 29, 30, 31 };

static const struct h15_pin_set_modes initial_debug_bus_out_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(0)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(0)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(0)),
};

static const struct h15_pin_set_modes gpio0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(2)),
};

static const struct h15_pin_set_modes i2c0_current_src_en_out_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(3)),
};

static const struct h15_pin_set_modes flash_cs_out_pad_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(4)),
};

static const struct h15_pin_set_modes pwm0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(5)),
};

static const struct h15_pin_set_modes safety_out0_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(6)),
};

static const struct h15_pin_set_modes sdio0_gp_out_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(7)),
};

static const struct h15_pin_set_modes uart2_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(6)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(8))
};

static const struct h15_pin_set_modes i2c2_current_src_en_out_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(9)),
};

static const struct h15_pin_set_modes gpio1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(2)),
};

static const struct h15_pin_set_modes i2c1_current_src_en_out_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(3)),
};

static const struct h15_pin_set_modes flash_cs_out_pad_3_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(4)),
};

static const struct h15_pin_set_modes safety_out1_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(6)),
};

static const struct h15_pin_set_modes sdio1_gp_out_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(7)),
};

static const struct h15_pin_set_modes uart3_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(6)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(8))
};

static const struct h15_pin_set_modes i2c3_current_src_en_out_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__1_0, BIT(9)),
};

static const struct h15_pin_set_modes gpio2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(2)),
};

static const struct h15_pin_set_modes i2c0_current_src_en_out_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(3)),
};

static const struct h15_pin_set_modes isp_flash_trig_out_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(4)),
};

static const struct h15_pin_set_modes pwm1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(5)),
};

static const struct h15_pin_set_modes safety_out0_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(6)),
};

static const struct h15_pin_set_modes usb_drive_vbus_out_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(7)),
};

static const struct h15_pin_set_modes uart2_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(6)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(8))
};

static const struct h15_pin_set_modes i2c2_current_src_en_out_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(9)),
};

static const struct h15_pin_set_modes gpio3_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(2)),
};

static const struct h15_pin_set_modes i2c1_current_src_en_out_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(3)),
};

static const struct h15_pin_set_modes isp_pre_flash_out_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(4)),
};

static const struct h15_pin_set_modes safety_out1_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(6)),
};

static const struct h15_pin_set_modes sdio1_gp_out_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(7)),
};

static const struct h15_pin_set_modes uart3_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(6)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(8))
};

static const struct h15_pin_set_modes i2c3_current_src_en_out_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__3_2, BIT(9)),
};

static const struct h15_pin_set_modes gpio4_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, (BIT(2) | BIT(3) | BIT(4))),
};

static const struct h15_pin_set_modes pwm2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(5)),
};

static const struct h15_pin_set_modes safety_out0_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(6)),
};

static const struct h15_pin_set_modes sdio0_gp_out_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(7)),
};

static const struct h15_pin_set_modes uart2_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(6)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(8))
};

static const struct h15_pin_set_modes i2c2_current_src_en_out_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(9)),
};

static const struct h15_pin_set_modes gpio5_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(2)),
};

static const struct h15_pin_set_modes i2c3_current_src_en_out_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(3)),
};

static const struct h15_pin_set_modes I2S_SD_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(4)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(0)),
};

static const struct h15_pin_set_modes safety_out1_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(6)),
};

static const struct h15_pin_set_modes sdio1_gp_out_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(7)),
};

static const struct h15_pin_set_modes uart3_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(6)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(8))
};

static const struct h15_pin_set_modes usb_drive_vbus_out_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__5_4, BIT(9)),
};

static const struct h15_pin_set_modes gpio6_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(2)),
};

static const struct h15_pin_set_modes uart1_cts_rts_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(0))
};

static const struct h15_pin_set_modes sdio0_wp_in_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(3))
};

static const struct h15_pin_set_modes parallel_in_16pins_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(4)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(4))
};

static const struct h15_pin_set_modes timer_ext_in2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(5))
};

static const struct h15_pin_set_modes i2c2_0_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__7_6, BIT(7)) };

static const struct h15_pin_set_modes i2c3_0_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__7_6, BIT(8)) };

static const struct h15_pin_set_modes uart3_cts_rts_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(9))
};

static const struct h15_pin_set_modes gpio7_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(2)),
};

static const struct h15_pin_set_modes sdio1_wp_in_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(3))
};

static const struct h15_pin_set_modes timer_ext_in3_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(5))
};

static const struct h15_pin_set_modes parallel_in_24pins_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__7_6, BIT(4)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, BIT(4)),
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(4))
};

static const struct h15_pin_set_modes gpio8_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8,
			  (BIT(0) | BIT(2) | BIT(3) | BIT(5) | BIT(6))),
};

static const struct h15_pin_set_modes uart2_3_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__15_8, BIT(7)) };

static const struct h15_pin_set_modes sdio0_gp_in_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8,
			  (BIT(0) | BIT(3) | BIT(5) | BIT(6)))
};

static const struct h15_pin_set_modes sdio0_CD_in_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8,
			  (BIT(0) | BIT(3) | BIT(5) | BIT(6)))
};

static const struct h15_pin_set_modes uart0_cts_rts_0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, BIT(7))
};

static const struct h15_pin_set_modes gpio9_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, BIT(2)),
};

static const struct h15_pin_set_modes gpio10_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, BIT(2)),
};

static const struct h15_pin_set_modes gpio11_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, (BIT(2) | BIT(5))),
};

static const struct h15_pin_set_modes gpio12_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, BIT(2)),
};

static const struct h15_pin_set_modes gpio13_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, (BIT(2) | BIT(5))),
};

static const struct h15_pin_set_modes gpio14_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, (BIT(2) | BIT(3) | BIT(7))),
};

static const struct h15_pin_set_modes gpio15_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, (BIT(2) | BIT(6))),
};

static const struct h15_pin_set_modes usb_overcurrent_in_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, (BIT(0) | BIT(3) | BIT(6)))
};

static const struct h15_pin_set_modes usb_drive_vbus_out_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, (BIT(0) | BIT(3) | BIT(6)))
};

static const struct h15_pin_set_modes safety_out1_3_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, BIT(5))
};

static const struct h15_pin_set_modes uart3_3_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__15_8, BIT(7)) };

static const struct h15_pin_set_modes sdio1_gp_in_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, (BIT(0) | BIT(6)))
};

static const struct h15_pin_set_modes i2c1_current_src_en_out_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, BIT(3))
};

static const struct h15_pin_set_modes sdio1_CD_in_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, BIT(0))
};

static const struct h15_pin_set_modes safety_out0_3_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8, BIT(5))
};

static const struct h15_pin_set_modes pcie_mperst_out_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__15_8,
			  (BIT(0) | BIT(3) | BIT(5) | BIT(7)))
};

static const struct h15_pin_set_modes gpio16_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(2) | BIT(3) | BIT(6))) };

static const struct h15_pin_set_modes gpio17_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(2) | BIT(3) | BIT(6))) };

static const struct h15_pin_set_modes gpio18_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(2)),
};

static const struct h15_pin_set_modes gpio19_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(2)),
};

static const struct h15_pin_set_modes gpio20_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(2) | BIT(3) | BIT(6))) };

static const struct h15_pin_set_modes gpio21_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(2) | BIT(3) | BIT(6))) };

static const struct h15_pin_set_modes gpio22_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(2) | BIT(6))) };

static const struct h15_pin_set_modes gpio23_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(2) | BIT(6))) };

static const struct h15_pin_set_modes gpio24_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(2) | BIT(6))) };

static const struct h15_pin_set_modes gpio25_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, BIT(2)) };

static const struct h15_pin_set_modes gpio26_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, BIT(2)) };

static const struct h15_pin_set_modes gpio27_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(2) | BIT(3) | BIT(6))) };

static const struct h15_pin_set_modes gpio28_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(2) | BIT(3) | BIT(6))) };

static const struct h15_pin_set_modes gpio29_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(2) | BIT(3) | BIT(6))) };

static const struct h15_pin_set_modes gpio30_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(2) | BIT(6))) };

static const struct h15_pin_set_modes gpio31_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(2) | BIT(6))) };

static const struct h15_pin_set_modes uart2_4_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, BIT(5)) };

static const struct h15_pin_set_modes timer_ext_in0_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(0))
};

static const struct h15_pin_set_modes timer_ext_in1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(0))
};

static const struct h15_pin_set_modes i2c3_1_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, BIT(5)) };

static const struct h15_pin_set_modes pwm3_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, BIT(7)) };

static const struct h15_pin_set_modes i2c2_1_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, (BIT(3) | BIT(5))) };

static const struct h15_pin_set_modes i2c2_current_src_en_out_3_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(3))
};

static const struct h15_pin_set_modes i2c0_current_src_en_out_2_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(5))
};

static const struct h15_pin_set_modes i2c0_current_src_en_out_3_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(3) | BIT(6)))
};

static const struct h15_pin_set_modes i2c1_current_src_en_out_3_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(5))
};

static const struct h15_pin_set_modes i2c1_current_src_en_out_4_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(3) | BIT(6)))
};

static const struct h15_pin_set_modes uart1_cts_rts_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(5))
};

static const struct h15_pin_set_modes sdio1_gp_out_3_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(7))
};

static const struct h15_pin_set_modes pwm4_set_modes[] = { H15_PIN_SET_MODES(
	H15_PIN_SET_VALUE__31_16, BIT(0)) };

static const struct h15_pin_set_modes cpu_trace_clk_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(7))
};

static const struct h15_pin_set_modes uart2_cts_rts_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(5))
};

static const struct h15_pin_set_modes cpu_trace_data_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, BIT(7))
};

static const struct h15_pin_set_modes uart0_cts_rts_1_set_modes[] = {
	H15_PIN_SET_MODES(H15_PIN_SET_VALUE__31_16, (BIT(0) | BIT(3) | BIT(5)))
};

static const struct h15_pin_group h15_pin_groups[] = {
	/*
	* The first 32 pin groups are gpio pin groups.
	*/
	H15_PIN_GROUP(gpio0),
	H15_PIN_GROUP(gpio1),
	H15_PIN_GROUP(gpio2),
	H15_PIN_GROUP(gpio3),
	H15_PIN_GROUP(gpio4),
	H15_PIN_GROUP(gpio5),
	H15_PIN_GROUP(gpio6),
	H15_PIN_GROUP(gpio7),
	H15_PIN_GROUP(gpio8),
	H15_PIN_GROUP(gpio9),
	H15_PIN_GROUP(gpio10),
	H15_PIN_GROUP(gpio11),
	H15_PIN_GROUP(gpio12),
	H15_PIN_GROUP(gpio13),
	H15_PIN_GROUP(gpio14),
	H15_PIN_GROUP(gpio15),
	H15_PIN_GROUP(gpio16),
	H15_PIN_GROUP(gpio17),
	H15_PIN_GROUP(gpio18),
	H15_PIN_GROUP(gpio19),
	H15_PIN_GROUP(gpio20),
	H15_PIN_GROUP(gpio21),
	H15_PIN_GROUP(gpio22),
	H15_PIN_GROUP(gpio23),
	H15_PIN_GROUP(gpio24),
	H15_PIN_GROUP(gpio25),
	H15_PIN_GROUP(gpio26),
	H15_PIN_GROUP(gpio27),
	H15_PIN_GROUP(gpio28),
	H15_PIN_GROUP(gpio29),
	H15_PIN_GROUP(gpio30),
	H15_PIN_GROUP(gpio31),

	H15_PIN_GROUP(I2S_SD),
	H15_PIN_GROUP(cpu_trace_clk),
	H15_PIN_GROUP(cpu_trace_data),
	H15_PIN_GROUP(flash_cs_out_pad_2),
	H15_PIN_GROUP(flash_cs_out_pad_3),
	H15_PIN_GROUP(i2c0_current_src_en_out_0),
	H15_PIN_GROUP(i2c0_current_src_en_out_1),
	H15_PIN_GROUP(i2c0_current_src_en_out_2),
	H15_PIN_GROUP(i2c0_current_src_en_out_3),
	H15_PIN_GROUP(i2c1_current_src_en_out_0),
	H15_PIN_GROUP(i2c1_current_src_en_out_1),
	H15_PIN_GROUP(i2c1_current_src_en_out_2),
	H15_PIN_GROUP(i2c1_current_src_en_out_3),
	H15_PIN_GROUP(i2c1_current_src_en_out_4),
	H15_PIN_GROUP(i2c2_0),
	H15_PIN_GROUP(i2c2_1),
	H15_PIN_GROUP(i2c2_current_src_en_out_0),
	H15_PIN_GROUP(i2c2_current_src_en_out_1),
	H15_PIN_GROUP(i2c2_current_src_en_out_2),
	H15_PIN_GROUP(i2c2_current_src_en_out_3),
	H15_PIN_GROUP(i2c3_0),
	H15_PIN_GROUP(i2c3_1),
	H15_PIN_GROUP(i2c3_current_src_en_out_0),
	H15_PIN_GROUP(i2c3_current_src_en_out_1),
	H15_PIN_GROUP(i2c3_current_src_en_out_2),
	H15_PIN_GROUP(initial_debug_bus_out),
	H15_PIN_GROUP(isp_flash_trig_out),
	H15_PIN_GROUP(isp_pre_flash_out),
	H15_PIN_GROUP(parallel_in_16pins),
	H15_PIN_GROUP(parallel_in_24pins),
	H15_PIN_GROUP(pcie_mperst_out),
	H15_PIN_GROUP(pwm0),
	H15_PIN_GROUP(pwm1),
	H15_PIN_GROUP(pwm2),
	H15_PIN_GROUP(pwm3),
	H15_PIN_GROUP(pwm4),
	H15_PIN_GROUP(safety_out0_0),
	H15_PIN_GROUP(safety_out0_1),
	H15_PIN_GROUP(safety_out0_2),
	H15_PIN_GROUP(safety_out0_2),
	H15_PIN_GROUP(safety_out1_0),
	H15_PIN_GROUP(safety_out1_1),
	H15_PIN_GROUP(safety_out1_2),
	H15_PIN_GROUP(safety_out1_3),
	H15_PIN_GROUP(sdio0_CD_in),
	H15_PIN_GROUP(sdio0_gp_in),
	H15_PIN_GROUP(sdio0_gp_out_0),
	H15_PIN_GROUP(sdio0_gp_out_1),
	H15_PIN_GROUP(sdio0_wp_in),
	H15_PIN_GROUP(sdio1_CD_in),
	H15_PIN_GROUP(sdio1_gp_in),
	H15_PIN_GROUP(sdio1_gp_out_0),
	H15_PIN_GROUP(sdio1_gp_out_1),
	H15_PIN_GROUP(sdio1_gp_out_2),
	H15_PIN_GROUP(sdio1_gp_out_3),
	H15_PIN_GROUP(sdio1_wp_in),
	H15_PIN_GROUP(timer_ext_in0),
	H15_PIN_GROUP(timer_ext_in1),
	H15_PIN_GROUP(timer_ext_in2),
	H15_PIN_GROUP(timer_ext_in3),
	H15_PIN_GROUP(uart0_cts_rts_0),
	H15_PIN_GROUP(uart0_cts_rts_1),
	H15_PIN_GROUP(uart1_cts_rts_0),
	H15_PIN_GROUP(uart1_cts_rts_1),
	H15_PIN_GROUP(uart2_0),
	H15_PIN_GROUP(uart2_1),
	H15_PIN_GROUP(uart2_2),
	H15_PIN_GROUP(uart2_3),
	H15_PIN_GROUP(uart2_4),
	H15_PIN_GROUP(uart2_cts_rts),
	H15_PIN_GROUP(uart3_0),
	H15_PIN_GROUP(uart3_1),
	H15_PIN_GROUP(uart3_2),
	H15_PIN_GROUP(uart3_3),
	H15_PIN_GROUP(uart3_cts_rts),
	H15_PIN_GROUP(usb_drive_vbus_out_0),
	H15_PIN_GROUP(usb_drive_vbus_out_1),
	H15_PIN_GROUP(usb_drive_vbus_out_2),
	H15_PIN_GROUP(usb_overcurrent_in),
};

static const char *const initial_debug_bus_out_grps[] = {
	"initial_debug_bus_out_grp"
};

static const char *const i2c0_current_src_en_out_grps[] = {
	"i2c0_current_src_en_out_0_grp", "i2c0_current_src_en_out_1_grp",
	"i2c0_current_src_en_out_2_grp", "i2c0_current_src_en_out_3_grp"
};

static const char *const flash_cs_out_pad_2_grps[] = {
	"flash_cs_out_pad_2_grp"
};

static const char *const pwm0_grps[] = { "pwm0_grp" };

static const char *const safety_out0_grps[] = { "safety_out0_0_grp",
						"safety_out0_1_grp",
						"safety_out0_2_grp",
						"safety_out0_3_grp" };

static const char *const sdio0_gp_out_grps[] = { "sdio0_gp_out_0_grp",
						 "sdio0_gp_out_1_grp" };

static const char *const uart2_grps[] = { "uart2_0_grp", "uart2_1_grp",
					  "uart2_2_grp", "uart2_3_grp",
					  "uart2_4_grp" };

static const char *const i2c2_current_src_en_out_grps[] = {
	"i2c2_current_src_en_out_0_grp", "i2c2_current_src_en_out_1_grp",
	"i2c2_current_src_en_out_2_grp", "i2c2_current_src_en_out_3_grp"
};

static const char *const i2c1_current_src_en_out_grps[] = {
	"i2c1_current_src_en_out_0_grp", "i2c1_current_src_en_out_1_grp",
	"i2c1_current_src_en_out_2_grp", "i2c1_current_src_en_out_3_grp",
	"i2c1_current_src_en_out_4_grp"
};

static const char *const flash_cs_out_pad_3_grps[] = {
	"flash_cs_out_pad_3_grp"
};

static const char *const safety_out1_grps[] = { "safety_out1_0_grp",
						"safety_out1_1_grp",
						"safety_out1_2_grp",
						"safety_out1_3_grp" };

static const char *const sdio1_gp_out_grps[] = { "sdio1_gp_out_0_grp",
						 "sdio1_gp_out_1_grp",
						 "sdio1_gp_out_2_grp",
						 "sdio1_gp_out_3_grp" };

static const char *const uart3_grps[] = { "uart3_0_grp", "uart3_1_grp",
					  "uart3_2_grp", "uart3_3_grp" };

static const char *const i2c3_current_src_en_out_grps[] = {
	"i2c3_current_src_en_out_0_grp", "i2c3_current_src_en_out_1_grp",
	"i2c3_current_src_en_out_2_grp"
};

static const char *const isp_flash_trig_out_grps[] = {
	"isp_flash_trig_out_grp"
};

static const char *const pwm1_grps[] = { "pwm1_grp" };

static const char *const usb_drive_vbus_out_grps[] = {
	"usb_drive_vbus_out_0_grp", "usb_drive_vbus_out_1_grp",
	"usb_drive_vbus_out_2_grp"
};

static const char *const isp_pre_flash_out_grps[] = { "isp_pre_flash_out_grp" };

static const char *const pwm2_grps[] = { "pwm2_grp" };

static const char *const gpio_grps[] = {
	"gpio0_grp",  "gpio1_grp",  "gpio2_grp",  "gpio3_grp",	"gpio4_grp",
	"gpio5_grp",  "gpio6_grp",  "gpio7_grp",  "gpio8_grp",	"gpio9_grp",
	"gpio10_grp", "gpio11_grp", "gpio12_grp", "gpio13_grp", "gpio14_grp",
	"gpio15_grp", "gpio16_grp", "gpio17_grp", "gpio18_grp", "gpio19_grp",
	"gpio20_grp", "gpio21_grp", "gpio22_grp", "gpio23_grp", "gpio24_grp",
	"gpio25_grp", "gpio26_grp", "gpio27_grp", "gpio28_grp", "gpio29_grp",
	"gpio30_grp", "gpio31_grp"
};

static const char *const I2S_SD_grps[] = { "I2S_SD_grp" };

static const char *const uart1_cts_rts_grps[] = { "uart1_cts_rts_0_grp",
						  "uart1_cts_rts_1_grp" };

static const char *const sdio0_wp_in_grps[] = { "sdio0_wp_in_grp" };

static const char *const parallel_in_16pins_grps[] = {
	"parallel_in_16pins_grp"
};

static const char *const timer_ext_in2_grps[] = { "timer_ext_in2_grp" };

static const char *const i2c2_grps[] = { "i2c2_0_grp", "i2c2_1_grp" };

static const char *const i2c3_grps[] = { "i2c3_0_grp", "i2c3_1_grp" };

static const char *const uart3_cts_rts_grps[] = { "uart3_cts_rts_grp" };

static const char *const sdio1_wp_in_grps[] = { "sdio1_wp_in_grp" };

static const char *const timer_ext_in3_grps[] = { "timer_ext_in3_grp" };

static const char *const parallel_in_24pins_grps[] = {
	"parallel_in_24pins_grp"
};

static const char *const sdio0_gp_in_grps[] = { "sdio0_gp_in_grp" };

static const char *const sdio0_CD_in_grps[] = { "sdio0_CD_in_grp" };

static const char *const uart0_cts_rts_grps[] = { "uart0_cts_rts_0_grp",
						  "uart0_cts_rts_1_grp" };

static const char *const usb_overcurrent_in_grps[] = {
	"usb_overcurrent_in_grp"
};

static const char *const sdio1_gp_in_grps[] = { "sdio1_gp_in_grp" };

static const char *const sdio1_CD_in_grps[] = { "sdio1_CD_in_grp" };

static const char *const pcie_mperst_out_grps[] = { "pcie_mperst_out_grp" };

static const char *const timer_ext_in0_grps[] = { "timer_ext_in0_grp" };

static const char *const timer_ext_in1_grps[] = { "timer_ext_in1_grp" };

static const char *const pwm3_grps[] = { "pwm3_grp" };

static const char *const pwm4_grps[] = { "pwm4_grp" };

static const char *const cpu_trace_clk_grps[] = { "cpu_trace_clk_grp" };

static const char *const uart2_cts_rts_grps[] = { "uart2_cts_rts_grp" };

static const char *const cpu_trace_data_grps[] = { "cpu_trace_data_grp" };

static const struct h15_pin_function h15_pin_functions[] = {
	H15_PIN_FUNCTION(gpio),

	H15_PIN_FUNCTION(uart2),
	H15_PIN_FUNCTION(uart3),

	H15_PIN_FUNCTION(uart0_cts_rts),
	H15_PIN_FUNCTION(uart1_cts_rts),
	H15_PIN_FUNCTION(uart2_cts_rts),
	H15_PIN_FUNCTION(uart3_cts_rts),

	H15_PIN_FUNCTION(i2c0_current_src_en_out),
	H15_PIN_FUNCTION(i2c1_current_src_en_out),
	H15_PIN_FUNCTION(i2c2_current_src_en_out),
	H15_PIN_FUNCTION(i2c3_current_src_en_out),

	H15_PIN_FUNCTION(pwm0),
	H15_PIN_FUNCTION(pwm1),
	H15_PIN_FUNCTION(pwm2),
	H15_PIN_FUNCTION(pwm3),
	H15_PIN_FUNCTION(pwm4),

	H15_PIN_FUNCTION(i2c2),
	H15_PIN_FUNCTION(i2c3),

	H15_PIN_FUNCTION(parallel_in_16pins),
	H15_PIN_FUNCTION(parallel_in_24pins),

	H15_PIN_FUNCTION(timer_ext_in0),
	H15_PIN_FUNCTION(timer_ext_in2),
	H15_PIN_FUNCTION(timer_ext_in3),

	H15_PIN_FUNCTION(safety_out0),
	H15_PIN_FUNCTION(safety_out1),

	H15_PIN_FUNCTION(sdio0_gp_out),
	H15_PIN_FUNCTION(sdio0_gp_in),
	H15_PIN_FUNCTION(sdio0_CD_in),
	H15_PIN_FUNCTION(sdio0_wp_in),

	H15_PIN_FUNCTION(sdio1_gp_out),
	H15_PIN_FUNCTION(sdio1_gp_in),
	H15_PIN_FUNCTION(sdio1_CD_in),
	H15_PIN_FUNCTION(sdio1_wp_in),

	H15_PIN_FUNCTION(initial_debug_bus_out),
	H15_PIN_FUNCTION(flash_cs_out_pad_2),
	H15_PIN_FUNCTION(flash_cs_out_pad_3),
	H15_PIN_FUNCTION(isp_flash_trig_out),
	H15_PIN_FUNCTION(isp_pre_flash_out),
	H15_PIN_FUNCTION(I2S_SD),

	H15_PIN_FUNCTION(usb_overcurrent_in),

	H15_PIN_FUNCTION(pcie_mperst_out),

	H15_PIN_FUNCTION(cpu_trace_clk),
	H15_PIN_FUNCTION(cpu_trace_data),
};

struct h15_pin_set h15_pin_sets[H15_PIN_SET_VALUE__COUNT] = {
	H15_PIN_SET(H15_PIN_SET_VALUE__1_0, 10),
	H15_PIN_SET(H15_PIN_SET_VALUE__3_2, 10),
	H15_PIN_SET(H15_PIN_SET_VALUE__5_4, 10),
	H15_PIN_SET(H15_PIN_SET_VALUE__7_6, 10),
	H15_PIN_SET(H15_PIN_SET_VALUE__15_8, 8),
	H15_PIN_SET(H15_PIN_SET_VALUE__31_16, 8),
};

/*
 * The shift in the pads_pinmux_mode for every pins set.
*/
int pads_pinmux_mode_shift_value[H15_PIN_SET_VALUE__COUNT] = {
	[H15_PIN_SET_VALUE__1_0] = 0,	[H15_PIN_SET_VALUE__3_2] = 4,
	[H15_PIN_SET_VALUE__5_4] = 8,	[H15_PIN_SET_VALUE__7_6] = 12,
	[H15_PIN_SET_VALUE__15_8] = 16, [H15_PIN_SET_VALUE__31_16] = 20,
};

int pads_pinmux_mode_reg_mask_value[H15_PIN_SET_VALUE__COUNT] = {
	[H15_PIN_SET_VALUE__1_0] = 0xf,	 [H15_PIN_SET_VALUE__3_2] = 0xf,
	[H15_PIN_SET_VALUE__5_4] = 0xf,	 [H15_PIN_SET_VALUE__7_6] = 0xf,
	[H15_PIN_SET_VALUE__15_8] = 0x7, [H15_PIN_SET_VALUE__31_16] = 0x7
};

#endif /* _PINCTRL_HAILO15_PROPERTIES_H */