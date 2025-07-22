#ifndef __SCMI_HAILO_OPS_H
#define __SCMI_HAILO_OPS_H

#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/types.h>
#include <linux/kconfig.h>

#include <linux/scmi_protocol.h>
#include <linux/soc/hailo/scmi_hailo_protocol.h>

struct scmi_hailo_ops {
    int (*register_notifier)(u8 evt_id, struct notifier_block *nb);
    int (*get_boot_info)(struct scmi_hailo_get_boot_info_p2a *boot_info);
    int (*get_fuse_info)(struct scmi_hailo_get_fuse_info_p2a* fuse_info);
    int (*set_eth_rmii)(void);
    int (*start_measure)(struct scmi_hailo_noc_start_measure_a2p *params);
    int (*get_mbist_subservers_status)(struct scmi_hailo_mbist_subservers_status_p2a *params);
    int (*stop_measure)(bool *was_running);
    int (*send_boot_success_ind)(struct scmi_hailo_boot_success_indication_a2p *params);
	int (*send_swupdate_ind)(void);
    int (*send_components_version)(struct scmi_hailo_send_components_version_p2a *info);
	int (*set_i2s_source_clk)(struct scmi_hailo_set_i2s_source_clock_a2p *params);
    int (*set_spi_interrupt_forwarding)(struct scmi_hailo_set_spi_interrupt_forwarding_a2p *params);
};

#if IS_ENABLED(CONFIG_HAILO_SCMI_PROTOCOL)
const struct scmi_hailo_ops *scmi_hailo_get_ops(void);
#else
static inline const struct scmi_hailo_ops *scmi_hailo_get_ops(void)
{
    return ERR_PTR(-EINVAL);
}
#endif /* IS_ENABLED(CONFIG_HAILO_SCMI_PROTOCOL) */

#endif /* __SCMI_HAILO_OPS_H */
