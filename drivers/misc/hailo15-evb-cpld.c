/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2026 Hailo Technologies Ltd. All rights reserved.
 *
 * CPLD I2C driver for Hailo15 EVB boards.
 * Handles board register management, DT overlay application,
 * DIP switch status printing, and debugfs.
 */

#include <linux/bitfield.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/soc/hailo/hailo15-evb-cpld.h>

/* CPLD register addresses */
#define H15_CPLD_VERSION                        (0x0)
#define H15_CPLD_GPIO07_06_DIRECTION            (0x5)
#define H15_CPLD_BOARD_CONFIG                   (0x9)
#define H15_CPLD_BOOTSTRAP_STATUS               (0xA)
#define H15_CPLD_POWER_STATUS_0                 (0xE)
#define H15_CPLD_RESET_STATUS_0                 (0x11)
#define H15_CPLD_SDIO_ROUTE_STATUS              (0x19)
#define H15_CPLD_RESET_STATUS_1                 (0x1B)

/* Version register fields */
#define H15_CPLD_MINOR_VERSION                  GENMASK(4, 0)
#define H15_CPLD_BOARD_VERSION                  GENMASK(7, 5)

/* Bootstrap status fields */
#define H15_CPLD_POWER_INSERT                   BIT(5)

/* Power status fields */
#define H15_CPLD_POWER_ENABLE                   BIT(0)
#define H15_CPLD_POWER_GOOD                     BIT(1)

/* Reset status 0 fields */
#define H15_CPLD_RESET_H15                      BIT(0)
#define H15_CPLD_RESET_FTDI                     BIT(1)
#define H15_CPLD_RESET_CAM0                     BIT(2)
#define H15_CPLD_RESET_CAM1                     BIT(3)
#define H15_CPLD_RESET_DSI_HDMI                 BIT(4)
#define H15_CPLD_RESET_ETH_PHY                  BIT(5)
#define H15_CPLD_RESET_USB_HUB                  BIT(6)
#define H15_CPLD_RESET_AUDIO                    BIT(7)

/* Reset status 1 fields */
#define H15_CPLD_RESET_FLASH                    BIT(0)
#define H15_CPLD_RESET_EMMC                     BIT(1)

/* SDIO route fields */
#define H15_CPLD_SDIO_ROUTE_0                   GENMASK(1, 0)
#define H15_CPLD_SDIO_ROUTE_1                   GENMASK(3, 2)
#define H15_CPLD_SDIO_ROUTE_EMMC                (0)

/* Pin count for GPIO direction registers */
#define H15_CPLD_PIN_COUNT                      (32)
#define H15_CPLD_OUTPUT_ONLY_PINS               (6)

/* -----------------------------------------------------------------------------
 * DT Overlay Support
 */

struct hailo15_evb_cpld_of_overlay {
        void *begin;
        void *end;
};

#define HAILO15_EVB_CPLD_OF_DTB(type, property)                                    \
        extern char __dtb_hailo15_evb_cpld_of_##type##_##property##_begin[];\
        extern char __dtb_hailo15_evb_cpld_of_##type##_##property##_end[];

#define HAILO15_EVB_CPLD_OF_OVERLAY(type, property)                                \
        {                                                                        \
                .begin = __dtb_hailo15_evb_cpld_of_##type##_##property##_begin,        \
                .end = __dtb_hailo15_evb_cpld_of_##type##_##property##_end,        \
        }

HAILO15_EVB_CPLD_OF_DTB(evb, rev1);
HAILO15_EVB_CPLD_OF_DTB(evb, rev2);
HAILO15_EVB_CPLD_OF_DTB(evb, sdio8bit);

enum overlay_type_id {
        EVB_REV1 = 0,
        EVB_REV2,
        EVB_SDIO_8BIT,
};

static const struct hailo15_evb_cpld_of_overlay hailo15_evb_cpld_of_overlays[] = {
        HAILO15_EVB_CPLD_OF_OVERLAY(evb, rev1),
        HAILO15_EVB_CPLD_OF_OVERLAY(evb, rev2),
        HAILO15_EVB_CPLD_OF_OVERLAY(evb, sdio8bit),
};

/* -----------------------------------------------------------------------------
 * Register Structures and String Tables
 */

