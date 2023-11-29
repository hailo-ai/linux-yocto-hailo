#include <linux/module.h>
#include <linux/scmi_protocol.h>
#include <dt-bindings/soc/hailo15_scmi_api.h>
#include "common.h"

static int scmi_hailo_xfer(const struct scmi_protocol_handle *ph, unsigned int cmd_id, const void *request_buffer, size_t request_size, void *response_buffer, size_t response_size)
{
	struct scmi_xfer *transaction_info;
	int ret;

	ret = ph->xops->xfer_get_init(ph, cmd_id, request_size, response_size, &transaction_info);
	if (ret) {
		dev_err(ph->dev, "SCMI Hailo protocol: init message failed");
		return ret;
	}

	if (request_size > 0) {
		if (request_buffer == NULL)
			return -EINVAL;
		memcpy(transaction_info->tx.buf, request_buffer, request_size);
	}

	ret = ph->xops->do_xfer(ph, transaction_info);
	if (ret) {
		dev_err(ph->dev, "SCMI Hailo protocol: message transfer failed");
		return ret;
	}

	if (response_size > 0) {
		if (response_buffer == NULL)
			return -EINVAL;
		memcpy(response_buffer, transaction_info->rx.buf, response_size);
	}
	
	ph->xops->xfer_put(ph, transaction_info);

	return ret;
}

static int scmi_hailo_get_scu_boot_source(const struct scmi_protocol_handle *ph, u8 *boot_source)
{
	int ret = scmi_hailo_xfer(ph, HAILO15_SCMI_HAILO_GET_SCU_BOOT_SOURCE_ID, NULL, 0, boot_source, sizeof(*boot_source));
	return ret;
}

static int scmi_hailo_get_fuses_info(const struct scmi_protocol_handle *ph, u8 info[SCMI_HAILO_FUSE_INFO_SIZE])
{
	int ret = scmi_hailo_xfer(ph, HAILO15_SCMI_HAILO_GET_FUSE_INFO_ID, NULL, 0, info, SCMI_HAILO_FUSE_INFO_SIZE);
	return ret;
}

static int scmi_hailo_set_eth_rmii(const struct scmi_protocol_handle *ph)
{
	int ret = scmi_hailo_xfer(ph, HAILO15_SCMI_HAILO_SET_ETH_RMII_MODE, NULL, 0, NULL, 0);
	return ret;
}

static const struct scmi_hailo_proto_ops hailo_proto_ops = {
	.get_scu_boot_source = scmi_hailo_get_scu_boot_source,
	.get_fuses_info = scmi_hailo_get_fuses_info,
	.set_eth_rmii = scmi_hailo_set_eth_rmii,
};

static int scmi_hailo_protocol_init(const struct scmi_protocol_handle *ph)
{
	u32 version;

	ph->xops->version_get(ph, &version);

	if (version != SCU_FW_BUILD_VERSION) {
		dev_err(ph->dev, "Hailo Protocol version mismatch! Expected: %x but received received %x", SCU_FW_BUILD_VERSION, version);
		return -EINVAL;
	}

	dev_info(ph->dev, "Hailo Protocol Version %d.%d.%d\n",
		SCU_FW_MAJOR, SCU_FW_MINOR, SCU_FW_REVISION);

	return ph->set_priv(ph, NULL);
}

static const struct scmi_protocol scmi_hailo = {
	.id = SCMI_PROTOCOL_HAILO,
	.owner = THIS_MODULE,
	.instance_init = &scmi_hailo_protocol_init,
	.ops = &hailo_proto_ops,
	.events = NULL,
};

DEFINE_SCMI_PROTOCOL_REGISTER_UNREGISTER(hailo, scmi_hailo)