#include <linux/init.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/soc/hailo/scmi_hailo_ops.h>
#include <dt-bindings/soc/hailo15_release_version.h>
#include <linux/soc/hailo/scmi_hailo_protocol.h>

/* Hailo product ID definitions */
#define HAILO_SCMI_PRODUCT_ID__INVALID 0
#define HAILO_SCMI_PRODUCT_ID__15H 1
#define HAILO_SCMI_PRODUCT_ID__15M 2
#define HAILO_SCMI_PRODUCT_ID__15L 3
#define HAILO_SCMI_PRODUCT_ID__10H 4
#define HAILO_SCMI_PRODUCT_ID__12L 5

#define HAILO10_SCMI_BOARD_SKU_ID__INVALID (0xFFFF)

#define MAX_BOARD_ID_STR_LEN (100)
#define	SCMI_HAILO_BOOT_SUCCESS_AP_SOFTWARE  1
#define	SCMI_HAILO_BOOT_SUCCESS_SW_UPDATE 99
#define CHIP_SERIAL_NUMBER_SIZE_WORDS 3
#define CHIP_SERIAL_NUMBER_SIZE_BYTES (CHIP_SERIAL_NUMBER_SIZE_WORDS * sizeof(u32))

#define HOST_CURRENT_LIMIT_LOW_MA (900)
#define HOST_CURRENT_LIMIT_MEDIUM_MA (1500)
#define HOST_CURRENT_LIMIT_HIGH_MA (3000)

/* It is important to keep the string as "NA" because this is what HRT displays to the user */
#define HOST_CURRENT_NOT_SET_STRING "NA"


static const u32 allowed_host_currents[] = {HOST_CURRENT_LIMIT_LOW_MA, HOST_CURRENT_LIMIT_MEDIUM_MA, HOST_CURRENT_LIMIT_HIGH_MA};



static const struct scmi_hailo_ops *hailo_ops;

static const struct of_device_id hailo_soc_of_match[] = {
	{ .compatible = "hailo,hailo15" },
	{ .compatible = "hailo,hailo10h" },
	{ .compatible = "hailo,hailo15l" },
	{ .compatible = "hailo,hailo12l" },
	{}
};

struct __attribute__((packed)) hailo_fuse_file {
	u8 user_fuse_array[SCMI_HAILO_PROTOCOL_USER_FUSE_DATA_SIZE];
	u32 active_clusters;
};
struct __attribute__((packed)) hailo_chip_serial_file {
    u32 chip_serial[CHIP_SERIAL_NUMBER_SIZE_WORDS];
};

/* BIST mask at linux file is a failure indication:
   00 - success
   01 - BIHR failed
   10 - BIST failed
   11 - BISR failed */
struct __attribute__((packed)) hailo_mbist_status_file {
	u32 mbist_status;
};

struct __attribute__((packed)) hailo_identification_attributes_file {
	u8 lcs;
	u8 cryptocell_soc_id[CRYPTOCELL_SOC_ID_SIZE];
	uint32_t lvt;
	uint32_t svt;
	uint32_t ulvt;
};

struct hailo_soc {
	struct hailo_fuse_file fuse_file;
	struct hailo_chip_serial_file chip_serial_file;
	struct scmi_hailo_get_boot_info_p2a boot_info;
	u32 product_id;
	struct kernfs_node *fuse_kn;
	struct soc_device *soc_dev;
	struct scmi_hailo_send_components_version_p2a components_version;
	struct hailo_mbist_status_file mbist_status_file;
	struct hailo_identification_attributes_file identification_attributes_file;
	char board_id_str[MAX_BOARD_ID_STR_LEN];
	u32 host_current_limit_mA;
	bool host_current_limit_sent_flag;
};

#define H15__SCU_BOOT_BIT_MASK (3)

static const char *hailo15_boot_options[] = {
    [BOOT_SOURCE_BOOTSTRAP] = "BOOTSTRAP",
    [BOOT_SOURCE_SPI_FLASH] = "Flash",
    [BOOT_SOURCE_UART] = "UART",
    [BOOT_SOURCE_PCIE] = "PCIe",
    [BOOT_SOURCE_EMMC0] = "EMMC0",
    [BOOT_SOURCE_EMMC1] = "EMMC1"
};

