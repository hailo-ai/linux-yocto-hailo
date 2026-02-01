
#ifndef _PINCTRL_HAILO12L_DESCRIPTIONS_H
#define _PINCTRL_HAILO12L_DESCRIPTIONS_H

#include "pinctrl-hailo12l.h"

#define H12L_PINMUX_PIN_COUNT (62) // 62 for mars
#define H12L_IS_SDIO_PIN(_pin) (((_pin) >= 22 && (_pin) <= 27)) 

/* Pin capability constants */
#define MUXABLE true
#define NOT_MUXABLE false

/* Relevent only for RESET_N,OSC_XIN,OSC_XOUT*/
#define MUX_INDEX_DONT_CARE 0xFF
#define PAD_INDEX_DONT_CARE 0

#define H12L_PIN_GROUP(_group_name, _pin_offset, _mode)                         \
	{                                                                      \
		.name = __stringify(_group_name) "_grp", .pin = (_pin_offset), \
		.mode = (_mode),                                               \
	}

//this has to be the same as h12l_pin_functions in terms of the _0 (at the end of the name)

static const struct h12l_pin_group h12l_pin_groups[] = {
	H12L_PIN_GROUP(i2c0_scl_0, 0, 0),
	H12L_PIN_GROUP(i2c0_sda_0, 1, 0),
	H12L_PIN_GROUP(i2c1_scl_0, 2, 0),
	H12L_PIN_GROUP(i2c1_sda_0, 3, 0),
	H12L_PIN_GROUP(uart0_rxd_in_0, 4, 0),
	H12L_PIN_GROUP(uart1_cts_in_0, 4, 1),
	H12L_PIN_GROUP(uart4_rxd_in_0, 4, 2),
	H12L_PIN_GROUP(uart4_cts_in_0, 4, 3),
	H12L_PIN_GROUP(uart0_txd_out_0, 5, 0),
	H12L_PIN_GROUP(uart1_rts_out_0, 5, 1),
	H12L_PIN_GROUP(uart4_txd_out_0, 5, 2),
	H12L_PIN_GROUP(uart4_rts_out_0, 5, 3),
	H12L_PIN_GROUP(uart1_rxd_in_0, 6, 0),
	H12L_PIN_GROUP(uart0_cts_in_0, 6, 1),
	H12L_PIN_GROUP(uart4_rxd_in_1, 6, 2),
	H12L_PIN_GROUP(uart4_cts_in_1, 6, 3),
	H12L_PIN_GROUP(uart1_txd_out_0, 7, 0),
	H12L_PIN_GROUP(uart0_rts_out_0, 7, 1),
	H12L_PIN_GROUP(uart4_txd_out_1, 7, 2),
	H12L_PIN_GROUP(uart4_rts_out_1, 7, 3),
	H12L_PIN_GROUP(i2s0_sck_0, 8, 0),
	H12L_PIN_GROUP(gpio12_0, 8, 1),
	H12L_PIN_GROUP(i2s0_sdi0_in_0, 9, 0),
	H12L_PIN_GROUP(gpio13_0, 9, 1),
	H12L_PIN_GROUP(i2s0_sdo0_out_0, 10, 0),
	H12L_PIN_GROUP(gpio14_0, 10, 1),
	H12L_PIN_GROUP(i2s0_ws_0, 11, 0),
	H12L_PIN_GROUP(gpio15_0, 11, 1),
	H12L_PIN_GROUP(flash_spi_cs_0_n_out, 12, 0),
	H12L_PIN_GROUP(spi0_cs0_n_out_0, 12, 1),
	H12L_PIN_GROUP(spi1_cs0_n_out_0, 12, 2),
	H12L_PIN_GROUP(spi2_cs0_n_out_0, 12, 3),
	H12L_PIN_GROUP(flash_spi_dq_0, 13, 0),
	H12L_PIN_GROUP(spi0_miso_in_0, 13, 1),
	H12L_PIN_GROUP(spi1_miso_in_0, 13, 2),
	H12L_PIN_GROUP(spi2_miso_in_0, 13, 3),
	H12L_PIN_GROUP(flash_spi_dq_1, 14, 0),
	H12L_PIN_GROUP(spi0_mosi_out_0, 14, 1),
	H12L_PIN_GROUP(spi1_mosi_out_0, 14, 2),
	H12L_PIN_GROUP(spi2_mosi_out, 14, 3),
	H12L_PIN_GROUP(flash_spi_dq_2, 15, 0),
	H12L_PIN_GROUP(flash_spi_dq_3, 16, 0),
	H12L_PIN_GROUP(flash_spi_reset_n_out, 17, 0),
	H12L_PIN_GROUP(flash_spi_sclk_out, 18, 0),
	H12L_PIN_GROUP(spi0_sclk_out_0, 18, 1),
	H12L_PIN_GROUP(spi1_sclk_out_0, 18, 2),
	H12L_PIN_GROUP(spi2_sclk_out_0, 18, 3),
	H12L_PIN_GROUP(pcie_wake_n, 19, 0),
	H12L_PIN_GROUP(gpio7_0, 19, 1),
	H12L_PIN_GROUP(pcie_clkreq_n, 20, 0),
	H12L_PIN_GROUP(gpio14_1, 20, 1),
	H12L_PIN_GROUP(pcie_perst_n_in, 21, 0),
	H12L_PIN_GROUP(gpio15_1, 21, 1),
	H12L_PIN_GROUP(SDIO_DATA0, 22, 0),
	H12L_PIN_GROUP(SDIO_DATA1, 23, 0),
	H12L_PIN_GROUP(SDIO_DATA2, 24, 0),
	H12L_PIN_GROUP(SDIO_DATA3, 25, 0),
	H12L_PIN_GROUP(SDIO_CMD, 26, 0),
	H12L_PIN_GROUP(SDIO_SDCLK, 27, 0),
	H12L_PIN_GROUP(safety_fatal_n_out, 28, 0),
	H12L_PIN_GROUP(jtag_tck_in, 29, 0),
	H12L_PIN_GROUP(gpio16_0, 29, 1),
	H12L_PIN_GROUP(jtag_tdi_in, 30, 0),
	H12L_PIN_GROUP(gpio17_0, 30, 1),
	H12L_PIN_GROUP(jtag_tdo_out, 31, 0),
	H12L_PIN_GROUP(gpio18_0, 31, 1),
	H12L_PIN_GROUP(jtag_tms, 32, 0),
	H12L_PIN_GROUP(gpio19_0, 32, 1),
	H12L_PIN_GROUP(jtag_trstn_in, 33, 0),
	H12L_PIN_GROUP(gpio20_0, 33, 1),
	H12L_PIN_GROUP(gpio0, 34, 0),
	H12L_PIN_GROUP(uart0_cts_in_1, 34, 1),
	H12L_PIN_GROUP(pwm0_out_0, 34, 2),
	H12L_PIN_GROUP(sdio_uhs_sel_out_0, 34, 3),
	H12L_PIN_GROUP(aon_pwr_cmd_in_0, 34, 4),
	H12L_PIN_GROUP(spi2_cs0_n_out_1, 34, 5),
	H12L_PIN_GROUP(uart1_cts_in_1, 34, 6),
	H12L_PIN_GROUP(i2s1_mclk_0, 34, 7),
	H12L_PIN_GROUP(aon_pwr_ctrl_out_0, 34, 9),
	H12L_PIN_GROUP(debug_out0, 34, 11),
	H12L_PIN_GROUP(VDD_SWITCH_PG, 35, 0),
	H12L_PIN_GROUP(gpio2, 36, 0),
	H12L_PIN_GROUP(uart1_cts_in_2, 36, 1),
	H12L_PIN_GROUP(pwm2_out_0, 36, 2),
	H12L_PIN_GROUP(sdio_wp_in, 36, 3),
	H12L_PIN_GROUP(aon_pwr_cmd_in_1, 36, 4),
	H12L_PIN_GROUP(spi2_sclk_out_1, 36, 5),
	H12L_PIN_GROUP(safety_error_n_out_0, 36, 6),
	H12L_PIN_GROUP(uart0_cts_in_2, 36, 7),
	H12L_PIN_GROUP(debug_out2, 36, 11),
	H12L_PIN_GROUP(aon_pwr_ctrl_out_1, 36, 8),
	H12L_PIN_GROUP(gpio3, 37, 0),
	H12L_PIN_GROUP(uart1_rts_out_1, 37, 1),
	H12L_PIN_GROUP(pwm3_out_0, 37, 2),
	H12L_PIN_GROUP(aon_pwr_cmd_in_2, 37, 3),
	H12L_PIN_GROUP(aon_pwr_stat_out_0, 37, 4),
	H12L_PIN_GROUP(spi2_miso_in_1, 37, 5),
	H12L_PIN_GROUP(uart2_cts_in_0, 37, 6),
	H12L_PIN_GROUP(uart0_rts_out_1, 37, 7),
	H12L_PIN_GROUP(uart1_cts_in_3, 37, 9),
	H12L_PIN_GROUP(uart0_cts_in_3, 37, 10),
	H12L_PIN_GROUP(debug_out3, 37, 11),
	H12L_PIN_GROUP(aon_pwr_ctrl_out_2, 37, 8),
	H12L_PIN_GROUP(gpio4, 38, 0),
	H12L_PIN_GROUP(usb_drive_vbus_out_0, 38, 1),
	H12L_PIN_GROUP(pwm4_out_0, 38, 2),
	H12L_PIN_GROUP(sdio_gp_out, 38, 3),
	H12L_PIN_GROUP(uart2_rts_out_0, 38, 4),
	H12L_PIN_GROUP(aon_pwr_ctrl_out_3, 38, 5),
	H12L_PIN_GROUP(aon_pwr_stat_out_1, 38, 6),
	H12L_PIN_GROUP(i2s0_sdo1_out_0, 38, 7),
	H12L_PIN_GROUP(uart1_rts_out_2, 38, 9),
	H12L_PIN_GROUP(debug_out4, 38, 11),
	H12L_PIN_GROUP(uart0_rts_out_2, 38, 8),
	H12L_PIN_GROUP(boot_rom_failure_out, 39, 0),
	H12L_PIN_GROUP(gpio5, 39, 1),
	H12L_PIN_GROUP(pwm5_out_0, 39, 2),
	H12L_PIN_GROUP(aon_pwr_ctrl_out_4, 39, 3),
	H12L_PIN_GROUP(uart0_rts_out_3, 39, 4),
	H12L_PIN_GROUP(i2c0_current_src_en_out_0, 39, 5),
	H12L_PIN_GROUP(aon_pwr_stat_out_2, 39, 6),
	H12L_PIN_GROUP(uart1_rts_out_3, 39, 7),
	H12L_PIN_GROUP(gpio6, 40, 0),
	H12L_PIN_GROUP(aon_pwr_stat_out_3, 40, 1),
	H12L_PIN_GROUP(aon_pwr_ctrl_out_5, 40, 2),
	H12L_PIN_GROUP(uart0_rts_out_4, 40, 3),
	H12L_PIN_GROUP(uart1_rts_out_4, 40, 4),
	H12L_PIN_GROUP(debug_out5, 40, 11),
	H12L_PIN_GROUP(sdio_CD_in_0, 41, 0),
	H12L_PIN_GROUP(gpio7_1, 41, 1),
	H12L_PIN_GROUP(timer_ext_0_in, 41, 2),
	H12L_PIN_GROUP(sdio_vdd1_on_out_0, 41, 3),
	H12L_PIN_GROUP(i2c3_scl_0, 41, 4),
	H12L_PIN_GROUP(i2c1_current_src_en_out_0, 41, 5),
	H12L_PIN_GROUP(uart0_cts_in_4, 41, 6),
	H12L_PIN_GROUP(i2s0_mclk_0, 41, 7),
	H12L_PIN_GROUP(debug_out6, 41, 11),
	H12L_PIN_GROUP(flash_spi_ds_cs3n_lpbk_0, 41, 8),
	H12L_PIN_GROUP(gpio8, 42, 0),
	H12L_PIN_GROUP(usb_overcurrent_n_in_0, 42, 1),
	H12L_PIN_GROUP(timer_ext_1_in, 42, 2),
	H12L_PIN_GROUP(sdio_vdd1_on_out_1, 42, 3),
	H12L_PIN_GROUP(i2c3_sda_0, 42, 4),
	H12L_PIN_GROUP(safety_error_n_out_1, 42, 5),
	H12L_PIN_GROUP(uart0_rts_out_5, 42, 6),
	H12L_PIN_GROUP(cpu_trace_clk_out_0, 42, 7),
	H12L_PIN_GROUP(debug_out7, 42, 11),
	H12L_PIN_GROUP(i2s0_mclk_1, 42, 8),
	H12L_PIN_GROUP(gpio9, 43, 0),
	H12L_PIN_GROUP(uart2_cts_in_1, 43, 1),
	H12L_PIN_GROUP(timer_ext_2_in_0, 43, 2),
	H12L_PIN_GROUP(uart3_rxd_in_0, 43, 3),
	H12L_PIN_GROUP(uart2_rxd_in_0, 43, 4),
	H12L_PIN_GROUP(spi0_cs1_n_out, 43, 5),
	H12L_PIN_GROUP(uart1_cts_in_4, 43, 6),
	H12L_PIN_GROUP(cpu_trace_data0_out, 43, 7),
	H12L_PIN_GROUP(debug_out8, 43, 11),
	H12L_PIN_GROUP(i2s0_sdi1_in_0, 43, 8),
	H12L_PIN_GROUP(gpio10, 44, 0),
	H12L_PIN_GROUP(uart2_rts_out_1, 44, 1),
	H12L_PIN_GROUP(timer_ext_3_in_0, 44, 2),
	H12L_PIN_GROUP(uart3_txd_out_0, 44, 3),
	H12L_PIN_GROUP(uart2_txd_out_0, 44, 4),
	H12L_PIN_GROUP(spi0_cs2_n_out, 44, 5),
	H12L_PIN_GROUP(uart1_rts_out_5, 44, 6),
	H12L_PIN_GROUP(cpu_trace_data1_out, 44, 7),
	H12L_PIN_GROUP(debug_out9, 44, 11),
	H12L_PIN_GROUP(i2s1_mclk_1, 44, 8),
	H12L_PIN_GROUP(gpio11, 45, 0),
	H12L_PIN_GROUP(uart3_cts_in_0, 45, 1),
	H12L_PIN_GROUP(pwm0_out_1, 45, 2),
	H12L_PIN_GROUP(sdio_vdd1_on_out_2, 45, 3),
	H12L_PIN_GROUP(sdio_uhs_sel_out_1, 45, 4),
	H12L_PIN_GROUP(spi0_cs3_n_out, 45, 5),
	H12L_PIN_GROUP(spi1_cs0_n_out_1, 45, 6),
	H12L_PIN_GROUP(cpu_trace_data2_out, 45, 7),
	H12L_PIN_GROUP(debug_out10, 45, 11),
	H12L_PIN_GROUP(i2s1_mclk_2, 45, 8),
	H12L_PIN_GROUP(gpio12_1, 46, 0),
	H12L_PIN_GROUP(uart3_rts_out_0, 46, 1),
	H12L_PIN_GROUP(pwm1_out_0, 46, 2),
	H12L_PIN_GROUP(i2c1_current_src_en_out_1, 46, 4),
	H12L_PIN_GROUP(i2c0_current_src_en_out_1, 46, 5),
	H12L_PIN_GROUP(cpu_trace_data3_out, 46, 7),
	H12L_PIN_GROUP(debug_out11, 46, 11),
	H12L_PIN_GROUP(gpio13_1, 47, 0),
	H12L_PIN_GROUP(uart2_rxd_in_1, 47, 1),
	H12L_PIN_GROUP(pwm2_out_1, 47, 2),
	H12L_PIN_GROUP(flash_spi_cs2n_clkc_rebar_out_0, 47, 3),
	H12L_PIN_GROUP(sdio_gp_in, 47, 4),
	H12L_PIN_GROUP(uart2_cts_in_2, 47, 5),
	H12L_PIN_GROUP(spi1_mosi_out_1, 47, 6),
	H12L_PIN_GROUP(i2c0_current_src_en_out_2, 47, 7),
	H12L_PIN_GROUP(debug_out12, 47, 11),
	H12L_PIN_GROUP(i2s1_sck_0, 47, 8),
	H12L_PIN_GROUP(gpio14_2, 48, 0),
	H12L_PIN_GROUP(uart2_txd_out_1, 48, 1),
	H12L_PIN_GROUP(pwm3_out_1, 48, 2),
	H12L_PIN_GROUP(sdio_CD_in_1, 48, 3),
	H12L_PIN_GROUP(safety_error_n_out_2, 48, 4),
	H12L_PIN_GROUP(uart2_rts_out_2, 48, 5),
	H12L_PIN_GROUP(spi1_miso_in_1, 48, 6),
	H12L_PIN_GROUP(i2c1_current_src_en_out_2, 48, 7),
	H12L_PIN_GROUP(flash_spi_cs_1_n_out_0, 48, 9),
	H12L_PIN_GROUP(debug_out13, 48, 11),
	H12L_PIN_GROUP(i2s1_sdi0_in_0, 48, 8),
	H12L_PIN_GROUP(gpio15_2, 49, 0),
	H12L_PIN_GROUP(uart3_rxd_in_1, 49, 1),
	H12L_PIN_GROUP(pwm4_out_1, 49, 2),
	H12L_PIN_GROUP(flash_spi_ds_cs3n_lpbk_1, 49, 3),
	H12L_PIN_GROUP(safety_out_0_0, 49, 4),
	H12L_PIN_GROUP(uart3_cts_in_1, 49, 5),
	H12L_PIN_GROUP(spi1_sclk_out_1, 49, 6),
	H12L_PIN_GROUP(usb_drive_vbus_out_1, 49, 7),
	H12L_PIN_GROUP(debug_out14, 49, 11),
	H12L_PIN_GROUP(i2s1_sdo0_out_0, 49, 8),
	H12L_PIN_GROUP(gpio16_1, 50, 0),
	H12L_PIN_GROUP(uart3_txd_out_1, 50, 1),
	H12L_PIN_GROUP(pwm5_out_1, 50, 2),
	H12L_PIN_GROUP(sdio_CD_in_2, 50, 3),
	H12L_PIN_GROUP(safety_out_1_0, 50, 4),
	H12L_PIN_GROUP(uart3_rts_out_1, 50, 5),
	H12L_PIN_GROUP(sdio_vdd1_on_out_3, 50, 6),
	H12L_PIN_GROUP(usb_overcurrent_n_in_1, 50, 7),
	H12L_PIN_GROUP(spi1_cs1_n_out, 50, 9),
	H12L_PIN_GROUP(debug_out15, 50, 11),
	H12L_PIN_GROUP(i2s1_ws_0, 50, 8),
	H12L_PIN_GROUP(gpio17_1, 51, 0),
	H12L_PIN_GROUP(i2c2_scl_0, 51, 1),
	H12L_PIN_GROUP(uart3_cts_in_2, 51, 2),
	H12L_PIN_GROUP(uart0_cts_in_5, 51, 3),
	H12L_PIN_GROUP(i2c0_current_src_en_out_3, 51, 4),
	H12L_PIN_GROUP(cpu_trace_clk_out_1, 51, 5),
	H12L_PIN_GROUP(sdio_vdd1_on_out_4, 51, 6),
	H12L_PIN_GROUP(i2s1_sck_1, 51, 7),
	H12L_PIN_GROUP(spi3_cs0_n_in_0, 51, 9),
	H12L_PIN_GROUP(spi2_cs1_n_out, 51, 10),
	H12L_PIN_GROUP(debug_out16, 51, 11),
	H12L_PIN_GROUP(flash_spi_cs_1_n_out_1, 51, 8),
	H12L_PIN_GROUP(gpio18_1, 52, 0),
	H12L_PIN_GROUP(i2c2_sda_0, 52, 1),
	H12L_PIN_GROUP(uart3_rts_out_2, 52, 2),
	H12L_PIN_GROUP(uart1_rts_out_6, 52, 3),
	H12L_PIN_GROUP(i2c1_current_src_en_out_3, 52, 4),
	H12L_PIN_GROUP(safety_out_0_1, 52, 5),
	H12L_PIN_GROUP(sdio_host_vdd1_stable_in_0, 52, 6),
	H12L_PIN_GROUP(i2s1_sdi0_in_1, 52, 7),
	H12L_PIN_GROUP(spi3_mosi_in_0, 52, 9),
	H12L_PIN_GROUP(debug_out17, 52, 11),
	H12L_PIN_GROUP(i2s0_sdi1_in_1, 52, 8),
	H12L_PIN_GROUP(gpio19_1, 53, 0),
	H12L_PIN_GROUP(i2c3_scl_1, 53, 1),
	H12L_PIN_GROUP(uart2_cts_in_3, 53, 2),
	H12L_PIN_GROUP(timer_ext_2_in_1, 53, 3),
	H12L_PIN_GROUP(i2c2_current_src_en_out, 53, 4),
	H12L_PIN_GROUP(safety_out_1_1, 53, 5),
	H12L_PIN_GROUP(uart2_rxd_in_2, 53, 6),
	H12L_PIN_GROUP(i2s1_sdo0_out_1, 53, 7),
	H12L_PIN_GROUP(spi3_sclk_in_0, 53, 9),
	H12L_PIN_GROUP(debug_out18, 53, 11),
	H12L_PIN_GROUP(i2s0_sdo1_out_1, 53, 8),
	H12L_PIN_GROUP(gpio20_1, 54, 0),
	H12L_PIN_GROUP(i2c3_sda_1, 54, 1),
	H12L_PIN_GROUP(uart2_rts_out_3, 54, 2),
	H12L_PIN_GROUP(timer_ext_3_in_1, 54, 3),
	H12L_PIN_GROUP(i2c3_current_src_en_out, 54, 4),
	H12L_PIN_GROUP(cpu_trace_clk_out_2, 54, 5),
	H12L_PIN_GROUP(uart2_txd_out_2, 54, 6),
	H12L_PIN_GROUP(i2s1_ws_1, 54, 7),
	H12L_PIN_GROUP(spi3_miso_out_0, 54, 9),
	H12L_PIN_GROUP(debug_out19, 54, 11),
	H12L_PIN_GROUP(i2s0_mclk_2, 54, 8),
	H12L_PIN_GROUP(gpio21, 55, 0),
	H12L_PIN_GROUP(uart2_rxd_in_3, 55, 1),
	H12L_PIN_GROUP(pwm0_out_2, 55, 2),
	H12L_PIN_GROUP(sdio_host_vdd1_stable_in_1, 55, 3),
	H12L_PIN_GROUP(i2s1_sck_2, 55, 4),
	H12L_PIN_GROUP(i2c2_scl_1, 55, 5),
	H12L_PIN_GROUP(spi0_cs0_n_out_1, 55, 6),
	H12L_PIN_GROUP(uart0_cts_in_6, 55, 7),
	H12L_PIN_GROUP(debug_out20, 55, 11),
	H12L_PIN_GROUP(spi3_cs0_n_in_1, 55, 8),
	H12L_PIN_GROUP(gpio22, 56, 0),
	H12L_PIN_GROUP(uart2_txd_out_3, 56, 1),
	H12L_PIN_GROUP(pwm1_out_1, 56, 2),
	H12L_PIN_GROUP(sdio_CD_in_3, 56, 3),
	H12L_PIN_GROUP(i2s1_sdi0_in_2, 56, 4),
	H12L_PIN_GROUP(i2c2_sda_1, 56, 5),
	H12L_PIN_GROUP(spi0_mosi_out_1, 56, 6),
	H12L_PIN_GROUP(uart0_rts_out_6, 56, 7),
	H12L_PIN_GROUP(debug_out21, 56, 11),
	H12L_PIN_GROUP(spi3_mosi_in_1, 56, 8),
	H12L_PIN_GROUP(gpio23, 57, 0),
	H12L_PIN_GROUP(uart3_rxd_in_2, 57, 1),
	H12L_PIN_GROUP(pwm2_out_2, 57, 2),
	H12L_PIN_GROUP(sdio_host_vdd1_stable_in_2, 57, 3),
	H12L_PIN_GROUP(i2s1_sdo0_out_2, 57, 4),
	H12L_PIN_GROUP(i2c3_scl_2, 57, 5),
	H12L_PIN_GROUP(spi0_miso_in_1, 57, 6),
	H12L_PIN_GROUP(uart1_cts_in_5, 57, 7),
	H12L_PIN_GROUP(flash_spi_cs2n_clkc_rebar_out_1, 57, 9),
	H12L_PIN_GROUP(debug_out22, 57, 11),
	H12L_PIN_GROUP(spi3_miso_out_1, 57, 8),
	H12L_PIN_GROUP(gpio24, 58, 0),
	H12L_PIN_GROUP(uart3_txd_out_2, 58, 1),
	H12L_PIN_GROUP(pwm3_out_2, 58, 2),
	H12L_PIN_GROUP(sdio_CD_in_4, 58, 3),
	H12L_PIN_GROUP(i2s1_ws_2, 58, 4),
	H12L_PIN_GROUP(i2c3_sda_2, 58, 5),
	H12L_PIN_GROUP(spi0_sclk_out_1, 58, 6),
	H12L_PIN_GROUP(uart1_rts_out_7, 58, 7),
	H12L_PIN_GROUP(pcie_mperst_n_out, 58, 9),
	H12L_PIN_GROUP(debug_out23, 58, 11),
	H12L_PIN_GROUP(spi3_sclk_in_1, 58, 8),
	// H12L_PIN_GROUP(RESET_N, 59, 0),
	// H12L_PIN_GROUP(OSC_XOUT, 60, 0),
	// H12L_PIN_GROUP(OSC_XIN, 61, 0),
};