struct hailo15_evb_cpld_registers {
        struct {
                u8 board;
                u8 minor;
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
                u8 sdio_0;
                u8 sdio_1;
        } sdio_route_status;
};

static const char *hailo15_evb_cpld_board_revisions[] = {
        "EVB rev 1",
        "EVB rev 2",
        "SVB rev 1",
        "UNKNOWN (3)",
        "UNKNOWN (4)",
        "UNKNOWN (5)",
        "UNKNOWN (6)",
        "UNKNOWN (7)"
};

static const char *hailo15_evb_cpld_sdio_route[] = {
        "eMMC",
        "E key",
        "SD card",
        "N/A"
};

static const char *hailo15_evb_cpld_switch_state[] = {
        "OFF",
        "ON"
};

/* -----------------------------------------------------------------------------
 * Regmap Configuration
 */

/*
 * REGCACHE_RBTREE starts empty, so the first read of each register goes to
 * hardware and the result is cached.  This is safe because status registers
 * (version, power, reset, bootstrap, SDIO route) are latched from hardware
 * strapping at power-on and never change at runtime, while control registers
 * (GPIO direction, board config) are only modified by this driver.
 */
static const struct regmap_config hailo15_evb_cpld_regmap_config = {
        .reg_bits = 8,
        .val_bits = 8,
        .max_register = H15_CPLD_RESET_STATUS_1,
        .cache_type = REGCACHE_RBTREE,
};

/* -----------------------------------------------------------------------------
 * CPLD Context
 */

struct hailo15_evb_cpld {
        struct regmap *regmap;
        struct device *dev;
        struct dentry *debugfs_dir;
        int ovcs_id;
        bool probed;
};

/* -----------------------------------------------------------------------------
 * Register Access
 */

static int cpld_read_reg(struct hailo15_evb_cpld *cpld, unsigned int reg,
                         unsigned int *val, const char *name)
{
        int ret;

        ret = regmap_read(cpld->regmap, reg, val);
        if (ret < 0)
                dev_err(cpld->dev, "unable to read %s from CPLD\n", name);
        return ret;
}

static int hailo15_evb_cpld_get_version(struct hailo15_evb_cpld *cpld,
                                        struct hailo15_evb_cpld_registers *regs)
{
        unsigned int version;
        int ret;

        ret = cpld_read_reg(cpld, H15_CPLD_VERSION, &version, "version");
        if (ret < 0)
                return ret;

        regs->version.minor = FIELD_GET(H15_CPLD_MINOR_VERSION, version);
        regs->version.board = FIELD_GET(H15_CPLD_BOARD_VERSION, version);

        return 0;
}

static int hailo15_evb_cpld_get_power_status(struct hailo15_evb_cpld *cpld,
                                             struct hailo15_evb_cpld_registers *regs)
{
        unsigned int power_status;
        int ret;

        ret = cpld_read_reg(cpld, H15_CPLD_POWER_STATUS_0, &power_status,
                            "power status");
        if (ret < 0)
                return ret;

        regs->power_status.power_enable =
                FIELD_GET(H15_CPLD_POWER_ENABLE, power_status);
        regs->power_status.power_good =
                FIELD_GET(H15_CPLD_POWER_GOOD, power_status);

        return 0;
}

static int hailo15_evb_cpld_get_reset_status(struct hailo15_evb_cpld *cpld,
                                             struct hailo15_evb_cpld_registers *regs)
{
        unsigned int reset_status_0;
        unsigned int reset_status_1;
        int ret;

        ret = cpld_read_reg(cpld, H15_CPLD_RESET_STATUS_0, &reset_status_0,
                            "reset status 0");
        if (ret < 0)
                return ret;

        regs->reset_status.h15 = FIELD_GET(H15_CPLD_RESET_H15, reset_status_0);
        regs->reset_status.ftdi = FIELD_GET(H15_CPLD_RESET_FTDI, reset_status_0);
        regs->reset_status.cam_0 = FIELD_GET(H15_CPLD_RESET_CAM0, reset_status_0);
        regs->reset_status.cam_1 = FIELD_GET(H15_CPLD_RESET_CAM1, reset_status_0);
        regs->reset_status.dsi_to_hdmi =
                FIELD_GET(H15_CPLD_RESET_DSI_HDMI, reset_status_0);
        regs->reset_status.eth_phy =
                FIELD_GET(H15_CPLD_RESET_ETH_PHY, reset_status_0);
        regs->reset_status.usb_hub =
                FIELD_GET(H15_CPLD_RESET_USB_HUB, reset_status_0);
        regs->reset_status.audio_codec =
                FIELD_GET(H15_CPLD_RESET_AUDIO, reset_status_0);