static ssize_t boot_success_scu_bl_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       hailo_soc->boot_info.boot_status_bitmap.boot_success_scu_bl);
}

static ssize_t boot_success_scu_fw_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       hailo_soc->boot_info.boot_status_bitmap.boot_success_scu_fw);
}

static ssize_t boot_success_ap_bootloader_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       hailo_soc->boot_info.boot_status_bitmap.boot_success_ap_bootloader);
}

static ssize_t boot_success_ap_software_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       hailo_soc->boot_info.boot_status_bitmap.boot_success_ap_software);
}

static ssize_t boot_success_ap_software_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);
	struct scmi_hailo_boot_success_indication_a2p params;
	u8 val;
	int ret;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	hailo_ops = scmi_hailo_get_ops();
	if (IS_ERR(hailo_ops))
		return PTR_ERR(hailo_ops);

	// value == 1 indicates that linux has booted successfully
	if(val == SCMI_HAILO_BOOT_SUCCESS_AP_SOFTWARE)
	{
		hailo_soc->boot_info.boot_status_bitmap.boot_success_ap_software = val;

		pr_info("SCU booted from:                %s",
			hailo15_boot_options[
				hailo_soc->boot_info.active_boot_image_storage & H15__SCU_BOOT_BIT_MASK]);

		// Send SCMI message to SCU FW, indicating linux boot success
		params.component = SCMI_HAILO_BOOT_SUCCESS_COMPONENT_AP_SOFTWARE;
		ret = hailo_ops->send_boot_success_ind(&params);
		if (ret) {
			dev_err(dev, "Failed to send boot success indication\n");
			return ret;
		}
	}

	// value == 99 indicates swupdate procedure has just completed
	else if(val == SCMI_HAILO_BOOT_SUCCESS_SW_UPDATE)
	{
		// send SCMI message to SCU FW, indicating swupdate procedure has completed
		ret = hailo_ops->send_swupdate_ind();
		if (ret) {
			dev_err(dev, "Failed to send swupdate indication\n");
			return ret;
		}
	}

	else
	{
		dev_err(dev, "boot_success_ap_software_store: invalid value (=%d)\n", val);
		return -EINVAL;
	}

	return count;
}

static int hailo_soc_send_boot_success_indication(struct device *dev)
{
    struct scmi_hailo_boot_success_indication_a2p params;
    int ret;

	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

    /* Check if already marked as successful */
    if (hailo_soc->boot_info.boot_status_bitmap.boot_success_ap_software)
        return 0;

    hailo_soc->boot_info.boot_status_bitmap.boot_success_ap_software = SCMI_HAILO_BOOT_SUCCESS_AP_SOFTWARE;

    /* Send SCMI message to SCU FW, indicating linux boot success */
    params.component = SCMI_HAILO_BOOT_SUCCESS_COMPONENT_AP_SOFTWARE;
    ret = hailo_ops->send_boot_success_ind(&params);
    if (ret) {
        dev_err(dev, "Failed to send boot success indication\n");
        return ret;
    }

    return 0;
}

static ssize_t boot_count_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       hailo_soc->boot_info.boot_count);
}

static ssize_t active_image_desc_index_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       hailo_soc->boot_info.active_image_desc_index);
}

static ssize_t active_boot_image_storage_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       hailo_soc->boot_info.active_boot_image_storage);
}

static ssize_t active_boot_image_offset_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "0x%08X\n",
		       hailo_soc->boot_info.active_boot_image_offset);
}

static ssize_t bootstrap_image_storage_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n",
		       hailo_soc->boot_info.bootstrap_image_storage);
}

static ssize_t scu_to_ap_timer_offset_ns_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%llu\n",
				hailo_soc->boot_info.scu_to_ap_timer_offset_ns);
}

static ssize_t identification_attributes_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);
	memcpy(buf, &hailo_soc->identification_attributes_file, sizeof(hailo_soc->identification_attributes_file));
	return sizeof(hailo_soc->identification_attributes_file);
}