static const char *const i2c0_scl_grps[] = {
	"i2c0_scl_0_grp",
};

static const char *const i2c0_sda_grps[] = {
	"i2c0_sda_0_grp",
};

static const char *const i2c1_scl_grps[] = {
	"i2c1_scl_0_grp",
};

static const char *const i2c1_sda_grps[] = {
	"i2c1_sda_0_grp",
};

static const char *const uart0_rxd_in_grps[] = {
	"uart0_rxd_in_0_grp",
};

static const char *const uart1_cts_in_grps[] = {
	"uart1_cts_in_0_grp",
	"uart1_cts_in_1_grp",
	"uart1_cts_in_2_grp",
	"uart1_cts_in_3_grp",
	"uart1_cts_in_4_grp",
	"uart1_cts_in_5_grp",
};

static const char *const uart4_rxd_in_grps[] = {
	"uart4_rxd_in_0_grp",
	"uart4_rxd_in_1_grp",
};

static const char *const uart4_cts_in_grps[] = {
	"uart4_cts_in_0_grp",
	"uart4_cts_in_1_grp",
};

static const char *const uart0_txd_out_grps[] = {
	"uart0_txd_out_0_grp",
};

static const char *const uart1_rts_out_grps[] = {
	"uart1_rts_out_0_grp",
	"uart1_rts_out_1_grp",
	"uart1_rts_out_2_grp",
	"uart1_rts_out_3_grp",
	"uart1_rts_out_4_grp",
	"uart1_rts_out_5_grp",
	"uart1_rts_out_6_grp",
	"uart1_rts_out_7_grp",
};

