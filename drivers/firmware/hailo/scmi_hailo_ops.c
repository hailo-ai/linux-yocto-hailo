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
static int scmi_hailo_unregister_notifier(u8 evt_id, struct notifier_block *nb);
static int scmi_hailo_set_eth_rmii(void);
static int scmi_hailo_start_measure(struct scmi_hailo_noc_start_measure_a2p *params);
static int scmi_hailo_get_mbist_subservers_status(struct scmi_hailo_mbist_subservers_status_p2a *params);
static int scmi_hailo_stop_measure(bool *was_running);
static int scmi_hailo_get_boot_info(struct scmi_hailo_get_boot_info_p2a *boot_info);
static int scmi_hailo_get_fuse_info(struct scmi_hailo_get_fuse_info_p2a *fuse_info);
static int scmi_hailo_send_boot_success_ind(struct scmi_hailo_boot_success_indication_a2p *params);
static int scmi_hailo_send_swupdate_ind(void);
static int scmi_hailo_send_send_components_version(struct scmi_hailo_send_components_version_p2a *info);
static int scmi_hailo_get_jtag_selector(u8 *jtag_selector);
static int scmi_hailo_set_jtag_selector(u8 jtag_selector);
static int scmi_hailo_set_i2s_source_clk(struct scmi_hailo_set_i2s_source_clock_a2p *params);
static int scmi_hailo_set_spi_interrupt_forwarding(struct scmi_hailo_set_spi_interrupt_forwarding_a2p *params);
static int scmi_hailo_set_throttling_mode(struct scmi_hailo_set_throttling_mode_a2p *params);
static int scmi_hailo_get_throttling_mode(struct scmi_hailo_get_throttling_mode_a2p *params, struct scmi_hailo_get_throttling_mode_p2a *info);
static int scmi_hailo_set_source_clock(struct scmi_hailo_set_source_clock_a2p *params);
static int scmi_hailo_get_source_clock(struct scmi_hailo_get_source_clock_a2p *params, struct scmi_hailo_get_source_clock_p2a *info);
static int scmi_hailo_get_identification_attributes(struct scmi_hailo_identification_attributes_p2a *params);
static int scmi_hailo_get_sku_id(struct scmi_hailo_get_sku_id_p2a *params);

const struct scmi_hailo_ops ops = {
	.register_notifier = scmi_hailo_register_notifier,
	.unregister_notifier = scmi_hailo_unregister_notifier,
	.get_boot_info = scmi_hailo_get_boot_info,
	.get_fuse_info = scmi_hailo_get_fuse_info,
	.set_eth_rmii = scmi_hailo_set_eth_rmii,
	.start_measure = scmi_hailo_start_measure,
	.get_mbist_subservers_status = scmi_hailo_get_mbist_subservers_status,
	.stop_measure = scmi_hailo_stop_measure,
	.send_boot_success_ind = scmi_hailo_send_boot_success_ind,
	.send_swupdate_ind = scmi_hailo_send_swupdate_ind,
	.send_components_version = scmi_hailo_send_send_components_version,
	.get_jtag_selector = scmi_hailo_get_jtag_selector,
	.set_jtag_selector = scmi_hailo_set_jtag_selector,
	.set_i2s_source_clk = scmi_hailo_set_i2s_source_clk,
	.set_spi_interrupt_forwarding = scmi_hailo_set_spi_interrupt_forwarding,
	.set_throttling_mode = scmi_hailo_set_throttling_mode,
	.get_throttling_mode = scmi_hailo_get_throttling_mode,
	.set_source_clock = scmi_hailo_set_source_clock,
	.get_source_clock = scmi_hailo_get_source_clock,
	.get_identification_attributes = scmi_hailo_get_identification_attributes,
	.get_sku_id = scmi_hailo_get_sku_id,
};

