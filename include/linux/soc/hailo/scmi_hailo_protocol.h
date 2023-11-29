#ifndef __HAILO_PROTOCOL_H
#define __HAILO_PROTOCOL_H

#include <linux/err.h>
#include <linux/types.h>
#include <linux/kconfig.h>

struct scmi_hailo_ops {
    int (*get_scu_boot_source)(u8 *boot_source);
    int (*set_eth_rmii)(void);
};

#if IS_ENABLED(CONFIG_HAILO_SCMI_PROTOCOL)
const struct scmi_hailo_ops *scmi_hailo_get_ops(void);
#else
static inline const struct scmi_hailo_ops *scmi_hailo_get_ops(void)
{
    return ERR_PTR(-EINVAL);
}
#endif /* IS_ENABLED(CONFIG_HAILO_SCMI_PROTOCOL) */

#endif /* __HAILO_PROTOCOL_H */