static const char *const uart4_txd_out_grps[] = {
	"uart4_txd_out_0_grp",
	"uart4_txd_out_1_grp",
};

static const char *const uart4_rts_out_grps[] = {
	"uart4_rts_out_0_grp",
	"uart4_rts_out_1_grp",
};

static const char *const uart1_rxd_in_grps[] = {
	"uart1_rxd_in_0_grp",
};

static const char *const uart0_cts_in_grps[] = {
	"uart0_cts_in_0_grp",
	"uart0_cts_in_1_grp",
	"uart0_cts_in_2_grp",
	"uart0_cts_in_3_grp",
	"uart0_cts_in_4_grp",
	"uart0_cts_in_5_grp",
	"uart0_cts_in_6_grp",
};

static const char *const uart1_txd_out_grps[] = {
	"uart1_txd_out_0_grp",
};

static const char *const uart0_rts_out_grps[] = {
	"uart0_rts_out_0_grp",
	"uart0_rts_out_1_grp",
	"uart0_rts_out_2_grp",
	"uart0_rts_out_3_grp",
	"uart0_rts_out_4_grp",
	"uart0_rts_out_5_grp",
	"uart0_rts_out_6_grp",
};

static const char *const i2s0_sck_grps[] = {
	"i2s0_sck_0_grp",
};

