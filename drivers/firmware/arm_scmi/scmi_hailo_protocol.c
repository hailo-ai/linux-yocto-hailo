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

static int scmi_hailo_get_boot_info(const struct scmi_protocol_handle *ph, struct scmi_hailo_get_boot_info_p2a *info)
{
	int ret = scmi_hailo_xfer(ph, SCMI_HAILO_GET_BOOT_INFO_ID, NULL, 0, info, sizeof(*info));
	return ret;
}

static int scmi_hailo_get_fuses_info(const struct scmi_protocol_handle *ph, struct scmi_hailo_get_fuse_info_p2a *info)
{
	int ret = scmi_hailo_xfer(ph, SCMI_HAILO_GET_FUSE_INFO_ID, NULL, 0, info, sizeof(*info));
	return ret;
}

static int scmi_hailo_set_eth_rmii(const struct scmi_protocol_handle *ph)
{
	int ret = scmi_hailo_xfer(ph, SCMI_HAILO_SET_ETH_RMII_MODE_ID, NULL, 0, NULL, 0);
	return ret;
}
static int scmi_hailo_start_measure(const struct scmi_protocol_handle *ph, struct scmi_hailo_ddr_start_measure_a2p *params)
{
	int ret = scmi_hailo_xfer(ph, SCMI_HAILO_DDR_START_MEASURE_ID, params, sizeof(*params), NULL, 0);
	return ret;
}

static int scmi_hailo_stop_measure(const struct scmi_protocol_handle *ph)
{
	int ret = scmi_hailo_xfer(ph, SCMI_HAILO_DDR_STOP_MEASURE_ID, NULL, 0, NULL, 0);
	return ret;
}

static const struct scmi_hailo_proto_ops hailo_proto_ops = {
	.get_boot_info = scmi_hailo_get_boot_info,
	.get_fuses_info = scmi_hailo_get_fuses_info,
	.set_eth_rmii = scmi_hailo_set_eth_rmii,
	.start_measure = scmi_hailo_start_measure,
	.stop_measure = scmi_hailo_stop_measure,
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

static int hailo_scmi_get_num_sources(const struct scmi_protocol_handle *ph)
{
	return 1;
}

static int hailo_scmi_set_notify_enabled(const struct scmi_protocol_handle *ph, u8 evt_id, u32 src_id, bool enabled)
{
	return 0;
}

// Define function prototypes for each event handler
typedef void* (*scmi_hailo_notification_handler)(const struct scmi_protocol_handle*, u8, ktime_t, const void*, size_t, void*, u32*);

// Define event handler functions
static void* handle_ddr_measurement_trigger(const struct scmi_protocol_handle* ph, u8 evt_id, ktime_t timestamp,
											const void* payld, size_t payld_sz, void* report, u32* src_id)
{
	const struct scmi_hailo_ddr_measurement_trigger_notification *trigger_p = payld;
	struct scmi_hailo_ddr_measurement_trigger_notification *trigger_r = report;

	if (sizeof(*trigger_p) != payld_sz)
		return NULL;

	trigger_r->sample_index = le16_to_cpu(trigger_p->sample_index);

	return trigger_r;

}

static void* handle_ddr_measurement_ended(const struct scmi_protocol_handle* ph, u8 evt_id, ktime_t timestamp,
										  const void* payld, size_t payld_sz, void* report, u32* src_id)
{
	const struct scmi_hailo_ddr_measurement_ended_notification *ended_p = payld;
	struct scmi_hailo_ddr_measurement_ended_notification *ended_r = report;

	if (sizeof(*ended_p) != payld_sz)
		return NULL;

	ended_r->sample_start_index = le16_to_cpu(ended_p->sample_start_index);
	ended_r->sample_end_index = le16_to_cpu(ended_p->sample_end_index);

	return ended_r;
}

static const scmi_hailo_notification_handler eventHandlers[SCMI_HAILO_NOTIFICATION_COUNT] = {
	[SCMI_HAILO_DDR_MEASUREMENT_TRIGGER_NOTIFICATION_ID] = handle_ddr_measurement_trigger,
	[SCMI_HAILO_DDR_MEASUREMENT_ENDED_NOTIFICATION_ID] = handle_ddr_measurement_ended,
};

static void* hailo_scmi_fill_custom_report(const struct scmi_protocol_handle* ph, u8 evt_id, ktime_t timestamp,
										   const void* payld, size_t payld_sz, void* report, u32* src_id)
{
	report = NULL;

	if (src_id == NULL)
		return NULL;

	*src_id = 0;

	if (evt_id >= SCMI_HAILO_NOTIFICATION_COUNT || eventHandlers[evt_id] == NULL) {
		/* Invalid event ID */
		return NULL;
	}

	/* Call the corresponding event handler function based on the event ID */
	return eventHandlers[evt_id](ph, evt_id, timestamp, payld, payld_sz, report, src_id);
}

static const struct scmi_event events[] = {
	{
		.id = SCMI_HAILO_DDR_MEASUREMENT_TRIGGER_NOTIFICATION_ID,
		.max_payld_sz = sizeof(struct scmi_hailo_ddr_measurement_trigger_notification),
		.max_report_sz = sizeof(struct scmi_hailo_ddr_measurement_trigger_notification),
	},
	{
		.id = SCMI_HAILO_DDR_MEASUREMENT_ENDED_NOTIFICATION_ID,
		.max_payld_sz = sizeof(struct scmi_hailo_ddr_measurement_ended_notification),
		.max_report_sz = sizeof(struct scmi_hailo_ddr_measurement_ended_notification),
	},
};

static const struct scmi_event_ops events_ops = {
	.get_num_sources = hailo_scmi_get_num_sources,
	.set_notify_enabled = hailo_scmi_set_notify_enabled,
	.fill_custom_report = hailo_scmi_fill_custom_report,
};

static const struct scmi_protocol_events hailo_protocol_events = {
	.queue_sz = SCMI_PROTO_QUEUE_SZ,
	.ops = &events_ops,
	.evts = (struct scmi_event *)events,
	.num_events = SCMI_HAILO_NOTIFICATION_COUNT,
};

static const struct scmi_protocol scmi_hailo = {
	.id = SCMI_PROTOCOL_HAILO,
	.owner = THIS_MODULE,
	.instance_init = &scmi_hailo_protocol_init,
	.ops = &hailo_proto_ops,
	.events = &hailo_protocol_events,
};

DEFINE_SCMI_PROTOCOL_REGISTER_UNREGISTER(hailo, scmi_hailo)