        if (regs->version.board == 0)
                return 0;

        ret = cpld_read_reg(cpld, H15_CPLD_RESET_STATUS_1, &reset_status_1,
                            "reset status 1");
        if (ret < 0)
                return ret;

        regs->reset_status.flash = FIELD_GET(H15_CPLD_RESET_FLASH, reset_status_1);
        regs->reset_status.emmc = FIELD_GET(H15_CPLD_RESET_EMMC, reset_status_1);

        return 0;
}

static int hailo15_evb_cpld_get_bootstrap_status(struct hailo15_evb_cpld *cpld,
                                             struct hailo15_evb_cpld_registers *regs)
{
        unsigned int bootstrap_status;
        int ret;

        ret = cpld_read_reg(cpld, H15_CPLD_BOOTSTRAP_STATUS,
                            &bootstrap_status, "bootstrap status");
        if (ret < 0)
                return ret;

        regs->bootstrap_status.power_on_h15_on_power_insert =
                FIELD_GET(H15_CPLD_POWER_INSERT, bootstrap_status);

        return 0;
}

static int hailo15_evb_cpld_get_sdio_route_status(
        struct hailo15_evb_cpld *cpld, struct hailo15_evb_cpld_registers *regs)
{
        unsigned int sdio_route_status;
        int ret;

        if (regs->version.board == 0)
                return 0;

        ret = cpld_read_reg(cpld, H15_CPLD_SDIO_ROUTE_STATUS,
                            &sdio_route_status, "sdio route status");
        if (ret < 0)
                return ret;

        regs->sdio_route_status.sdio_0 =
                FIELD_GET(H15_CPLD_SDIO_ROUTE_0, sdio_route_status);
        regs->sdio_route_status.sdio_1 =
                FIELD_GET(H15_CPLD_SDIO_ROUTE_1, sdio_route_status);

        return 0;
}

static int hailo15_evb_cpld_get_registers(struct hailo15_evb_cpld *cpld,
                                          struct hailo15_evb_cpld_registers *regs)
{
        int ret;

        ret = hailo15_evb_cpld_get_version(cpld, regs);
        if (ret < 0)
                return ret;

        ret = hailo15_evb_cpld_get_power_status(cpld, regs);
        if (ret < 0)
                return ret;

        ret = hailo15_evb_cpld_get_reset_status(cpld, regs);
        if (ret < 0)
                return ret;

        ret = hailo15_evb_cpld_get_bootstrap_status(cpld, regs);
        if (ret < 0)
                return ret;

        ret = hailo15_evb_cpld_get_sdio_route_status(cpld, regs);
        if (ret < 0)
                return ret;

        return 0;
}

/* -----------------------------------------------------------------------------
 * DIP Switch Printing
 */

