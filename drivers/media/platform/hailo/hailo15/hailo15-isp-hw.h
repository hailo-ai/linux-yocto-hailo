#ifndef __HAILO15_ISP_HW_H
#define __HAILO15_ISP_HW_H

#include "hailo15-isp.h"

irqreturn_t hailo15_isp_irq_process(struct hailo15_isp_device *isp_dev);
irqreturn_t hailo15_isp_err_irq_process(struct hailo15_isp_device *isp_dev, int irq);

/* Legacy register functions */
uint32_t hailo15_isp_read_reg(struct hailo15_isp_device *, uint32_t);
void hailo15_isp_write_reg(struct hailo15_isp_device *, uint32_t, uint32_t);

void hailo15_isp_configure_frame_base(struct hailo15_isp_device *isp_dev,
				      dma_addr_t addr[FMT_MAX_PLANES],
				      int grp_id);
void hailo15_isp_configure_mcm_raw_frame_base(struct hailo15_isp_device *isp_dev,
				    dma_addr_t addr[FMT_MAX_PLANES], unsigned int vdid);
void hailo15_isp_configure_frame_size(struct hailo15_isp_device *, int);
void hailo15_fe_get_dev(struct vvcam_fe_dev** dev);
void hailo15_fe_set_address_space_base(struct vvcam_fe_dev* fe_dev, void __iomem* base);

#endif /*__HAILO15_ISP_HW_H*/
