#include <linux/errno.h>
#include <linux/types.h>
#include <linux/scmi_protocol.h>
#include <linux/soc/hailo/scmi_hailo_ops.h>

#include "scmi_hailo.h"

extern struct scmi_hailo_proto_ops *hailo_ops;
extern struct scmi_notify_ops *hailo_notify_ops;
extern struct scmi_protocol_handle *ph;

static struct scmi_device *hailo_sdev = NULL;

/* Hailo SCMI command definitions */

static int scmi_hailo_register_notifier(u8 evt_id, struct notifier_block *nb);
static int scmi_hailo_set_eth_rmii(void);
static int scmi_hailo_start_measure(struct scmi_hailo_ddr_start_measure_a2p *params);
static int scmi_hailo_stop_measure(bool *was_running);
static int scmi_hailo_get_boot_info(struct scmi_hailo_get_boot_info_p2a *boot_info);
static int scmi_hailo_get_fuse_info(struct scmi_hailo_get_fuse_info_p2a *fuse_info);

const struct scmi_hailo_ops ops = {
	.register_notifier = scmi_hailo_register_notifier,
	.get_boot_info = scmi_hailo_get_boot_info,
	.get_fuse_info = scmi_hailo_get_fuse_info,
	.set_eth_rmii = scmi_hailo_set_eth_rmii,
	.start_measure = scmi_hailo_start_measure,
	.stop_measure = scmi_hailo_stop_measure,
};

static int scmi_hailo_register_notifier(u8 evt_id, struct notifier_block *nb)
{
	if (!hailo_notify_ops || !hailo_notify_ops->devm_event_notifier_register)
		return -EOPNOTSUPP;

	return hailo_notify_ops->devm_event_notifier_register(hailo_sdev, SCMI_PROTOCOL_HAILO, evt_id, NULL, nb);
}

const struct scmi_hailo_ops *scmi_hailo_get_ops(void)
{
	if (NULL == hailo_ops || NULL == ph)  {
		/* Module is not initialized */
		return ERR_PTR(-EPROBE_DEFER);
	}

	return &ops;
}
EXPORT_SYMBOL_GPL(scmi_hailo_get_ops);

/* Hailo SCMI command implementations */

static int scmi_hailo_set_eth_rmii(void)
{
	return hailo_ops->set_eth_rmii(ph);
}

static int scmi_hailo_start_measure(struct scmi_hailo_ddr_start_measure_a2p *params)
{
	return hailo_ops->start_measure(ph, params);
}

static int scmi_hailo_stop_measure(bool *was_running)
{
	int result;
	struct scmi_hailo_ddr_stop_measure_p2a output;
	result = hailo_ops->stop_measure(ph, &output);
	*was_running = output.was_running;
	return result;
}

static int scmi_hailo_get_boot_info(struct scmi_hailo_get_boot_info_p2a *boot_info)
{
	return hailo_ops->get_boot_info(ph, boot_info);
}

static int scmi_hailo_get_fuse_info(struct scmi_hailo_get_fuse_info_p2a *fuse_info)
{
	return hailo_ops->get_fuse_info(ph, fuse_info);
}

int scmi_hailo_ops_init(struct scmi_device *sdev)
{
	hailo_sdev = sdev;
	return 0;
}