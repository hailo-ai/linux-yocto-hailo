
#ifndef _PINCTRL_HAILO15L_PROPERTIES_H
#define _PINCTRL_HAILO15L_PROPERTIES_H

#include "pinctrl-hailo15l.h"

#define H15L_PINMUX_PIN_COUNT (32)
#define H15L_GPIO_PIN_BASE (60)
#define H15L_IS_SDIO_PIN(_pin) (((_pin) >= 40 && (_pin) <= 51) || ((_pin) == 66))

#define H15L_PIN_GROUP(_group_name, _pin_offset, _mode)                       \
	{                                                                     \
		.name = __stringify(_group_name) "_grp",                       \
		.pin = (_pin_offset),                                         \
		.mode = (_mode),                                              \
	}

static const struct h15l_pin_group h15l_pin_groups[] = {
	H15L_PIN_GROUP(gpio0_4, 60, 0),
	H15L_PIN_GROUP(gpio1_3, 61, 0),
	H15L_PIN_GROUP(gpio2_3, 62, 0),
	H15L_PIN_GROUP(gpio3_3, 63, 0),
	H15L_PIN_GROUP(gpio4_3, 64, 0),
	H15L_PIN_GROUP(gpio5_3, 65, 1),
	H15L_PIN_GROUP(gpio6_3, 66, 0),
	H15L_PIN_GROUP(gpio7_3, 67, 2),
	H15L_PIN_GROUP(gpio8_3, 68, 0),
	H15L_PIN_GROUP(gpio9_5, 69, 2),
	H15L_PIN_GROUP(gpio10_4, 70, 0),
	H15L_PIN_GROUP(gpio11_3, 71, 0),
	H15L_PIN_GROUP(gpio12_2, 72, 0),
	H15L_PIN_GROUP(gpio13_2, 73, 0),
	H15L_PIN_GROUP(gpio14_2, 74, 0),
	H15L_PIN_GROUP(gpio15_2, 75, 0),
	H15L_PIN_GROUP(gpio16_3, 76, 0),
	H15L_PIN_GROUP(gpio17_3, 77, 0),
	H15L_PIN_GROUP(gpio18_3, 78, 0),
	H15L_PIN_GROUP(gpio19_3, 79, 0),
	H15L_PIN_GROUP(gpio20_3, 80, 0),
	H15L_PIN_GROUP(gpio21_3, 81, 0),
	H15L_PIN_GROUP(gpio22_3, 82, 0),
	H15L_PIN_GROUP(gpio23_3, 83, 0),
	H15L_PIN_GROUP(gpio24_3, 84, 0),
	H15L_PIN_GROUP(gpio25_4, 85, 0),
	H15L_PIN_GROUP(gpio26_3, 86, 0),
	H15L_PIN_GROUP(gpio27_2, 87, 0),
	H15L_PIN_GROUP(gpio28_2, 88, 0),
	H15L_PIN_GROUP(gpio29_2, 89, 0),
	H15L_PIN_GROUP(gpio30_2, 90, 0),
	H15L_PIN_GROUP(gpio31_2, 91, 0),

	H15L_PIN_GROUP(i2c0_scl_0, 0, 0),
	H15L_PIN_GROUP(i2c0_sda_0, 1, 0),
	H15L_PIN_GROUP(i2c1_scl_0, 2, 0),
	H15L_PIN_GROUP(i2c1_sda_0, 3, 0),
	H15L_PIN_GROUP(uart0_rxd_in_0, 4, 0),
	H15L_PIN_GROUP(can0_rx_in_0, 4, 1),
	H15L_PIN_GROUP(can1_rx_in_0, 4, 2),
	H15L_PIN_GROUP(uart0_txd_out_0, 5, 0),
	H15L_PIN_GROUP(can0_tx_out_0, 5, 1),
	H15L_PIN_GROUP(can1_tx_out_0, 5, 2),
	H15L_PIN_GROUP(can0_stby_out_0, 5, 3),
	H15L_PIN_GROUP(can1_stby_out_0, 5, 4),
	H15L_PIN_GROUP(uart1_rxd_in_0, 6, 0),
	H15L_PIN_GROUP(can0_rx_in_1, 6, 1),
	H15L_PIN_GROUP(can1_rx_in_1, 6, 2),
	H15L_PIN_GROUP(uart1_txd_out_0, 7, 0),
	H15L_PIN_GROUP(can0_tx_out_1, 7, 1),
	H15L_PIN_GROUP(can1_tx_out_1, 7, 2),
	H15L_PIN_GROUP(can0_stby_out_1, 7, 3),
	H15L_PIN_GROUP(can1_stby_out_1, 7, 4),
	H15L_PIN_GROUP(eth_mdc_out, 8, 0),
	H15L_PIN_GROUP(gpio16_0, 8, 1),
	H15L_PIN_GROUP(gpio0_0, 8, 2),
	H15L_PIN_GROUP(eth_mdio, 9, 0),
	H15L_PIN_GROUP(gpio17_0, 9, 1),
	H15L_PIN_GROUP(gpio1_0, 9, 2),
	H15L_PIN_GROUP(eth_rx_clk_in, 10, 0),
	H15L_PIN_GROUP(gpio18_0, 10, 1),
	H15L_PIN_GROUP(gpio2_0, 10, 2),
	H15L_PIN_GROUP(eth_rx_ctl_crs_dv_in, 11, 0),
	H15L_PIN_GROUP(gpio19_0, 11, 1),
	H15L_PIN_GROUP(gpio3_0, 11, 2),
	H15L_PIN_GROUP(eth_rxd_0_in, 12, 0),
	H15L_PIN_GROUP(gpio20_0, 12, 1),
	H15L_PIN_GROUP(gpio4_0, 12, 2),
	H15L_PIN_GROUP(uart0_cts_in_0, 12, 4),
	H15L_PIN_GROUP(eth_rxd_1_in, 13, 0),
	H15L_PIN_GROUP(gpio21_0, 13, 1),
	H15L_PIN_GROUP(gpio5_0, 13, 2),
	H15L_PIN_GROUP(uart1_cts_in_0, 13, 4),
	H15L_PIN_GROUP(eth_rxd_2_in, 14, 0),
	H15L_PIN_GROUP(gpio22_0, 14, 1),
	H15L_PIN_GROUP(gpio6_0, 14, 2),
	H15L_PIN_GROUP(pwm0_out_0, 14, 3),
	H15L_PIN_GROUP(uart2_cts_in_0, 14, 4),
	H15L_PIN_GROUP(can0_rx_in_2, 14, 5),
	H15L_PIN_GROUP(safety_error_n_out_0, 14, 6),
	H15L_PIN_GROUP(i2s0_sdi1_in_0, 14, 7),
	H15L_PIN_GROUP(can1_rx_in_2, 14, 8),
	H15L_PIN_GROUP(eth_rxd_3_er_in, 15, 0),
	H15L_PIN_GROUP(gpio23_0, 15, 1),
	H15L_PIN_GROUP(gpio7_0, 15, 2),
	H15L_PIN_GROUP(eth_tx_clk_out, 16, 0),
	H15L_PIN_GROUP(gpio24_0, 16, 1),
	H15L_PIN_GROUP(gpio8_0, 16, 2),
	H15L_PIN_GROUP(i2c2_scl_0, 16, 3),
	H15L_PIN_GROUP(eth_tx_ctl_en_out, 17, 0),
	H15L_PIN_GROUP(gpio25_0, 17, 1),
	H15L_PIN_GROUP(gpio9_0, 17, 2),
	H15L_PIN_GROUP(eth_txd_0_out, 18, 0),
	H15L_PIN_GROUP(gpio26_0, 18, 1),
	H15L_PIN_GROUP(gpio10_0, 18, 2),
	H15L_PIN_GROUP(uart0_rts_out_0, 18, 4),
	H15L_PIN_GROUP(cpu_trace_data0_out_0, 18, 7),
	H15L_PIN_GROUP(eth_txd_1_out, 19, 0),
	H15L_PIN_GROUP(gpio27_0, 19, 1),
	H15L_PIN_GROUP(gpio11_0, 19, 2),
	H15L_PIN_GROUP(uart1_rts_out_0, 19, 4),
	H15L_PIN_GROUP(cpu_trace_data1_out_0, 19, 7),
	H15L_PIN_GROUP(eth_txd_2_out, 20, 0),
	H15L_PIN_GROUP(gpio28_0, 20, 1),
	H15L_PIN_GROUP(gpio12_0, 20, 2),
	H15L_PIN_GROUP(pwm1_out_0, 20, 3),
	H15L_PIN_GROUP(uart2_rts_out_0, 20, 4),
	H15L_PIN_GROUP(can0_stby_out_2, 20, 5),
	H15L_PIN_GROUP(i2s0_sdo1_out_0, 20, 6),
	H15L_PIN_GROUP(cpu_trace_data2_out_0, 20, 7),
	H15L_PIN_GROUP(can1_stby_out_2, 20, 8),
	H15L_PIN_GROUP(eth_txd_3_out, 21, 0),
	H15L_PIN_GROUP(gpio29_0, 21, 1),
	H15L_PIN_GROUP(gpio13_0, 21, 2),
	H15L_PIN_GROUP(safety_error_n_out_1, 21, 3),
	H15L_PIN_GROUP(can0_tx_out_2, 21, 4),
	H15L_PIN_GROUP(can1_tx_out_2, 21, 6),
	H15L_PIN_GROUP(cpu_trace_data3_out_0, 21, 7),
	H15L_PIN_GROUP(i2s0_sck_0, 22, 0),
	H15L_PIN_GROUP(gpio30_0, 22, 1),
	H15L_PIN_GROUP(gpio14_0, 22, 2),
	H15L_PIN_GROUP(isp_flash_trig_out_0, 22, 5),
	H15L_PIN_GROUP(safety_out_0_0, 22, 6),
	H15L_PIN_GROUP(timer_ext_0_in_0, 22, 7),
	H15L_PIN_GROUP(i2s0_sdi0_in_0, 23, 0),
	H15L_PIN_GROUP(gpio31_0, 23, 1),
	H15L_PIN_GROUP(gpio15_0, 23, 2),
	H15L_PIN_GROUP(uart2_rxd_in_0, 23, 3),
	H15L_PIN_GROUP(usb_overcurrent_n_in_0, 23, 4),
	H15L_PIN_GROUP(isp_pre_flash_out_0, 23, 5),
	H15L_PIN_GROUP(safety_out_1_0, 23, 6),
	H15L_PIN_GROUP(timer_ext_1_in_0, 23, 7),
	H15L_PIN_GROUP(i2s0_sdo0_out_0, 24, 0),
	H15L_PIN_GROUP(gpio16_1, 24, 1),
	H15L_PIN_GROUP(gpio0_1, 24, 2),
	H15L_PIN_GROUP(uart2_txd_out_0, 24, 3),
	H15L_PIN_GROUP(usb_drive_vbus_out_0, 24, 4),
	H15L_PIN_GROUP(cpu_trace_clk_out_0, 24, 5),
	H15L_PIN_GROUP(i2s0_ws_0, 25, 0),
	H15L_PIN_GROUP(gpio17_1, 25, 1),
	H15L_PIN_GROUP(gpio1_1, 25, 2),
	H15L_PIN_GROUP(flash_spi_cs_0_n_out, 26, 0),
	H15L_PIN_GROUP(gpio18_1, 26, 1),
	H15L_PIN_GROUP(gpio2_1, 26, 2),
	H15L_PIN_GROUP(isp_flash_trig_out_1, 26, 3),
	H15L_PIN_GROUP(debug_out22, 26, 4),
	H15L_PIN_GROUP(can0_stby_out_3, 26, 5),
	H15L_PIN_GROUP(spi1_cs0_n_out_0, 26, 7),
	H15L_PIN_GROUP(spi0_cs0_n_out_0, 26, 8),
	H15L_PIN_GROUP(flash_spi_cs_1_n_out, 27, 0),
	H15L_PIN_GROUP(gpio19_1, 27, 1),
	H15L_PIN_GROUP(gpio3_1, 27, 2),
	H15L_PIN_GROUP(isp_pre_flash_out_1, 27, 3),
	H15L_PIN_GROUP(debug_out23, 27, 4),
	H15L_PIN_GROUP(uart1_rts_out_1, 27, 5),
	H15L_PIN_GROUP(usb_drive_vbus_out_1, 27, 6),
	H15L_PIN_GROUP(spi1_cs1_n_out_0, 27, 7),
	H15L_PIN_GROUP(spi0_cs1_n_out_0, 27, 8),
	H15L_PIN_GROUP(flash_spi_cs2n_clkc_rebar_out, 28, 0),
	H15L_PIN_GROUP(can0_tx_out_3, 28, 1),
	H15L_PIN_GROUP(can1_tx_out_3, 28, 2),
	H15L_PIN_GROUP(gpio20_1, 28, 3),
	H15L_PIN_GROUP(gpio4_1, 28, 4),
	H15L_PIN_GROUP(uart3_txd_out_0, 28, 5),
	H15L_PIN_GROUP(cpu_trace_data2_out_1, 28, 6),
	H15L_PIN_GROUP(spi1_mosi_out_0, 28, 7),
	H15L_PIN_GROUP(spi0_cs2_n_out_0, 28, 8),
	H15L_PIN_GROUP(flash_spi_ds_cs3n_lpbk, 29, 0),
	H15L_PIN_GROUP(can0_rx_in_3, 29, 1),
	H15L_PIN_GROUP(can1_rx_in_3, 29, 2),
	H15L_PIN_GROUP(gpio21_1, 29, 3),
	H15L_PIN_GROUP(gpio5_1, 29, 4),
	H15L_PIN_GROUP(uart3_rxd_in_0, 29, 5),
	H15L_PIN_GROUP(cpu_trace_data3_out_1, 29, 6),
	H15L_PIN_GROUP(spi1_sclk_out_0, 29, 7),
	H15L_PIN_GROUP(spi0_cs3_n_out_0, 29, 8),
	H15L_PIN_GROUP(flash_spi_dq_0, 30, 0),
	H15L_PIN_GROUP(gpio22_1, 30, 1),
	H15L_PIN_GROUP(gpio6_1, 30, 2),
	H15L_PIN_GROUP(sdio0_gp_in_0, 30, 3),
	H15L_PIN_GROUP(sdio0_CD_in_0, 30, 4),
	H15L_PIN_GROUP(uart0_cts_in_1, 30, 5),
	H15L_PIN_GROUP(spi1_miso_in_0, 30, 7),
	H15L_PIN_GROUP(spi0_mosi_out_0, 30, 8),
	H15L_PIN_GROUP(flash_spi_dq_1, 31, 0),
	H15L_PIN_GROUP(gpio23_1, 31, 1),
	H15L_PIN_GROUP(gpio7_1, 31, 2),
	H15L_PIN_GROUP(sdio1_gp_in_0, 31, 3),
	H15L_PIN_GROUP(sdio1_CD_in_0, 31, 4),
	H15L_PIN_GROUP(uart0_rts_out_1, 31, 5),
	H15L_PIN_GROUP(spi0_miso_in_0, 31, 8),
	H15L_PIN_GROUP(flash_spi_dq_2, 32, 0),
	H15L_PIN_GROUP(gpio24_1, 32, 1),
	H15L_PIN_GROUP(gpio8_1, 32, 2),
	H15L_PIN_GROUP(pwm0_out_1, 32, 3),
	H15L_PIN_GROUP(i2c0_current_src_en_out_0, 32, 4),
	H15L_PIN_GROUP(uart1_cts_in_1, 32, 5),
	H15L_PIN_GROUP(sdio0_CD_in_1, 32, 6),
	H15L_PIN_GROUP(spi0_sclk_out_0, 32, 8),
	H15L_PIN_GROUP(flash_spi_dq_3, 33, 0),
	H15L_PIN_GROUP(gpio25_1, 33, 1),
	H15L_PIN_GROUP(gpio9_1, 33, 2),
	H15L_PIN_GROUP(pwm1_out_1, 33, 3),
	H15L_PIN_GROUP(i2c1_current_src_en_out_0, 33, 4),
	H15L_PIN_GROUP(uart1_rts_out_2, 33, 5),
	H15L_PIN_GROUP(sdio1_CD_in_1, 33, 6),
	H15L_PIN_GROUP(flash_spi_reset_n_out, 34, 0),
	H15L_PIN_GROUP(gpio26_1, 34, 1),
	H15L_PIN_GROUP(gpio10_1, 34, 2),
	H15L_PIN_GROUP(sdio0_gp_out_0, 34, 3),
	H15L_PIN_GROUP(can0_stby_out_4, 34, 5),
	H15L_PIN_GROUP(pwm1_out_2, 34, 6),
	H15L_PIN_GROUP(can1_stby_out_3, 34, 7),
	H15L_PIN_GROUP(flash_spi_sclk_out, 35, 0),
	H15L_PIN_GROUP(gpio27_1, 35, 1),
	H15L_PIN_GROUP(gpio11_1, 35, 2),
	H15L_PIN_GROUP(sdio1_gp_out_0, 35, 3),
	H15L_PIN_GROUP(pwm0_out_2, 35, 4),
	H15L_PIN_GROUP(pcie_wake_n, 36, 0),
	H15L_PIN_GROUP(flash_ospi_dq_4, 36, 1),
	H15L_PIN_GROUP(gpio23_2, 36, 2),
	H15L_PIN_GROUP(gpio7_2, 36, 3),
	H15L_PIN_GROUP(debug_out24, 36, 4),
	H15L_PIN_GROUP(uart2_rxd_in_1, 36, 5),
	H15L_PIN_GROUP(sdio0_gp_in_1, 36, 6),
	H15L_PIN_GROUP(pcie_clkreq_n, 37, 0),
	H15L_PIN_GROUP(flash_ospi_dq_5, 37, 1),
	H15L_PIN_GROUP(gpio24_2, 37, 2),
	H15L_PIN_GROUP(gpio10_2, 37, 3),
	H15L_PIN_GROUP(debug_out25, 37, 4),
	H15L_PIN_GROUP(uart2_txd_out_1, 37, 5),
	H15L_PIN_GROUP(sdio0_gp_out_1, 37, 6),
	H15L_PIN_GROUP(cpu_trace_data0_out_1, 37, 7),
	H15L_PIN_GROUP(pcie_perst_n_in, 38, 0),
	H15L_PIN_GROUP(flash_ospi_dq_6, 38, 1),
	H15L_PIN_GROUP(gpio25_2, 38, 2),
	H15L_PIN_GROUP(gpio11_2, 38, 3),
	H15L_PIN_GROUP(debug_out26, 38, 4),
	H15L_PIN_GROUP(uart3_rxd_in_1, 38, 5),
	H15L_PIN_GROUP(sdio0_CD_in_2, 38, 6),
	H15L_PIN_GROUP(cpu_trace_data1_out_1, 38, 7),
	H15L_PIN_GROUP(pcie_mperst_n_out, 39, 0),
	H15L_PIN_GROUP(flash_ospi_dq_7, 39, 1),
	H15L_PIN_GROUP(gpio26_2, 39, 2),
	H15L_PIN_GROUP(gpio9_2, 39, 3),
	H15L_PIN_GROUP(debug_out27, 39, 4),
	H15L_PIN_GROUP(uart3_txd_out_1, 39, 5),
	H15L_PIN_GROUP(can0_stby_out_5, 39, 6),
	H15L_PIN_GROUP(cpu_trace_data2_out_2, 39, 7),
	H15L_PIN_GROUP(can1_stby_out_4, 39, 8),
	H15L_PIN_GROUP(SDIO0_DATA0_SDIO1_DATA4, 40, 0),
	H15L_PIN_GROUP(gpio28_1, 40, 1),
	H15L_PIN_GROUP(gpio12_1, 40, 2),
	H15L_PIN_GROUP(i2c2_scl_1, 40, 3),
	H15L_PIN_GROUP(uart0_rxd_in_1, 40, 4),
	H15L_PIN_GROUP(uart2_rxd_in_2, 40, 5),
	H15L_PIN_GROUP(i2s1_sck_0, 40, 6),
	H15L_PIN_GROUP(SDIO0_DATA1_SDIO1_DATA5, 41, 0),
	H15L_PIN_GROUP(gpio29_1, 41, 1),
	H15L_PIN_GROUP(gpio13_1, 41, 2),
	H15L_PIN_GROUP(i2c2_sda_0, 41, 3),
	H15L_PIN_GROUP(uart0_txd_out_1, 41, 4),
	H15L_PIN_GROUP(uart2_txd_out_2, 41, 5),
	H15L_PIN_GROUP(i2s1_sdi0_in_0, 41, 6),
	H15L_PIN_GROUP(SDIO0_DATA2_SDIO1_DATA6, 42, 0),
	H15L_PIN_GROUP(gpio30_1, 42, 1),
	H15L_PIN_GROUP(gpio14_1, 42, 2),
	H15L_PIN_GROUP(i2c3_scl_0, 42, 3),
	H15L_PIN_GROUP(uart1_rxd_in_1, 42, 4),
	H15L_PIN_GROUP(uart3_rxd_in_2, 42, 5),
	H15L_PIN_GROUP(i2s1_sdo0_out_0, 42, 6),
	H15L_PIN_GROUP(SDIO0_DATA3_SDIO1_DATA7, 43, 0),
	H15L_PIN_GROUP(gpio31_1, 43, 1),
	H15L_PIN_GROUP(gpio15_1, 43, 2),
	H15L_PIN_GROUP(i2c3_sda_0, 43, 3),
	H15L_PIN_GROUP(uart1_txd_out_1, 43, 4),
	H15L_PIN_GROUP(uart3_txd_out_2, 43, 5),
	H15L_PIN_GROUP(i2s1_ws_0, 43, 6),
	H15L_PIN_GROUP(i2s0_mclk_0, 43, 7),
	H15L_PIN_GROUP(SDIO0_SDCLK_SDIO1_DS, 44, 0),
	H15L_PIN_GROUP(gpio4_2, 44, 1),
	H15L_PIN_GROUP(i2s0_sdi1_in_1, 44, 2),
	H15L_PIN_GROUP(uart0_cts_in_2, 44, 3),
	H15L_PIN_GROUP(uart1_cts_in_2, 44, 4),
	H15L_PIN_GROUP(uart3_cts_in_0, 44, 5),
	H15L_PIN_GROUP(i2s1_mclk_0, 44, 6),
	H15L_PIN_GROUP(SDIO0_CMD_SDIO1_RSTN, 45, 0),
	H15L_PIN_GROUP(gpio5_2, 45, 1),
	H15L_PIN_GROUP(i2s0_sdo1_out_1, 45, 2),
	H15L_PIN_GROUP(uart0_rts_out_2, 45, 3),
	H15L_PIN_GROUP(uart1_rts_out_3, 45, 4),
	H15L_PIN_GROUP(uart3_rts_out_0, 45, 5),
	H15L_PIN_GROUP(SDIO1_DATA0, 46, 0),
	H15L_PIN_GROUP(gpio8_2, 46, 1),
	H15L_PIN_GROUP(uart2_rxd_in_3, 46, 2),
	H15L_PIN_GROUP(i2c2_scl_2, 46, 3),
	H15L_PIN_GROUP(i2s1_sck_1, 46, 4),
	H15L_PIN_GROUP(uart0_rxd_in_2, 46, 5),
	H15L_PIN_GROUP(SDIO1_DATA1, 47, 0),
	H15L_PIN_GROUP(gpio9_3, 47, 1),
	H15L_PIN_GROUP(uart2_txd_out_3, 47, 2),
	H15L_PIN_GROUP(i2c2_sda_1, 47, 3),
	H15L_PIN_GROUP(i2s1_sdi0_in_1, 47, 4),
	H15L_PIN_GROUP(uart0_txd_out_2, 47, 5),
	H15L_PIN_GROUP(SDIO1_DATA2, 48, 0),
	H15L_PIN_GROUP(gpio20_2, 48, 1),
	H15L_PIN_GROUP(uart3_rxd_in_3, 48, 2),
	H15L_PIN_GROUP(i2c3_scl_1, 48, 3),
	H15L_PIN_GROUP(i2s1_sdo0_out_1, 48, 4),
	H15L_PIN_GROUP(uart1_rxd_in_2, 48, 5),
	H15L_PIN_GROUP(SDIO1_DATA3, 49, 0),
	H15L_PIN_GROUP(gpio21_2, 49, 1),
	H15L_PIN_GROUP(uart3_txd_out_3, 49, 2),
	H15L_PIN_GROUP(i2c3_sda_1, 49, 3),
	H15L_PIN_GROUP(i2s1_ws_1, 49, 4),
	H15L_PIN_GROUP(uart1_txd_out_2, 49, 5),
	H15L_PIN_GROUP(SDIO1_SDCLK, 51, 0),
	H15L_PIN_GROUP(gpio25_3, 51, 1),
	H15L_PIN_GROUP(gpio9_4, 51, 2),
	H15L_PIN_GROUP(parallel_pclk_in, 52, 0),
	H15L_PIN_GROUP(uart1_cts_in_3, 52, 1),
	H15L_PIN_GROUP(uart2_cts_in_1, 52, 2),
	H15L_PIN_GROUP(i2c2_current_src_en_out_0, 52, 3),
	H15L_PIN_GROUP(debug_out28, 52, 4),
	H15L_PIN_GROUP(isp_flash_trig_out_2, 52, 5),
	H15L_PIN_GROUP(gpio0_2, 52, 6),
	H15L_PIN_GROUP(i2s1_mclk_1, 52, 7),
	H15L_PIN_GROUP(i2s0_mclk_1, 52, 8),
	H15L_PIN_GROUP(usb_drive_vbus_out_2, 53, 0),
	H15L_PIN_GROUP(uart1_rts_out_4, 53, 1),
	H15L_PIN_GROUP(uart2_rts_out_1, 53, 2),
	H15L_PIN_GROUP(i2c3_current_src_en_out_0, 53, 3),
	H15L_PIN_GROUP(debug_out29, 53, 4),
	H15L_PIN_GROUP(isp_pre_flash_out_2, 53, 5),
	H15L_PIN_GROUP(gpio10_3, 53, 6),
	H15L_PIN_GROUP(jtag_tck_in, 55, 0),
	H15L_PIN_GROUP(gpio16_2, 55, 1),
	H15L_PIN_GROUP(gpio0_3, 55, 2),
	H15L_PIN_GROUP(timer_ext_0_in_1, 55, 3),
	H15L_PIN_GROUP(uart3_cts_in_1, 55, 4),
	H15L_PIN_GROUP(uart2_rxd_in_4, 55, 5),
	H15L_PIN_GROUP(i2c2_scl_3, 55, 6),
	H15L_PIN_GROUP(i2s1_sck_2, 55, 7),
	H15L_PIN_GROUP(jtag_tdi_in, 56, 0),
	H15L_PIN_GROUP(gpio17_2, 56, 1),
	H15L_PIN_GROUP(gpio1_2, 56, 2),
	H15L_PIN_GROUP(timer_ext_1_in_1, 56, 3),
	H15L_PIN_GROUP(uart3_rts_out_1, 56, 4),
	H15L_PIN_GROUP(uart2_txd_out_4, 56, 5),
	H15L_PIN_GROUP(i2c2_sda_2, 56, 6),
	H15L_PIN_GROUP(i2s1_sdi0_in_2, 56, 7),
	H15L_PIN_GROUP(jtag_tdo_out, 57, 0),
	H15L_PIN_GROUP(gpio18_2, 57, 1),
	H15L_PIN_GROUP(gpio2_2, 57, 2),
	H15L_PIN_GROUP(pwm2_out_0, 57, 3),
	H15L_PIN_GROUP(safety_out_0_1, 57, 4),
	H15L_PIN_GROUP(uart3_txd_out_4, 57, 5),
	H15L_PIN_GROUP(i2c3_scl_2, 57, 6),
	H15L_PIN_GROUP(i2s1_sdo0_out_2, 57, 7),
	H15L_PIN_GROUP(jtag_tms, 58, 0),
	H15L_PIN_GROUP(gpio19_2, 58, 1),
	H15L_PIN_GROUP(gpio3_2, 58, 2),
	H15L_PIN_GROUP(timer_ext_3_in_0, 58, 3),
	H15L_PIN_GROUP(safety_out_1_1, 58, 4),
	H15L_PIN_GROUP(uart3_rxd_in_4, 58, 5),
	H15L_PIN_GROUP(i2c3_sda_2, 58, 6),
	H15L_PIN_GROUP(i2s1_ws_2, 58, 7),
	H15L_PIN_GROUP(jtag_trstn_in, 59, 0),
	H15L_PIN_GROUP(gpio22_2, 59, 1),
	H15L_PIN_GROUP(gpio6_2, 59, 2),
	H15L_PIN_GROUP(safety_error_n_out_2, 59, 3),
	H15L_PIN_GROUP(timer_ext_2_in_0, 59, 4),
	H15L_PIN_GROUP(i2s1_mclk_2, 59, 7),

	H15L_PIN_GROUP(eth_txd_4_out, 60, 1),
	H15L_PIN_GROUP(pwm0_out_3, 60, 2),
	H15L_PIN_GROUP(debug_out0, 60, 3),
	H15L_PIN_GROUP(i2c0_current_src_en_out_1, 60, 4),
	H15L_PIN_GROUP(can0_stby_out_6, 60, 5),
	H15L_PIN_GROUP(can1_stby_out_5, 60, 6),
	H15L_PIN_GROUP(sdio0_uhs_sel_out_0, 60, 7),
	H15L_PIN_GROUP(sdio1_uhs_sel_out_0, 60, 8),

	H15L_PIN_GROUP(eth_txd_5_out, 61, 1),
	H15L_PIN_GROUP(pwm1_out_3, 61, 2),
	H15L_PIN_GROUP(debug_out1, 61, 3),
	H15L_PIN_GROUP(i2c1_current_src_en_out_1, 61, 4),
	H15L_PIN_GROUP(usb_overcurrent_n_in_1, 61, 5),
	H15L_PIN_GROUP(uart0_rts_out_3, 61, 6),
	H15L_PIN_GROUP(can1_rx_in_4, 61, 7),
	H15L_PIN_GROUP(can1_stby_out_6, 61, 8),

	H15L_PIN_GROUP(eth_txd_6_out, 62, 1),
	H15L_PIN_GROUP(pwm2_out_1, 62, 2),
	H15L_PIN_GROUP(debug_out2, 62, 3),
	H15L_PIN_GROUP(uart2_rxd_in_5, 62, 4),
	H15L_PIN_GROUP(usb_overcurrent_n_in_2, 62, 5),
	H15L_PIN_GROUP(safety_error_n_out_3, 62, 6),
	H15L_PIN_GROUP(uart0_cts_in_3, 62, 7),
	H15L_PIN_GROUP(spi2_cs0_n_out, 62, 8),

	H15L_PIN_GROUP(eth_txd_7_out, 63, 1),
	H15L_PIN_GROUP(pwm3_out_0, 63, 2),
	H15L_PIN_GROUP(debug_out3, 63, 3),
	H15L_PIN_GROUP(uart2_txd_out_5, 63, 4),
	H15L_PIN_GROUP(can0_rx_in_4, 63, 5),
	H15L_PIN_GROUP(sdio1_vdd1_on_out_0, 63, 6),
	H15L_PIN_GROUP(uart0_rts_out_4, 63, 7),
	H15L_PIN_GROUP(spi2_cs1_n_out, 63, 8),

	H15L_PIN_GROUP(eth_gmii_tx_er_out, 64, 1),
	H15L_PIN_GROUP(pwm4_out_0, 64, 2),
	H15L_PIN_GROUP(debug_out4, 64, 3),
	H15L_PIN_GROUP(usb_drive_vbus_out_3, 64, 4),
	H15L_PIN_GROUP(uart1_rts_out_5, 64, 5),
	H15L_PIN_GROUP(safety_out_0_2, 64, 6),
	H15L_PIN_GROUP(can0_stby_out_7, 64, 7),
	H15L_PIN_GROUP(spi2_sclk_out, 64, 8),
	H15L_PIN_GROUP(boot_rom_failure_out, 65, 0),

	H15L_PIN_GROUP(pwm5_out_0, 65, 2),
	H15L_PIN_GROUP(debug_out5, 65, 3),
	H15L_PIN_GROUP(can0_tx_out_4, 65, 4),
	H15L_PIN_GROUP(uart2_rts_out_2, 65, 5),
	H15L_PIN_GROUP(safety_out_1_2, 65, 6),
	H15L_PIN_GROUP(can1_tx_out_4, 65, 7),
	H15L_PIN_GROUP(spi2_mosi_out, 65, 8),

	H15L_PIN_GROUP(eth_rxd_4_in, 66, 1),
	H15L_PIN_GROUP(sdio0_gp_in_2, 66, 2),
	H15L_PIN_GROUP(debug_out6, 66, 3),
	H15L_PIN_GROUP(i2c3_scl_3, 66, 4),
	H15L_PIN_GROUP(uart0_cts_in_4, 66, 5),
	H15L_PIN_GROUP(sdio0_vdd1_on_out_0, 66, 6),
	H15L_PIN_GROUP(parallel0_in, 66, 7),
	H15L_PIN_GROUP(spi2_miso_in, 66, 8),
	H15L_PIN_GROUP(sdio0_CD_in_3, 67, 0),
	H15L_PIN_GROUP(eth_rxd_5_in, 67, 1),

	H15L_PIN_GROUP(debug_out7, 67, 3),
	H15L_PIN_GROUP(i2c3_sda_3, 67, 4),
	H15L_PIN_GROUP(uart1_cts_in_4, 67, 5),
	H15L_PIN_GROUP(timer_ext_0_in_2, 67, 6),
	H15L_PIN_GROUP(parallel1_in, 67, 7),
	H15L_PIN_GROUP(sdio0_vdd1_on_out_1, 67, 8),

	H15L_PIN_GROUP(eth_rxd_6_in, 68, 1),
	H15L_PIN_GROUP(sdio1_gp_in_1, 68, 2),
	H15L_PIN_GROUP(debug_out8, 68, 3),
	H15L_PIN_GROUP(isp_flash_trig_out_3, 68, 4),
	H15L_PIN_GROUP(uart2_cts_in_2, 68, 5),
	H15L_PIN_GROUP(timer_ext_1_in_2, 68, 6),
	H15L_PIN_GROUP(parallel2_in, 68, 7),
	H15L_PIN_GROUP(sdio0_host_vdd1_stable_in, 68, 8),
	H15L_PIN_GROUP(sdio1_CD_in_2, 69, 0),
	H15L_PIN_GROUP(eth_rxd_7_in, 69, 1),

	H15L_PIN_GROUP(debug_out9, 69, 3),
	H15L_PIN_GROUP(safety_error_n_out_4, 69, 4),
	H15L_PIN_GROUP(uart2_rts_out_3, 69, 5),
	H15L_PIN_GROUP(timer_ext_2_in_1, 69, 6),
	H15L_PIN_GROUP(parallel3_in, 69, 7),
	H15L_PIN_GROUP(sdio1_vdd1_on_out_1, 69, 8),

	H15L_PIN_GROUP(gmii_rx_er_in, 70, 1),
	H15L_PIN_GROUP(debug_out10, 70, 2),
	H15L_PIN_GROUP(i2s0_sdi1_in_2, 70, 3),
	H15L_PIN_GROUP(sdio0_wp_in, 70, 4),
	H15L_PIN_GROUP(timer_ext_3_in_1, 70, 5),
	H15L_PIN_GROUP(can0_rx_in_5, 70, 6),
	H15L_PIN_GROUP(parallel4_in, 70, 7),
	H15L_PIN_GROUP(spi1_cs0_n_out_1, 70, 8),

	H15L_PIN_GROUP(gmii_rx_cs_in, 71, 1),
	H15L_PIN_GROUP(debug_out11, 71, 2),
	H15L_PIN_GROUP(i2s0_sdo1_out_2, 71, 3),
	H15L_PIN_GROUP(sdio1_wp_in_0, 71, 4),
	H15L_PIN_GROUP(sdio1_CD_in_3, 71, 5),
	H15L_PIN_GROUP(can0_tx_out_5, 71, 6),
	H15L_PIN_GROUP(parallel5_in, 71, 7),
	H15L_PIN_GROUP(spi1_cs1_n_out_1, 71, 8),

	H15L_PIN_GROUP(mii_tx_clk_in, 72, 1),
	H15L_PIN_GROUP(debug_out12, 72, 2),
	H15L_PIN_GROUP(i2s0_sck_1, 72, 3),
	H15L_PIN_GROUP(sdio0_gp_out_2, 72, 4),
	H15L_PIN_GROUP(uart3_cts_in_2, 72, 5),
	H15L_PIN_GROUP(i2s1_sck_3, 72, 6),
	H15L_PIN_GROUP(parallel6_in, 72, 7),
	H15L_PIN_GROUP(spi1_mosi_out_1, 72, 8),

	H15L_PIN_GROUP(gmii_rx_col_in, 73, 1),
	H15L_PIN_GROUP(debug_out13, 73, 2),
	H15L_PIN_GROUP(i2s0_sdi0_in_1, 73, 3),
	H15L_PIN_GROUP(safety_error_n_out_5, 73, 4),
	H15L_PIN_GROUP(uart3_rts_out_2, 73, 5),
	H15L_PIN_GROUP(i2s1_sdi0_in_3, 73, 6),
	H15L_PIN_GROUP(parallel7_in, 73, 7),
	H15L_PIN_GROUP(spi1_miso_in_1, 73, 8),

	H15L_PIN_GROUP(cpu_trace_clk_out_1, 74, 1),
	H15L_PIN_GROUP(debug_out14, 74, 2),
	H15L_PIN_GROUP(i2s0_sdo0_out_1, 74, 3),
	H15L_PIN_GROUP(can0_stby_out_8, 74, 4),
	H15L_PIN_GROUP(uart2_cts_in_3, 74, 5),
	H15L_PIN_GROUP(i2s1_sdo0_out_3, 74, 6),
	H15L_PIN_GROUP(parallel8_in, 74, 7),
	H15L_PIN_GROUP(spi1_sclk_out_1, 74, 8),

	H15L_PIN_GROUP(isp_pre_flash_out_3, 75, 1),
	H15L_PIN_GROUP(debug_out15, 75, 2),
	H15L_PIN_GROUP(i2s0_ws_1, 75, 3),
	H15L_PIN_GROUP(safety_out_1_3, 75, 4),
	H15L_PIN_GROUP(uart2_rts_out_4, 75, 5),
	H15L_PIN_GROUP(i2s1_ws_3, 75, 6),
	H15L_PIN_GROUP(parallel9_in, 75, 7),
	H15L_PIN_GROUP(sdio0_vdd1_on_out_2, 75, 8),

	H15L_PIN_GROUP(i2c2_scl_4, 76, 1),
	H15L_PIN_GROUP(uart3_cts_in_3, 76, 2),
	H15L_PIN_GROUP(safety_out_0_3, 76, 3),
	H15L_PIN_GROUP(sdio0_CD_in_4, 76, 4),
	H15L_PIN_GROUP(uart1_cts_in_5, 76, 5),
	H15L_PIN_GROUP(i2s1_mclk_3, 76, 6),
	H15L_PIN_GROUP(parallel10_in, 76, 7),
	H15L_PIN_GROUP(sdio1_vdd1_on_out_2, 76, 8),

	H15L_PIN_GROUP(i2c2_sda_3, 77, 1),
	H15L_PIN_GROUP(uart3_rts_out_3, 77, 2),
	H15L_PIN_GROUP(safety_out_1_4, 77, 3),
	H15L_PIN_GROUP(sdio1_CD_in_4, 77, 4),
	H15L_PIN_GROUP(uart1_rts_out_6, 77, 5),
	H15L_PIN_GROUP(i2s0_mclk_2, 77, 6),
	H15L_PIN_GROUP(parallel11_in, 77, 7),
	H15L_PIN_GROUP(sdio1_host_vdd1_stable_in, 77, 8),

	H15L_PIN_GROUP(i2c3_scl_4, 78, 1),
	H15L_PIN_GROUP(uart2_cts_in_4, 78, 2),
	H15L_PIN_GROUP(cpu_trace_clk_out_2, 78, 3),
	H15L_PIN_GROUP(i2c0_current_src_en_out_2, 78, 4),
	H15L_PIN_GROUP(i2s0_sdi1_in_3, 78, 5),
	H15L_PIN_GROUP(can0_rx_in_6, 78, 6),
	H15L_PIN_GROUP(parallel12_in, 78, 7),
	H15L_PIN_GROUP(uart2_rxd_in_6, 78, 8),

	H15L_PIN_GROUP(i2c3_sda_4, 79, 1),
	H15L_PIN_GROUP(uart2_rts_out_5, 79, 2),
	H15L_PIN_GROUP(isp_pre_flash_out_4, 79, 3),
	H15L_PIN_GROUP(i2c1_current_src_en_out_2, 79, 4),
	H15L_PIN_GROUP(i2s0_sdo1_out_3, 79, 5),
	H15L_PIN_GROUP(can0_tx_out_6, 79, 6),
	H15L_PIN_GROUP(parallel13_in, 79, 7),
	H15L_PIN_GROUP(uart2_txd_out_6, 79, 8),

	H15L_PIN_GROUP(uart2_rxd_in_7, 80, 1),
	H15L_PIN_GROUP(pwm0_out_4, 80, 2),
	H15L_PIN_GROUP(i2c0_current_src_en_out_3, 80, 3),
	H15L_PIN_GROUP(debug_out16, 80, 4),
	H15L_PIN_GROUP(sdio0_gp_in_3, 80, 5),
	H15L_PIN_GROUP(i2s1_sck_4, 80, 6),
	H15L_PIN_GROUP(parallel14_in, 80, 7),
	H15L_PIN_GROUP(spi0_cs0_n_out_1, 80, 8),

	H15L_PIN_GROUP(uart2_txd_out_7, 81, 1),
	H15L_PIN_GROUP(pwm1_out_4, 81, 2),
	H15L_PIN_GROUP(i2c1_current_src_en_out_3, 81, 3),
	H15L_PIN_GROUP(debug_out17, 81, 4),
	H15L_PIN_GROUP(sdio0_CD_in_5, 81, 5),
	H15L_PIN_GROUP(i2s1_sdi0_in_4, 81, 6),
	H15L_PIN_GROUP(parallel15_in, 81, 7),
	H15L_PIN_GROUP(spi0_cs1_n_out_1, 81, 8),

	H15L_PIN_GROUP(uart3_rxd_in_5, 82, 1),
	H15L_PIN_GROUP(pwm2_out_2, 82, 2),
	H15L_PIN_GROUP(i2c2_current_src_en_out_1, 82, 3),
	H15L_PIN_GROUP(debug_out18, 82, 4),
	H15L_PIN_GROUP(sdio1_gp_in_2, 82, 5),
	H15L_PIN_GROUP(i2s1_sdo0_out_4, 82, 6),
	H15L_PIN_GROUP(parallel16_in, 82, 7),
	H15L_PIN_GROUP(spi0_cs2_n_out_1, 82, 8),

	H15L_PIN_GROUP(uart3_txd_out_5, 83, 1),
	H15L_PIN_GROUP(pwm3_out_1, 83, 2),
	H15L_PIN_GROUP(i2c3_current_src_en_out_1, 83, 3),
	H15L_PIN_GROUP(debug_out19, 83, 4),
	H15L_PIN_GROUP(sdio1_CD_in_5, 83, 5),
	H15L_PIN_GROUP(i2s1_ws_4, 83, 6),
	H15L_PIN_GROUP(parallel17_in, 83, 7),
	H15L_PIN_GROUP(spi0_cs3_n_out_1, 83, 8),

	H15L_PIN_GROUP(uart0_cts_in_5, 84, 1),
	H15L_PIN_GROUP(pwm4_out_1, 84, 2),
	H15L_PIN_GROUP(cpu_trace_data0_out_2, 84, 3),
	H15L_PIN_GROUP(debug_out20, 84, 4),
	H15L_PIN_GROUP(sdio0_vdd1_on_out_3, 84, 5),
	H15L_PIN_GROUP(can0_rx_in_7, 84, 6),
	H15L_PIN_GROUP(parallel18_in, 84, 7),
	H15L_PIN_GROUP(spi0_mosi_out_1, 84, 8),

	H15L_PIN_GROUP(uart0_rts_out_5, 85, 1),
	H15L_PIN_GROUP(pwm5_out_1, 85, 2),
	H15L_PIN_GROUP(cpu_trace_data1_out_2, 85, 3),
	H15L_PIN_GROUP(debug_out21, 85, 4),
	H15L_PIN_GROUP(sdio1_wp_in_1, 85, 5),
	H15L_PIN_GROUP(can0_tx_out_7, 85, 6),
	H15L_PIN_GROUP(parallel19_in, 85, 7),
	H15L_PIN_GROUP(spi0_miso_in_1, 85, 8),

	H15L_PIN_GROUP(pwm0_out_5, 86, 1),
	H15L_PIN_GROUP(sdio0_gp_out_3, 86, 2),
	H15L_PIN_GROUP(i2s0_sdi1_in_4, 86, 3),
	H15L_PIN_GROUP(cpu_trace_data2_out_3, 86, 4),
	H15L_PIN_GROUP(debug_out30, 86, 5),
	H15L_PIN_GROUP(uart1_cts_in_6, 86, 6),
	H15L_PIN_GROUP(parallel20_in, 86, 7),
	H15L_PIN_GROUP(spi0_sclk_out_1, 86, 8),

	H15L_PIN_GROUP(pwm1_out_5, 87, 1),
	H15L_PIN_GROUP(sdio1_gp_out_1, 87, 2),
	H15L_PIN_GROUP(i2s0_sdo1_out_4, 87, 3),
	H15L_PIN_GROUP(cpu_trace_data3_out_2, 87, 4),
	H15L_PIN_GROUP(debug_out31, 87, 5),
	H15L_PIN_GROUP(uart1_rts_out_7, 87, 6),
	H15L_PIN_GROUP(parallel21_in, 87, 7),
	H15L_PIN_GROUP(can1_stby_out_7, 87, 8),

	H15L_PIN_GROUP(can0_rx_in_8, 88, 1),
	H15L_PIN_GROUP(i2s1_sck_5, 88, 2),
	H15L_PIN_GROUP(can0_stby_out_9, 88, 3),
	H15L_PIN_GROUP(usb_overcurrent_n_in_3, 88, 4),
	H15L_PIN_GROUP(spi3_cs0_n_in, 88, 5),
	H15L_PIN_GROUP(uart2_rxd_in_8, 88, 6),
	H15L_PIN_GROUP(parallel22_in, 88, 7),
	H15L_PIN_GROUP(can1_rx_in_5, 88, 8),

	H15L_PIN_GROUP(can0_tx_out_8, 89, 1),
	H15L_PIN_GROUP(i2s1_sdi0_in_5, 89, 2),
	H15L_PIN_GROUP(usb_overcurrent_n_in_4, 89, 3),
	H15L_PIN_GROUP(usb_drive_vbus_out_4, 89, 4),
	H15L_PIN_GROUP(spi3_mosi_in, 89, 5),
	H15L_PIN_GROUP(uart2_txd_out_8, 89, 6),
	H15L_PIN_GROUP(parallel23_in, 89, 7),
	H15L_PIN_GROUP(can1_tx_out_5, 89, 8),

	H15L_PIN_GROUP(can0_rx_in_9, 90, 1),
	H15L_PIN_GROUP(i2s1_sdo0_out_5, 90, 2),
	H15L_PIN_GROUP(can0_stby_out_10, 90, 3),
	H15L_PIN_GROUP(pwm0_out_6, 90, 4),
	H15L_PIN_GROUP(spi3_miso_out, 90, 5),
	H15L_PIN_GROUP(sdio0_uhs_sel_out_1, 90, 6),
	H15L_PIN_GROUP(parallel_hsync_in, 90, 7),
	H15L_PIN_GROUP(sdio1_uhs_sel_out_1, 90, 8),

	H15L_PIN_GROUP(can0_tx_out_9, 91, 1),
	H15L_PIN_GROUP(i2s1_ws_5, 91, 2),
	H15L_PIN_GROUP(usb_overcurrent_n_in_5, 91, 3),
	H15L_PIN_GROUP(pwm1_out_6, 91, 4),
	H15L_PIN_GROUP(spi3_sclk_in, 91, 5),
	H15L_PIN_GROUP(sdio1_uhs_sel_out_2, 91, 6),
	H15L_PIN_GROUP(parallel_vsync_in, 91, 7),
	H15L_PIN_GROUP(sdio0_uhs_sel_out_2, 91, 8),
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
	"uart0_rxd_in_1_grp",
	"uart0_rxd_in_2_grp",
};