static DEVICE_ATTR_RO(identification_attributes);
static DEVICE_ATTR_RO(boot_success_scu_bl);
static DEVICE_ATTR_RO(boot_success_scu_fw);
static DEVICE_ATTR_RO(boot_success_ap_bootloader);
static DEVICE_ATTR_RW(boot_success_ap_software);
static DEVICE_ATTR_RO(boot_count);
static DEVICE_ATTR_RO(active_image_desc_index);
static DEVICE_ATTR_RO(active_boot_image_storage);
static DEVICE_ATTR_RO(active_boot_image_offset);
static DEVICE_ATTR_RO(bootstrap_image_storage);
static DEVICE_ATTR_RO(scu_to_ap_timer_offset_ns);

static struct attribute *hailo_boot_info_attrs[] = {
	&dev_attr_boot_success_scu_bl.attr,
	&dev_attr_boot_success_scu_fw.attr,
	&dev_attr_boot_success_ap_bootloader.attr,
	&dev_attr_boot_success_ap_software.attr,
	&dev_attr_boot_count.attr,
	&dev_attr_active_image_desc_index.attr,
	&dev_attr_active_boot_image_storage.attr,
	&dev_attr_active_boot_image_offset.attr,
	&dev_attr_bootstrap_image_storage.attr,
	&dev_attr_scu_to_ap_timer_offset_ns.attr,
	NULL,
};

static const struct attribute_group hailo_boot_info_group = { .name = "boot_info", .attrs = hailo_boot_info_attrs, };

static ssize_t fuse_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	memcpy(buf, &hailo_soc->fuse_file, sizeof(hailo_soc->fuse_file));
	return sizeof(hailo_soc->fuse_file);
}

static DEVICE_ATTR_RO(fuse);

static ssize_t product_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);
	size_t i;

	static const struct {
		u32 id;
		const char *name;
	} sku_id_map[] = {
		{ .id = HAILO_SCMI_PRODUCT_ID__15H, .name = "Hailo-15H" },
		{ .id = HAILO_SCMI_PRODUCT_ID__15M, .name = "Hailo-15M" },
		{ .id = HAILO_SCMI_PRODUCT_ID__15L, .name = "Hailo-15L" },
		{ .id = HAILO_SCMI_PRODUCT_ID__12L, .name = "Hailo-12L" },
		{ .id = HAILO_SCMI_PRODUCT_ID__10H, .name = "Hailo-10H" },
	};


	for (i = 0; i < ARRAY_SIZE(sku_id_map); i++) {
		if (hailo_soc->product_id == sku_id_map[i].id) {
			return sprintf(buf, "%s\n", sku_id_map[i].name);
		}
	}

	dev_err(dev, "Unknown SKU ID: %u\n", hailo_soc->product_id);
	return -EINVAL;
}

static DEVICE_ATTR_RO(product);

void board_sku_id_to_str(u32 board_sku_id, char *name, size_t name_size)
{
	if (board_sku_id == HAILO10_SCMI_BOARD_SKU_ID__INVALID) {
		snprintf(name, name_size, "NA");
	} else {
		snprintf(name, name_size, "%u", board_sku_id);
		}
}
EXPORT_SYMBOL_GPL(board_sku_id_to_str);

static ssize_t board_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", hailo_soc->board_id_str);
}

static DEVICE_ATTR_RO(board_id);

static ssize_t current_limit_show(struct device *dev, struct device_attribute *attr,
                               char *buf)
{
    struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

    if (!hailo_soc->host_current_limit_sent_flag) {
        return sprintf(buf, HOST_CURRENT_NOT_SET_STRING "\n");
    }

    return sprintf(buf, "%u\n", hailo_soc->host_current_limit_mA);
}

static ssize_t current_limit_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);
	struct scmi_hailo_send_host_current_limit_a2p params;
	u32 limit;
	bool valid_current = false;
	int ret;
	size_t i;

	if (hailo_soc->product_id != HAILO_SCMI_PRODUCT_ID__10H &&
	    hailo_soc->product_id != HAILO_SCMI_PRODUCT_ID__12L) {
		dev_err(dev,
			"Setting host current supply limit not supported for this product\n");
		return -EPERM;
	}

	if (kstrtou32(buf, 10, &limit))
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(allowed_host_currents); i++) {
		if (limit == allowed_host_currents[i]) {
			valid_current = true;
			break;
		}
	}

	if (!valid_current) {
		dev_err(dev, "Invalid host current limit");
		return -EINVAL;
	}

	hailo_ops = scmi_hailo_get_ops();
	if (IS_ERR(hailo_ops))
		return PTR_ERR(hailo_ops);

	params.host_current_limit_mA = limit;
	ret = hailo_ops->send_host_current_limit(&params);

	if (ret) {
		dev_err(dev, "Failed to send host current limit SCMI\n");
		return ret;
	}

	// Mark as written
	hailo_soc->host_current_limit_mA = limit;
	hailo_soc->host_current_limit_sent_flag = true;

	return count;
}

