#include <linux/types.h>
#include <linux/scmi_protocol.h>
#include <linux/soc/hailo/scmi_hailo_ops.h>

#include "scmi_hailo.h"

extern struct scmi_hailo_proto_ops *hailo_ops;
extern struct scmi_notify_ops *hailo_notify_ops;
extern struct scmi_protocol_handle *ph;

/* Hailo SCMI command definitions */

static int scmi_hailo_register_notifier(struct scmi_device *sdev, u8 evt_id, struct notifier_block *nb);
static int scmi_hailo_get_scu_boot_source(u8 *boot_source);
static int scmi_hailo_set_eth_rmii(void);
static int scmi_hailo_start_measure(struct scmi_hailo_ddr_start_measure_a2p *params);
static int scmi_hailo_stop_measure(void);

const struct scmi_hailo_ops ops = {
	.register_notifier = scmi_hailo_register_notifier,
	.get_scu_boot_source = scmi_hailo_get_scu_boot_source,
	.set_eth_rmii = scmi_hailo_set_eth_rmii,
	.start_measure = scmi_hailo_start_measure,
	.stop_measure = scmi_hailo_stop_measure,
};

static int scmi_hailo_register_notifier(struct scmi_device *sdev, u8 evt_id, struct notifier_block *nb)
{
	/* We don't use event sources */
	u32 src_id;

	if (!hailo_notify_ops || !hailo_notify_ops->devm_event_notifier_register)
		return -EOPNOTSUPP;


	return hailo_notify_ops->devm_event_notifier_register(sdev, SCMI_PROTOCOL_HAILO, evt_id, &src_id, nb);
}

const struct scmi_hailo_ops *scmi_hailo_get_ops(void)
{
	if (NULL == hailo_ops || NULL == ph)  {
		/* Module is not initialized */
		return NULL;
	}

	return &ops;
}
EXPORT_SYMBOL_GPL(scmi_hailo_get_ops);

/* Hailo SCMI command implementations */

static int scmi_hailo_get_scu_boot_source(u8 *boot_source)
{
	int ret;
	struct scmi_hailo_get_boot_info_p2a boot_info = {0};

	ret = hailo_ops->get_boot_info(ph, (void *)&boot_info);
	if (ret) {
		return ret;
	}
	*boot_source = boot_info.scu_bootstrap;
	return 0;
}

static int scmi_hailo_set_eth_rmii(void)
{
	return hailo_ops->set_eth_rmii(ph);
}

static int scmi_hailo_start_measure(struct scmi_hailo_ddr_start_measure_a2p *params)
{
	return hailo_ops->start_measure(ph, params);
}

static int scmi_hailo_stop_measure(void)
{
	return hailo_ops->stop_measure(ph);
}
