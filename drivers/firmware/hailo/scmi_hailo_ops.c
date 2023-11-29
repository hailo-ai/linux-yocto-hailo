#include <linux/types.h>
#include <linux/scmi_protocol.h>
#include <linux/soc/hailo/scmi_hailo_protocol.h>

#include "scmi_hailo.h"

extern struct scmi_hailo_proto_ops *hailo_ops;
extern struct scmi_protocol_handle *ph;

/* Hailo SCMI command definitions */

static int scmi_hailo_get_scu_boot_source(u8 *boot_source);
static int scmi_hailo_set_eth_rmii(void);

const struct scmi_hailo_ops ops = {
    .get_scu_boot_source = scmi_hailo_get_scu_boot_source,
	.set_eth_rmii = scmi_hailo_set_eth_rmii,
};

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
	return hailo_ops->get_scu_boot_source(ph, boot_source);
}

static int scmi_hailo_set_eth_rmii(void)
{
	return hailo_ops->set_eth_rmii(ph);
}