static const char *const gpio12_grps[] = {
	"gpio12_0_grp",
	"gpio12_1_grp",
};

static const char *const i2s0_sdi0_in_grps[] = {
	"i2s0_sdi0_in_0_grp",
};

static const char *const gpio13_grps[] = {
	"gpio13_0_grp",
	"gpio13_1_grp",
};

static const char *const i2s0_sdo0_out_grps[] = {
	"i2s0_sdo0_out_0_grp",
};

static const char *const gpio14_grps[] = {
	"gpio14_0_grp",
	"gpio14_1_grp",
	"gpio14_2_grp",
};

static const char *const i2s0_ws_grps[] = {
	"i2s0_ws_0_grp",
};

static const char *const gpio15_grps[] = {
	"gpio15_0_grp",
	"gpio15_1_grp",
	"gpio15_2_grp",
};

static const char *const flash_spi_cs_0_n_out_grps[] = {
	"flash_spi_cs_0_n_out_grp",
};

static const char *const spi0_cs0_n_out_grps[] = {
	"spi0_cs0_n_out_0_grp",
	"spi0_cs0_n_out_1_grp",
};

static const char *const spi1_cs0_n_out_grps[] = {
	"spi1_cs0_n_out_0_grp",
	"spi1_cs0_n_out_1_grp",
};

static const char *const spi2_cs0_n_out_grps[] = {
	"spi2_cs0_n_out_0_grp",
	"spi2_cs0_n_out_1_grp",
};

static const char *const flash_spi_dq_0_grps[] = {
	"flash_spi_dq_0_grp",
};

static const char *const spi0_miso_in_grps[] = {
	"spi0_miso_in_0_grp",
	"spi0_miso_in_1_grp",
};

static const char *const spi1_miso_in_grps[] = {
	"spi1_miso_in_0_grp",
	"spi1_miso_in_1_grp",
};

static const char *const spi2_miso_in_grps[] = {
	"spi2_miso_in_0_grp",
	"spi2_miso_in_1_grp",
};

static const char *const flash_spi_dq_1_grps[] = {
	"flash_spi_dq_1_grp",
};

static const char *const spi0_mosi_out_grps[] = {
	"spi0_mosi_out_0_grp",
	"spi0_mosi_out_1_grp",
};

static const char *const spi1_mosi_out_grps[] = {
	"spi1_mosi_out_0_grp",
	"spi1_mosi_out_1_grp",
};

static const char *const spi2_mosi_out_grps[] = {
	"spi2_mosi_out_grp",
};

static const char *const flash_spi_dq_2_grps[] = {
	"flash_spi_dq_2_grp",
};

static const char *const flash_spi_dq_3_grps[] = {
	"flash_spi_dq_3_grp",
};

static const char *const flash_spi_reset_n_out_grps[] = {
	"flash_spi_reset_n_out_grp",
};

static const char *const flash_spi_sclk_out_grps[] = {
	"flash_spi_sclk_out_grp",
};

static const char *const spi0_sclk_out_grps[] = {
	"spi0_sclk_out_0_grp",
	"spi0_sclk_out_1_grp",
};

static const char *const spi1_sclk_out_grps[] = {
	"spi1_sclk_out_0_grp",
	"spi1_sclk_out_1_grp",
};

static const char *const spi2_sclk_out_grps[] = {
	"spi2_sclk_out_0_grp",
	"spi2_sclk_out_1_grp",
};

static const char *const pcie_wake_n_grps[] = {
	"pcie_wake_n_grp",
};

static const char *const gpio7_grps[] = {
	"gpio7_0_grp",
	"gpio7_1_grp",
};

static const char *const pcie_clkreq_n_grps[] = {
	"pcie_clkreq_n_grp",
};

static const char *const pcie_perst_n_in_grps[] = {
	"pcie_perst_n_in_grp",
};

static const char *const SDIO_DATA0_grps[] = {
	"SDIO_DATA0_grp",
};

static const char *const SDIO_DATA1_grps[] = {
	"SDIO_DATA1_grp",
};

static const char *const SDIO_DATA2_grps[] = {
	"SDIO_DATA2_grp",
};

static const char *const SDIO_DATA3_grps[] = {
	"SDIO_DATA3_grp",
};

static const char *const SDIO_CMD_grps[] = {
	"SDIO_CMD_grp",
};

static const char *const SDIO_SDCLK_grps[] = {
	"SDIO_SDCLK_grp",
};

static const char *const safety_fatal_n_out_grps[] = {
	"safety_fatal_n_out_grp",
};

static const char *const jtag_tck_in_grps[] = {
	"jtag_tck_in_grp",
};

static const char *const gpio16_grps[] = {
	"gpio16_0_grp",
	"gpio16_1_grp",
};

static const char *const jtag_tdi_in_grps[] = {
	"jtag_tdi_in_grp",
};

static const char *const gpio17_grps[] = {
	"gpio17_0_grp",
	"gpio17_1_grp",
};

static const char *const jtag_tdo_out_grps[] = {
	"jtag_tdo_out_grp",
};

static const char *const gpio18_grps[] = {
	"gpio18_0_grp",
	"gpio18_1_grp",
};

static const char *const jtag_tms_grps[] = {
	"jtag_tms_grp",
};

static const char *const gpio19_grps[] = {
	"gpio19_0_grp",
	"gpio19_1_grp",
};

static const char *const jtag_trstn_in_grps[] = {
	"jtag_trstn_in_grp",
};

static const char *const gpio20_grps[] = {
	"gpio20_0_grp",
	"gpio20_1_grp",
};

static const char *const gpio0_grps[] = {
	"gpio0_grp",
};

static const char *const pwm0_out_grps[] = {
	"pwm0_out_0_grp",
	"pwm0_out_1_grp",
	"pwm0_out_2_grp",
};

static const char *const sdio_uhs_sel_out_grps[] = {
	"sdio_uhs_sel_out_0_grp",
	"sdio_uhs_sel_out_1_grp",
};

static const char *const aon_pwr_cmd_in_grps[] = {
	"aon_pwr_cmd_in_0_grp",
	"aon_pwr_cmd_in_1_grp",
	"aon_pwr_cmd_in_2_grp",
};

static const char *const i2s1_mclk_grps[] = {
	"i2s1_mclk_0_grp",
	"i2s1_mclk_1_grp",
	"i2s1_mclk_2_grp",
};

static const char *const aon_pwr_ctrl_out_grps[] = {
	"aon_pwr_ctrl_out_0_grp",
	"aon_pwr_ctrl_out_1_grp",
	"aon_pwr_ctrl_out_2_grp",
	"aon_pwr_ctrl_out_3_grp",
	"aon_pwr_ctrl_out_4_grp",
	"aon_pwr_ctrl_out_5_grp",
};

static const char *const debug_out0_grps[] = {
	"debug_out0_grp",
};

static const char *const VDD_SWITCH_PG_grps[] = {
	"VDD_SWITCH_PG_grp",
};

static const char *const gpio2_grps[] = {
	"gpio2_grp",
};

static const char *const pwm2_out_grps[] = {
	"pwm2_out_0_grp",
	"pwm2_out_1_grp",
	"pwm2_out_2_grp",
};

static const char *const sdio_wp_in_grps[] = {
	"sdio_wp_in_grp",
};

static const char *const safety_error_n_out_grps[] = {
	"safety_error_n_out_0_grp",
	"safety_error_n_out_1_grp",
	"safety_error_n_out_2_grp",
};

static const char *const debug_out2_grps[] = {
	"debug_out2_grp",
};

static const char *const gpio3_grps[] = {
	"gpio3_grp",
};

static const char *const pwm3_out_grps[] = {
	"pwm3_out_0_grp",
	"pwm3_out_1_grp",
	"pwm3_out_2_grp",
};

static const char *const aon_pwr_stat_out_grps[] = {
	"aon_pwr_stat_out_0_grp",
	"aon_pwr_stat_out_1_grp",
	"aon_pwr_stat_out_2_grp",
	"aon_pwr_stat_out_3_grp",
};

static const char *const uart2_cts_in_grps[] = {
	"uart2_cts_in_0_grp",
	"uart2_cts_in_1_grp",
	"uart2_cts_in_2_grp",
	"uart2_cts_in_3_grp",
};

static const char *const debug_out3_grps[] = {
	"debug_out3_grp",
};

static const char *const gpio4_grps[] = {
	"gpio4_grp",
};

static const char *const usb_drive_vbus_out_grps[] = {
	"usb_drive_vbus_out_0_grp",
	"usb_drive_vbus_out_1_grp",
};

static const char *const pwm4_out_grps[] = {
	"pwm4_out_0_grp",
	"pwm4_out_1_grp",
};

static const char *const sdio_gp_out_grps[] = {
	"sdio_gp_out_grp",
};

