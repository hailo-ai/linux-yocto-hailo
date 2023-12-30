#include <linux/kernel.h>
#include <linux/scmi_protocol.h>

#include "scmi_hailo.h"

#define HAILO_FUSE_FILE_LOT_ID_SIZE (8)

struct __attribute__((packed)) hailo_fuse_file {
	/* HailoRT info */
	u32 active_clusters;

	/* ULT info */
	u8 lot_id[HAILO_FUSE_FILE_LOT_ID_SIZE];
	u8 wafer_id;
	u8 x_location;
	u8 y_location;

	u8 sku;
};

extern struct scmi_hailo_proto_ops *hailo_ops;
extern struct scmi_protocol_handle *ph;

static ssize_t scmi_hailo_fuses_info_file_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	hailo_user_fuse_t user_fuse;
	struct hailo_fuse_file *file_content = (struct hailo_fuse_file *)buf;
	
	if (hailo_ops->get_fuses_info(ph, (u8 *)&user_fuse)) {
		return -EIO;
	}

	file_content->active_clusters = (user_fuse.misc_bits >> HAILO_USER_FUSE_BINNING_SHIFT) & HAILO_USER_FUSE_BINNING_MASK;
	memcpy(file_content->lot_id, (void *)user_fuse.lot_id, sizeof(file_content->lot_id));
	file_content->wafer_id = user_fuse.wafer_info.wafer_id;
	file_content->x_location = user_fuse.wafer_info.x_location;
	file_content->y_location = user_fuse.wafer_info.y_location;
	file_content->sku = (user_fuse.misc_bits >> HAILO_USER_FUSE_SKU_SHIFT) & HAILO_USER_FUSE_SKU_MASK;

	return sizeof(*file_content);
}

struct device_attribute dev_attr_scmi_hailo_fuses_info_file = { 
	.attr = {.name = "fuse", .mode = VERIFY_OCTAL_PERMISSIONS(0444) },
	.show = scmi_hailo_fuses_info_file_show,
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

int scmi_hailo_fs_init(struct scmi_device *sdev)
{
    return scmi_hailo_fuses_info_file_create(sdev->dev.parent->parent);
}

void scmi_hailo_fs_fini(struct scmi_device *sdev)
{
    scmi_hailo_fuses_info_file_remove(sdev->dev.parent->parent);
}