static const char *const can0_rx_in_grps[] = {
	"can0_rx_in_0_grp",
	"can0_rx_in_1_grp",
	"can0_rx_in_2_grp",
	"can0_rx_in_3_grp",
	"can0_rx_in_4_grp",
	"can0_rx_in_5_grp",
	"can0_rx_in_6_grp",
	"can0_rx_in_7_grp",
	"can0_rx_in_8_grp",
	"can0_rx_in_9_grp",
};

static const char *const can1_rx_in_grps[] = {
	"can1_rx_in_0_grp",
	"can1_rx_in_1_grp",
	"can1_rx_in_2_grp",
	"can1_rx_in_3_grp",
	"can1_rx_in_4_grp",
	"can1_rx_in_5_grp",
};

static const char *const uart0_txd_out_grps[] = {
	"uart0_txd_out_0_grp",
	"uart0_txd_out_1_grp",
	"uart0_txd_out_2_grp",
};

static const char *const can0_tx_out_grps[] = {
	"can0_tx_out_0_grp",
	"can0_tx_out_1_grp",
	"can0_tx_out_2_grp",
	"can0_tx_out_3_grp",
	"can0_tx_out_4_grp",
	"can0_tx_out_5_grp",
	"can0_tx_out_6_grp",
	"can0_tx_out_7_grp",
	"can0_tx_out_8_grp",
	"can0_tx_out_9_grp",
};

