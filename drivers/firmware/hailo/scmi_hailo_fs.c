#include <linux/kernel.h>
#include <linux/scmi_protocol.h>

#include "scmi_hailo.h"

#define HAILO_FUSE_FILE_LOT_ID_SIZE (8)

struct __attribute__((packed)) hailo_fuse_file {
	/* HailoRT info */
	u32 invalid_clusters;

	/* ULT info */
	u8 lot_id[HAILO_FUSE_FILE_LOT_ID_SIZE];
	u8 wafer_id;
	u8 x_location;
	u8 y_location;

	u8 sku;
	u32 active_clusters;
};

extern struct scmi_hailo_proto_ops *hailo_ops;
extern struct scmi_protocol_handle *ph;

static ssize_t scmi_hailo_fuses_info_file_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct scmi_hailo_get_fuse_info_p2a fuse_info;
	struct hailo_fuse_file *file_content = (struct hailo_fuse_file *)buf;

	if (hailo_ops->get_fuses_info(ph, &fuse_info)) {
		return -EIO;
	}

	file_content->invalid_clusters =
		(fuse_info.user_fuse.misc_bits >> HAILO_USER_FUSE_BINNING_SHIFT) &
		HAILO_USER_FUSE_BINNING_MASK;
	memcpy(file_content->lot_id, (void *)fuse_info.user_fuse.lot_id,
	       sizeof(file_content->lot_id));
	file_content->wafer_id = fuse_info.user_fuse.wafer_info.wafer_id;
	file_content->x_location = fuse_info.user_fuse.wafer_info.x_location;
	file_content->y_location = fuse_info.user_fuse.wafer_info.y_location;
	file_content->sku = (fuse_info.user_fuse.misc_bits >> HAILO_USER_FUSE_SKU_SHIFT) &
			    HAILO_USER_FUSE_SKU_MASK;
	file_content->active_clusters = fuse_info.active_clusters;

	return sizeof(*file_content);
}

static ssize_t scmi_hailo_boot_info_file_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct scmi_hailo_get_boot_info_p2a boot_info = { 0 };

	if (hailo_ops->get_boot_info(ph, &boot_info)) {
		return -EIO;
	}

	memcpy(buf, &boot_info, sizeof(boot_info));

	return sizeof(boot_info);
}

struct device_attribute dev_attr_scmi_hailo_fuses_info_file = {
	.attr = { .name = "fuse", .mode = VERIFY_OCTAL_PERMISSIONS(0444) },
	.show = scmi_hailo_fuses_info_file_show,
	.store = NULL
};

struct device_attribute dev_attr_scmi_hailo_boot_info_file = {
	.attr = { .name = "boot", .mode = VERIFY_OCTAL_PERMISSIONS(0444) },
	.show = scmi_hailo_boot_info_file_show,
	.store = NULL
};

static int scmi_hailo_fuses_info_file_create(struct device *dev)
{
	return device_create_file(dev, &dev_attr_scmi_hailo_fuses_info_file);
}

static void scmi_hailo_fuses_info_file_remove(struct device *dev)
{
	device_remove_file(dev, &dev_attr_scmi_hailo_fuses_info_file);
}

static int scmi_hailo_boot_info_file_create(struct device *dev)
{
	return device_create_file(dev, &dev_attr_scmi_hailo_boot_info_file);
}

static void scmi_hailo_boot_info_file_remove(struct device *dev)
{
	device_remove_file(dev, &dev_attr_scmi_hailo_boot_info_file);
}

int scmi_hailo_fs_init(struct scmi_device *sdev)
{
	int ret;

	ret = scmi_hailo_boot_info_file_create(sdev->dev.parent->parent);
	if (ret) {
		return ret;
	}

	ret = scmi_hailo_fuses_info_file_create(sdev->dev.parent->parent);
	if (ret) {
		return ret;
	}

	return 0;
}

void scmi_hailo_fs_fini(struct scmi_device *sdev)
{
	scmi_hailo_fuses_info_file_remove(sdev->dev.parent->parent);
	scmi_hailo_boot_info_file_remove(sdev->dev.parent->parent);
}