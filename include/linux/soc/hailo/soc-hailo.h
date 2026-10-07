#ifndef __SOC_HAILO_H
#define __SOC_HAILO_H

#include <linux/kernel.h>
#include <dt-bindings/soc/hailo15_scmi_api.h>

static inline void hailo_board_sku_id_to_str(u32 board_sku_id, char *name, size_t name_size)
{
	if (board_sku_id == HAILO10_SCMI_BOARD_SKU_ID__INVALID)
		snprintf(name, name_size, "NA");
	else
		snprintf(name, name_size, "%u", board_sku_id);
}

#endif /* __SOC_HAILO_H */