static const char *const can1_tx_out_grps[] = {
	"can1_tx_out_0_grp",
	"can1_tx_out_1_grp",
	"can1_tx_out_2_grp",
	"can1_tx_out_3_grp",
	"can1_tx_out_4_grp",
	"can1_tx_out_5_grp",
};

static const char *const can0_stby_out_grps[] = {
	"can0_stby_out_0_grp",
	"can0_stby_out_1_grp",
	"can0_stby_out_2_grp",
	"can0_stby_out_3_grp",
	"can0_stby_out_4_grp",
	"can0_stby_out_5_grp",
	"can0_stby_out_6_grp",
	"can0_stby_out_7_grp",
	"can0_stby_out_8_grp",
	"can0_stby_out_9_grp",
	"can0_stby_out_10_grp",
};

static const char *const can1_stby_out_grps[] = {
	"can1_stby_out_0_grp",
	"can1_stby_out_1_grp",
	"can1_stby_out_2_grp",
	"can1_stby_out_3_grp",
	"can1_stby_out_4_grp",
	"can1_stby_out_5_grp",
	"can1_stby_out_6_grp",
	"can1_stby_out_7_grp",
};

static const char *const uart1_rxd_in_grps[] = {
	"uart1_rxd_in_0_grp",
	"uart1_rxd_in_1_grp",
	"uart1_rxd_in_2_grp",
};

