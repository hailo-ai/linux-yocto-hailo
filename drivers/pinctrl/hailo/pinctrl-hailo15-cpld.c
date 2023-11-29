/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * Driver for the Hailo15 CPLD-pinctrl device.
 * The CPLD is only applicable for HAILO15-evb.
 *
 */
 
#include "pinctrl-hailo15-cpld.h"


/* -----------------------------------------------------------------------------
 * Generic Overlay Handling
 */

struct hailo15_cpld_of_overlay {
	void *begin;
	void *end;
};

#define HAILO15_CPLD_OF_DTB(type, property)     \
	extern char __dtb_hailo15_cpld_of_##type##_##property##_begin[];	\
	extern char __dtb_hailo15_cpld_of_##type##_##property##_end[];	\


#define HAILO15_CPLD_OF_OVERLAY(type, property)					\
	{								\
		.begin = __dtb_hailo15_cpld_of_##type##_##property##_begin,	\
		.end = __dtb_hailo15_cpld_of_##type##_##property##_end,		\
	}

/* -----------------------------------------------------------------------------
 * EVBS Overlays
 */

HAILO15_CPLD_OF_DTB(evb, rev1);
HAILO15_CPLD_OF_DTB(evb, rev2);
HAILO15_CPLD_OF_DTB(evb, sdio8bit);

enum overlay_type_id {
	EVB_REV1 = 0, 
	EVB_REV2,
	EVB_SDIO_8BIT
};

static const struct hailo15_cpld_of_overlay hailo15_cpld_of_overlays[] __initconst = {
       HAILO15_CPLD_OF_OVERLAY(evb, rev1),
       HAILO15_CPLD_OF_OVERLAY(evb, rev2),
       HAILO15_CPLD_OF_OVERLAY(evb, sdio8bit),
       { /* Sentinel */ },
};