static int scmi_hailo_register_notifier(u8 evt_id, struct notifier_block *nb)
{
	if (!hailo_notify_ops || !hailo_notify_ops->devm_event_notifier_register)
		return -EOPNOTSUPP;

	return hailo_notify_ops->devm_event_notifier_register(hailo_sdev, SCMI_PROTOCOL_HAILO, evt_id, NULL, nb);
}

static int scmi_hailo_unregister_notifier(u8 evt_id, struct notifier_block *nb)
{
	if (!hailo_notify_ops || !hailo_notify_ops->devm_event_notifier_unregister)
		return -EOPNOTSUPP;

	return hailo_notify_ops->devm_event_notifier_unregister(hailo_sdev, SCMI_PROTOCOL_HAILO, evt_id, NULL, nb);
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

static int scmi_hailo_start_measure(struct scmi_hailo_noc_start_measure_a2p *params)
{
	return hailo_ops->start_measure(ph, params);
}

static int scmi_hailo_get_mbist_subservers_status(struct scmi_hailo_mbist_subservers_status_p2a *mbist_subservers_status)
{
	return hailo_ops->get_mbist_subservers_status(ph, mbist_subservers_status);
}

static int scmi_hailo_stop_measure(bool *was_running)
{
	int result;
	struct scmi_hailo_noc_stop_measure_p2a output;
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

static int scmi_hailo_send_boot_success_ind(struct scmi_hailo_boot_success_indication_a2p *params)
{
	return hailo_ops->send_boot_success_ind(ph, params);
}

static int scmi_hailo_send_swupdate_ind(void)
{
	return hailo_ops->send_swupdate_ind(ph);
}

static int scmi_hailo_send_send_components_version(struct scmi_hailo_send_components_version_p2a *info)
{
	return hailo_ops->send_components_version(ph, info);
}

static int scmi_hailo_get_jtag_selector(u8 *jtag_selector)
{
	struct scmi_hailo_get_jtag_p2a info;
	int ret;

	ret = hailo_ops->get_jtag_selector(ph, &info);
	if (ret)
		return ret;

	*jtag_selector = info.value;
	return 0;
}

static int scmi_hailo_set_jtag_selector(u8 jtag_selector)
{
	struct scmi_hailo_set_jtag_a2p params = {
		.value = jtag_selector,
	};

	return hailo_ops->set_jtag_selector(ph, &params);
}

static int scmi_hailo_set_i2s_source_clk(struct scmi_hailo_set_i2s_source_clock_a2p *params)
{
	return hailo_ops->set_i2s_source_clk(ph, params);
}

static int scmi_hailo_set_spi_interrupt_forwarding(struct scmi_hailo_set_spi_interrupt_forwarding_a2p *params)
{
	return hailo_ops->set_spi_interrupt_forwarding(ph, params);
}

static int scmi_hailo_set_throttling_mode(struct scmi_hailo_set_throttling_mode_a2p *params)
{
	return hailo_ops->set_throttling_mode(ph, params);
}

static int scmi_hailo_get_throttling_mode(struct scmi_hailo_get_throttling_mode_a2p *params, struct scmi_hailo_get_throttling_mode_p2a  *info)
{
	return hailo_ops->get_throttling_mode(ph, params, info);
}

static int scmi_hailo_set_source_clock(struct scmi_hailo_set_source_clock_a2p *params)
{
	return hailo_ops->set_source_clock(ph, params);
}

static int scmi_hailo_get_source_clock(struct scmi_hailo_get_source_clock_a2p *params, struct scmi_hailo_get_source_clock_p2a *info)
{
	return hailo_ops->get_source_clock(ph, params, info);
}

static int scmi_hailo_get_identification_attributes(struct scmi_hailo_identification_attributes_p2a *params)
{
	return hailo_ops->get_identification_attributes(ph, params);
}

static int scmi_hailo_get_sku_id(struct scmi_hailo_get_sku_id_p2a *params)
{
	return hailo_ops->get_sku_id(ph, params);
}

int scmi_hailo_ops_init(struct scmi_device *sdev)
{
	hailo_sdev = sdev;
	return 0;
}