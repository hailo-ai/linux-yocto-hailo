#ifndef __SCMI_HAILO_OPS_H
#define __SCMI_HAILO_OPS_H

#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/types.h>
#include <linux/kconfig.h>

#include <linux/scmi_protocol.h>

struct scmi_hailo_ops {
    int (*register_notifier)(struct scmi_device *sdev, u8 evt_id, struct notifier_block *nb);
    int (*get_scu_boot_source)(u8 *boot_source);
    int (*set_eth_rmii)(void);
    int (*start_measure)(struct scmi_hailo_ddr_start_measure_a2p *params);
    int (*stop_measure)(void);
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