#define HAILO15_CPLD_LOG_FORMAT(file, format, ...) do { \
	if (file) { \
		seq_printf(file, format, ##__VA_ARGS__); \
	} else { \
		pr_info(format, ##__VA_ARGS__); \
	} \
} while (false);

static const char *hailo15_cpld_board_revisions[] = {
	"EVB rev 1",
	"EVB rev 2",
	"SVB rev 1",
	"UNKNOWN (3)",
	"UNKNOWN (4)",
	"UNKNOWN (5)",
	"UNKNOWN (6)",
	"UNKNOWN (7)"
};

static const char *hailo15_cpld_sdio_route[] = {
	"eMMC",
	"E key",
	"SD card",
	"N/A"
};

static const char *hailo15_cpld_switch_state[] = {
	"OFF",
	"ON"
};

struct hailo15_cpld_registers {
	struct {
		uint8_t board;
		uint8_t minor;
	} version;

	struct {
		bool power_enable;
		bool power_good;
	} power_status;

	struct {
		bool h15;
		bool ftdi;
		bool cam_0;
		bool cam_1;
		bool dsi_to_hdmi;
		bool eth_phy;
		bool usb_hub;
		bool audio_codec;
		bool flash;
		bool emmc;
	} reset_status;

	struct {
		bool power_on_h15_on_power_insert;
	} bootstrap_status;

	struct {
		uint8_t sdio_0;
		uint8_t sdio_1;
	} sdio_route_status;
};

static bool hailo15_cpld_is_sdio1_8_bit(struct hailo15_cpld_registers *registers){
	uint8_t sdio_0_route_status = (registers->sdio_route_status.sdio_0 & H15_CPLD__SDIO_ROUTE_MASK);
	uint8_t sdio_1_route_status = (registers->sdio_route_status.sdio_1 & H15_CPLD__SDIO_ROUTE_MASK);
	
	if ((strcmp(hailo15_cpld_sdio_route[sdio_0_route_status],"eMMC")== 0) &&
		strcmp(hailo15_cpld_sdio_route[sdio_1_route_status],"eMMC")== 0) {
		
		return true;
	}
	 
	return false;
}

static int __init hailo15_cpld_of_apply_overlay(const struct hailo15_cpld_of_overlay *dtbs, struct hailo15_cpld_registers *registers)
{
	const struct hailo15_cpld_of_overlay *dtb = NULL;
	int ovcs_id;
	int board_version = registers->version.board;
	
	if (board_version == 0) {
		dtb = &dtbs[EVB_REV1];
	} else {
		/* Getting Swich state is Not supported in EVB REV 1 */
		if (hailo15_cpld_is_sdio1_8_bit(registers)) {
			dtb = &dtbs[EVB_SDIO_8BIT];
		} else {
			dtb = &dtbs[EVB_REV2];
		}
	}
	ovcs_id = 0;
	return of_overlay_fdt_apply(dtb->begin, dtb->end - dtb->begin,
				    &ovcs_id);
}

int pinctrl_hailo15_cpld_i2c_write(struct i2c_client *client, uint8_t addr,
				   uint8_t write_value)
{
	uint8_t buf[] = { addr, write_value };
	int return_value;

	return_value = i2c_master_send(client, buf, sizeof(buf));
	if (return_value < 0) {
		pr_err("Error %d writing to i2c address 0x%x\n", return_value,
		       addr);
	}

	return return_value;
}

int pinctrl_hailo15_cpld_i2c_read(struct i2c_client *client, uint8_t addr,
				  uint8_t *read_value)
{
	int return_value;

	return_value = i2c_master_send(client, &addr, sizeof(addr));
	if (return_value < 0)
		goto fail;

	return_value = i2c_master_recv(client, read_value, sizeof(*read_value));
	if (return_value < 0)
		goto fail;

	return return_value;

fail:
	pr_err("Error %d reading from i2c address 0x%x\n", return_value, addr);
	return return_value;
}

int hailo15_cpld_set_board_config(struct hailo15_pinctrl *pinctrl)
{
	int ret;
	ret = pinctrl_hailo15_cpld_i2c_write(pinctrl->pinctrl_cpld_i2c_client,
					     H15_CPLD__BOARD_CONFIG,
					     pinctrl->board_config);
	return ret;
}

static int hailo15_cpld_get_register(struct hailo15_pinctrl *pinctrl, uint8_t *reg, unsigned int address, char *register_name)
{
	int ret = 0;

	ret = pinctrl_hailo15_cpld_i2c_read(pinctrl->pinctrl_cpld_i2c_client, address, reg);
	if (ret < 0) {
		dev_err(pinctrl->dev, "unable to read %s from cpld\n", register_name);
		return ret;
	}

	return ret;
}

int hailo15_cpld_get_version(struct hailo15_pinctrl *pinctrl, struct hailo15_cpld_registers *registers)
{
	uint8_t version = 0;
	int ret = 0;

	ret = hailo15_cpld_get_register(pinctrl, &version, H15_CPLD__VERSION, "version");
	if (ret < 0) {
		return ret;
	}

	registers->version.minor = (version >> H15_CPLD__MINOR_VERSION_BIT_OFFSET) & H15_CPLD__MINOR_VERSION_MASK;
	registers->version.board = (version >> H15_CPLD__BOARD_VERSION_BIT_OFFSET) & H15_CPLD__BOARD_VERSION_MASK;

	return ret;
}

int hailo15_cpld_get_bootstrap_status(struct hailo15_pinctrl *pinctrl, struct hailo15_cpld_registers *registers)
{
	uint8_t bootstrap_status = 0;
	int ret = 0;

	ret = hailo15_cpld_get_register(pinctrl, &bootstrap_status, H15_CPLD__BOOTSTRAP_STATUS, "boostrap status");
	if (ret < 0) {
		return ret;
	}

	registers->bootstrap_status.power_on_h15_on_power_insert = (bootstrap_status >> H15_CPLD__POWER_INSER_BIT_OFFSET
		) & H15_CPLD__POWER_INSER_BIT_MASK;

	return ret;
}

int hailo15_cpld_get_power_status(struct hailo15_pinctrl *pinctrl, struct hailo15_cpld_registers *registers)
{
	uint8_t power_status = 0;
	int ret = 0;

	ret = hailo15_cpld_get_register(pinctrl, &power_status, H15_CPLD__POWER_STATUS_0, "power status");
	if (ret < 0) {
		return ret;
	}

	registers->power_status.power_enable = (power_status >> H15_CPLD__POWER_ENABLE_BIT_OFFSET) & H15_CPLD__POWER_ENABLE_MASK;
	registers->power_status.power_good = (power_status >> H15_CPLD__POWER_GOOD_BIT_OFFSET) & H15_CPLD__POWER_GOOD_MASK;

	return ret;
}

int hailo15_cpld_get_reset_status(struct hailo15_pinctrl *pinctrl, struct hailo15_cpld_registers *registers, uint8_t board_version)
{
	uint8_t reset_status_0 = 0;
	uint8_t reset_status_1 = 0;
	int ret = 0;

	ret = hailo15_cpld_get_register(pinctrl, &reset_status_0, H15_CPLD__RESET_STATUS_0, "reset status 0");
	if (ret < 0) {
		return ret;
	}

	registers->reset_status.h15 = (reset_status_0 >> H15_CPLD__RESET_STATUS_0_H15_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;
	registers->reset_status.ftdi = (reset_status_0 >> H15_CPLD__RESET_STATUS_0_FTDI_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;
	registers->reset_status.cam_0 = (reset_status_0 >> H15_CPLD__RESET_STATUS_0_CAM0_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;
	registers->reset_status.cam_1 = (reset_status_0 >> H15_CPLD__RESET_STATUS_0_CAM1_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;
	registers->reset_status.dsi_to_hdmi = (reset_status_0 >> H15_CPLD__RESET_STATUS_0_DSI_TO_HDMI_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;
	registers->reset_status.eth_phy = (reset_status_0 >> H15_CPLD__RESET_STATUS_0_ETH_PHY_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;
	registers->reset_status.usb_hub = (reset_status_0 >> H15_CPLD__RESET_STATUS_0_USB_HUB_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;
	registers->reset_status.audio_codec = (reset_status_0 >> H15_CPLD__RESET_STATUS_0_AUDIO_CODEC_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;

	if (0 == board_version) {
		/* EVB REV 1 does not support reset_status_1 */
		return ret;
	}

	ret = hailo15_cpld_get_register(pinctrl, &reset_status_1, H15_CPLD__RESET_STATUS_1, "reset status 1");
	if (ret < 0) {
		return ret;
	}

	registers->reset_status.flash = (reset_status_1 >> H15_CPLD__RESET_STATUS_1_FLASH_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;
	registers->reset_status.emmc = (reset_status_1 >> H15_CPLD__RESET_STATUS_1_EMMC_BIT_OFFSET) & H15_CPLD__RESET_STATUS_MASK;

	return ret;
}

int hailo15_cpld_get_sdio_route_status(struct hailo15_pinctrl *pinctrl, struct hailo15_cpld_registers *registers, uint8_t board_version)
{
	uint8_t sdio_route_status = 0;
	int ret = 0;
	if (0 == board_version) {
		/* EVB REV 1 does not support sdio route status */
		return ret;
	}

	ret = hailo15_cpld_get_register(pinctrl, &sdio_route_status, H15_CPLD__SDIO_ROUTE_STATUS, "sdio route status");

	registers->sdio_route_status.sdio_0 = (sdio_route_status >> H15_CPLD__SDIO_ROUTE_0_BIT_OFFSET) & H15_CPLD__SDIO_ROUTE_MASK;
	registers->sdio_route_status.sdio_1 = (sdio_route_status >> H15_CPLD__SDIO_ROUTE_1_BIT_OFFSET) & H15_CPLD__SDIO_ROUTE_MASK;

	return ret;
}

int hailo15_cpld_get_registers(struct hailo15_pinctrl *pinctrl, struct hailo15_cpld_registers *registers)
{
	int board_version = 0;
	int ret = 0;

	ret = hailo15_cpld_get_version(pinctrl, registers);
	if (ret < 0) {
		return ret;
	}

	board_version = registers->version.board;

	ret = hailo15_cpld_get_power_status(pinctrl, registers);
	if (ret < 0) {
		return ret;
	}

	ret = hailo15_cpld_get_reset_status(pinctrl, registers, board_version);
	if (ret < 0) {
		return ret;
	}

	ret = hailo15_cpld_get_bootstrap_status(pinctrl, registers);
	if (ret < 0) {
		return ret;
	}

	ret = hailo15_cpld_get_sdio_route_status(pinctrl, registers, board_version);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

void hailo15_cpld_print_dip_switches(struct hailo15_cpld_registers *registers,
									  struct seq_file *file)
{
	int board_version = registers->version.board;

	HAILO15_CPLD_LOG_FORMAT(file, "CPLD DIP switches state:\n");

	HAILO15_CPLD_LOG_FORMAT(file, "Board revision:                 %s\n",
				hailo15_cpld_board_revisions[(registers->version.board) & H15_CPLD__BOARD_VERSION_MASK]);

	HAILO15_CPLD_LOG_FORMAT(file, "CPLD version:                   %d.%d\n",
				registers->version.board,
				registers->version.minor);

	HAILO15_CPLD_LOG_FORMAT(
		file, "Power enable:                   %s\n",
		hailo15_cpld_switch_state[registers->power_status.power_enable &
					  H15_CPLD__POWER_ENABLE_MASK]);
	HAILO15_CPLD_LOG_FORMAT(
		file, "Power good:                     %s\n",
		hailo15_cpld_switch_state[registers->power_status.power_enable &
					  H15_CPLD__POWER_GOOD_MASK]);

	HAILO15_CPLD_LOG_FORMAT(file, "Reset status:\n");
	HAILO15_CPLD_LOG_FORMAT(
		file, "  Hailo15:                      %s\n",
		hailo15_cpld_switch_state[registers->reset_status.h15 &
					  H15_CPLD__RESET_STATUS_MASK]);
	HAILO15_CPLD_LOG_FORMAT(
		file, "  FTDI:                         %s\n",
		hailo15_cpld_switch_state[registers->reset_status.ftdi &
					  H15_CPLD__RESET_STATUS_MASK]);
	HAILO15_CPLD_LOG_FORMAT(
		file, "  Cam 0:                        %s\n",
		hailo15_cpld_switch_state[registers->reset_status.cam_0 &
					  H15_CPLD__RESET_STATUS_MASK]);
	HAILO15_CPLD_LOG_FORMAT(
		file, "  Cam 1:                        %s\n",
		hailo15_cpld_switch_state[registers->reset_status.cam_1 &
					  H15_CPLD__RESET_STATUS_MASK]);
	HAILO15_CPLD_LOG_FORMAT(
		file, "  DSI to HDMI:                  %s\n",
		hailo15_cpld_switch_state[registers->reset_status.dsi_to_hdmi &
					  H15_CPLD__RESET_STATUS_MASK]);
	HAILO15_CPLD_LOG_FORMAT(
		file, "  Eth PHY:                      %s\n",
		hailo15_cpld_switch_state[registers->reset_status.eth_phy &
					  H15_CPLD__RESET_STATUS_MASK]);
	HAILO15_CPLD_LOG_FORMAT(
		file, "  USB hub:                      %s\n",
		hailo15_cpld_switch_state[registers->reset_status.usb_hub &
					  H15_CPLD__RESET_STATUS_MASK]);
	HAILO15_CPLD_LOG_FORMAT(
		file, "  Audio codec:                  %s\n",
		hailo15_cpld_switch_state[registers->reset_status.audio_codec &
					  H15_CPLD__RESET_STATUS_MASK]);
	if (board_version > 0) {
		/* Not supported in EVB REV 1 */
		HAILO15_CPLD_LOG_FORMAT(
			file, "  Flash:                        %s\n",
			hailo15_cpld_switch_state[registers->reset_status.flash &
						  H15_CPLD__RESET_STATUS_MASK]);
		HAILO15_CPLD_LOG_FORMAT(
			file, "  eMMC:                         %s\n",
			hailo15_cpld_switch_state[registers->reset_status.emmc &
						  H15_CPLD__RESET_STATUS_MASK]);
	}

	HAILO15_CPLD_LOG_FORMAT(file, "Bootstrap status:\n");
	HAILO15_CPLD_LOG_FORMAT(
		file, "  Power on H15 on power insert: %s\n",
		hailo15_cpld_switch_state[registers->bootstrap_status.power_on_h15_on_power_insert & H15_CPLD__POWER_INSER_BIT_MASK]);

	if (board_version > 0) {
		HAILO15_CPLD_LOG_FORMAT(file, "SDIO route:\n");
		/* Not supported in EVB REV 1 */
		HAILO15_CPLD_LOG_FORMAT(
			file, "  SDIO 0:                       %s\n",
			hailo15_cpld_sdio_route[registers->sdio_route_status
							.sdio_0 &
						H15_CPLD__SDIO_ROUTE_MASK]);
		HAILO15_CPLD_LOG_FORMAT(
			file, "  SDIO 1:                       %s\n",
			hailo15_cpld_sdio_route[registers->sdio_route_status
							.sdio_1 &
						H15_CPLD__SDIO_ROUTE_MASK]);
	}
}

struct i2c_board_info cpld_board_info = {
	I2C_BOARD_INFO("pinctrl-cpld-hailo15",
		       H15_PINCTRL_CPLD_I2C_CLIENT_ADDRESS),
};

int hailo15_cpld_init(struct hailo15_pinctrl *pinctrl, struct device_node *node)
{
	struct device_node *np;
	struct i2c_adapter *i2c_adp;
	const u32 *board_config;
	static const u32 i2c_funcs = I2C_FUNC_I2C;
	int ret;
	struct hailo15_cpld_registers registers = {0, };

	np = of_parse_phandle(node, "i2c-bus", 0);
	if (!np) {
		dev_err(pinctrl->dev, "missing 'i2c-bus' property\n");
		return -ENODEV;
	}

	i2c_adp = of_find_i2c_adapter_by_node(np);
	of_node_put(np);
	if (!i2c_adp) {
		return -EPROBE_DEFER;
	}

	board_config = of_get_property(node, "board-config", NULL);
	if (!board_config) {
		dev_err(pinctrl->dev, "missing 'board-config' property\n");
		return -ENODEV;
	}

	pinctrl->board_config = be32_to_cpu(*board_config);
	pinctrl->pinctrl_cpld_i2c_client =
		i2c_new_client_device(i2c_adp, &cpld_board_info);
	if (IS_ERR(pinctrl->pinctrl_cpld_i2c_client)) {
		dev_err(pinctrl->dev, "Failure to register missing\n");
	}

	if (!i2c_check_functionality(pinctrl->pinctrl_cpld_i2c_client->adapter,
				     i2c_funcs)) {
		return -ENOSYS;
	}

	i2c_set_clientdata(pinctrl->pinctrl_cpld_i2c_client, pinctrl);

	ret = hailo15_cpld_set_board_config(pinctrl);
	if (ret < 0) {
		dev_err(pinctrl->dev,
			"Error in hailo15 CPLD set board config value\n");
		return ret;
	}

	ret = hailo15_cpld_get_registers(pinctrl, &registers);
	if (ret < 0) {
		dev_err(pinctrl->dev, "Get CPLD registers failed with err: %d \n", ret);
		return ret;
	}

	ret = hailo15_cpld_of_apply_overlay(hailo15_cpld_of_overlays, &registers);
	if (ret) {
		dev_err(pinctrl->dev, "Fail do apply overlay ret %d\n", ret);
		return ret;
	}

	hailo15_cpld_print_dip_switches(&registers, NULL);

	return 0;
}

int hailo15_cpld_gpio_set_direction(struct pinctrl_dev *pctrl_dev,
				    struct pinctrl_gpio_range *range,
				    unsigned int offset, bool input)
{
	struct hailo15_pinctrl *pinctrl = pinctrl_dev_get_drvdata(pctrl_dev);
	struct i2c_client *i2c_client = pinctrl->pinctrl_cpld_i2c_client;
	int err = 0;
	uint8_t read_value = 0, reg_addr = 0, mask = 0;

	err = hailo15_pinctrl_check_valid_direction(offset, input);
	if (err < 0) {
		return err;
	}

	/* For pins offset that smaller then H15_PINMUX_OUTPUT_ONLY_PINS_AMOUNT must to be only output, 
         * so the pinctrl pin direction didn't need to configure for these pins.
    	*/
	if (offset < H15_PINMUX_OUTPUT_ONLY_PINS_AMOUNT) {
		return 0;
	}

	if (offset < H15_PINMUX_PIN_COUNT) {
		reg_addr = H15_CPLD__GPIO07_06_DIRECTION + (offset / 8);
	} else {
		return -EIO;
	}

	err = pinctrl_hailo15_cpld_i2c_read(i2c_client, reg_addr, &read_value);
	if (err < 0) {
		return err;
	}

	mask = BIT(offset % 8);

	/*
	 * Turn off the relevant bit for that pin.
	*/
	read_value &= ~mask;

	/* 
	 * In case of output direction turn on the relevant bit for that pin.
	*/
	if (!input) {
		read_value |= mask;
	}

	err = pinctrl_hailo15_cpld_i2c_write(i2c_client, reg_addr, read_value);
	if (err < 0) {
		return err;
	}
	return 0;
}