static DEVICE_ATTR_RW(current_limit);

static ssize_t jtag_selector_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	u8 value;

	int ret = hailo_ops->get_jtag_selector(&value);
	if (ret) {
		dev_err(dev, "Failed to get JTAG selector: %d\n", ret);
		return ret;
	}

	return sprintf(buf, "%d\n", value);
}

static ssize_t jtag_selector_store(struct device *dev, struct device_attribute *attr,
                       const char *buf, size_t count)
{
	int value;
	int ret;

	if (kstrtouint(buf, 0, &value))
		return -EINVAL;

	ret = hailo_ops->set_jtag_selector(value);
	if (ret) {
		dev_err(dev, "Failed to set JTAG selector: %d\n", ret);
		return ret;
	}

	return count;
}

static DEVICE_ATTR_RW(jtag_selector);

static ssize_t mbist_status_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	memcpy(buf, &hailo_soc->mbist_status_file, sizeof(hailo_soc->mbist_status_file));
	return sizeof(hailo_soc->mbist_status_file);
}

static DEVICE_ATTR_RO(mbist_status);


static ssize_t chip_serial_number_show(struct device *dev, struct device_attribute *attr,
             char *buf)
{
    struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

    memcpy(buf, &hailo_soc->chip_serial_file, sizeof(hailo_soc->chip_serial_file));
    return sizeof(hailo_soc->chip_serial_file);
}

static DEVICE_ATTR_RO(chip_serial_number);


static ssize_t hailo_scu_fw_version_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d.%d.%d\n",
		((hailo_soc->components_version.scu_version >> 24) & 0xff),
		((hailo_soc->components_version.scu_version >> 16) & 0xff),
		 (hailo_soc->components_version.scu_version & 0xffff));
}

static ssize_t hailo_uboot_version_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "%d.%d.%d\n",
		((hailo_soc->components_version.uboot_version >> 24) & 0xff),
		((hailo_soc->components_version.uboot_version >> 16) & 0xff),
		 (hailo_soc->components_version.uboot_version & 0xffff));
}

static ssize_t hailo_linux_version_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%d.%d.%d\n",
		((HAILO_LINUX_RELEASE_BUILD_VERSION >> 24) & 0xff),
		((HAILO_LINUX_RELEASE_BUILD_VERSION >> 16) & 0xff),
		 (HAILO_LINUX_RELEASE_BUILD_VERSION & 0xffff));
}

static DEVICE_ATTR_RO(hailo_scu_fw_version);
static DEVICE_ATTR_RO(hailo_uboot_version);
static DEVICE_ATTR_RO(hailo_linux_version);

static struct attribute *hailo_versions_attrs[] = {
	&dev_attr_hailo_scu_fw_version.attr,
	&dev_attr_hailo_uboot_version.attr,
	&dev_attr_hailo_linux_version.attr,
	NULL,
};

static const struct attribute_group hailo_versions_group = { .name = "hailo_versions", .attrs = hailo_versions_attrs, };

static struct attribute *hailo_attrs[] = {
	&dev_attr_fuse.attr,
	&dev_attr_jtag_selector.attr,
	&dev_attr_mbist_status.attr,
	&dev_attr_identification_attributes.attr,
	&dev_attr_product.attr,
	&dev_attr_board_id.attr,
	&dev_attr_chip_serial_number.attr,
	&dev_attr_current_limit.attr,
	NULL
};

ATTRIBUTE_GROUPS(hailo);