static const char *const uart2_rts_out_grps[] = {
	"uart2_rts_out_0_grp",
	"uart2_rts_out_1_grp",
	"uart2_rts_out_2_grp",
	"uart2_rts_out_3_grp",
};

static const char *const i2s0_sdo1_out_grps[] = {
	"i2s0_sdo1_out_0_grp",
	"i2s0_sdo1_out_1_grp",
};

static const char *const debug_out4_grps[] = {
	"debug_out4_grp",
};

static const char *const boot_rom_failure_out_grps[] = {
	"boot_rom_failure_out_grp",
};

static const char *const gpio5_grps[] = {
	"gpio5_grp",
};

static const char *const pwm5_out_grps[] = {
	"pwm5_out_0_grp",
	"pwm5_out_1_grp",
};

static const char *const i2c0_current_src_en_out_grps[] = {
	"i2c0_current_src_en_out_0_grp",
	"i2c0_current_src_en_out_1_grp",
	"i2c0_current_src_en_out_2_grp",
	"i2c0_current_src_en_out_3_grp",
};

static const char *const gpio6_grps[] = {
	"gpio6_grp",
};

static const char *const debug_out5_grps[] = {
	"debug_out5_grp",
};

static const char *const sdio_CD_in_grps[] = {
	"sdio_CD_in_0_grp",
	"sdio_CD_in_1_grp",
	"sdio_CD_in_2_grp",
	"sdio_CD_in_3_grp",
	"sdio_CD_in_4_grp",
};

static const char *const timer_ext_0_in_grps[] = {
	"timer_ext_0_in_grp",
};

static const char *const sdio_vdd1_on_out_grps[] = {
	"sdio_vdd1_on_out_0_grp",
	"sdio_vdd1_on_out_1_grp",
	"sdio_vdd1_on_out_2_grp",
	"sdio_vdd1_on_out_3_grp",
	"sdio_vdd1_on_out_4_grp",
};

static const char *const i2c3_scl_grps[] = {
	"i2c3_scl_0_grp",
	"i2c3_scl_1_grp",
	"i2c3_scl_2_grp",
};

static const char *const i2c1_current_src_en_out_grps[] = {
	"i2c1_current_src_en_out_0_grp",
	"i2c1_current_src_en_out_1_grp",
	"i2c1_current_src_en_out_2_grp",
	"i2c1_current_src_en_out_3_grp",
};

static const char *const i2s0_mclk_grps[] = {
	"i2s0_mclk_0_grp",
	"i2s0_mclk_1_grp",
	"i2s0_mclk_2_grp",
};

static const char *const flash_spi_ds_cs3n_lpbk_grps[] = {
	"flash_spi_ds_cs3n_lpbk_0_grp",
	"flash_spi_ds_cs3n_lpbk_1_grp",
};

static const char *const debug_out6_grps[] = {
	"debug_out6_grp",
};

static const char *const gpio8_grps[] = {
	"gpio8_grp",
};

static const char *const usb_overcurrent_n_in_grps[] = {
	"usb_overcurrent_n_in_0_grp",
	"usb_overcurrent_n_in_1_grp",
};

static const char *const timer_ext_1_in_grps[] = {
	"timer_ext_1_in_grp",
};

static const char *const i2c3_sda_grps[] = {
	"i2c3_sda_0_grp",
	"i2c3_sda_1_grp",
	"i2c3_sda_2_grp",
};

static const char *const cpu_trace_clk_out_grps[] = {
	"cpu_trace_clk_out_0_grp",
	"cpu_trace_clk_out_1_grp",
	"cpu_trace_clk_out_2_grp",
};

static const char *const debug_out7_grps[] = {
	"debug_out7_grp",
};

static const char *const gpio9_grps[] = {
	"gpio9_grp",
};

static const char *const timer_ext_2_in_grps[] = {
	"timer_ext_2_in_0_grp",
	"timer_ext_2_in_1_grp",
};

static const char *const uart3_rxd_in_grps[] = {
	"uart3_rxd_in_0_grp",
	"uart3_rxd_in_1_grp",
	"uart3_rxd_in_2_grp",
};

static const char *const uart2_rxd_in_grps[] = {
	"uart2_rxd_in_0_grp",
	"uart2_rxd_in_1_grp",
	"uart2_rxd_in_2_grp",
	"uart2_rxd_in_3_grp",
};

static const char *const spi0_cs1_n_out_grps[] = {
	"spi0_cs1_n_out_grp",
};

static const char *const cpu_trace_data0_out_grps[] = {
	"cpu_trace_data0_out_grp",
};

static const char *const i2s0_sdi1_in_grps[] = {
	"i2s0_sdi1_in_0_grp",
	"i2s0_sdi1_in_1_grp",
};

static const char *const debug_out8_grps[] = {
	"debug_out8_grp",
};

static const char *const gpio10_grps[] = {
	"gpio10_grp",
};

static const char *const timer_ext_3_in_grps[] = {
	"timer_ext_3_in_0_grp",
	"timer_ext_3_in_1_grp",
};

static const char *const uart3_txd_out_grps[] = {
	"uart3_txd_out_0_grp",
	"uart3_txd_out_1_grp",
	"uart3_txd_out_2_grp",
};

static const char *const uart2_txd_out_grps[] = {
	"uart2_txd_out_0_grp",
	"uart2_txd_out_1_grp",
	"uart2_txd_out_2_grp",
	"uart2_txd_out_3_grp",
};

static const char *const spi0_cs2_n_out_grps[] = {
	"spi0_cs2_n_out_grp",
};

static const char *const cpu_trace_data1_out_grps[] = {
	"cpu_trace_data1_out_grp",
};

static const char *const debug_out9_grps[] = {
	"debug_out9_grp",
};

static const char *const gpio11_grps[] = {
	"gpio11_grp",
};

static const char *const uart3_cts_in_grps[] = {
	"uart3_cts_in_0_grp",
	"uart3_cts_in_1_grp",
	"uart3_cts_in_2_grp",
};

static const char *const spi0_cs3_n_out_grps[] = {
	"spi0_cs3_n_out_0_grp",
};

static const char *const cpu_trace_data2_out_grps[] = {
	"cpu_trace_data2_out_0_grp",
};

static const char *const debug_out10_grps[] = {
	"debug_out10_grp",
};

static const char *const uart3_rts_out_grps[] = {
	"uart3_rts_out_0_grp",
	"uart3_rts_out_1_grp",
	"uart3_rts_out_2_grp",
};

static const char *const pwm1_out_grps[] = {
	"pwm1_out_0_grp",
	"pwm1_out_1_grp",
};

static const char *const cpu_trace_data3_out_grps[] = {
	"cpu_trace_data3_out_0_grp",
};

static const char *const debug_out11_grps[] = {
	"debug_out11_grp",
};

static const char *const flash_spi_cs2n_clkc_rebar_out_grps[] = {
	"flash_spi_cs2n_clkc_rebar_out_0_grp",
	"flash_spi_cs2n_clkc_rebar_out_1_grp",
};

static const char *const sdio_gp_in_grps[] = {
	"sdio_gp_in_grp",
};

static const char *const i2s1_sck_grps[] = {
	"i2s1_sck_0_grp",
	"i2s1_sck_1_grp",
	"i2s1_sck_2_grp",
};

static const char *const debug_out12_grps[] = {
	"debug_out12_grp",
};

static const char *const i2s1_sdi0_in_grps[] = {
	"i2s1_sdi0_in_0_grp",
	"i2s1_sdi0_in_1_grp",
	"i2s1_sdi0_in_2_grp",
};

static const char *const flash_spi_cs_1_n_out_grps[] = {
	"flash_spi_cs_1_n_out_0_grp",
	"flash_spi_cs_1_n_out_1_grp",
};

static const char *const debug_out13_grps[] = {
	"debug_out13_grp",
};

static const char *const safety_out_0_grps[] = {
	"safety_out_0_0_grp",
	"safety_out_0_1_grp",
};

static const char *const i2s1_sdo0_out_grps[] = {
	"i2s1_sdo0_out_0_grp",
	"i2s1_sdo0_out_1_grp",
	"i2s1_sdo0_out_2_grp",
};

static const char *const debug_out14_grps[] = {
	"debug_out14_grp",
};

static const char *const safety_out_1_grps[] = {
	"safety_out_1_0_grp",
	"safety_out_1_1_grp",
};

static const char *const i2s1_ws_grps[] = {
	"i2s1_ws_0_grp",
	"i2s1_ws_1_grp",
	"i2s1_ws_2_grp",
};

static const char *const spi1_cs1_n_out_grps[] = {
	"spi1_cs1_n_out_0_grp",
};

static const char *const debug_out15_grps[] = {
	"debug_out15_grp",
};

static const char *const i2c2_scl_grps[] = {
	"i2c2_scl_0_grp",
	"i2c2_scl_1_grp",
};

static const char *const spi3_cs0_n_in_grps[] = {
	"spi3_cs0_n_in_0_grp",
	"spi3_cs0_n_in_1_grp",
};

static const char *const spi2_cs1_n_out_grps[] = {
	"spi2_cs1_n_out_grp",
};

static const char *const debug_out16_grps[] = {
	"debug_out16_grp",
};

static const char *const i2c2_sda_grps[] = {
	"i2c2_sda_0_grp",
	"i2c2_sda_1_grp",
};

