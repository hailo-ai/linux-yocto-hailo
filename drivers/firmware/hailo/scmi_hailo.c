#include <linux/scmi_protocol.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/soc/hailo/scmi_hailo_protocol.h>

#include "scmi_hailo.h"

struct scmi_hailo_proto_ops *hailo_ops = NULL;
struct scmi_notify_ops *hailo_notify_ops = NULL;
struct scmi_protocol_handle *ph = NULL;

static int scmi_hailo_probe(struct scmi_device *sdev)
{
	struct scmi_handle *handle = sdev->handle;
	struct scmi_protocol_handle *proto_handle;
	const struct scmi_hailo_proto_ops *proto_ops;
	int ret;

	if (!handle)
		return -ENODEV;

	proto_ops = handle->devm_protocol_get(sdev, SCMI_PROTOCOL_HAILO, &proto_handle);
	if (IS_ERR(proto_ops))
		return PTR_ERR(proto_ops);

	/* Assign global ph and hailo_ops after we validate ops */
	ph = proto_handle;
	hailo_ops = (struct scmi_hailo_proto_ops *)proto_ops;

	hailo_notify_ops = (struct scmi_notify_ops *)handle->notify_ops;

	ret = scmi_hailo_fs_init(sdev);
	if (ret) {
		pr_err("Failed to create fuse file\n");
		return ret;
	}

	return 0;
}

static void scmi_hailo_remove(struct scmi_device *sdev)
{
    const struct scmi_handle *handle = sdev->handle;

	scmi_hailo_fs_fini(sdev);

	/* Remove global ph and hailo_ops so that they won't be used via ops */
	hailo_ops = NULL;
	hailo_notify_ops = NULL;
	ph = NULL;

    handle->devm_protocol_put(sdev, SCMI_PROTOCOL_HAILO);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_HAILO, "scmi-hailo" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);


static struct scmi_driver scmi_hailo_driver = {
	.name = "scmi-hailo",
	.probe = scmi_hailo_probe,
    .remove = scmi_hailo_remove,
	.id_table = scmi_id_table,
};

module_scmi_driver(scmi_hailo_driver);