static const char *const uart1_txd_out_grps[] = {
	"uart1_txd_out_0_grp",
	"uart1_txd_out_1_grp",
	"uart1_txd_out_2_grp",
};

static const char *const eth_mdc_out_grps[] = {
	"eth_mdc_out_grp",
};

static const char *const gpio16_grps[] = {
	"gpio16_0_grp",
	"gpio16_1_grp",
	"gpio16_2_grp",
	"gpio16_3_grp",
};

static const char *const gpio0_grps[] = {
	"gpio0_0_grp",
	"gpio0_1_grp",
	"gpio0_2_grp",
	"gpio0_3_grp",
	"gpio0_4_grp",
};

static const char *const eth_mdio_grps[] = {
	"eth_mdio_grp",
};

static const char *const gpio17_grps[] = {
	"gpio17_0_grp",
	"gpio17_1_grp",
	"gpio17_2_grp",
	"gpio17_3_grp",
};

static const char *const gpio1_grps[] = {
	"gpio1_0_grp",
	"gpio1_1_grp",
	"gpio1_2_grp",
	"gpio1_3_grp",
};

static const char *const eth_rx_clk_in_grps[] = {
	"eth_rx_clk_in_grp",
};

static const char *const gpio18_grps[] = {
	"gpio18_0_grp",
	"gpio18_1_grp",
	"gpio18_2_grp",
	"gpio18_3_grp",
};

static const char *const gpio2_grps[] = {
	"gpio2_0_grp",
	"gpio2_1_grp",
	"gpio2_2_grp",
	"gpio2_3_grp",
};

static const char *const eth_rx_ctl_crs_dv_in_grps[] = {
	"eth_rx_ctl_crs_dv_in_grp",
};

static const char *const gpio19_grps[] = {
	"gpio19_0_grp",
	"gpio19_1_grp",
	"gpio19_2_grp",
	"gpio19_3_grp",
};

static const char *const gpio3_grps[] = {
	"gpio3_0_grp",
	"gpio3_1_grp",
	"gpio3_2_grp",
	"gpio3_3_grp",
};

static const char *const eth_rxd_0_in_grps[] = {
	"eth_rxd_0_in_grp",
};

static const char *const gpio20_grps[] = {
	"gpio20_0_grp",
	"gpio20_1_grp",
	"gpio20_2_grp",
	"gpio20_3_grp",
};

static const char *const gpio4_grps[] = {
	"gpio4_0_grp",
	"gpio4_1_grp",
	"gpio4_2_grp",
	"gpio4_3_grp",
};

static const char *const uart0_cts_in_grps[] = {
	"uart0_cts_in_0_grp",
	"uart0_cts_in_1_grp",
	"uart0_cts_in_2_grp",
	"uart0_cts_in_3_grp",
	"uart0_cts_in_4_grp",
	"uart0_cts_in_5_grp",
};

static const char *const eth_rxd_1_in_grps[] = {
	"eth_rxd_1_in_grp",
};

static const char *const gpio21_grps[] = {
	"gpio21_0_grp",
	"gpio21_1_grp",
	"gpio21_2_grp",
	"gpio21_3_grp",
};

static const char *const gpio5_grps[] = {
	"gpio5_0_grp",
	"gpio5_1_grp",
	"gpio5_2_grp",
	"gpio5_3_grp",
};

static const char *const uart1_cts_in_grps[] = {
	"uart1_cts_in_0_grp",
	"uart1_cts_in_1_grp",
	"uart1_cts_in_2_grp",
	"uart1_cts_in_3_grp",
	"uart1_cts_in_4_grp",
	"uart1_cts_in_5_grp",
	"uart1_cts_in_6_grp",
};

static const char *const eth_rxd_2_in_grps[] = {
	"eth_rxd_2_in_grp",
};

static const char *const gpio22_grps[] = {
	"gpio22_0_grp",
	"gpio22_1_grp",
	"gpio22_2_grp",
	"gpio22_3_grp",
};

static const char *const gpio6_grps[] = {
	"gpio6_0_grp",
	"gpio6_1_grp",
	"gpio6_2_grp",
	"gpio6_3_grp",
};

static const char *const pwm0_out_grps[] = {
	"pwm0_out_0_grp",
	"pwm0_out_1_grp",
	"pwm0_out_2_grp",
	"pwm0_out_3_grp",
	"pwm0_out_4_grp",
	"pwm0_out_5_grp",
	"pwm0_out_6_grp",
};

static const char *const uart2_cts_in_grps[] = {
	"uart2_cts_in_0_grp",
	"uart2_cts_in_1_grp",
	"uart2_cts_in_2_grp",
	"uart2_cts_in_3_grp",
	"uart2_cts_in_4_grp",
};

static const char *const safety_error_n_out_grps[] = {
	"safety_error_n_out_0_grp",
	"safety_error_n_out_1_grp",
	"safety_error_n_out_2_grp",
	"safety_error_n_out_3_grp",
	"safety_error_n_out_4_grp",
	"safety_error_n_out_5_grp",
};

static const char *const i2s0_sdi1_in_grps[] = {
	"i2s0_sdi1_in_0_grp",
	"i2s0_sdi1_in_1_grp",
	"i2s0_sdi1_in_2_grp",
	"i2s0_sdi1_in_3_grp",
	"i2s0_sdi1_in_4_grp",
};

static const char *const eth_rxd_3_er_in_grps[] = {
	"eth_rxd_3_er_in_grp",
};

static const char *const gpio23_grps[] = {
	"gpio23_0_grp",
	"gpio23_1_grp",
	"gpio23_2_grp",
	"gpio23_3_grp",
};

static const char *const gpio7_grps[] = {
	"gpio7_0_grp",
	"gpio7_1_grp",
	"gpio7_2_grp",
	"gpio7_3_grp",
};

static const char *const eth_tx_clk_out_grps[] = {
	"eth_tx_clk_out_grp",
};

static const char *const gpio24_grps[] = {
	"gpio24_0_grp",
	"gpio24_1_grp",
	"gpio24_2_grp",
	"gpio24_3_grp",
};

static const char *const gpio8_grps[] = {
	"gpio8_0_grp",
	"gpio8_1_grp",
	"gpio8_2_grp",
	"gpio8_3_grp",
};

static const char *const i2c2_scl_grps[] = {
	"i2c2_scl_0_grp",
	"i2c2_scl_1_grp",
	"i2c2_scl_2_grp",
	"i2c2_scl_3_grp",
	"i2c2_scl_4_grp",
};

static const char *const eth_tx_ctl_en_out_grps[] = {
	"eth_tx_ctl_en_out_grp",
};

static const char *const gpio25_grps[] = {
	"gpio25_0_grp",
	"gpio25_1_grp",
	"gpio25_2_grp",
	"gpio25_3_grp",
	"gpio25_4_grp",
};

static const char *const gpio9_grps[] = {
	"gpio9_0_grp",
	"gpio9_1_grp",
	"gpio9_2_grp",
	"gpio9_3_grp",
	"gpio9_4_grp",
	"gpio9_5_grp",
};

static const char *const eth_txd_0_out_grps[] = {
	"eth_txd_0_out_grp",
};

static const char *const gpio26_grps[] = {
	"gpio26_0_grp",
	"gpio26_1_grp",
	"gpio26_2_grp",
	"gpio26_3_grp",
};

static const char *const gpio10_grps[] = {
	"gpio10_0_grp",
	"gpio10_1_grp",
	"gpio10_2_grp",
	"gpio10_3_grp",
	"gpio10_4_grp",
};

static const char *const uart0_rts_out_grps[] = {
	"uart0_rts_out_0_grp",
	"uart0_rts_out_1_grp",
	"uart0_rts_out_2_grp",
	"uart0_rts_out_3_grp",
	"uart0_rts_out_4_grp",
	"uart0_rts_out_5_grp",
};

static const char *const cpu_trace_data0_out_grps[] = {
	"cpu_trace_data0_out_0_grp",
	"cpu_trace_data0_out_1_grp",
	"cpu_trace_data0_out_2_grp",
};

static const char *const eth_txd_1_out_grps[] = {
	"eth_txd_1_out_grp",
};

static const char *const gpio27_grps[] = {
	"gpio27_0_grp",
	"gpio27_1_grp",
	"gpio27_2_grp",
};