static const char *const sdio_host_vdd1_stable_in_grps[] = {
	"sdio_host_vdd1_stable_in_0_grp",
	"sdio_host_vdd1_stable_in_1_grp",
	"sdio_host_vdd1_stable_in_2_grp",
};

static const char *const spi3_mosi_in_grps[] = {
	"spi3_mosi_in_0_grp",
	"spi3_mosi_in_1_grp",
};

static const char *const debug_out17_grps[] = {
	"debug_out17_grp",
};

static const char *const i2c2_current_src_en_out_grps[] = {
	"i2c2_current_src_en_out_0_grp",
};

static const char *const spi3_sclk_in_grps[] = {
	"spi3_sclk_in_0_grp",
	"spi3_sclk_in_1_grp",
};

static const char *const debug_out18_grps[] = {
	"debug_out18_grp",
};

static const char *const i2c3_current_src_en_out_grps[] = {
	"i2c3_current_src_en_out_0_grp",
};

static const char *const spi3_miso_out_grps[] = {
	"spi3_miso_out_0_grp",
	"spi3_miso_out_1_grp",
};

static const char *const debug_out19_grps[] = {
	"debug_out19_grp",
};

static const char *const gpio21_grps[] = {
	"gpio21_grp",
};

static const char *const debug_out20_grps[] = {
	"debug_out20_grp",
};

static const char *const gpio22_grps[] = {
	"gpio22_0_grp",
};

static const char *const debug_out21_grps[] = {
	"debug_out21_grp",
};

static const char *const gpio23_grps[] = {
	"gpio23_0_grp",
};

static const char *const debug_out22_grps[] = {
	"debug_out22_grp",
};

static const char *const gpio24_grps[] = {
	"gpio24_0_grp",
};

static const char *const pcie_mperst_n_out_grps[] = {
	"pcie_mperst_n_out_grp",
};

static const char *const debug_out23_grps[] = {
	"debug_out23_grp",
};

// static const char *const RESET_N_grps[] = {
// 	"RESET_N_grp",
// };

// static const char *const OSC_XOUT_grps[] = {
// 	"OSC_XOUT_grp",
// };

// static const char *const OSC_XIN_grps[] = {
// 	"OSC_XIN_grp",
// };

#define H12L_PIN_FUNCTION(func)                                                 \
	{                                                                      \
		.name = #func, .groups = func##_grps,                          \
		.num_groups = ARRAY_SIZE(func##_grps),                         \
	}

/* NOTE: Keep 24 first functions as they are (GPIO).
         gpioN must be at index N.  - this comment is from pluto*/
static const struct h12l_pin_function h12l_pin_functions[] = {
	H12L_PIN_FUNCTION(gpio0),
	H12L_PIN_FUNCTION(gpio2),
	H12L_PIN_FUNCTION(gpio3),
	H12L_PIN_FUNCTION(gpio4),
	H12L_PIN_FUNCTION(gpio5),
	H12L_PIN_FUNCTION(gpio6),
	H12L_PIN_FUNCTION(gpio7),
	H12L_PIN_FUNCTION(gpio8),
	H12L_PIN_FUNCTION(gpio9),
	H12L_PIN_FUNCTION(gpio10),
	H12L_PIN_FUNCTION(gpio11),
	H12L_PIN_FUNCTION(gpio12),
	H12L_PIN_FUNCTION(gpio13),
	H12L_PIN_FUNCTION(gpio14),
	H12L_PIN_FUNCTION(gpio15),
	H12L_PIN_FUNCTION(gpio16),
	H12L_PIN_FUNCTION(gpio17),
	H12L_PIN_FUNCTION(gpio18),
	H12L_PIN_FUNCTION(gpio19),
	H12L_PIN_FUNCTION(gpio20),
	H12L_PIN_FUNCTION(gpio21),
	H12L_PIN_FUNCTION(gpio22),
	H12L_PIN_FUNCTION(gpio23),
	H12L_PIN_FUNCTION(gpio24),

	H12L_PIN_FUNCTION(i2c0_scl),
	H12L_PIN_FUNCTION(i2c0_sda),
	H12L_PIN_FUNCTION(i2c1_scl),
	H12L_PIN_FUNCTION(i2c1_sda),
	H12L_PIN_FUNCTION(uart0_rxd_in),
	H12L_PIN_FUNCTION(uart1_cts_in),
	H12L_PIN_FUNCTION(uart4_rxd_in),
	H12L_PIN_FUNCTION(uart4_cts_in),
	H12L_PIN_FUNCTION(uart0_txd_out),
	H12L_PIN_FUNCTION(uart1_rts_out),
	H12L_PIN_FUNCTION(uart4_txd_out),
	H12L_PIN_FUNCTION(uart4_rts_out),
	H12L_PIN_FUNCTION(uart1_rxd_in),
	H12L_PIN_FUNCTION(uart0_cts_in),
	H12L_PIN_FUNCTION(uart1_txd_out),
	H12L_PIN_FUNCTION(uart0_rts_out),
	H12L_PIN_FUNCTION(i2s0_sck),
	H12L_PIN_FUNCTION(i2s0_sdi0_in),
	H12L_PIN_FUNCTION(i2s0_sdo0_out),
	H12L_PIN_FUNCTION(i2s0_ws),
	H12L_PIN_FUNCTION(flash_spi_cs_0_n_out),
	H12L_PIN_FUNCTION(spi0_cs0_n_out),
	H12L_PIN_FUNCTION(spi1_cs0_n_out),
	H12L_PIN_FUNCTION(spi2_cs0_n_out),
	H12L_PIN_FUNCTION(flash_spi_dq_0),
	H12L_PIN_FUNCTION(spi0_miso_in),
	H12L_PIN_FUNCTION(spi1_miso_in),
	H12L_PIN_FUNCTION(spi2_miso_in),
	H12L_PIN_FUNCTION(flash_spi_dq_1),
	H12L_PIN_FUNCTION(spi0_mosi_out),
	H12L_PIN_FUNCTION(spi1_mosi_out),
	H12L_PIN_FUNCTION(spi2_mosi_out),
	H12L_PIN_FUNCTION(flash_spi_dq_2),
	H12L_PIN_FUNCTION(flash_spi_dq_3),
	H12L_PIN_FUNCTION(flash_spi_reset_n_out),
	H12L_PIN_FUNCTION(flash_spi_sclk_out),
	H12L_PIN_FUNCTION(spi0_sclk_out),
	H12L_PIN_FUNCTION(spi1_sclk_out),
	H12L_PIN_FUNCTION(spi2_sclk_out),
	H12L_PIN_FUNCTION(pcie_wake_n),
	H12L_PIN_FUNCTION(pcie_clkreq_n),
	H12L_PIN_FUNCTION(pcie_perst_n_in),
	H12L_PIN_FUNCTION(SDIO_DATA0),
	H12L_PIN_FUNCTION(SDIO_DATA1),
	H12L_PIN_FUNCTION(SDIO_DATA2),
	H12L_PIN_FUNCTION(SDIO_DATA3),
	H12L_PIN_FUNCTION(SDIO_CMD),
	H12L_PIN_FUNCTION(SDIO_SDCLK),
	H12L_PIN_FUNCTION(safety_fatal_n_out),
	H12L_PIN_FUNCTION(jtag_tck_in),
	H12L_PIN_FUNCTION(jtag_tdi_in),
	H12L_PIN_FUNCTION(jtag_tdo_out),
	H12L_PIN_FUNCTION(jtag_tms),
	H12L_PIN_FUNCTION(jtag_trstn_in),
	H12L_PIN_FUNCTION(pwm0_out),
	H12L_PIN_FUNCTION(sdio_uhs_sel_out),
	H12L_PIN_FUNCTION(aon_pwr_cmd_in),
	H12L_PIN_FUNCTION(i2s1_mclk),
	H12L_PIN_FUNCTION(aon_pwr_ctrl_out),
	H12L_PIN_FUNCTION(debug_out0),
	H12L_PIN_FUNCTION(VDD_SWITCH_PG),
	H12L_PIN_FUNCTION(pwm2_out),
	H12L_PIN_FUNCTION(sdio_wp_in),
	H12L_PIN_FUNCTION(safety_error_n_out),
	H12L_PIN_FUNCTION(debug_out2),
	H12L_PIN_FUNCTION(pwm3_out),
	H12L_PIN_FUNCTION(aon_pwr_stat_out),
	H12L_PIN_FUNCTION(uart2_cts_in),
	H12L_PIN_FUNCTION(debug_out3),
	H12L_PIN_FUNCTION(usb_drive_vbus_out),
	H12L_PIN_FUNCTION(pwm4_out),
	H12L_PIN_FUNCTION(sdio_gp_out),
	H12L_PIN_FUNCTION(uart2_rts_out),
	H12L_PIN_FUNCTION(i2s0_sdo1_out),
	H12L_PIN_FUNCTION(debug_out4),
	H12L_PIN_FUNCTION(boot_rom_failure_out),
	H12L_PIN_FUNCTION(pwm5_out),
	H12L_PIN_FUNCTION(i2c0_current_src_en_out),
	H12L_PIN_FUNCTION(debug_out5),
	H12L_PIN_FUNCTION(sdio_CD_in),
	H12L_PIN_FUNCTION(timer_ext_0_in),
	H12L_PIN_FUNCTION(sdio_vdd1_on_out),
	H12L_PIN_FUNCTION(i2c3_scl),
	H12L_PIN_FUNCTION(i2c1_current_src_en_out),
	H12L_PIN_FUNCTION(i2s0_mclk),
	H12L_PIN_FUNCTION(flash_spi_ds_cs3n_lpbk),
	H12L_PIN_FUNCTION(debug_out6),
	H12L_PIN_FUNCTION(usb_overcurrent_n_in),
	H12L_PIN_FUNCTION(timer_ext_1_in),
	H12L_PIN_FUNCTION(i2c3_sda),
	H12L_PIN_FUNCTION(cpu_trace_clk_out),
	H12L_PIN_FUNCTION(debug_out7),
	H12L_PIN_FUNCTION(timer_ext_2_in),
	H12L_PIN_FUNCTION(uart3_rxd_in),
	H12L_PIN_FUNCTION(uart2_rxd_in),
	H12L_PIN_FUNCTION(spi0_cs1_n_out),
	H12L_PIN_FUNCTION(cpu_trace_data0_out),
	H12L_PIN_FUNCTION(i2s0_sdi1_in),
	H12L_PIN_FUNCTION(debug_out8),
	H12L_PIN_FUNCTION(timer_ext_3_in),
	H12L_PIN_FUNCTION(uart3_txd_out),
	H12L_PIN_FUNCTION(uart2_txd_out),
	H12L_PIN_FUNCTION(spi0_cs2_n_out),
	H12L_PIN_FUNCTION(cpu_trace_data1_out),
	H12L_PIN_FUNCTION(debug_out9),
	H12L_PIN_FUNCTION(uart3_cts_in),
	H12L_PIN_FUNCTION(spi0_cs3_n_out),
	H12L_PIN_FUNCTION(cpu_trace_data2_out),
	H12L_PIN_FUNCTION(debug_out10),
	H12L_PIN_FUNCTION(uart3_rts_out),
	H12L_PIN_FUNCTION(pwm1_out),
	H12L_PIN_FUNCTION(cpu_trace_data3_out),
	H12L_PIN_FUNCTION(debug_out11),
	H12L_PIN_FUNCTION(flash_spi_cs2n_clkc_rebar_out),
	H12L_PIN_FUNCTION(sdio_gp_in),
	H12L_PIN_FUNCTION(i2s1_sck),
	H12L_PIN_FUNCTION(debug_out12),
	H12L_PIN_FUNCTION(i2s1_sdi0_in),
	H12L_PIN_FUNCTION(flash_spi_cs_1_n_out),
	H12L_PIN_FUNCTION(debug_out13),
	H12L_PIN_FUNCTION(safety_out_0),
	H12L_PIN_FUNCTION(i2s1_sdo0_out),
	H12L_PIN_FUNCTION(debug_out14),
	H12L_PIN_FUNCTION(safety_out_1),
	H12L_PIN_FUNCTION(i2s1_ws),
	H12L_PIN_FUNCTION(spi1_cs1_n_out),
	H12L_PIN_FUNCTION(debug_out15),
	H12L_PIN_FUNCTION(i2c2_scl),
	H12L_PIN_FUNCTION(spi3_cs0_n_in),
	H12L_PIN_FUNCTION(spi2_cs1_n_out),
	H12L_PIN_FUNCTION(debug_out16),
	H12L_PIN_FUNCTION(i2c2_sda),
	H12L_PIN_FUNCTION(sdio_host_vdd1_stable_in),
	H12L_PIN_FUNCTION(spi3_mosi_in),
	H12L_PIN_FUNCTION(debug_out17),
	H12L_PIN_FUNCTION(i2c2_current_src_en_out),
	H12L_PIN_FUNCTION(spi3_sclk_in),
	H12L_PIN_FUNCTION(debug_out18),
	H12L_PIN_FUNCTION(i2c3_current_src_en_out),
	H12L_PIN_FUNCTION(spi3_miso_out),
	H12L_PIN_FUNCTION(debug_out19),
	H12L_PIN_FUNCTION(debug_out20),
	H12L_PIN_FUNCTION(debug_out21),
	H12L_PIN_FUNCTION(debug_out22),
	H12L_PIN_FUNCTION(pcie_mperst_n_out),
	H12L_PIN_FUNCTION(debug_out23),
	// H12L_PIN_FUNCTION(RESET_N),
	// H12L_PIN_FUNCTION(OSC_XOUT),
	// H12L_PIN_FUNCTION(OSC_XIN),
};

#define H12L_PINMUX_INVALID_FUNCTION_SELECTOR (ARRAY_SIZE(h12l_pin_functions))

#define HAILO_PIN_DESC_STATIC_DRV_DATA(_pin_index, _muxable, _mux_index,       \
				       _pad_type, _pad_index, ...)             \
	[_pin_index] = {                                                       \
		.mux_index = (_mux_index),                                     \
		.is_muxable = (_muxable),                                      \
		.pad_type = _pad_type,                                         \
		.pad_index = _pad_index,                                       \
		.func_selector = H12L_PINMUX_INVALID_FUNCTION_SELECTOR,         \
		.is_occupied = false,                                          \
	}