static int hailo_soc_fill_sku_ids(struct hailo_soc *hailo_soc, const char *compat)
{
	struct scmi_hailo_get_sku_id_p2a sku_id;
	const char *model;
	int ret;

	ret = hailo_ops->get_sku_id(&sku_id);
	if (ret) {
		return ret;
	}

	hailo_soc->product_id = sku_id.product;

	if (strcmp(compat, "hailo,hailo10h") == 0) {
		board_sku_id_to_str(sku_id.board, hailo_soc->board_id_str, MAX_BOARD_ID_STR_LEN);
		snprintf(hailo_soc->board_id_str, MAX_BOARD_ID_STR_LEN, "%s", hailo_soc->board_id_str);
	} else {
		if (of_property_read_string(of_root, "model", &model) == 0) {
			snprintf(hailo_soc->board_id_str, MAX_BOARD_ID_STR_LEN, "%s", model);
		} else {
			snprintf(hailo_soc->board_id_str, MAX_BOARD_ID_STR_LEN, "Failed to read model property");
		}
	}

	return 0;
}

static int hailo_soc_fill_fuse_and_chip_serial_files(struct hailo_fuse_file *fuse_file, struct hailo_chip_serial_file *chip_serial_file)
{
	struct scmi_hailo_get_fuse_info_p2a fuse_info;
	int ret;

	ret = hailo_ops->get_fuse_info(&fuse_info);
	if (ret) {
		return ret;
	}

	memcpy(&fuse_file->user_fuse_array, &fuse_info.user_fuse, sizeof(struct scmi_hailo_user_fuse));
	fuse_file->active_clusters = fuse_info.active_clusters;

	memcpy(&chip_serial_file->chip_serial, &fuse_info.chip_serial, sizeof(fuse_info.chip_serial));

	return 0;
}

static int hailo_soc_fill_mbist_status_file(struct hailo_mbist_status_file *mbist_status_file)
{
	struct scmi_hailo_mbist_subservers_status_p2a mbist_status;
	int ret;

	ret = hailo_ops->get_mbist_subservers_status(&mbist_status);
	if (ret) {
		return ret;
	}

	memcpy(&mbist_status_file->mbist_status, &mbist_status.mbist_status_bitmask, sizeof(struct scmi_hailo_mbist_subservers_status_p2a));

	return 0;
}

static ssize_t hailo_throttling_show(enum scmi_hailo_throttling_domain domain, struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_get_throttling_mode_a2p params;
	struct scmi_hailo_get_throttling_mode_p2a info;
	int rc;

	params.domain = domain;
	rc = hailo_ops->get_throttling_mode(&params, &info);
	return sprintf(buf, "%s\n", info.ctrl ? "auto" : "manual");
}

static ssize_t ap_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return hailo_throttling_show(SCMI_HAILO_THROTTLING_DOMAIN_AP, dev, attr, buf);
}

static ssize_t nncore_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return hailo_throttling_show(SCMI_HAILO_THROTTLING_DOMAIN_NNCORE, dev, attr, buf);
}

static ssize_t dsp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return hailo_throttling_show(SCMI_HAILO_THROTTLING_DOMAIN_DSP, dev, attr, buf);
}

static ssize_t available_modes_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "auto, manual\n");
}

static ssize_t hailo_throttling_store(enum scmi_hailo_throttling_domain domain, struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct scmi_hailo_set_throttling_mode_a2p params;
	int rc;

	if (sysfs_streq(buf, "auto")) {
		params.ctrl = true; // Enable auto mode
	} else if (sysfs_streq(buf, "manual")) {
		params.ctrl = false; // Disable auto mode
	} else {
		dev_err(dev, "invalid throttling mode (=%s), please read available_modes attribute \n", buf);
		return -EINVAL;
	}
	params.domain = domain;
	rc = hailo_ops->set_throttling_mode(&params);

	return count;
}

static int hailo_soc_fill_identification_attributes_file(struct hailo_identification_attributes_file *identification_attributes_file)
{
	struct scmi_hailo_identification_attributes_p2a identification_attributes;
	int ret;
	ret = hailo_ops->get_identification_attributes(&identification_attributes);
	if (ret) {
		return ret;
	}

	memcpy(identification_attributes_file, &identification_attributes, sizeof(struct scmi_hailo_identification_attributes_p2a));

	return 0;
}