#define HAILO15_EVB_CPLD_LOG(file, format, ...) do {                        \
        if (file)                                                        \
                seq_printf(file, format, ##__VA_ARGS__);                \
        else                                                                \
                pr_info(format, ##__VA_ARGS__);                                \
} while (0)

static void hailo15_evb_cpld_print_dip_switches(
        struct hailo15_evb_cpld_registers *regs, struct seq_file *file)
{
        int board_version = regs->version.board;

        HAILO15_EVB_CPLD_LOG(file, "CPLD DIP switches state:\n");

        HAILO15_EVB_CPLD_LOG(file, "Board revision:                 %s\n",
                             hailo15_evb_cpld_board_revisions[regs->version.board]);

        HAILO15_EVB_CPLD_LOG(file, "CPLD version:                   %d.%d\n",
                             regs->version.board, regs->version.minor);

        HAILO15_EVB_CPLD_LOG(file, "Power enable:                   %s\n",
                             hailo15_evb_cpld_switch_state[regs->power_status.power_enable]);
        HAILO15_EVB_CPLD_LOG(file, "Power good:                     %s\n",
                             hailo15_evb_cpld_switch_state[regs->power_status.power_good]);

        HAILO15_EVB_CPLD_LOG(file, "Reset status:\n");
        HAILO15_EVB_CPLD_LOG(file, "  Hailo15:                      %s\n",
                             hailo15_evb_cpld_switch_state[regs->reset_status.h15]);
        HAILO15_EVB_CPLD_LOG(file, "  FTDI:                         %s\n",
                             hailo15_evb_cpld_switch_state[regs->reset_status.ftdi]);
        HAILO15_EVB_CPLD_LOG(file, "  Cam 0:                        %s\n",
                             hailo15_evb_cpld_switch_state[regs->reset_status.cam_0]);
        HAILO15_EVB_CPLD_LOG(file, "  Cam 1:                        %s\n",
                             hailo15_evb_cpld_switch_state[regs->reset_status.cam_1]);
        HAILO15_EVB_CPLD_LOG(file, "  DSI to HDMI:                  %s\n",
                             hailo15_evb_cpld_switch_state[regs->reset_status.dsi_to_hdmi]);
        HAILO15_EVB_CPLD_LOG(file, "  Eth PHY:                      %s\n",
                             hailo15_evb_cpld_switch_state[regs->reset_status.eth_phy]);
        HAILO15_EVB_CPLD_LOG(file, "  USB hub:                      %s\n",
                             hailo15_evb_cpld_switch_state[regs->reset_status.usb_hub]);
        HAILO15_EVB_CPLD_LOG(file, "  Audio codec:                  %s\n",
                             hailo15_evb_cpld_switch_state[regs->reset_status.audio_codec]);
        if (board_version > 0) {
                HAILO15_EVB_CPLD_LOG(file, "  Flash:                        %s\n",
                                     hailo15_evb_cpld_switch_state[regs->reset_status.flash]);
                HAILO15_EVB_CPLD_LOG(file, "  eMMC:                         %s\n",
                                     hailo15_evb_cpld_switch_state[regs->reset_status.emmc]);
        }

        HAILO15_EVB_CPLD_LOG(file, "Bootstrap status:\n");
        HAILO15_EVB_CPLD_LOG(file, "  Power on H15 on power insert: %s\n",
                             hailo15_evb_cpld_switch_state[regs->bootstrap_status.power_on_h15_on_power_insert]);

        if (board_version > 0) {
                HAILO15_EVB_CPLD_LOG(file, "SDIO route:\n");
                HAILO15_EVB_CPLD_LOG(file, "  SDIO 0:                       %s\n",
                                     hailo15_evb_cpld_sdio_route[regs->sdio_route_status.sdio_0]);
                HAILO15_EVB_CPLD_LOG(file, "  SDIO 1:                       %s\n",
                                     hailo15_evb_cpld_sdio_route[regs->sdio_route_status.sdio_1]);
        }
}

/* -----------------------------------------------------------------------------
 * Debugfs
 */

static int switches_show(struct seq_file *s, void *unused)
{
        struct hailo15_evb_cpld *cpld = s->private;
        struct hailo15_evb_cpld_registers regs = {0};
        int ret;

        ret = hailo15_evb_cpld_get_registers(cpld, &regs);
        if (ret < 0)
                return ret;

        hailo15_evb_cpld_print_dip_switches(&regs, s);
        return 0;
}
DEFINE_SHOW_ATTRIBUTE(switches);

/* -----------------------------------------------------------------------------
 * DT Overlay Application
 */

static bool hailo15_evb_cpld_is_sdio1_8_bit(struct hailo15_evb_cpld_registers *regs)
{
        return (regs->sdio_route_status.sdio_0 == H15_CPLD_SDIO_ROUTE_EMMC) &&
               (regs->sdio_route_status.sdio_1 == H15_CPLD_SDIO_ROUTE_EMMC);
}

static int hailo15_evb_cpld_apply_overlay(struct hailo15_evb_cpld *cpld,
                                          struct hailo15_evb_cpld_registers *regs)
{
        const struct hailo15_evb_cpld_of_overlay *dtb;
        int board_version = regs->version.board;
        enum overlay_type_id type_id;

        if (board_version == 0) {
                type_id = EVB_REV1;
        } else if (hailo15_evb_cpld_is_sdio1_8_bit(regs)) {
                type_id = EVB_SDIO_8BIT;
        } else {
                type_id = EVB_REV2;
        }
        dtb = &hailo15_evb_cpld_of_overlays[type_id];

        cpld->ovcs_id = 0;
        return of_overlay_fdt_apply(dtb->begin, dtb->end - dtb->begin,
                                    &cpld->ovcs_id);
}

/* -----------------------------------------------------------------------------
 * GPIO Direction (exported for pinctrl)
 */

int hailo15_evb_cpld_set_gpio_direction(struct hailo15_evb_cpld *cpld,
                                        unsigned int offset, bool input)
{
        unsigned int reg_addr, mask, val;

        if (!cpld || !cpld->probed)
                return -ENODEV;

        if (offset < H15_CPLD_OUTPUT_ONLY_PINS)
                return 0;

        if (offset >= H15_CPLD_PIN_COUNT)
                return -EIO;

        reg_addr = H15_CPLD_GPIO07_06_DIRECTION + (offset / 8);
        mask = BIT(offset % 8);
        val = input ? 0 : mask;

        return regmap_update_bits(cpld->regmap, reg_addr, mask, val);
}
EXPORT_SYMBOL_GPL(hailo15_evb_cpld_set_gpio_direction);

/* -----------------------------------------------------------------------------
 * I2C Driver
 */

static int hailo15_evb_cpld_probe(struct i2c_client *client,
                                  const struct i2c_device_id *id)
{
        struct device *dev = &client->dev;
        struct hailo15_evb_cpld *cpld;
        u32 board_config;
        struct hailo15_evb_cpld_registers regs = {0};
        int ret;

        cpld = devm_kzalloc(dev, sizeof(*cpld), GFP_KERNEL);
        if (!cpld)
                return -ENOMEM;

        cpld->regmap = devm_regmap_init_i2c(client,
                                             &hailo15_evb_cpld_regmap_config);
        if (IS_ERR(cpld->regmap))
                return dev_err_probe(dev, PTR_ERR(cpld->regmap),
                                     "failed to init regmap\n");

        cpld->dev = dev;
        i2c_set_clientdata(client, cpld);

        ret = of_property_read_u32(dev->of_node, "board-config", &board_config);
        if (ret) {
                dev_err(dev, "missing 'board-config' property\n");
                return ret;
        }

        ret = regmap_write(cpld->regmap, H15_CPLD_BOARD_CONFIG, board_config);
        if (ret < 0) {
                dev_err(dev, "Error in CPLD set board config\n");
                return ret;
        }

        ret = hailo15_evb_cpld_get_registers(cpld, &regs);
        if (ret < 0) {
                dev_err(dev, "Get CPLD registers failed with err: %d\n", ret);
                return ret;
        }

        ret = hailo15_evb_cpld_apply_overlay(cpld, &regs);
        if (ret) {
                dev_err(dev, "Failed to apply CPLD overlay: %d\n", ret);
                return ret;
        }

        hailo15_evb_cpld_print_dip_switches(&regs, NULL);

        cpld->debugfs_dir = debugfs_create_dir("hailo15-evb-cpld", NULL);
        debugfs_create_file("switches", 0444, cpld->debugfs_dir,
                            cpld, &switches_fops);

        cpld->probed = true;

        return 0;
}

static int hailo15_evb_cpld_remove(struct i2c_client *client)
{
        struct hailo15_evb_cpld *cpld = i2c_get_clientdata(client);

        debugfs_remove_recursive(cpld->debugfs_dir);
        of_overlay_remove(&cpld->ovcs_id);
        return 0;
}

static const struct of_device_id hailo15_evb_cpld_of_match[] = {
        { .compatible = "hailo,hailo15-evb-cpld" },
        {},
};
MODULE_DEVICE_TABLE(of, hailo15_evb_cpld_of_match);

static struct i2c_driver hailo15_evb_cpld_driver = {
        .driver = {
                .name = "hailo15-evb-cpld",
                .of_match_table = hailo15_evb_cpld_of_match,
        },
        .probe = hailo15_evb_cpld_probe,
        .remove = hailo15_evb_cpld_remove,
};
module_i2c_driver(hailo15_evb_cpld_driver);
MODULE_LICENSE("GPL");