static struct h12l_pin_data hailo12l_pins_drv_data[] = {
	/* HAILO_PIN_DESC_STATIC_DRV_DATA(pin_index, is_muxable, pad_type, index_in_pad) */

	/* I2C */
	HAILO_PIN_DESC_STATIC_DRV_DATA(0, NOT_MUXABLE, 0, H12L_SLOW_PAD, 1),
	HAILO_PIN_DESC_STATIC_DRV_DATA(1, NOT_MUXABLE, 1, H12L_SLOW_PAD, 0),
	HAILO_PIN_DESC_STATIC_DRV_DATA(2, NOT_MUXABLE, 2, H12L_SLOW_PAD, 3),
	HAILO_PIN_DESC_STATIC_DRV_DATA(3, NOT_MUXABLE, 3, H12L_SLOW_PAD, 2),

	/* UART */
	/* TODO: MSW-12914 enable muxing UART pins */
	HAILO_PIN_DESC_STATIC_DRV_DATA(4, NOT_MUXABLE, 0, H12L_AON_PAD, 1),
	HAILO_PIN_DESC_STATIC_DRV_DATA(5, NOT_MUXABLE, 1, H12L_AON_PAD, 0),
	HAILO_PIN_DESC_STATIC_DRV_DATA(6, NOT_MUXABLE, 2, H12L_AON_PAD, 3),
	HAILO_PIN_DESC_STATIC_DRV_DATA(7, NOT_MUXABLE, 3, H12L_AON_PAD, 2),

	/* I2S */
	HAILO_PIN_DESC_STATIC_DRV_DATA(8, MUXABLE, 4, H12L_SLOW_PAD, 9),
	HAILO_PIN_DESC_STATIC_DRV_DATA(9, MUXABLE, 5, H12L_SLOW_PAD, 12),
	HAILO_PIN_DESC_STATIC_DRV_DATA(10, MUXABLE, 6, H12L_SLOW_PAD, 11),
	HAILO_PIN_DESC_STATIC_DRV_DATA(11, MUXABLE, 7, H12L_SLOW_PAD, 10),

	/* Flash */
	HAILO_PIN_DESC_STATIC_DRV_DATA(12, MUXABLE, 8, H12L_GENERAL_PAD, 1),
	HAILO_PIN_DESC_STATIC_DRV_DATA(13, MUXABLE, 9, H12L_GENERAL_PAD, 3),
	HAILO_PIN_DESC_STATIC_DRV_DATA(14, MUXABLE, 10, H12L_GENERAL_PAD, 4),
	HAILO_PIN_DESC_STATIC_DRV_DATA(15, NOT_MUXABLE, 11, H12L_GENERAL_PAD, 5),
	HAILO_PIN_DESC_STATIC_DRV_DATA(16, NOT_MUXABLE, 12, H12L_GENERAL_PAD, 6),
	HAILO_PIN_DESC_STATIC_DRV_DATA(17, NOT_MUXABLE, 13, H12L_GENERAL_PAD, 2),
	HAILO_PIN_DESC_STATIC_DRV_DATA(18, MUXABLE, 14, H12L_GENERAL_PAD, 0),

	/* PCIe */
	HAILO_PIN_DESC_STATIC_DRV_DATA(19, MUXABLE, 4, H12L_AON_PAD, 5),
	HAILO_PIN_DESC_STATIC_DRV_DATA(20, MUXABLE, 5, H12L_AON_PAD, 4),
	HAILO_PIN_DESC_STATIC_DRV_DATA(21, MUXABLE, 6, H12L_AON_PAD, 6),

	/* SDIO */
	HAILO_PIN_DESC_STATIC_DRV_DATA(22, NOT_MUXABLE, 15, H12L_GENERAL_PAD, 7),
	HAILO_PIN_DESC_STATIC_DRV_DATA(23, NOT_MUXABLE, 16, H12L_GENERAL_PAD, 8),
	HAILO_PIN_DESC_STATIC_DRV_DATA(24, NOT_MUXABLE, 17, H12L_GENERAL_PAD, 9),
    HAILO_PIN_DESC_STATIC_DRV_DATA(25, NOT_MUXABLE, 18, H12L_GENERAL_PAD, 10),
    HAILO_PIN_DESC_STATIC_DRV_DATA(26, NOT_MUXABLE, 19, H12L_GENERAL_PAD, 11),
    HAILO_PIN_DESC_STATIC_DRV_DATA(27, NOT_MUXABLE, 20, H12L_GENERAL_PAD, 12),