static ssize_t nncore_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return hailo_throttling_store(SCMI_HAILO_THROTTLING_DOMAIN_NNCORE, dev, attr, buf, count);
}

static DEVICE_ATTR_RO(ap);
static DEVICE_ATTR_RW(nncore);
static DEVICE_ATTR_RO(dsp);
static DEVICE_ATTR_RO(available_modes);

static struct attribute *hailo_throttling_mode_attrs[] = {
	&dev_attr_ap.attr,
	&dev_attr_nncore.attr,
	&dev_attr_dsp.attr,
	&dev_attr_available_modes.attr,
	NULL,
};

static const struct attribute_group hailo_throttling_mode_group = { .name = "throttling_mode", .attrs = hailo_throttling_mode_attrs, };

static int hailo_soc_validate_product_id(struct hailo_soc *hailo_soc, const char *machine)
{
	static u32 hailo15_product_ids[] = {
		HAILO_SCMI_PRODUCT_ID__15H,
		HAILO_SCMI_PRODUCT_ID__15M,
	};

	static u32 hailo15l_product_ids[] = {
		HAILO_SCMI_PRODUCT_ID__15L,
	};

	static u32 hailo10h_product_ids[] = {
		HAILO_SCMI_PRODUCT_ID__15H,
		HAILO_SCMI_PRODUCT_ID__10H,
	};

	static u32 hailo12l_product_ids[] = {
		HAILO_SCMI_PRODUCT_ID__12L,
	};

	static const struct {
		const char *machine;
		u32 const *valid_product_ids;
		size_t num_valid_ids;
	} matches[] = {
		{ .machine = "Hailo-15", .valid_product_ids = hailo15_product_ids, .num_valid_ids = ARRAY_SIZE(hailo15_product_ids) },
		{ .machine = "Hailo-15l", .valid_product_ids = hailo15l_product_ids, .num_valid_ids = ARRAY_SIZE(hailo15l_product_ids) },
		{ .machine = "Hailo-10h", .valid_product_ids = hailo10h_product_ids, .num_valid_ids = ARRAY_SIZE(hailo10h_product_ids) },
		{ .machine = "Hailo-12L", .valid_product_ids = hailo12l_product_ids, .num_valid_ids = ARRAY_SIZE(hailo12l_product_ids) },
	};

	size_t i, j;

	for (i = 0; i < ARRAY_SIZE(matches); i++) {
		if (strcmp(matches[i].machine, machine) == 0) {
			for (j = 0; j < matches[i].num_valid_ids; j++) {
				if (hailo_soc->product_id == matches[i].valid_product_ids[j]) {
					return 0; // Valid product ID
				}
			}
			return -EINVAL; // Invalid product ID for this machine
		}
	}

	return -EINVAL; // Machine not found
}