static const char *const gpio11_grps[] = {
	"gpio11_0_grp",
	"gpio11_1_grp",
	"gpio11_2_grp",
	"gpio11_3_grp",
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

static const char *const cpu_trace_data1_out_grps[] = {
	"cpu_trace_data1_out_0_grp",
	"cpu_trace_data1_out_1_grp",
	"cpu_trace_data1_out_2_grp",
};

static const char *const eth_txd_2_out_grps[] = {
	"eth_txd_2_out_grp",
};

static const char *const gpio28_grps[] = {
	"gpio28_0_grp",
	"gpio28_1_grp",
	"gpio28_2_grp",
};

static const char *const gpio12_grps[] = {
	"gpio12_0_grp",
	"gpio12_1_grp",
	"gpio12_2_grp",
};

static const char *const pwm1_out_grps[] = {
	"pwm1_out_0_grp",
	"pwm1_out_1_grp",
	"pwm1_out_2_grp",
	"pwm1_out_3_grp",
	"pwm1_out_4_grp",
	"pwm1_out_5_grp",
	"pwm1_out_6_grp",
};

static const char *const uart2_rts_out_grps[] = {
	"uart2_rts_out_0_grp",
	"uart2_rts_out_1_grp",
	"uart2_rts_out_2_grp",
	"uart2_rts_out_3_grp",
	"uart2_rts_out_4_grp",
	"uart2_rts_out_5_grp",
};

static const char *const i2s0_sdo1_out_grps[] = {
	"i2s0_sdo1_out_0_grp",
	"i2s0_sdo1_out_1_grp",
	"i2s0_sdo1_out_2_grp",
	"i2s0_sdo1_out_3_grp",
	"i2s0_sdo1_out_4_grp",
};

static const char *const cpu_trace_data2_out_grps[] = {
	"cpu_trace_data2_out_0_grp",
	"cpu_trace_data2_out_1_grp",
	"cpu_trace_data2_out_2_grp",
	"cpu_trace_data2_out_3_grp",
};

static const char *const eth_txd_3_out_grps[] = {
	"eth_txd_3_out_grp",
};

static const char *const gpio29_grps[] = {
	"gpio29_0_grp",
	"gpio29_1_grp",
	"gpio29_2_grp",
};

static const char *const gpio13_grps[] = {
	"gpio13_0_grp",
	"gpio13_1_grp",
	"gpio13_2_grp",
};

static const char *const cpu_trace_data3_out_grps[] = {
	"cpu_trace_data3_out_0_grp",
	"cpu_trace_data3_out_1_grp",
	"cpu_trace_data3_out_2_grp",
};

static const char *const i2s0_sck_grps[] = {
	"i2s0_sck_0_grp",
	"i2s0_sck_1_grp",
};

static const char *const gpio30_grps[] = {
	"gpio30_0_grp",
	"gpio30_1_grp",
	"gpio30_2_grp",
};

static const char *const gpio14_grps[] = {
	"gpio14_0_grp",
	"gpio14_1_grp",
	"gpio14_2_grp",
};

static const char *const isp_flash_trig_out_grps[] = {
	"isp_flash_trig_out_0_grp",
	"isp_flash_trig_out_1_grp",
	"isp_flash_trig_out_2_grp",
	"isp_flash_trig_out_3_grp",
};

static const char *const safety_out_0_grps[] = {
	"safety_out_0_0_grp",
	"safety_out_0_1_grp",
	"safety_out_0_2_grp",
	"safety_out_0_3_grp",
};

static const char *const timer_ext_0_in_grps[] = {
	"timer_ext_0_in_0_grp",
	"timer_ext_0_in_1_grp",
	"timer_ext_0_in_2_grp",
};

static const char *const i2s0_sdi0_in_grps[] = {
	"i2s0_sdi0_in_0_grp",
	"i2s0_sdi0_in_1_grp",
};

static const char *const gpio31_grps[] = {
	"gpio31_0_grp",
	"gpio31_1_grp",
	"gpio31_2_grp",
};

static const char *const gpio15_grps[] = {
	"gpio15_0_grp",
	"gpio15_1_grp",
	"gpio15_2_grp",
};

static const char *const uart2_rxd_in_grps[] = {
	"uart2_rxd_in_0_grp",
	"uart2_rxd_in_1_grp",
	"uart2_rxd_in_2_grp",
	"uart2_rxd_in_3_grp",
	"uart2_rxd_in_4_grp",
	"uart2_rxd_in_5_grp",
	"uart2_rxd_in_6_grp",
	"uart2_rxd_in_7_grp",
	"uart2_rxd_in_8_grp",
};

static const char *const usb_overcurrent_n_in_grps[] = {
	"usb_overcurrent_n_in_0_grp",
	"usb_overcurrent_n_in_1_grp",
	"usb_overcurrent_n_in_2_grp",
	"usb_overcurrent_n_in_3_grp",
	"usb_overcurrent_n_in_4_grp",
	"usb_overcurrent_n_in_5_grp",
};

static const char *const isp_pre_flash_out_grps[] = {
	"isp_pre_flash_out_0_grp",
	"isp_pre_flash_out_1_grp",
	"isp_pre_flash_out_2_grp",
	"isp_pre_flash_out_3_grp",
	"isp_pre_flash_out_4_grp",
};

static const char *const safety_out_1_grps[] = {
	"safety_out_1_0_grp",
	"safety_out_1_1_grp",
	"safety_out_1_2_grp",
	"safety_out_1_3_grp",
	"safety_out_1_4_grp",
};

static const char *const timer_ext_1_in_grps[] = {
	"timer_ext_1_in_0_grp",
	"timer_ext_1_in_1_grp",
	"timer_ext_1_in_2_grp",
};

static const char *const i2s0_sdo0_out_grps[] = {
	"i2s0_sdo0_out_0_grp",
	"i2s0_sdo0_out_1_grp",
};

static const char *const uart2_txd_out_grps[] = {
	"uart2_txd_out_0_grp",
	"uart2_txd_out_1_grp",
	"uart2_txd_out_2_grp",
	"uart2_txd_out_3_grp",
	"uart2_txd_out_4_grp",
	"uart2_txd_out_5_grp",
	"uart2_txd_out_6_grp",
	"uart2_txd_out_7_grp",
	"uart2_txd_out_8_grp",
};

static const char *const usb_drive_vbus_out_grps[] = {
	"usb_drive_vbus_out_0_grp",
	"usb_drive_vbus_out_1_grp",
	"usb_drive_vbus_out_2_grp",
	"usb_drive_vbus_out_3_grp",
	"usb_drive_vbus_out_4_grp",
};

static const char *const cpu_trace_clk_out_grps[] = {
	"cpu_trace_clk_out_0_grp",
	"cpu_trace_clk_out_1_grp",
	"cpu_trace_clk_out_2_grp",
};

static const char *const i2s0_ws_grps[] = {
	"i2s0_ws_0_grp",
	"i2s0_ws_1_grp",
};

static const char *const flash_spi_cs_0_n_out_grps[] = {
	"flash_spi_cs_0_n_out_grp",
};

static const char *const debug_out22_grps[] = {
	"debug_out22_grp",
};

static const char *const spi1_cs0_n_out_grps[] = {
	"spi1_cs0_n_out_0_grp",
	"spi1_cs0_n_out_1_grp",
};

static const char *const spi0_cs0_n_out_grps[] = {
	"spi0_cs0_n_out_0_grp",
	"spi0_cs0_n_out_1_grp",
};

static const char *const flash_spi_cs_1_n_out_grps[] = {
	"flash_spi_cs_1_n_out_grp",
};

static const char *const debug_out23_grps[] = {
	"debug_out23_grp",
};

static const char *const spi1_cs1_n_out_grps[] = {
	"spi1_cs1_n_out_0_grp",
	"spi1_cs1_n_out_1_grp",
};

static const char *const spi0_cs1_n_out_grps[] = {
	"spi0_cs1_n_out_0_grp",
	"spi0_cs1_n_out_1_grp",
};

static const char *const flash_spi_cs2n_clkc_rebar_out_grps[] = {
	"flash_spi_cs2n_clkc_rebar_out_grp",
};

static const char *const uart3_txd_out_grps[] = {
	"uart3_txd_out_0_grp",
	"uart3_txd_out_1_grp",
	"uart3_txd_out_2_grp",
	"uart3_txd_out_3_grp",
	"uart3_txd_out_4_grp",
	"uart3_txd_out_5_grp",
};

static const char *const spi1_mosi_out_grps[] = {
	"spi1_mosi_out_0_grp",
	"spi1_mosi_out_1_grp",
};

static const char *const spi0_cs2_n_out_grps[] = {
	"spi0_cs2_n_out_0_grp",
	"spi0_cs2_n_out_1_grp",
};

static const char *const flash_spi_ds_cs3n_lpbk_grps[] = {
	"flash_spi_ds_cs3n_lpbk_grp",
};

static const char *const uart3_rxd_in_grps[] = {
	"uart3_rxd_in_0_grp",
	"uart3_rxd_in_1_grp",
	"uart3_rxd_in_2_grp",
	"uart3_rxd_in_3_grp",
	"uart3_rxd_in_4_grp",
	"uart3_rxd_in_5_grp",
};

static const char *const spi1_sclk_out_grps[] = {
	"spi1_sclk_out_0_grp",
	"spi1_sclk_out_1_grp",
};

static const char *const spi0_cs3_n_out_grps[] = {
	"spi0_cs3_n_out_0_grp",
	"spi0_cs3_n_out_1_grp",
};

static const char *const flash_spi_dq_0_grps[] = {
	"flash_spi_dq_0_grp",
};

static const char *const sdio0_gp_in_grps[] = {
	"sdio0_gp_in_0_grp",
	"sdio0_gp_in_1_grp",
	"sdio0_gp_in_2_grp",
	"sdio0_gp_in_3_grp",
};

static const char *const sdio0_CD_in_grps[] = {
	"sdio0_CD_in_0_grp",
	"sdio0_CD_in_1_grp",
	"sdio0_CD_in_2_grp",
	"sdio0_CD_in_3_grp",
	"sdio0_CD_in_4_grp",
	"sdio0_CD_in_5_grp",
};

static const char *const spi1_miso_in_grps[] = {
	"spi1_miso_in_0_grp",
	"spi1_miso_in_1_grp",
};

static const char *const spi0_mosi_out_grps[] = {
	"spi0_mosi_out_0_grp",
	"spi0_mosi_out_1_grp",
};

static const char *const flash_spi_dq_1_grps[] = {
	"flash_spi_dq_1_grp",
};

static const char *const sdio1_gp_in_grps[] = {
	"sdio1_gp_in_0_grp",
	"sdio1_gp_in_1_grp",
	"sdio1_gp_in_2_grp",
};

static const char *const sdio1_CD_in_grps[] = {
	"sdio1_CD_in_0_grp",
	"sdio1_CD_in_1_grp",
	"sdio1_CD_in_2_grp",
	"sdio1_CD_in_3_grp",
	"sdio1_CD_in_4_grp",
	"sdio1_CD_in_5_grp",
};

static const char *const spi0_miso_in_grps[] = {
	"spi0_miso_in_0_grp",
	"spi0_miso_in_1_grp",
};

static const char *const flash_spi_dq_2_grps[] = {
	"flash_spi_dq_2_grp",
};

static const char *const i2c0_current_src_en_out_grps[] = {
	"i2c0_current_src_en_out_0_grp",
	"i2c0_current_src_en_out_1_grp",
	"i2c0_current_src_en_out_2_grp",
	"i2c0_current_src_en_out_3_grp",
};

static const char *const spi0_sclk_out_grps[] = {
	"spi0_sclk_out_0_grp",
	"spi0_sclk_out_1_grp",
};

static const char *const flash_spi_dq_3_grps[] = {
	"flash_spi_dq_3_grp",
};

static const char *const i2c1_current_src_en_out_grps[] = {
	"i2c1_current_src_en_out_0_grp",
	"i2c1_current_src_en_out_1_grp",
	"i2c1_current_src_en_out_2_grp",
	"i2c1_current_src_en_out_3_grp",
};

static const char *const flash_spi_reset_n_out_grps[] = {
	"flash_spi_reset_n_out_grp",
};

static const char *const sdio0_gp_out_grps[] = {
	"sdio0_gp_out_0_grp",
	"sdio0_gp_out_1_grp",
	"sdio0_gp_out_2_grp",
	"sdio0_gp_out_3_grp",
};

static const char *const flash_spi_sclk_out_grps[] = {
	"flash_spi_sclk_out_grp",
};

static const char *const sdio1_gp_out_grps[] = {
	"sdio1_gp_out_0_grp",
	"sdio1_gp_out_1_grp",
};

static const char *const pcie_wake_n_grps[] = {
	"pcie_wake_n_grp",
};

static const char *const flash_ospi_dq_4_grps[] = {
	"flash_ospi_dq_4_grp",
};

static const char *const debug_out24_grps[] = {
	"debug_out24_grp",
};

static const char *const pcie_clkreq_n_grps[] = {
	"pcie_clkreq_n_grp",
};

static const char *const flash_ospi_dq_5_grps[] = {
	"flash_ospi_dq_5_grp",
};

static const char *const debug_out25_grps[] = {
	"debug_out25_grp",
};

static const char *const pcie_perst_n_in_grps[] = {
	"pcie_perst_n_in_grp",
};

static const char *const flash_ospi_dq_6_grps[] = {
	"flash_ospi_dq_6_grp",
};

static const char *const debug_out26_grps[] = {
	"debug_out26_grp",
};

static const char *const pcie_mperst_n_out_grps[] = {
	"pcie_mperst_n_out_grp",
};

static const char *const flash_ospi_dq_7_grps[] = {
	"flash_ospi_dq_7_grp",
};

static const char *const debug_out27_grps[] = {
	"debug_out27_grp",
};

static const char *const SDIO0_DATA0_SDIO1_DATA4_grps[] = {
	"SDIO0_DATA0_SDIO1_DATA4_grp",
};

static const char *const i2s1_sck_grps[] = {
	"i2s1_sck_0_grp",
	"i2s1_sck_1_grp",
	"i2s1_sck_2_grp",
	"i2s1_sck_3_grp",
	"i2s1_sck_4_grp",
	"i2s1_sck_5_grp",
};

static const char *const SDIO0_DATA1_SDIO1_DATA5_grps[] = {
	"SDIO0_DATA1_SDIO1_DATA5_grp",
};

static const char *const i2c2_sda_grps[] = {
	"i2c2_sda_0_grp",
	"i2c2_sda_1_grp",
	"i2c2_sda_2_grp",
	"i2c2_sda_3_grp",
};

static const char *const i2s1_sdi0_in_grps[] = {
	"i2s1_sdi0_in_0_grp",
	"i2s1_sdi0_in_1_grp",
	"i2s1_sdi0_in_2_grp",
	"i2s1_sdi0_in_3_grp",
	"i2s1_sdi0_in_4_grp",
	"i2s1_sdi0_in_5_grp",
};

static const char *const SDIO0_DATA2_SDIO1_DATA6_grps[] = {
	"SDIO0_DATA2_SDIO1_DATA6_grp",
};

static const char *const i2c3_scl_grps[] = {
	"i2c3_scl_0_grp",
	"i2c3_scl_1_grp",
	"i2c3_scl_2_grp",
	"i2c3_scl_3_grp",
	"i2c3_scl_4_grp",
};

static const char *const i2s1_sdo0_out_grps[] = {
	"i2s1_sdo0_out_0_grp",
	"i2s1_sdo0_out_1_grp",
	"i2s1_sdo0_out_2_grp",
	"i2s1_sdo0_out_3_grp",
	"i2s1_sdo0_out_4_grp",
	"i2s1_sdo0_out_5_grp",
};

static const char *const SDIO0_DATA3_SDIO1_DATA7_grps[] = {
	"SDIO0_DATA3_SDIO1_DATA7_grp",
};

static const char *const i2c3_sda_grps[] = {
	"i2c3_sda_0_grp",
	"i2c3_sda_1_grp",
	"i2c3_sda_2_grp",
	"i2c3_sda_3_grp",
	"i2c3_sda_4_grp",
};

static const char *const i2s1_ws_grps[] = {
	"i2s1_ws_0_grp",
	"i2s1_ws_1_grp",
	"i2s1_ws_2_grp",
	"i2s1_ws_3_grp",
	"i2s1_ws_4_grp",
	"i2s1_ws_5_grp",
};

static const char *const i2s0_mclk_grps[] = {
	"i2s0_mclk_0_grp",
	"i2s0_mclk_1_grp",
	"i2s0_mclk_2_grp",
};

static const char *const SDIO0_SDCLK_SDIO1_DS_grps[] = {
	"SDIO0_SDCLK_SDIO1_DS_grp",
};

static const char *const uart3_cts_in_grps[] = {
	"uart3_cts_in_0_grp",
	"uart3_cts_in_1_grp",
	"uart3_cts_in_2_grp",
	"uart3_cts_in_3_grp",
};

static const char *const i2s1_mclk_grps[] = {
	"i2s1_mclk_0_grp",
	"i2s1_mclk_1_grp",
	"i2s1_mclk_2_grp",
	"i2s1_mclk_3_grp",
};

static const char *const SDIO0_CMD_SDIO1_RSTN_grps[] = {
	"SDIO0_CMD_SDIO1_RSTN_grp",
};

static const char *const uart3_rts_out_grps[] = {
	"uart3_rts_out_0_grp",
	"uart3_rts_out_1_grp",
	"uart3_rts_out_2_grp",
	"uart3_rts_out_3_grp",
};

static const char *const SDIO1_DATA0_grps[] = {
	"SDIO1_DATA0_grp",
};

static const char *const SDIO1_DATA1_grps[] = {
	"SDIO1_DATA1_grp",
};

static const char *const SDIO1_DATA2_grps[] = {
	"SDIO1_DATA2_grp",
};

static const char *const SDIO1_DATA3_grps[] = {
	"SDIO1_DATA3_grp",
};

static const char *const SDIO1_SDCLK_grps[] = {
	"SDIO1_SDCLK_grp",
};

static const char *const parallel_pclk_in_grps[] = {
	"parallel_pclk_in_grp",
};

static const char *const i2c2_current_src_en_out_grps[] = {
	"i2c2_current_src_en_out_0_grp",
	"i2c2_current_src_en_out_1_grp",
};

static const char *const debug_out28_grps[] = {
	"debug_out28_grp",
};

static const char *const i2c3_current_src_en_out_grps[] = {
	"i2c3_current_src_en_out_0_grp",
	"i2c3_current_src_en_out_1_grp",
};

static const char *const debug_out29_grps[] = {
	"debug_out29_grp",
};

static const char *const jtag_tck_in_grps[] = {
	"jtag_tck_in_grp",
};

static const char *const jtag_tdi_in_grps[] = {
	"jtag_tdi_in_grp",
};

static const char *const jtag_tdo_out_grps[] = {
	"jtag_tdo_out_grp",
};

static const char *const pwm2_out_grps[] = {
	"pwm2_out_0_grp",
	"pwm2_out_1_grp",
	"pwm2_out_2_grp",
};

static const char *const jtag_tms_grps[] = {
	"jtag_tms_grp",
};

static const char *const timer_ext_3_in_grps[] = {
	"timer_ext_3_in_0_grp",
	"timer_ext_3_in_1_grp",
};

static const char *const jtag_trstn_in_grps[] = {
	"jtag_trstn_in_grp",
};

static const char *const timer_ext_2_in_grps[] = {
	"timer_ext_2_in_0_grp",
	"timer_ext_2_in_1_grp",
};

static const char *const eth_txd_4_out_grps[] = {
	"eth_txd_4_out_grp",
};

static const char *const debug_out0_grps[] = {
	"debug_out0_grp",
};

static const char *const sdio0_uhs_sel_out_grps[] = {
	"sdio0_uhs_sel_out_0_grp",
	"sdio0_uhs_sel_out_1_grp",
	"sdio0_uhs_sel_out_2_grp",
};

static const char *const sdio1_uhs_sel_out_grps[] = {
	"sdio1_uhs_sel_out_0_grp",
	"sdio1_uhs_sel_out_1_grp",
	"sdio1_uhs_sel_out_2_grp",
};

static const char *const eth_txd_5_out_grps[] = {
	"eth_txd_5_out_grp",
};

static const char *const debug_out1_grps[] = {
	"debug_out1_grp",
};

static const char *const eth_txd_6_out_grps[] = {
	"eth_txd_6_out_grp",
};

static const char *const debug_out2_grps[] = {
	"debug_out2_grp",
};

static const char *const spi2_cs0_n_out_grps[] = {
	"spi2_cs0_n_out_grp",
};

static const char *const eth_txd_7_out_grps[] = {
	"eth_txd_7_out_grp",
};

static const char *const pwm3_out_grps[] = {
	"pwm3_out_0_grp",
	"pwm3_out_1_grp",
};

static const char *const debug_out3_grps[] = {
	"debug_out3_grp",
};

static const char *const sdio1_vdd1_on_out_grps[] = {
	"sdio1_vdd1_on_out_0_grp",
	"sdio1_vdd1_on_out_1_grp",
	"sdio1_vdd1_on_out_2_grp",
};

static const char *const spi2_cs1_n_out_grps[] = {
	"spi2_cs1_n_out_grp",
};

static const char *const eth_gmii_tx_er_out_grps[] = {
	"eth_gmii_tx_er_out_grp",
};

static const char *const pwm4_out_grps[] = {
	"pwm4_out_0_grp",
	"pwm4_out_1_grp",
};

static const char *const debug_out4_grps[] = {
	"debug_out4_grp",
};

static const char *const spi2_sclk_out_grps[] = {
	"spi2_sclk_out_grp",
};

static const char *const boot_rom_failure_out_grps[] = {
	"boot_rom_failure_out_grp",
};

static const char *const pwm5_out_grps[] = {
	"pwm5_out_0_grp",
	"pwm5_out_1_grp",
};

static const char *const debug_out5_grps[] = {
	"debug_out5_grp",
};

static const char *const spi2_mosi_out_grps[] = {
	"spi2_mosi_out_grp",
};

static const char *const eth_rxd_4_in_grps[] = {
	"eth_rxd_4_in_grp",
};

static const char *const debug_out6_grps[] = {
	"debug_out6_grp",
};

static const char *const sdio0_vdd1_on_out_grps[] = {
	"sdio0_vdd1_on_out_0_grp",
	"sdio0_vdd1_on_out_1_grp",
	"sdio0_vdd1_on_out_2_grp",
	"sdio0_vdd1_on_out_3_grp",
};

static const char *const parallel0_in_grps[] = {
	"parallel0_in_grp",
};

static const char *const spi2_miso_in_grps[] = {
	"spi2_miso_in_grp",
};

static const char *const eth_rxd_5_in_grps[] = {
	"eth_rxd_5_in_grp",
};

static const char *const debug_out7_grps[] = {
	"debug_out7_grp",
};

static const char *const parallel1_in_grps[] = {
	"parallel1_in_grp",
};

static const char *const eth_rxd_6_in_grps[] = {
	"eth_rxd_6_in_grp",
};

static const char *const debug_out8_grps[] = {
	"debug_out8_grp",
};

static const char *const parallel2_in_grps[] = {
	"parallel2_in_grp",
};

static const char *const sdio0_host_vdd1_stable_in_grps[] = {
	"sdio0_host_vdd1_stable_in_grp",
};

static const char *const eth_rxd_7_in_grps[] = {
	"eth_rxd_7_in_grp",
};

static const char *const debug_out9_grps[] = {
	"debug_out9_grp",
};

static const char *const parallel3_in_grps[] = {
	"parallel3_in_grp",
};

static const char *const gmii_rx_er_in_grps[] = {
	"gmii_rx_er_in_grp",
};

static const char *const debug_out10_grps[] = {
	"debug_out10_grp",
};

static const char *const sdio0_wp_in_grps[] = {
	"sdio0_wp_in_grp",
};

static const char *const parallel4_in_grps[] = {
	"parallel4_in_grp",
};

static const char *const gmii_rx_cs_in_grps[] = {
	"gmii_rx_cs_in_grp",
};

static const char *const debug_out11_grps[] = {
	"debug_out11_grp",
};

static const char *const sdio1_wp_in_grps[] = {
	"sdio1_wp_in_0_grp",
	"sdio1_wp_in_1_grp",
};

static const char *const parallel5_in_grps[] = {
	"parallel5_in_grp",
};

static const char *const mii_tx_clk_in_grps[] = {
	"mii_tx_clk_in_grp",
};

static const char *const debug_out12_grps[] = {
	"debug_out12_grp",
};

static const char *const parallel6_in_grps[] = {
	"parallel6_in_grp",
};

static const char *const gmii_rx_col_in_grps[] = {
	"gmii_rx_col_in_grp",
};

static const char *const debug_out13_grps[] = {
	"debug_out13_grp",
};

static const char *const parallel7_in_grps[] = {
	"parallel7_in_grp",
};

static const char *const debug_out14_grps[] = {
	"debug_out14_grp",
};

static const char *const parallel8_in_grps[] = {
	"parallel8_in_grp",
};

static const char *const debug_out15_grps[] = {
	"debug_out15_grp",
};

static const char *const parallel9_in_grps[] = {
	"parallel9_in_grp",
};

static const char *const parallel10_in_grps[] = {
	"parallel10_in_grp",
};

static const char *const parallel11_in_grps[] = {
	"parallel11_in_grp",
};

static const char *const sdio1_host_vdd1_stable_in_grps[] = {
	"sdio1_host_vdd1_stable_in_grp",
};

static const char *const parallel12_in_grps[] = {
	"parallel12_in_grp",
};

static const char *const parallel13_in_grps[] = {
	"parallel13_in_grp",
};

static const char *const debug_out16_grps[] = {
	"debug_out16_grp",
};

static const char *const parallel14_in_grps[] = {
	"parallel14_in_grp",
};

static const char *const debug_out17_grps[] = {
	"debug_out17_grp",
};

static const char *const parallel15_in_grps[] = {
	"parallel15_in_grp",
};

static const char *const debug_out18_grps[] = {
	"debug_out18_grp",
};

static const char *const parallel16_in_grps[] = {
	"parallel16_in_grp",
};

static const char *const debug_out19_grps[] = {
	"debug_out19_grp",
};

static const char *const parallel17_in_grps[] = {
	"parallel17_in_grp",
};

static const char *const debug_out20_grps[] = {
	"debug_out20_grp",
};

static const char *const parallel18_in_grps[] = {
	"parallel18_in_grp",
};

static const char *const debug_out21_grps[] = {
	"debug_out21_grp",
};

static const char *const parallel19_in_grps[] = {
	"parallel19_in_grp",
};

static const char *const debug_out30_grps[] = {
	"debug_out30_grp",
};

static const char *const parallel20_in_grps[] = {
	"parallel20_in_grp",
};

static const char *const debug_out31_grps[] = {
	"debug_out31_grp",
};

static const char *const parallel21_in_grps[] = {
	"parallel21_in_grp",
};

static const char *const spi3_cs0_n_in_grps[] = {
	"spi3_cs0_n_in_grp",
};

static const char *const parallel22_in_grps[] = {
	"parallel22_in_grp",
};

static const char *const spi3_mosi_in_grps[] = {
	"spi3_mosi_in_grp",
};

static const char *const parallel23_in_grps[] = {
	"parallel23_in_grp",
};

static const char *const spi3_miso_out_grps[] = {
	"spi3_miso_out_grp",
};

static const char *const parallel_hsync_in_grps[] = {
	"parallel_hsync_in_grp",
};

static const char *const spi3_sclk_in_grps[] = {
	"spi3_sclk_in_grp",
};

static const char *const parallel_vsync_in_grps[] = {
	"parallel_vsync_in_grp",
};

#define H15_PIN_FUNCTION(func)                                                 \
	{                                                                      \
		.name = #func, .groups = func##_grps,                          \
		.num_groups = ARRAY_SIZE(func##_grps),                         \
	}

/* NOTE: Keep 32 first functions as they are (GPIO).
         gpioN must be at index N. */
static const struct h15l_pin_function h15l_pin_functions[] = {
	H15_PIN_FUNCTION(gpio0),
	H15_PIN_FUNCTION(gpio1),
	H15_PIN_FUNCTION(gpio2),
	H15_PIN_FUNCTION(gpio3),
	H15_PIN_FUNCTION(gpio4),
	H15_PIN_FUNCTION(gpio5),
	H15_PIN_FUNCTION(gpio6),
	H15_PIN_FUNCTION(gpio7),
	H15_PIN_FUNCTION(gpio8),
	H15_PIN_FUNCTION(gpio9),
	H15_PIN_FUNCTION(gpio10),
	H15_PIN_FUNCTION(gpio11),
	H15_PIN_FUNCTION(gpio12),
	H15_PIN_FUNCTION(gpio13),
	H15_PIN_FUNCTION(gpio14),
	H15_PIN_FUNCTION(gpio15),
	H15_PIN_FUNCTION(gpio16),
	H15_PIN_FUNCTION(gpio17),
	H15_PIN_FUNCTION(gpio18),
	H15_PIN_FUNCTION(gpio19),
	H15_PIN_FUNCTION(gpio20),
	H15_PIN_FUNCTION(gpio21),
	H15_PIN_FUNCTION(gpio22),
	H15_PIN_FUNCTION(gpio23),
	H15_PIN_FUNCTION(gpio24),
	H15_PIN_FUNCTION(gpio25),
	H15_PIN_FUNCTION(gpio26),
	H15_PIN_FUNCTION(gpio27),
	H15_PIN_FUNCTION(gpio28),
	H15_PIN_FUNCTION(gpio29),
	H15_PIN_FUNCTION(gpio30),
	H15_PIN_FUNCTION(gpio31),

	H15_PIN_FUNCTION(boot_rom_failure_out),
	H15_PIN_FUNCTION(can0_rx_in),
	H15_PIN_FUNCTION(can0_stby_out),
	H15_PIN_FUNCTION(can0_tx_out),
	H15_PIN_FUNCTION(can1_rx_in),
	H15_PIN_FUNCTION(can1_stby_out),
	H15_PIN_FUNCTION(can1_tx_out),
	H15_PIN_FUNCTION(cpu_trace_clk_out),
	H15_PIN_FUNCTION(cpu_trace_data0_out),
	H15_PIN_FUNCTION(cpu_trace_data1_out),
	H15_PIN_FUNCTION(cpu_trace_data2_out),
	H15_PIN_FUNCTION(cpu_trace_data3_out),
	H15_PIN_FUNCTION(debug_out0),
	H15_PIN_FUNCTION(debug_out1),
	H15_PIN_FUNCTION(debug_out10),
	H15_PIN_FUNCTION(debug_out11),
	H15_PIN_FUNCTION(debug_out12),
	H15_PIN_FUNCTION(debug_out13),
	H15_PIN_FUNCTION(debug_out14),
	H15_PIN_FUNCTION(debug_out15),
	H15_PIN_FUNCTION(debug_out16),
	H15_PIN_FUNCTION(debug_out17),
	H15_PIN_FUNCTION(debug_out18),
	H15_PIN_FUNCTION(debug_out19),
	H15_PIN_FUNCTION(debug_out2),
	H15_PIN_FUNCTION(debug_out20),
	H15_PIN_FUNCTION(debug_out21),
	H15_PIN_FUNCTION(debug_out22),
	H15_PIN_FUNCTION(debug_out23),
	H15_PIN_FUNCTION(debug_out24),
	H15_PIN_FUNCTION(debug_out25),
	H15_PIN_FUNCTION(debug_out26),
	H15_PIN_FUNCTION(debug_out27),
	H15_PIN_FUNCTION(debug_out28),
	H15_PIN_FUNCTION(debug_out29),
	H15_PIN_FUNCTION(debug_out3),
	H15_PIN_FUNCTION(debug_out30),
	H15_PIN_FUNCTION(debug_out31),
	H15_PIN_FUNCTION(debug_out4),
	H15_PIN_FUNCTION(debug_out5),
	H15_PIN_FUNCTION(debug_out6),
	H15_PIN_FUNCTION(debug_out7),
	H15_PIN_FUNCTION(debug_out8),
	H15_PIN_FUNCTION(debug_out9),
	H15_PIN_FUNCTION(eth_gmii_tx_er_out),
	H15_PIN_FUNCTION(eth_mdc_out),
	H15_PIN_FUNCTION(eth_mdio),
	H15_PIN_FUNCTION(eth_rx_clk_in),
	H15_PIN_FUNCTION(eth_rx_ctl_crs_dv_in),
	H15_PIN_FUNCTION(eth_rxd_0_in),
	H15_PIN_FUNCTION(eth_rxd_1_in),
	H15_PIN_FUNCTION(eth_rxd_2_in),
	H15_PIN_FUNCTION(eth_rxd_3_er_in),
	H15_PIN_FUNCTION(eth_rxd_4_in),
	H15_PIN_FUNCTION(eth_rxd_5_in),
	H15_PIN_FUNCTION(eth_rxd_6_in),
	H15_PIN_FUNCTION(eth_rxd_7_in),
	H15_PIN_FUNCTION(eth_tx_clk_out),
	H15_PIN_FUNCTION(eth_tx_ctl_en_out),
	H15_PIN_FUNCTION(eth_txd_0_out),
	H15_PIN_FUNCTION(eth_txd_1_out),
	H15_PIN_FUNCTION(eth_txd_2_out),
	H15_PIN_FUNCTION(eth_txd_3_out),
	H15_PIN_FUNCTION(eth_txd_4_out),
	H15_PIN_FUNCTION(eth_txd_5_out),
	H15_PIN_FUNCTION(eth_txd_6_out),
	H15_PIN_FUNCTION(eth_txd_7_out),
	H15_PIN_FUNCTION(flash_ospi_dq_4),
	H15_PIN_FUNCTION(flash_ospi_dq_5),
	H15_PIN_FUNCTION(flash_ospi_dq_6),
	H15_PIN_FUNCTION(flash_ospi_dq_7),
	H15_PIN_FUNCTION(flash_spi_cs_0_n_out),
	H15_PIN_FUNCTION(flash_spi_cs_1_n_out),
	H15_PIN_FUNCTION(flash_spi_cs2n_clkc_rebar_out),
	H15_PIN_FUNCTION(flash_spi_dq_0),
	H15_PIN_FUNCTION(flash_spi_dq_1),
	H15_PIN_FUNCTION(flash_spi_dq_2),
	H15_PIN_FUNCTION(flash_spi_dq_3),
	H15_PIN_FUNCTION(flash_spi_ds_cs3n_lpbk),
	H15_PIN_FUNCTION(flash_spi_reset_n_out),
	H15_PIN_FUNCTION(flash_spi_sclk_out),
	H15_PIN_FUNCTION(gmii_rx_col_in),
	H15_PIN_FUNCTION(gmii_rx_cs_in),
	H15_PIN_FUNCTION(gmii_rx_er_in),
	H15_PIN_FUNCTION(i2c0_current_src_en_out),
	H15_PIN_FUNCTION(i2c1_current_src_en_out),
	H15_PIN_FUNCTION(i2c2_current_src_en_out),
	H15_PIN_FUNCTION(i2c2_scl),
	H15_PIN_FUNCTION(i2c2_sda),
	H15_PIN_FUNCTION(i2c3_current_src_en_out),
	H15_PIN_FUNCTION(i2c3_scl),
	H15_PIN_FUNCTION(i2c3_sda),
	H15_PIN_FUNCTION(i2s0_mclk),
	H15_PIN_FUNCTION(i2s0_sck),
	H15_PIN_FUNCTION(i2s0_sdi0_in),
	H15_PIN_FUNCTION(i2s0_sdi1_in),
	H15_PIN_FUNCTION(i2s0_sdo0_out),
	H15_PIN_FUNCTION(i2s0_sdo1_out),
	H15_PIN_FUNCTION(i2s0_ws),
	H15_PIN_FUNCTION(i2s1_mclk),
	H15_PIN_FUNCTION(i2s1_sck),
	H15_PIN_FUNCTION(i2s1_sdi0_in),
	H15_PIN_FUNCTION(i2s1_sdo0_out),
	H15_PIN_FUNCTION(i2s1_ws),
	H15_PIN_FUNCTION(isp_flash_trig_out),
	H15_PIN_FUNCTION(isp_pre_flash_out),
	H15_PIN_FUNCTION(jtag_tck_in),
	H15_PIN_FUNCTION(jtag_tdi_in),
	H15_PIN_FUNCTION(jtag_tdo_out),
	H15_PIN_FUNCTION(jtag_tms),
	H15_PIN_FUNCTION(jtag_trstn_in),
	H15_PIN_FUNCTION(mii_tx_clk_in),
	H15_PIN_FUNCTION(parallel_hsync_in),
	H15_PIN_FUNCTION(parallel_pclk_in),
	H15_PIN_FUNCTION(parallel_vsync_in),
	H15_PIN_FUNCTION(parallel0_in),
	H15_PIN_FUNCTION(parallel1_in),
	H15_PIN_FUNCTION(parallel10_in),
	H15_PIN_FUNCTION(parallel11_in),
	H15_PIN_FUNCTION(parallel12_in),
	H15_PIN_FUNCTION(parallel13_in),
	H15_PIN_FUNCTION(parallel14_in),
	H15_PIN_FUNCTION(parallel15_in),
	H15_PIN_FUNCTION(parallel16_in),
	H15_PIN_FUNCTION(parallel17_in),
	H15_PIN_FUNCTION(parallel18_in),
	H15_PIN_FUNCTION(parallel19_in),
	H15_PIN_FUNCTION(parallel2_in),
	H15_PIN_FUNCTION(parallel20_in),
	H15_PIN_FUNCTION(parallel21_in),
	H15_PIN_FUNCTION(parallel22_in),
	H15_PIN_FUNCTION(parallel23_in),
	H15_PIN_FUNCTION(parallel3_in),
	H15_PIN_FUNCTION(parallel4_in),
	H15_PIN_FUNCTION(parallel5_in),
	H15_PIN_FUNCTION(parallel6_in),
	H15_PIN_FUNCTION(parallel7_in),
	H15_PIN_FUNCTION(parallel8_in),
	H15_PIN_FUNCTION(parallel9_in),
	H15_PIN_FUNCTION(pcie_clkreq_n),
	H15_PIN_FUNCTION(pcie_mperst_n_out),
	H15_PIN_FUNCTION(pcie_perst_n_in),
	H15_PIN_FUNCTION(pcie_wake_n),
	H15_PIN_FUNCTION(pwm0_out),
	H15_PIN_FUNCTION(pwm1_out),
	H15_PIN_FUNCTION(pwm2_out),
	H15_PIN_FUNCTION(pwm3_out),
	H15_PIN_FUNCTION(pwm4_out),
	H15_PIN_FUNCTION(pwm5_out),
	H15_PIN_FUNCTION(safety_error_n_out),
	H15_PIN_FUNCTION(safety_out_0),
	H15_PIN_FUNCTION(safety_out_1),
	H15_PIN_FUNCTION(sdio0_CD_in),
	H15_PIN_FUNCTION(SDIO0_CMD_SDIO1_RSTN),
	H15_PIN_FUNCTION(SDIO0_DATA0_SDIO1_DATA4),
	H15_PIN_FUNCTION(SDIO0_DATA1_SDIO1_DATA5),
	H15_PIN_FUNCTION(SDIO0_DATA2_SDIO1_DATA6),
	H15_PIN_FUNCTION(SDIO0_DATA3_SDIO1_DATA7),
	H15_PIN_FUNCTION(sdio0_gp_in),
	H15_PIN_FUNCTION(sdio0_gp_out),
	H15_PIN_FUNCTION(sdio0_host_vdd1_stable_in),
	H15_PIN_FUNCTION(SDIO0_SDCLK_SDIO1_DS),
	H15_PIN_FUNCTION(sdio0_uhs_sel_out),
	H15_PIN_FUNCTION(sdio0_vdd1_on_out),
	H15_PIN_FUNCTION(sdio0_wp_in),
	H15_PIN_FUNCTION(sdio1_CD_in),
	H15_PIN_FUNCTION(SDIO1_DATA0),
	H15_PIN_FUNCTION(SDIO1_DATA1),
	H15_PIN_FUNCTION(SDIO1_DATA2),
	H15_PIN_FUNCTION(SDIO1_DATA3),
	H15_PIN_FUNCTION(sdio1_gp_in),
	H15_PIN_FUNCTION(sdio1_gp_out),
	H15_PIN_FUNCTION(sdio1_host_vdd1_stable_in),
	H15_PIN_FUNCTION(SDIO1_SDCLK),
	H15_PIN_FUNCTION(sdio1_uhs_sel_out),
	H15_PIN_FUNCTION(sdio1_vdd1_on_out),
	H15_PIN_FUNCTION(sdio1_wp_in),
	H15_PIN_FUNCTION(spi0_cs0_n_out),
	H15_PIN_FUNCTION(spi0_cs1_n_out),
	H15_PIN_FUNCTION(spi0_cs2_n_out),
	H15_PIN_FUNCTION(spi0_cs3_n_out),
	H15_PIN_FUNCTION(spi0_miso_in),
	H15_PIN_FUNCTION(spi0_mosi_out),
	H15_PIN_FUNCTION(spi0_sclk_out),
	H15_PIN_FUNCTION(spi1_cs0_n_out),
	H15_PIN_FUNCTION(spi1_cs1_n_out),
	H15_PIN_FUNCTION(spi1_miso_in),
	H15_PIN_FUNCTION(spi1_mosi_out),
	H15_PIN_FUNCTION(spi1_sclk_out),
	H15_PIN_FUNCTION(spi2_cs0_n_out),
	H15_PIN_FUNCTION(spi2_cs1_n_out),
	H15_PIN_FUNCTION(spi2_miso_in),
	H15_PIN_FUNCTION(spi2_mosi_out),
	H15_PIN_FUNCTION(spi2_sclk_out),
	H15_PIN_FUNCTION(spi3_cs0_n_in),
	H15_PIN_FUNCTION(spi3_miso_out),
	H15_PIN_FUNCTION(spi3_mosi_in),
	H15_PIN_FUNCTION(spi3_sclk_in),
	H15_PIN_FUNCTION(timer_ext_0_in),
	H15_PIN_FUNCTION(timer_ext_1_in),
	H15_PIN_FUNCTION(timer_ext_2_in),
	H15_PIN_FUNCTION(timer_ext_3_in),
	H15_PIN_FUNCTION(uart0_cts_in),
	H15_PIN_FUNCTION(uart0_rts_out),
	H15_PIN_FUNCTION(uart0_rxd_in),
	H15_PIN_FUNCTION(uart0_txd_out),
	H15_PIN_FUNCTION(uart1_cts_in),
	H15_PIN_FUNCTION(uart1_rts_out),
	H15_PIN_FUNCTION(uart1_rxd_in),
	H15_PIN_FUNCTION(uart1_txd_out),
	H15_PIN_FUNCTION(uart2_cts_in),
	H15_PIN_FUNCTION(uart2_rts_out),
	H15_PIN_FUNCTION(uart2_rxd_in),
	H15_PIN_FUNCTION(uart2_txd_out),
	H15_PIN_FUNCTION(uart3_cts_in),
	H15_PIN_FUNCTION(uart3_rts_out),
	H15_PIN_FUNCTION(uart3_rxd_in),
	H15_PIN_FUNCTION(uart3_txd_out),
	H15_PIN_FUNCTION(usb_drive_vbus_out),
	H15_PIN_FUNCTION(usb_overcurrent_n_in),
	H15_PIN_FUNCTION(i2c0_scl),
	H15_PIN_FUNCTION(i2c0_sda),
	H15_PIN_FUNCTION(i2c1_scl),
	H15_PIN_FUNCTION(i2c1_sda),
};

#define H15L_PINMUX_INVALID_FUNCTION_SELECTOR (ARRAY_SIZE(h15l_pin_functions))

#define H15L_PIN_DESC_STATIC_DRV_DATA(_pin_index, _muxable, _mux_index,       \
                                      _pad_type, _pad_index)                  \
	[_pin_index] = {  .mux_index = (_mux_index),                          \
			  .is_muxable = (_muxable),                           \
			  .pad_type = _pad_type,                              \
			  .pad_index = _pad_index,                            \
			  .func_selector =                                    \
				H15L_PINMUX_INVALID_FUNCTION_SELECTOR,        \
			  .is_occupied = false,                               \
			}

static struct h15l_pin_data hailo15l_pins_drv_data[] = {
	/* H15L_PIN_DESC_STATIC_DRV_DATA(pin_index, is_muxable, pad_type, index_in_pad) */

	/* I2C */
	H15L_PIN_DESC_STATIC_DRV_DATA(0, false, 0, H15L_SLOW_PAD, 5),
	H15L_PIN_DESC_STATIC_DRV_DATA(1, false, 1, H15L_SLOW_PAD, 4),
	H15L_PIN_DESC_STATIC_DRV_DATA(2, false, 2, H15L_SLOW_PAD, 7),
	H15L_PIN_DESC_STATIC_DRV_DATA(3, false, 3, H15L_SLOW_PAD, 6),

	/* UART */
	H15L_PIN_DESC_STATIC_DRV_DATA(4, true, 4, H15L_SLOW_PAD, 1),
	H15L_PIN_DESC_STATIC_DRV_DATA(5, true, 5, H15L_SLOW_PAD, 0),
	H15L_PIN_DESC_STATIC_DRV_DATA(6, true, 6, H15L_SLOW_PAD, 3),
	H15L_PIN_DESC_STATIC_DRV_DATA(7, true, 7, H15L_SLOW_PAD, 2),

	/* Ethernet */
	H15L_PIN_DESC_STATIC_DRV_DATA(8, true, 8, H15L_GENERAL_PAD, 12),
	H15L_PIN_DESC_STATIC_DRV_DATA(9, true, 9, H15L_GENERAL_PAD, 13),
	H15L_PIN_DESC_STATIC_DRV_DATA(10, true, 10, H15L_GENERAL_PAD, 10),
	H15L_PIN_DESC_STATIC_DRV_DATA(11, true, 11, H15L_GENERAL_PAD, 11),
	H15L_PIN_DESC_STATIC_DRV_DATA(12, true, 12, H15L_GENERAL_PAD, 6),
	H15L_PIN_DESC_STATIC_DRV_DATA(13, true, 13, H15L_GENERAL_PAD, 7),
	H15L_PIN_DESC_STATIC_DRV_DATA(14, true, 14, H15L_GENERAL_PAD, 8),
	H15L_PIN_DESC_STATIC_DRV_DATA(15, true, 15, H15L_GENERAL_PAD, 9),
	H15L_PIN_DESC_STATIC_DRV_DATA(16, true, 16, H15L_GENERAL_PAD, 0),
	H15L_PIN_DESC_STATIC_DRV_DATA(17, true, 17, H15L_GENERAL_PAD, 1),
	H15L_PIN_DESC_STATIC_DRV_DATA(18, true, 18, H15L_GENERAL_PAD, 2),
	H15L_PIN_DESC_STATIC_DRV_DATA(19, true, 19, H15L_GENERAL_PAD, 3),
	H15L_PIN_DESC_STATIC_DRV_DATA(20, true, 20, H15L_GENERAL_PAD, 4),
	H15L_PIN_DESC_STATIC_DRV_DATA(21, true, 21, H15L_GENERAL_PAD, 5),

	/* I2S */
	H15L_PIN_DESC_STATIC_DRV_DATA(22, true, 22, H15L_SLOW_PAD, 13),
	H15L_PIN_DESC_STATIC_DRV_DATA(23, true, 23, H15L_SLOW_PAD, 16),
	H15L_PIN_DESC_STATIC_DRV_DATA(24, true, 24, H15L_SLOW_PAD, 15),
	H15L_PIN_DESC_STATIC_DRV_DATA(25, true, 25, H15L_SLOW_PAD, 14),

	/* Flash */
	H15L_PIN_DESC_STATIC_DRV_DATA(26, true, 26, H15L_GENERAL_PAD, 15),
	H15L_PIN_DESC_STATIC_DRV_DATA(27, true, 27, H15L_GENERAL_PAD, 16),
	H15L_PIN_DESC_STATIC_DRV_DATA(28, true, 28, H15L_GENERAL_PAD, 17),
	H15L_PIN_DESC_STATIC_DRV_DATA(29, true, 29, H15L_GENERAL_PAD, 18),
	H15L_PIN_DESC_STATIC_DRV_DATA(30, true, 30, H15L_GENERAL_PAD, 20),
	H15L_PIN_DESC_STATIC_DRV_DATA(31, true, 31, H15L_GENERAL_PAD, 21),
	H15L_PIN_DESC_STATIC_DRV_DATA(32, true, 32, H15L_GENERAL_PAD, 22),
	H15L_PIN_DESC_STATIC_DRV_DATA(33, true, 33, H15L_GENERAL_PAD, 23),
	H15L_PIN_DESC_STATIC_DRV_DATA(34, true, 34, H15L_GENERAL_PAD, 19),
	H15L_PIN_DESC_STATIC_DRV_DATA(35, true, 35, H15L_GENERAL_PAD, 14),

	/* PCIe */
	H15L_PIN_DESC_STATIC_DRV_DATA(36, true, 56, H15L_GENERAL_PAD, 25),
	H15L_PIN_DESC_STATIC_DRV_DATA(37, true, 57, H15L_GENERAL_PAD, 24),
	H15L_PIN_DESC_STATIC_DRV_DATA(38, true, 58, H15L_GENERAL_PAD, 26),
	H15L_PIN_DESC_STATIC_DRV_DATA(39, true, 59, H15L_GENERAL_PAD, 27),

	/* SDIO */
	H15L_PIN_DESC_STATIC_DRV_DATA(40, true, 36, H15L_GENERAL_PAD, 28),
	H15L_PIN_DESC_STATIC_DRV_DATA(41, true, 37, H15L_GENERAL_PAD, 29),
	H15L_PIN_DESC_STATIC_DRV_DATA(42, true, 38, H15L_GENERAL_PAD, 30),
	H15L_PIN_DESC_STATIC_DRV_DATA(43, true, 39, H15L_GENERAL_PAD, 31),
	H15L_PIN_DESC_STATIC_DRV_DATA(44, true, 40, H15L_GENERAL_PAD, 32),
	H15L_PIN_DESC_STATIC_DRV_DATA(45, true, 41, H15L_GENERAL_PAD, 33),
	H15L_PIN_DESC_STATIC_DRV_DATA(46, true, 42, H15L_GENERAL_PAD, 34),
	H15L_PIN_DESC_STATIC_DRV_DATA(47, true, 43, H15L_GENERAL_PAD, 35),
	H15L_PIN_DESC_STATIC_DRV_DATA(48, true, 44, H15L_GENERAL_PAD, 36),
	H15L_PIN_DESC_STATIC_DRV_DATA(49, true, 45, H15L_GENERAL_PAD, 37),
	H15L_PIN_DESC_STATIC_DRV_DATA(50, false, 46, H15L_GENERAL_PAD, 38),
	H15L_PIN_DESC_STATIC_DRV_DATA(51, true, 47, H15L_GENERAL_PAD, 39),

	/* Parallel */
	H15L_PIN_DESC_STATIC_DRV_DATA(52, true, 48,  H15L_SLOW_PAD, 19),

	/* USB */
	H15L_PIN_DESC_STATIC_DRV_DATA(53, true, 49, H15L_SLOW_PAD, 17),

	/* Safety */
	H15L_PIN_DESC_STATIC_DRV_DATA(54, false, 50, H15L_GENERAL_PAD, 18),

	/* JTAG */
	H15L_PIN_DESC_STATIC_DRV_DATA(55, true, 51, H15L_SLOW_PAD, 11),
	H15L_PIN_DESC_STATIC_DRV_DATA(56, true, 52, H15L_SLOW_PAD, 8),
	H15L_PIN_DESC_STATIC_DRV_DATA(57, true, 53, H15L_SLOW_PAD, 9),
	H15L_PIN_DESC_STATIC_DRV_DATA(58, true, 54, H15L_SLOW_PAD, 10),
	H15L_PIN_DESC_STATIC_DRV_DATA(59, true, 55, H15L_SLOW_PAD, 12),

	/* GPIO */
	H15L_PIN_DESC_STATIC_DRV_DATA(60, true, 60, H15L_GPIO_PAD, 0),
	H15L_PIN_DESC_STATIC_DRV_DATA(61, true, 61, H15L_GPIO_PAD, 1),
	H15L_PIN_DESC_STATIC_DRV_DATA(62, true, 62, H15L_GPIO_PAD, 2),
	H15L_PIN_DESC_STATIC_DRV_DATA(63, true, 63, H15L_GPIO_PAD, 3),
	H15L_PIN_DESC_STATIC_DRV_DATA(64, true, 64, H15L_GPIO_PAD, 4),
	H15L_PIN_DESC_STATIC_DRV_DATA(65, true, 65, H15L_GPIO_PAD, 5),
	H15L_PIN_DESC_STATIC_DRV_DATA(66, true, 66, H15L_GPIO_PAD, 6),
	H15L_PIN_DESC_STATIC_DRV_DATA(67, true, 67, H15L_GPIO_PAD, 7),
	H15L_PIN_DESC_STATIC_DRV_DATA(68, true, 68, H15L_GPIO_PAD, 8),
	H15L_PIN_DESC_STATIC_DRV_DATA(69, true, 69, H15L_GPIO_PAD, 9),
	H15L_PIN_DESC_STATIC_DRV_DATA(70, true, 70, H15L_GPIO_PAD, 10),
	H15L_PIN_DESC_STATIC_DRV_DATA(71, true, 71, H15L_GPIO_PAD, 11),
	H15L_PIN_DESC_STATIC_DRV_DATA(72, true, 72, H15L_GPIO_PAD, 12),
	H15L_PIN_DESC_STATIC_DRV_DATA(73, true, 73, H15L_GPIO_PAD, 13),
	H15L_PIN_DESC_STATIC_DRV_DATA(74, true, 74, H15L_GPIO_PAD, 14),
	H15L_PIN_DESC_STATIC_DRV_DATA(75, true, 75, H15L_GPIO_PAD, 15),
	H15L_PIN_DESC_STATIC_DRV_DATA(76, true, 76, H15L_GPIO_PAD, 16),
	H15L_PIN_DESC_STATIC_DRV_DATA(77, true, 77, H15L_GPIO_PAD, 17),
	H15L_PIN_DESC_STATIC_DRV_DATA(78, true, 78, H15L_GPIO_PAD, 18),
	H15L_PIN_DESC_STATIC_DRV_DATA(79, true, 79, H15L_GPIO_PAD, 19),
	H15L_PIN_DESC_STATIC_DRV_DATA(80, true, 80, H15L_GPIO_PAD, 20),
	H15L_PIN_DESC_STATIC_DRV_DATA(81, true, 81, H15L_GPIO_PAD, 21),
	H15L_PIN_DESC_STATIC_DRV_DATA(82, true, 82, H15L_GPIO_PAD, 22),
	H15L_PIN_DESC_STATIC_DRV_DATA(83, true, 83, H15L_GPIO_PAD, 23),
	H15L_PIN_DESC_STATIC_DRV_DATA(84, true, 84, H15L_GPIO_PAD, 24),
	H15L_PIN_DESC_STATIC_DRV_DATA(85, true, 85, H15L_GPIO_PAD, 25),
	H15L_PIN_DESC_STATIC_DRV_DATA(86, true, 86, H15L_GPIO_PAD, 26),
	H15L_PIN_DESC_STATIC_DRV_DATA(87, true, 87, H15L_GPIO_PAD, 27),
	H15L_PIN_DESC_STATIC_DRV_DATA(88, true, 88, H15L_GPIO_PAD, 28),
	H15L_PIN_DESC_STATIC_DRV_DATA(89, true, 89, H15L_GPIO_PAD, 29),
	H15L_PIN_DESC_STATIC_DRV_DATA(90, true, 90, H15L_GPIO_PAD, 30),
	H15L_PIN_DESC_STATIC_DRV_DATA(91, true, 91, H15L_GPIO_PAD, 31),

	/* Reset */
	H15L_PIN_DESC_STATIC_DRV_DATA(92, false, 0xFF, H15L_NO_PAD, 0),

	/* OSC */
	H15L_PIN_DESC_STATIC_DRV_DATA(93, false, 0xFF, H15L_NO_PAD, 0),
	H15L_PIN_DESC_STATIC_DRV_DATA(94, false, 0xFF, H15L_NO_PAD, 0),
};

#define H15L_PIN(_pin_index, _name) \
	[(_pin_index)] = { .number = (_pin_index), \
			   .name = (_name), \
			   .drv_data = &hailo15l_pins_drv_data[(_pin_index)] }

static const struct pinctrl_pin_desc hailo15l_pins[] = {
	H15L_PIN(0, "I2C0_SCL"),
	H15L_PIN(1, "I2C0_SDA"),
	H15L_PIN(2, "I2C1_SCL"),
	H15L_PIN(3, "I2C1_SDA"),
	H15L_PIN(4, "UART0_RXD"),
	H15L_PIN(5, "UART0_TXD"),
	H15L_PIN(6, "UART1_RXD"),
	H15L_PIN(7, "UART1_TXD"),
	H15L_PIN(8, "ETH_MDC"),
	H15L_PIN(9, "ETH_MDIO"),
	H15L_PIN(10, "ETH_RX_CLK"),
	H15L_PIN(11, "ETH_RX_CTL_DV"),
	H15L_PIN(12, "ETH_RXD0"),
	H15L_PIN(13, "ETH_RXD1"),
	H15L_PIN(14, "ETH_RXD2"),
	H15L_PIN(15, "ETH_RXD3"),
	H15L_PIN(16, "ETH_TX_CLK"),
	H15L_PIN(17, "ETH_TX_CTL_EN"),
	H15L_PIN(18, "ETH_TXD0"),
	H15L_PIN(19, "ETH_TXD1"),
	H15L_PIN(20, "ETH_TXD2"),
	H15L_PIN(21, "ETH_TXD3"),
	H15L_PIN(22, "I2S0_SCK"),
	H15L_PIN(23, "I2S0_SDI"),
	H15L_PIN(24, "I2S0_SDO"),
	H15L_PIN(25, "I2S0_WS"),
	H15L_PIN(26, "FLASH_CS0_N"),
	H15L_PIN(27, "FLASH_CS1_N"),
	H15L_PIN(28, "FLASH_CS2_N"),
	H15L_PIN(29, "FLASH_DS"),
	H15L_PIN(30, "FLASH_DQ0_MOSI"),
	H15L_PIN(31, "FLASH_DQ1_MISO"),
	H15L_PIN(32, "FLASH_DQ2"),
	H15L_PIN(33, "FLASH_DQ3"),
	H15L_PIN(34, "FLASH_RESET_N"),
	H15L_PIN(35, "FLASH_SCLK"),
	H15L_PIN(36, "PCIE_WAKE_N"),
	H15L_PIN(37, "PCIE_CLKREQ_N"),
	H15L_PIN(38, "PCIE_PERST_N"),
	H15L_PIN(39, "PCIE_MPERST_N"),
	H15L_PIN(40, "SDIO0_DATA0_SDIO1_DATA4"),
	H15L_PIN(41, "SDIO0_DATA1_SDIO1_DATA5"),
	H15L_PIN(42, "SDIO0_DATA2_SDIO1_DATA6"),
	H15L_PIN(43, "SDIO0_DATA3_SDIO1_DATA7"),
	H15L_PIN(44, "SDIO0_SDCLK_SDIO1_DS"),
	H15L_PIN(45, "SDIO0_CMD_SDIO1_RSTN"),
	H15L_PIN(46, "SDIO1_DATA0"),
	H15L_PIN(47, "SDIO1_DATA1"),
	H15L_PIN(48, "SDIO1_DATA2"),
	H15L_PIN(49, "SDIO1_DATA3"),
	H15L_PIN(50, "SDIO1_CMD"),
	H15L_PIN(51, "SDIO1_SDCLK"),
	H15L_PIN(52, "PARALLEL_PCLK"),
	H15L_PIN(53, "USB_DRIVE_VBUS"),
	H15L_PIN(54, "SAFETY_FATAL_N"),
	H15L_PIN(55, "JTAG_TCK"),
	H15L_PIN(56, "JTAG_TDI"),
	H15L_PIN(57, "JTAG_TDO"),
	H15L_PIN(58, "JTAG_TMS"),
	H15L_PIN(59, "JTAG_TRSTN"),
	H15L_PIN(60, "GPIO_0"),
	H15L_PIN(61, "GPIO_1"),
	H15L_PIN(62, "GPIO_2"),
	H15L_PIN(63, "GPIO_3"),
	H15L_PIN(64, "GPIO_4"),
	H15L_PIN(65, "GPIO_5"),
	H15L_PIN(66, "GPIO_6"),
	H15L_PIN(67, "GPIO_7"),
	H15L_PIN(68, "GPIO_8"),
	H15L_PIN(69, "GPIO_9"),
	H15L_PIN(70, "GPIO_10"),
	H15L_PIN(71, "GPIO_11"),
	H15L_PIN(72, "GPIO_12"),
	H15L_PIN(73, "GPIO_13"),
	H15L_PIN(74, "GPIO_14"),
	H15L_PIN(75, "GPIO_15"),
	H15L_PIN(76, "GPIO_16"),
	H15L_PIN(77, "GPIO_17"),
	H15L_PIN(78, "GPIO_18"),
	H15L_PIN(79, "GPIO_19"),
	H15L_PIN(80, "GPIO_20"),
	H15L_PIN(81, "GPIO_21"),
	H15L_PIN(82, "GPIO_22"),
	H15L_PIN(83, "GPIO_23"),
	H15L_PIN(84, "GPIO_24"),
	H15L_PIN(85, "GPIO_25"),
	H15L_PIN(86, "GPIO_26"),
	H15L_PIN(87, "GPIO_27"),
	H15L_PIN(88, "GPIO_28"),
	H15L_PIN(89, "GPIO_29"),
	H15L_PIN(90, "GPIO_30"),
	H15L_PIN(91, "GPIO_31"),
	H15L_PIN(92, "RESET_N"),
	H15L_PIN(93, "OSC_XOUT"),
	H15L_PIN(94, "OSC_XIN"),
};

#endif /* _PINCTRL_HAILO15_PROPERTIES_H */