	/* Safety */
	HAILO_PIN_DESC_STATIC_DRV_DATA(28, NOT_MUXABLE, 7, H12L_AON_PAD, 7, ),

	/* JTAG */
	HAILO_PIN_DESC_STATIC_DRV_DATA(29, MUXABLE, 21, H12L_SLOW_PAD, 7),
	HAILO_PIN_DESC_STATIC_DRV_DATA(30, MUXABLE, 22, H12L_SLOW_PAD, 4),
	HAILO_PIN_DESC_STATIC_DRV_DATA(31, MUXABLE, 23, H12L_SLOW_PAD, 5),
	HAILO_PIN_DESC_STATIC_DRV_DATA(32, MUXABLE, 24, H12L_SLOW_PAD, 6),
	HAILO_PIN_DESC_STATIC_DRV_DATA(33, MUXABLE, 25, H12L_SLOW_PAD, 8),

	/* AON GPIOS */
	HAILO_PIN_DESC_STATIC_DRV_DATA(34, MUXABLE, 8, H12L_AON_GPIO_PAD, 0),
	HAILO_PIN_DESC_STATIC_DRV_DATA(35, NOT_MUXABLE, 9, H12L_AON_GPIO_PAD, 1),
	HAILO_PIN_DESC_STATIC_DRV_DATA(36, MUXABLE, 10, H12L_AON_GPIO_PAD, 2),
	HAILO_PIN_DESC_STATIC_DRV_DATA(37, MUXABLE, 11, H12L_AON_GPIO_PAD, 3),
	HAILO_PIN_DESC_STATIC_DRV_DATA(38, MUXABLE, 12, H12L_AON_GPIO_PAD, 4),
	HAILO_PIN_DESC_STATIC_DRV_DATA(39, MUXABLE, 13, H12L_AON_GPIO_PAD, 5),
	HAILO_PIN_DESC_STATIC_DRV_DATA(40, MUXABLE, 14, H12L_AON_GPIO_PAD, 6),

	/* GPIOS */
	HAILO_PIN_DESC_STATIC_DRV_DATA(41, MUXABLE, 26, H12L_GPIO_PAD, 7),
	HAILO_PIN_DESC_STATIC_DRV_DATA(42, MUXABLE, 27, H12L_GPIO_PAD, 8),
	HAILO_PIN_DESC_STATIC_DRV_DATA(43, MUXABLE, 28, H12L_GPIO_PAD, 9),
	HAILO_PIN_DESC_STATIC_DRV_DATA(44, MUXABLE, 29, H12L_GPIO_PAD, 10),
	HAILO_PIN_DESC_STATIC_DRV_DATA(45, MUXABLE, 30, H12L_GPIO_PAD, 11),
	HAILO_PIN_DESC_STATIC_DRV_DATA(46, MUXABLE, 31, H12L_GPIO_PAD, 12),
	HAILO_PIN_DESC_STATIC_DRV_DATA(47, MUXABLE, 32, H12L_GPIO_PAD, 13),
	HAILO_PIN_DESC_STATIC_DRV_DATA(48, MUXABLE, 33, H12L_GPIO_PAD, 14),
	HAILO_PIN_DESC_STATIC_DRV_DATA(49, MUXABLE, 34, H12L_GPIO_PAD, 15),
	HAILO_PIN_DESC_STATIC_DRV_DATA(50, MUXABLE, 35, H12L_GPIO_PAD, 16),
	HAILO_PIN_DESC_STATIC_DRV_DATA(51, MUXABLE, 36, H12L_GPIO_PAD, 17),
	HAILO_PIN_DESC_STATIC_DRV_DATA(52, MUXABLE, 37, H12L_GPIO_PAD, 18),
	HAILO_PIN_DESC_STATIC_DRV_DATA(53, MUXABLE, 38, H12L_GPIO_PAD, 19),
	HAILO_PIN_DESC_STATIC_DRV_DATA(54, MUXABLE, 39, H12L_GPIO_PAD, 20),
	HAILO_PIN_DESC_STATIC_DRV_DATA(55, MUXABLE, 40, H12L_GPIO_PAD, 21),
	HAILO_PIN_DESC_STATIC_DRV_DATA(56, MUXABLE, 41, H12L_GPIO_PAD, 22),
	HAILO_PIN_DESC_STATIC_DRV_DATA(57, MUXABLE, 42, H12L_GPIO_PAD, 23),
	HAILO_PIN_DESC_STATIC_DRV_DATA(58, MUXABLE, 43, H12L_GPIO_PAD, 24),

	/* Reset */
	HAILO_PIN_DESC_STATIC_DRV_DATA(59, NOT_MUXABLE, MUX_INDEX_DONT_CARE, H12L_NO_PAD, PAD_INDEX_DONT_CARE),

	/* OSC */
	HAILO_PIN_DESC_STATIC_DRV_DATA(60, NOT_MUXABLE, MUX_INDEX_DONT_CARE, H12L_NO_PAD, PAD_INDEX_DONT_CARE),
	HAILO_PIN_DESC_STATIC_DRV_DATA(61, NOT_MUXABLE, MUX_INDEX_DONT_CARE, H12L_NO_PAD, PAD_INDEX_DONT_CARE),
};

#define H12L_PIN(_pin_index, _name)                                             \
	[(_pin_index)] = { .number = (_pin_index),                             \
			   .name = (_name),                                    \
			   .drv_data = &hailo12l_pins_drv_data[(_pin_index)] }

static const struct pinctrl_pin_desc hailo12l_pins[] = {
	H12L_PIN(0, "I2C0_SCL"),	       H12L_PIN(1, "I2C0_SDA"),
	H12L_PIN(2, "I2C1_SCL"),	       H12L_PIN(3, "I2C1_SDA"),

	H12L_PIN(4, "UART0_RXD"),       H12L_PIN(5, "UART0_TXD"),
	H12L_PIN(6, "UART1_RXD"),       H12L_PIN(7, "UART1_TXD"),

	H12L_PIN(8, "I2S0_SCK"),	       H12L_PIN(9, "I2S0_SDI"),
	H12L_PIN(10, "I2S0_SDO"),       H12L_PIN(11, "I2S0_WS"),

	H12L_PIN(12, "FLASH_CS0_N"),    H12L_PIN(13, "FLASH_DQ0_MOSI"),
	H12L_PIN(14, "FLASH_DQ1_MISO"), H12L_PIN(15, "FLASH_DQ2"),
	H12L_PIN(16, "FLASH_DQ3"),      H12L_PIN(17, "FLASH_RESET_N"),
	H12L_PIN(18, "FLASH_SCLK"),

	H12L_PIN(19, "PCIE_WAKE_N"),    H12L_PIN(20, "PCIE_CLKREQ_N"),
	H12L_PIN(21, "PCIE_PERST_N"),

	H12L_PIN(22, "SDIO_DATA0"),     H12L_PIN(23, "SDIO_DATA1"),
	H12L_PIN(24, "SDIO_DATA2"),     H12L_PIN(25, "SDIO_DATA3"),
	H12L_PIN(26, "SDIO_CMD"),       H12L_PIN(27, "SDIO_SDCLK"),

	H12L_PIN(28, "SAFETY_FATAL_N"),

	H12L_PIN(29, "JTAG_TCK"),       H12L_PIN(30, "JTAG_TDI"),
	H12L_PIN(31, "JTAG_TDO"),       H12L_PIN(32, "JTAG_TMS"),
	H12L_PIN(33, "JTAG_TRSTN"),

	H12L_PIN(34, "GPIO_0_AON"),	       H12L_PIN(35, "GPIO_1_AON"),
	H12L_PIN(36, "GPIO_2_AON"),	       H12L_PIN(37, "GPIO_3_AON"),
	H12L_PIN(38, "GPIO_4_AON"),	       H12L_PIN(39, "GPIO_5_AON"),
	H12L_PIN(40, "GPIO_6_AON"),	       H12L_PIN(41, "GPIO_7"),
	H12L_PIN(42, "GPIO_8"),	       H12L_PIN(43, "GPIO_9"),
	H12L_PIN(44, "GPIO_10"),	       H12L_PIN(45, "GPIO_11"),
	H12L_PIN(46, "GPIO_12"),	       H12L_PIN(47, "GPIO_13"),
	H12L_PIN(48, "GPIO_14"),	       H12L_PIN(49, "GPIO_15"),
	H12L_PIN(50, "GPIO_16"),	       H12L_PIN(51, "GPIO_17"),
	H12L_PIN(52, "GPIO_18"),	       H12L_PIN(53, "GPIO_19"),
	H12L_PIN(54, "GPIO_20"),	       H12L_PIN(55, "GPIO_21"),
	H12L_PIN(56, "GPIO_22"),	       H12L_PIN(57, "GPIO_23"),
	H12L_PIN(58, "GPIO_24"),

	H12L_PIN(59, "RESET_N"),
	H12L_PIN(60, "OSC_XOUT"),
	H12L_PIN(61, "OSC_XIN"),
};

/* ============================================================================
 * COMPILE-TIME VALIDATION
 * ============================================================================ */
_Static_assert(ARRAY_SIZE(hailo12l_pins_drv_data) == H12L_PINMUX_PIN_COUNT,
	       "Pin driver data count mismatch");
_Static_assert(ARRAY_SIZE(hailo12l_pins) == H12L_PINMUX_PIN_COUNT,
	       "Pin descriptor count mismatch");

#endif /* _PINCTRL_HAILO12L_DESCRIPTIONS_H */