static int hailo_soc_probe(struct platform_device *pdev)
{
	struct soc_device *soc_dev;
	struct device *dev;
	struct soc_device_attribute *soc_dev_attr;
	struct hailo_soc *hailo_soc;
	struct device_node *np = pdev->dev.of_node;
	const char *compat;

	int ret;

	hailo_ops = scmi_hailo_get_ops();
	if (IS_ERR(hailo_ops)) {
		return PTR_ERR(hailo_ops);
	}

	hailo_soc = kzalloc(sizeof(*hailo_soc), GFP_KERNEL);
	if (!hailo_soc)
		return -ENOMEM;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return -ENOMEM;

	if (of_property_read_string(np, "compatible", &compat) != 0) {
		dev_err(&pdev->dev, "Failed to get device compatible\n");
		return -EINVAL;
	}

	hailo_soc->host_current_limit_sent_flag = false;

	soc_dev_attr->family = "Hailo-1x";
	if (strcmp(compat, "hailo,hailo15") == 0)
		soc_dev_attr->machine = "Hailo-15";
	else if (strcmp(compat, "hailo,hailo10h") == 0)
		soc_dev_attr->machine = "Hailo-10h";
	else if (strcmp(compat, "hailo,hailo15l") == 0)
		soc_dev_attr->machine = "Hailo-15l";
	else if (strcmp(compat, "hailo,hailo12l") == 0)
		soc_dev_attr->machine = "Hailo-12L";
	else {
		dev_err(&pdev->dev, "Invalid compatible\n");
		return -EINVAL;
	}

	soc_dev_attr->custom_attr_group = hailo_groups[0];

	ret = hailo_soc_fill_fuse_and_chip_serial_files(&hailo_soc->fuse_file, &hailo_soc->chip_serial_file);
	if (ret) {
		dev_err(&pdev->dev, "Failed to fill fuse info and chip serial number\n");
		return ret;
	}

	ret = hailo_soc_fill_mbist_status_file(&hailo_soc->mbist_status_file);
	if (ret) {
		dev_err(&pdev->dev, "Failed to mbist status\n");
		return ret;
	}

	ret = hailo_soc_fill_identification_attributes_file(&hailo_soc->identification_attributes_file);
	if (ret) {
		dev_err(&pdev->dev, "Failed to fill identification attributes\n");
		return ret;
	}

	ret = hailo_soc_fill_sku_ids(hailo_soc, compat);
	if (ret) {
		dev_err(&pdev->dev, "Failed to fill product id\n");
		return ret;
	}

	ret = hailo_soc_validate_product_id(hailo_soc, soc_dev_attr->machine);
	if (ret) {
		dev_err(&pdev->dev, "Machine %s does not support product id %u\n",
			soc_dev_attr->machine, hailo_soc->product_id);
		return ret;
	}

	ret = hailo_ops->get_boot_info(&hailo_soc->boot_info);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get boot info\n");
		return ret;
	}

	ret = hailo_ops->send_components_version(&hailo_soc->components_version);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get fw versions\n");
		return ret;
	}

	dev_info(&pdev->dev, "scu version %d.%d.%d, uboot version %d.%d.%d, linux version %d.%d.%d\n",
		((hailo_soc->components_version.scu_version >> 24) & 0xff), ((hailo_soc->components_version.scu_version >> 16) & 0xff), (hailo_soc->components_version.scu_version & 0xffff),
		((hailo_soc->components_version.uboot_version >> 24) & 0xff), ((hailo_soc->components_version.uboot_version >> 16) & 0xff), (hailo_soc->components_version.uboot_version & 0xffff),
		((HAILO_LINUX_RELEASE_BUILD_VERSION >> 24) & 0xff), ((HAILO_LINUX_RELEASE_BUILD_VERSION >> 16) & 0xff), (HAILO_LINUX_RELEASE_BUILD_VERSION & 0xffff));

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		kfree(soc_dev_attr);
		return -ENODEV;
	}

	dev = soc_device_to_device(soc_dev);

	// Create attribute group "boot_info" under hailo soc device
	ret = sysfs_create_group(&dev->kobj, &hailo_boot_info_group);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create boot_info group\n");
		return ret;
	}

	// Create attribute group "hailo_versions" under hailo soc device
	ret = sysfs_create_group(&dev->kobj, &hailo_versions_group);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create hailo_versions group\n");
		return ret;
	}

	// Create attribute group "hailo_throttling_mode" under hailo soc device
	ret = sysfs_create_group(&dev->kobj, &hailo_throttling_mode_group);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create hailo_throttling_mode group\n");
		return ret;
	}
	hailo_soc->soc_dev = soc_dev;
	dev_set_drvdata(dev, hailo_soc);

	/* This is relevent only to Asus = USB(flash) + 10H */
	if (hailo_soc->product_id == HAILO_SCMI_PRODUCT_ID__10H &&
        (hailo_soc->boot_info.active_boot_image_storage & H15__SCU_BOOT_BIT_MASK) == BOOT_SOURCE_SPI_FLASH) {
        /* Send boot success indication during probe */
        ret = hailo_soc_send_boot_success_indication(dev);
        if (ret) {
            dev_err(&pdev->dev, "Failed to send boot success indication: %d\n", ret);
        }
    }
	return 0;
}

static int hailo_soc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	soc_device_unregister(hailo_soc->soc_dev);

	return 0;
}

static struct platform_driver hailo_soc_driver = {
	.probe = hailo_soc_probe,
	.remove = hailo_soc_remove,
	.driver = {
		.name = "hailo-soc",
		.of_match_table = hailo_soc_of_match,
	},
};
builtin_platform_driver(hailo_soc_driver);
