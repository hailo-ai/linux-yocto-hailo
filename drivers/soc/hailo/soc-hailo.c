#include <linux/init.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/soc/hailo/scmi_hailo_ops.h>

static const struct of_device_id hailo_soc_of_match[] = {
	{ .compatible = "hailo,hailo15" },
	{}
};

struct __attribute__((packed)) hailo_fuse_file {
	u8 user_fuse_array[SCMI_HAILO_PROTOCOL_USER_FUSE_DATA_SIZE];
	u32 active_clusters;
};

struct hailo_soc {
	struct hailo_fuse_file fuse_file;
	struct scmi_hailo_get_boot_info_p2a boot_info;
	struct kernfs_node *fuse_kn;
	struct soc_device *soc_dev;
};

static ssize_t qspi_flash_ab_offset_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	return sprintf(buf, "0x%08X\n",
		       hailo_soc->boot_info.qspi_flash_ab_offset);
}

static DEVICE_ATTR_RO(qspi_flash_ab_offset);

static ssize_t fuse_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct hailo_soc *hailo_soc = dev_get_drvdata(dev);

	memcpy(buf, &hailo_soc->fuse_file, sizeof(hailo_soc->fuse_file));
	return sizeof(hailo_soc->fuse_file);
}

static DEVICE_ATTR_RO(fuse);

static struct attribute *hailo_attrs[] = { &dev_attr_qspi_flash_ab_offset.attr,
					   &dev_attr_fuse.attr, NULL };

ATTRIBUTE_GROUPS(hailo);

static int hailo_soc_fill_fuse_file(const struct scmi_hailo_ops *hailo_ops,
				    struct hailo_fuse_file *fuse_file)
{
	struct scmi_hailo_get_fuse_info_p2a fuse_info;
	int ret;

	ret = hailo_ops->get_fuse_info(&fuse_info);
	if (ret) {
		return ret;
	}

	memcpy(&fuse_file->user_fuse_array, &fuse_info.user_fuse, sizeof(struct scmi_hailo_user_fuse));

	fuse_file->active_clusters = fuse_info.active_clusters;

	return 0;
}

static int hailo_soc_probe(struct platform_device *pdev)
{
	struct soc_device *soc_dev;
	struct device *dev;
	struct soc_device_attribute *soc_dev_attr;
	struct hailo_soc *hailo_soc;
	const struct scmi_hailo_ops *hailo_ops;
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

	soc_dev_attr->family = "Hailo15";
	soc_dev_attr->custom_attr_group = hailo_groups[0];

	ret = hailo_soc_fill_fuse_file(hailo_ops, &hailo_soc->fuse_file);
	if (ret) {
		dev_err(&pdev->dev, "Failed to fill fuse info\n");
		return ret;
	}

	ret = hailo_ops->get_boot_info(&hailo_soc->boot_info);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get boot info\n");
		return ret;
	}

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		kfree(soc_dev_attr);
		return -ENODEV;
	}

	dev = soc_device_to_device(soc_dev);

	hailo_soc->soc_dev = soc_dev;
	dev_set_drvdata(dev, hailo_soc);

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
