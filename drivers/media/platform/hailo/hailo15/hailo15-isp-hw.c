#include <media/v4l2-mediabus.h>
#include <media/v4l2-event.h>
#include <linux/delay.h>
#include "hailo15-isp-hw.h"
#include "hailo15-isp.h"

#define SCALE_FACTOR 0x10000
#define MSRZ_SCALE_CALC(in, out)                                               \
	((uint32_t)((((out)-1) * SCALE_FACTOR) / ((in)-1)) + 1)

/* @TODO get real vdid when calling read/write reg */
uint32_t hailo15_isp_read_reg(struct hailo15_isp_device *isp_dev, uint32_t reg)
{
	uint32_t val = 0;
	int ret;

	if(isp_dev->fe_enable && isp_dev->fe_dev) {
		ret = isp_dev->fe_dev->fe_read_reg(isp_dev->fe_dev, 0, reg, &val);
		if(!ret)
			return val;
		isp_dev->fe_enable = 0;
	}

	return readl(isp_dev->base + reg);
}
EXPORT_SYMBOL(hailo15_isp_read_reg);

int hailo15_isp_write_reg(struct hailo15_isp_device *isp_dev, uint32_t reg,
			   uint32_t val)
{
	int ret;

	if (isp_dev->fe_enable && isp_dev->fe_dev) {
		ret = isp_dev->fe_dev->fe_write_reg(isp_dev->fe_dev, 0, reg, val);
		if (!ret)
			return ret;
		isp_dev->fe_enable = 0;
	}

	writel(val, isp_dev->base + reg);
	return 0;
}
EXPORT_SYMBOL(hailo15_isp_write_reg);

void hailo15_isp_wrapper_write_reg(struct hailo15_isp_device *isp_dev,
				   uint32_t reg, uint32_t val)
{
	writel(val, isp_dev->wrapper_base + reg);
}

void hailo15_config_isp_wrapper(struct hailo15_isp_device *isp_dev)
{
	pr_debug("%s - writting to isp wrapper interrupt masks\n", __func__);
	hailo15_isp_wrapper_write_reg(isp_dev,
		isp_dev->wrapper_cfg->fatal_asf_int_mask_offset,
		isp_dev->wrapper_cfg->fatal_asf_int_mask_value);
	hailo15_isp_wrapper_write_reg(isp_dev, 
		isp_dev->wrapper_cfg->func_int_mask_offset,
		isp_dev->wrapper_cfg->func_int_mask_value);
	hailo15_isp_wrapper_write_reg(isp_dev,
		isp_dev->wrapper_cfg->err_int_mask_offset,
		isp_dev->wrapper_cfg->err_int_mask_value);
}

void hailo15_isp_reset_hw(struct hailo15_isp_device* isp_dev){
	writel(VI_IRCL_RESET_ISP, isp_dev->base + VI_IRCL);
	mdelay(10);
	writel(VI_IRCL_RESET_ISP_CLEAR, isp_dev->base + VI_IRCL);
}
EXPORT_SYMBOL(hailo15_isp_reset_hw);

static enum mcm_rd_fmt hailo15_isp_mcm_rd_cfg(int mcm_mode) {
    switch (mcm_mode) {
        case ISP_MCM_MODE_STITCHING:
            return MCM_RD_FMT_20BIT;
        case ISP_MCM_MODE_INJECTION:
            return MCM_RD_FMT_12BIT;
        case ISP_MCM_MODE_OFF:
        case ISP_MCM_MODE_MAX:
        default:
            return MCM_RD_FMT_INVALID;
    }
}

static void hailo15_isp_configure_mcm_rdma(struct hailo15_isp_device* isp_dev){

	int width, height;
	uint32_t mi_mcm_ctrl, mcm_rd_cfg, mi_ctrl, mi_mcm_fmt, mi_imsc;

	uint32_t rd_cfg_for_mcm_mode = hailo15_isp_mcm_rd_cfg(isp_dev->mcm_mode);

	if (rd_cfg_for_mcm_mode == MCM_RD_FMT_INVALID) {
		pr_err("Invalid MCM mode %d\n", isp_dev->mcm_mode);
		return;
	}

	width = isp_dev->input_fmt.format.width;
	height = isp_dev->input_fmt.format.height;
	mi_ctrl = hailo15_isp_read_reg(isp_dev, MI_CTRL);
	mi_ctrl &= ~MI_CTRL_MCM_RAW_RDMA_START_CON;
	mi_ctrl |= MI_CTRL_MCM_RAW_RDMA_PATH_ENABLE;
	hailo15_isp_write_reg(isp_dev, MI_CTRL, mi_ctrl);
	mi_mcm_ctrl = hailo15_isp_read_reg(isp_dev, MI_MCM_CTRL);
	hailo15_isp_write_reg(isp_dev, MI_MCM_DMA_RAW_PIC_WIDTH, width);
	hailo15_isp_write_reg(isp_dev, MI_MCM_DMA_RAW_PIC_LLENGTH, width*sizeof(uint16_t));
	hailo15_isp_write_reg(isp_dev, MI_MCM_DMA_RAW_PIC_LVAL, width*sizeof(uint16_t));
	hailo15_isp_write_reg(isp_dev, MI_MCM_DMA_RAW_PIC_SIZE, width*height*sizeof(uint16_t));
	mcm_rd_cfg = hailo15_isp_read_reg(isp_dev, MCM_RD_CFG);
	mcm_rd_cfg = rd_cfg_for_mcm_mode;
	hailo15_isp_write_reg(isp_dev, MCM_RD_CFG, mcm_rd_cfg);
	mi_mcm_fmt = hailo15_isp_read_reg(isp_dev, MI_MCM_FMT);
	mi_mcm_fmt |= MCM_RD_RAW_BIT;
	hailo15_isp_write_reg(isp_dev, MI_MCM_FMT, mi_mcm_fmt);
	hailo15_isp_write_reg(isp_dev, MI_MCM_CTRL, mi_mcm_ctrl | MCM_RD_CFG_UPD);
	mi_imsc = hailo15_isp_read_reg(isp_dev, MI_IMSC);
	mi_imsc |= MCM_DMA_RAW_READY;
	hailo15_isp_write_reg(isp_dev, MI_IMSC, mi_imsc);
	if(isp_dev->mcm_mode == ISP_MCM_MODE_STITCHING) {
		hailo15_isp_write_reg(isp_dev, MCM_RETIMING0, MCM_RETIMING_VSYNC);
		hailo15_isp_write_reg(isp_dev, MCM_RETIMING1, MCM_RETIMING_HSYNC);
	}
}

void hailo15_isp_configure_frame_size(struct hailo15_isp_device *isp_dev,
				      int path)
{
	const struct hailo15_video_fmt *format;
	uint32_t line_length;
	int bytesperline;
	uint32_t y_size_init_addr, cb_size_init_addr, cr_size_init_addr;

	if (path >= ISP_MAX_PATH || path < 0)
		return;
	
	if(path == ISP_MCM_IN){
		hailo15_isp_configure_mcm_rdma(isp_dev);
		return;
	}

	y_size_init_addr =
		path == ISP_MP ? MI_MP_Y_SIZE_INIT : MI_SP2_Y_SIZE_INIT;
	cb_size_init_addr =
		path == ISP_MP ? MI_MP_CB_SIZE_INIT : MI_SP2_CB_SIZE_INIT;
	cr_size_init_addr =
		path == ISP_MP ? MI_MP_CR_SIZE_INIT : MI_SP2_CR_SIZE_INIT;

	format = hailo15_code_get_format(isp_dev->fmt[path].format.code);
	if (!format) {
		pr_err("%s - failed to get format for code: %u\n", __func__,
		       isp_dev->fmt[path].format.code);
		return;
	}

	line_length = ALIGN_UP(isp_dev->fmt[path].format.width, STRIDE_ALIGN);
	bytesperline =
		hailo15_plane_get_bytesperline(format, line_length, PLANE_Y);

	hailo15_isp_write_reg(isp_dev, y_size_init_addr,
			      hailo15_plane_get_sizeimage(
				      format, isp_dev->fmt[path].format.height,
				      bytesperline, PLANE_Y));

	bytesperline =
		hailo15_plane_get_bytesperline(format, line_length, PLANE_CB);
	hailo15_isp_write_reg(isp_dev, cb_size_init_addr,
			      hailo15_plane_get_sizeimage(
				      format, isp_dev->fmt[path].format.height,
				      bytesperline, PLANE_CB));

	bytesperline =
		hailo15_plane_get_bytesperline(format, line_length, PLANE_CR);
	hailo15_isp_write_reg(isp_dev, cr_size_init_addr,
			      hailo15_plane_get_sizeimage(
				      format, isp_dev->fmt[path].format.height,
				      bytesperline, PLANE_CR));
}
EXPORT_SYMBOL(hailo15_isp_configure_frame_size);

int hailo15_isp_is_path_enabled(struct hailo15_isp_device *isp_dev, int path)
{
	uint32_t enabled_mask;
	uint32_t mi_ctrl;

	if (path >= ISP_MAX_PATH || path < 0)
		return 0;
	
	if(path == ISP_MCM_IN)
		return isp_dev->rdma_enable;

	enabled_mask = path == ISP_MP ? MP_YCBCR_PATH_ENABLE_MASK :
					SP2_YCBCR_PATH_ENABLE_MASK;

	mi_ctrl = hailo15_isp_read_reg(isp_dev, MI_CTRL);

	return !!(mi_ctrl & enabled_mask);
}
static inline void
hailo15_isp_configure_rdma_frame_base(struct hailo15_isp_device *isp_dev,
				    dma_addr_t addr[FMT_MAX_PLANES])
{
	int mi_mcm_ctrl;
	struct isp_fe_switch_t fe_switch;

	trace_printk("y shadow: %x\n", readl(isp_dev->base + 0x1370));

	memset(&fe_switch, 0, sizeof(fe_switch));
	fe_switch.vd_mode = 1;
	hailo15_isp_write_reg(isp_dev, MIV2_MCM_DMA_RAW_PIC_START_AD, addr[PLANE_Y]);
	mi_mcm_ctrl = hailo15_isp_read_reg(isp_dev, MI_MCM_CTRL);
	mi_mcm_ctrl |= MCM_RD_CFG_UPD;
	hailo15_isp_write_reg(isp_dev, MI_MCM_CTRL, mi_mcm_ctrl);
	memset(&fe_switch, 0, sizeof(fe_switch));
	fe_switch.vd_mode = 1;

	// FE switch will write to MI_CTRL with MI_CTRL_MCM_RAW_RDMA_START to trigger MCM RDMA start
	// It will do so after the FE is finished (fe_switch wait for completion, and then executes this write)
	if (isp_dev->fe_enable)
		isp_dev->fe_dev->fe_switch(isp_dev->fe_dev, &fe_switch);
}

static inline void
hailo15_isp_configure_mp_frame_base(struct hailo15_isp_device *isp_dev,
				    dma_addr_t addr[FMT_MAX_PLANES])
{
	trace_printk("Configuring MP frame base with addr: Y: 0x%llx, CB: 0x%llx, CR: 0x%llx\n",
		   (unsigned long long)addr[PLANE_Y],
		   (unsigned long long)addr[PLANE_CB],
		   (unsigned long long)addr[PLANE_CR]);
	hailo15_isp_write_reg(isp_dev, MIV2_MP_Y_BASE_AD_INIT, addr[PLANE_Y]);
	hailo15_isp_write_reg(isp_dev, MIV2_MP_CB_BASE_AD_INIT, addr[PLANE_CB]);
	hailo15_isp_write_reg(isp_dev, MIV2_MP_CR_BASE_AD_INIT, addr[PLANE_CR]);
}

static inline void
hailo15_isp_configure_sp2_frame_base(struct hailo15_isp_device *isp_dev,
				     dma_addr_t addr[FMT_MAX_PLANES])
{
	hailo15_isp_write_reg(isp_dev, MIV2_SP2_Y_BASE_AD_INIT, addr[PLANE_Y]);
	hailo15_isp_write_reg(isp_dev, MIV2_SP2_CB_BASE_AD_INIT,
			      addr[PLANE_CB]);
	hailo15_isp_write_reg(isp_dev, MIV2_SP2_CR_BASE_AD_INIT,
			      addr[PLANE_CR]);
}

void hailo15_isp_configure_frame_base(struct hailo15_isp_device *isp_dev,
				      dma_addr_t addr[FMT_MAX_PLANES],
				      unsigned int pad)
{
	if (ISP_MP == pad) {
		hailo15_isp_configure_mp_frame_base(isp_dev, addr);
	}

	if (ISP_SP2 == pad) {
		hailo15_isp_configure_sp2_frame_base(isp_dev, addr);
	}

	if(ISP_MCM_IN == pad){
		hailo15_isp_configure_rdma_frame_base(isp_dev, addr);
	}
}
EXPORT_SYMBOL(hailo15_isp_configure_frame_base);

int hailo15_isp_dma_set_enable(struct hailo15_isp_device *isp_dev, int path,
			       int enable)
{
	/* MSW-3003 */
	return 0;
}
EXPORT_SYMBOL(hailo15_isp_dma_set_enable);

static void hailo15_isp_post_irq_event(struct hailo15_isp_device *isp_dev,
				       int irq_id)
{
	struct video_device *vdev = isp_dev->sd.devnode;
	struct v4l2_event event;
	struct hailo15_isp_irq_status_event *irq_event;

	if (isp_dev->mi_stopped[ISP_MP] && isp_dev->mi_stopped[ISP_SP2] && !isp_dev->mcm_mode)
		return;

	memset(&event, 0, sizeof(event));
	irq_event = (void *)event.u.data;

	switch (irq_id) {
	case HAILO15_ISP_IRQ_EVENT_ISP_MIS:
		irq_event->irq_status = isp_dev->irq_status.isp_mis;
		break;
	case HAILO15_ISP_IRQ_EVENT_MI_MIS:
		irq_event->irq_status = isp_dev->irq_status.isp_miv2_mis;
		break;
	case HAILO15_ISP_IRQ_EVENT_MI_MIS1:
		irq_event->irq_status = isp_dev->irq_status.isp_miv2_mis1;
		break;
	case HAILO15_ISP_IRQ_EVENT_FE:
		irq_event->irq_status = isp_dev->irq_status.isp_fe;
		break;
	default:
		pr_err("%s - got bad irq_id: %d\n", __func__, irq_id);
		return;
	}

	event.type = HAILO15_ISP_EVENT_IRQ;
	event.id = irq_id;

	// Avoid sending events with empty irq_status
	if (irq_event->irq_status == 0)
		return;
	
	v4l2_event_queue(vdev, &event);
}

static int hailo15_isp_get_event_queue_size(struct hailo15_isp_device *isp_dev)
{
	struct video_device *vdev = isp_dev->sd.devnode;
	struct v4l2_subscribed_event *sev = NULL;
	unsigned long flags;
	struct v4l2_fh *fh = NULL;
	struct v4l2_subscribed_event *s = NULL;
	int size = -1;

	spin_lock_irqsave(&vdev->fh_lock, flags);

	list_for_each_entry (fh, &vdev->fh_list, list) {
		list_for_each_entry (s, &fh->subscribed, list) {
			if (s->type == HAILO15_ISP_EVENT_IRQ) {
				sev = s;
				break;
			}
		}

		if (sev == NULL) {
			continue;
		}

		size = sev->in_use;

		/* Assuming only one file handler is registered to this event */
		break;
	}
	spin_unlock_irqrestore(&vdev->fh_lock, flags);

	return size;
}

static int hailo15_isp_process_delta(int delta)
{
	int sign = !!(delta & ISP_VSM_DELTA_SIGN_MASK);
	delta = delta & ~(ISP_VSM_DELTA_SIGN_MASK);
	if (sign)
		return -1 * (ISP_VSM_DELTA_SIGN_MASK - delta);
	return delta;
}

static inline int __hailo15_isp_frame_rx_sp2_raw_3dnr(struct hailo15_isp_device *isp_dev, int miv2_mis)
{
	return !!(miv2_mis & MIV2_SP2_RAW_FRAME_END) && !!(isp_dev->irq_status.isp_miv2_mis & MIV2_SP2_RAW_FRAME_END);
}

static inline int __hailo15_isp_frame_rx_mp(int miv2_mis)
{
	return !!(miv2_mis & MIV2_MP_YCBCR_FRAME_END_MASK);
}

static inline int __hailo15_isp_frame_rx_sp2(int miv2_mis)
{
	return !!(miv2_mis & MIV2_SP2_YCBCR_FRAME_END_MASK);
}

static inline int __hailo15_isp_frame_rx_rdma_ready(int miv2_mis)
{
	return !!(miv2_mis & MIV2_MCM_DMA_RAW_READY_MASK);
}

static void hailo15_isp_handle_frame_rx_rdma(struct hailo15_isp_device *isp_dev,
					     int irq_status)
{
	if(!isp_dev->mcm_mode)
		return;
	
	if(__hailo15_isp_frame_rx_rdma_ready(irq_status)){
		isp_dev->dma_ready = 1;
		trace_printk("MCM RDMA ready!\n");

	}
	if(__hailo15_isp_frame_rx_mp(irq_status) || __hailo15_isp_frame_rx_sp2(irq_status)){
		isp_dev->frame_end = 1;
	}
	if(isp_dev->frame_end){
		atomic_set(&isp_dev->frame_received, 1);
	}
	if(isp_dev->frame_end && isp_dev->dma_ready &&  (!isp_dev->fe_enable || isp_dev->fe_ready)){
		isp_dev->dma_ready = 0;
		isp_dev->frame_end = 0;
		isp_dev->fe_ready = 0;

		if (isp_dev->mcm_mode == ISP_MCM_MODE_INJECTION) {
			mutex_lock(&isp_dev->ready_lock);
			if (isp_dev->output_ready){
				/* do rx_rdma */
				hailo15_isp_buffer_done(isp_dev, ISP_MCM_IN);
			} else {
				/* indicates that the MCM is waiting for the MP to have a buffer ready */
				isp_dev->mcm_waiting = 1;
			}
			mutex_unlock(&isp_dev->ready_lock);
		} else {
			/* do rx_rdma without waiting in HDR */
			hailo15_isp_buffer_done(isp_dev, ISP_MCM_IN);
		}
	}
}

static void hailo15_isp_handle_frame_rx_sp2_raw_3dnr(struct hailo15_isp_device *isp_dev,
					   int irq_status)
{
	int mi_ctrl;

	if (!__hailo15_isp_frame_rx_sp2_raw_3dnr(isp_dev, irq_status))
		return;

	mi_ctrl = hailo15_isp_read_reg(isp_dev, MI_CTRL);
	mi_ctrl |= SP2_RAW_RDMA_START | SP2_RAW_RDMA_START_CON;
	hailo15_isp_write_reg(isp_dev, MI_CTRL, mi_ctrl);
	isp_dev->irq_status.isp_miv2_mis &= ~MIV2_SP2_RAW_FRAME_END;
}

static void hailo15_isp_handle_frame_rx_mp(struct hailo15_isp_device *isp_dev,
					   int irq_status)
{
	if (!__hailo15_isp_frame_rx_mp(irq_status))
		return;

	if (isp_dev->mi_stopped[ISP_MP] && !isp_dev->mcm_mode)
		return;

	atomic_set(&isp_dev->frame_received, 1);

	/*do_rx_mp*/
	hailo15_isp_buffer_done(isp_dev, ISP_MP);
}

static void hailo15_isp_handle_frame_rx_sp2(struct hailo15_isp_device *isp_dev,
					    int irq_status)
{
	if (!__hailo15_isp_frame_rx_sp2(irq_status))
		return;

	if (isp_dev->mi_stopped[ISP_SP2] && !isp_dev->mcm_mode)
		return;

	atomic_set(&isp_dev->frame_received, 1);

	/*do_rx_sp2*/
	hailo15_isp_buffer_done(isp_dev, ISP_SP2);
}

void hailo15_isp_handle_frame_rx(struct work_struct *work)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)container_of(
			work, struct hailo15_isp_device, miv2_mis_w);
	struct hailo15_miv2_mis* miv2_mis, *miv2_pos, *miv2_npos;
	struct list_head miv2_tmp_list;
	unsigned long flags;

	INIT_LIST_HEAD(&miv2_tmp_list);
	spin_lock_irqsave(&isp_dev->miv2_mis_lock, flags);
	while((miv2_mis = list_first_entry_or_null(&isp_dev->miv2_mis_queue, struct hailo15_miv2_mis, list))){
		list_del(&miv2_mis->list);
		list_add_tail(&miv2_mis->list, &miv2_tmp_list);
	}
	spin_unlock_irqrestore(&isp_dev->miv2_mis_lock, flags);

	list_for_each_entry_safe(miv2_pos, miv2_npos, &miv2_tmp_list, list){
		trace_printk("handling frame rx with %x\n", miv2_pos->miv2_mis);
		hailo15_isp_handle_frame_rx_sp2_raw_3dnr(isp_dev, miv2_pos->miv2_mis);
		hailo15_isp_handle_frame_rx_mp(isp_dev, miv2_pos->miv2_mis);
		hailo15_isp_handle_frame_rx_sp2(isp_dev, miv2_pos->miv2_mis);
		hailo15_isp_handle_frame_rx_rdma(isp_dev, miv2_pos->miv2_mis);
		list_del(&miv2_pos->list);
		kfree(miv2_pos);
		trace_printk("Handled frame rx!\n");
	}
}

void hailo15_isp_handle_afm_int(struct work_struct *work)
{
	struct hailo15_isp_device *isp_dev =
		(struct hailo15_isp_device *)container_of(
			work, struct hailo15_isp_device, af_w);
	uint32_t sum_a, sum_b, sum_c, lum_a, lum_b, lum_c;
	sum_a = hailo15_isp_read_reg(isp_dev, ISP_AFM_SUM_A);
	sum_b = hailo15_isp_read_reg(isp_dev, ISP_AFM_SUM_B);
	sum_c = hailo15_isp_read_reg(isp_dev, ISP_AFM_SUM_C);
	lum_a = hailo15_isp_read_reg(isp_dev, ISP_AFM_LUM_A);
	lum_b = hailo15_isp_read_reg(isp_dev, ISP_AFM_LUM_B);
	lum_c = hailo15_isp_read_reg(isp_dev, ISP_AFM_LUM_C);

	mutex_lock(&isp_dev->af_kevent->data_lock);
	if (isp_dev->af_kevent->ready == 1) {
		pr_debug("%s - AF event not handled in time, dropping measurements\n",
			__func__);
		mutex_unlock(&isp_dev->af_kevent->data_lock);
		return;
	}

	isp_dev->af_kevent->sum_a = sum_a;
	isp_dev->af_kevent->sum_b = sum_b;
	isp_dev->af_kevent->sum_c = sum_c;
	isp_dev->af_kevent->lum_a = lum_a;
	isp_dev->af_kevent->lum_b = lum_b;
	isp_dev->af_kevent->lum_c = lum_c;
	isp_dev->af_kevent->ready = 1;
	mutex_unlock(&isp_dev->af_kevent->data_lock);

	wake_up_interruptible_all(&isp_dev->af_kevent->wait_q);
}

static void hailo15_isp_handle_int(struct hailo15_isp_device *isp_dev)
{
	int event_size = 0;
	int raised_irq_count = 0;
	int isp_imsc = 0;
	int mi_ctrl;
	unsigned long flags;
	struct hailo15_miv2_mis *miv2_mis;
	uint32_t masked_mis;

	/* clear the hw interrupt*/
	isp_dev->irq_status.isp_mis = hailo15_isp_read_reg(isp_dev, ISP_MIS);
	hailo15_isp_write_reg(isp_dev, ISP_ICR, isp_dev->irq_status.isp_mis);
	if (isp_dev->irq_status.isp_mis != 0) {
		raised_irq_count++;

		if (isp_dev->irq_status.isp_mis & ISP_MIS_DATA_LOSS) {
			pr_err("fatal: isp data loss detected!\n");
			isp_imsc = hailo15_isp_read_reg(isp_dev, ISP_IMSC);
			isp_imsc &= ~(ISP_MIS_DATA_LOSS);
			hailo15_isp_write_reg(isp_dev, ISP_IMSC, isp_imsc);
		}
	}

	trace_printk("irq status: 0x%x\n", isp_dev->irq_status.isp_mis);

	if (isp_dev->irq_status.isp_mis & (ISP_MIS_VSM_DONE)) {
		isp_dev->current_vsm.dx = hailo15_isp_process_delta(
			hailo15_isp_read_reg(isp_dev, ISP_VSM_DELTA_H));
		isp_dev->current_vsm.dy = hailo15_isp_process_delta(
			hailo15_isp_read_reg(isp_dev, ISP_VSM_DELTA_V));
	}

	if (isp_dev->irq_status.isp_mis &
	    (ISP_MIS_AFM_SUM_OF | ISP_MIS_AFM_LUM_OF)) {
		pr_warn("%s - AFM overflow - mis = 0x%x\n", __func__,
			isp_dev->irq_status.isp_mis);
	} else if (isp_dev->irq_status.isp_mis & (ISP_MIS_AFM_FIN)) {
		queue_work(isp_dev->af_wq, &isp_dev->af_w);
	}

    hailo15_process_irq_stats_events(isp_dev, HAILO15_ISP_IRQ_EVENT_ISP_MIS, isp_dev->irq_status.isp_mis);
	isp_dev->irq_status.isp_miv2_mis = hailo15_isp_read_reg(isp_dev, MIV2_MIS);
	hailo15_isp_write_reg(isp_dev, MIV2_ICR, isp_dev->irq_status.isp_miv2_mis);

	trace_printk("irq status miv2mis: 0x%x\n", isp_dev->irq_status.isp_miv2_mis);

	/* Handle raw frame from 3dnr */
	if (isp_dev->irq_status.isp_miv2_mis & MIV2_SP2_RAW_FRAME_END) {
		mi_ctrl = hailo15_isp_read_reg(isp_dev, MI_CTRL);
		mi_ctrl |= SP2_RAW_RDMA_START | SP2_RAW_RDMA_START_CON;
		
		/* Handling might fail if FE is currently running.
		 * In that case, don't delete MIV2_SP2_RAW_FRAME_END from irq status, deferring the handling (miv2_mis_w workqueue).
		 * The deferred work comes at the expense of the risk of handling too-late (low probability).
		 * So that's why we should be greedy here, i.e. try to handle in irq context first, and use workqueue when having no other choice.
		 */
		if (hailo15_isp_write_reg(isp_dev, MI_CTRL, mi_ctrl) == 0) {
			isp_dev->irq_status.isp_miv2_mis &= ~MIV2_SP2_RAW_FRAME_END;
		}
	}

	/* Handle YCBCR MP/SP2 frame end, RDMA ready, raw 3dnr frame end - do that using deferred work */
	masked_mis = isp_dev->irq_status.isp_miv2_mis & (MIV2_MP_YCBCR_FRAME_END_MASK | MIV2_SP2_YCBCR_FRAME_END_MASK | MIV2_MCM_DMA_RAW_READY_MASK | MIV2_SP2_RAW_FRAME_END);
	if (masked_mis != 0) {
		miv2_mis = kzalloc(sizeof(struct hailo15_miv2_mis), 	GFP_ATOMIC);
		if (miv2_mis){
			miv2_mis->miv2_mis = masked_mis;

			/* Since queue_work won't queue if isp_dev->miv2_mis_w is already queued, we use a list of miv2_mis to handle */
			spin_lock_irqsave(&isp_dev->miv2_mis_lock, flags);
			list_add_tail(&miv2_mis->list, &isp_dev->miv2_mis_queue);
			spin_unlock_irqrestore(&isp_dev->miv2_mis_lock, flags);
			queue_work(isp_dev->miv2_mis_wq, &isp_dev->miv2_mis_w);
		}
		raised_irq_count++;
	}
	
	hailo15_process_irq_stats_events(isp_dev, HAILO15_ISP_IRQ_EVENT_MI_MIS, isp_dev->irq_status.isp_miv2_mis);
 	
	if(isp_dev->rdma_enable){
		isp_dev->irq_status.isp_miv2_mis &= ~MIV2_MCM_RAW0_FRAME_END;
		isp_dev->irq_status.isp_miv2_mis &= ~MIV2_MCM_DMA_RAW_READY_MASK;
	}
	
	isp_dev->irq_status.isp_miv2_mis1 =
		hailo15_isp_read_reg(isp_dev, MIV2_MIS1);
	hailo15_isp_write_reg(isp_dev, MIV2_ICR1,
			      isp_dev->irq_status.isp_miv2_mis1);
	if (isp_dev->irq_status.isp_miv2_mis1 != 0) {
		raised_irq_count++;
	}
	
	if(isp_dev->fe_enable){
		isp_dev->irq_status.isp_fe = hailo15_isp_read_reg(isp_dev, FE_MIS);

		if(isp_dev->irq_status.isp_fe != 0)
			raised_irq_count++;

		if(isp_dev->fe_dev){
			isp_dev->fe_dev->fe_dma_irq(isp_dev->fe_dev);
		}

		if(isp_dev->irq_status.isp_fe & 1){
			isp_dev->fe_ready = 1;
			trace_printk("y shadow: %x\n", readl(isp_dev->base + 0x1370));
		}
	}

	event_size = hailo15_isp_get_event_queue_size(isp_dev);

	if (event_size >= HAILO15_ISP_EVENT_QUEUE_SIZE - raised_irq_count) {
		pr_err("hailo15_isp: event queue full! dropping event.");
	} else {
		if (event_size == (HAILO15_ISP_EVENT_QUEUE_SIZE / 2))
			pr_warn("hailo15_isp: event queue reached half of queue size.\n");

		hailo15_isp_post_irq_event(isp_dev,
					   HAILO15_ISP_IRQ_EVENT_ISP_MIS);
		hailo15_isp_post_irq_event(isp_dev,
					   HAILO15_ISP_IRQ_EVENT_MI_MIS);
		hailo15_isp_post_irq_event(isp_dev,
					   HAILO15_ISP_IRQ_EVENT_MI_MIS1);
		if(isp_dev->fe_enable){
			hailo15_isp_post_irq_event(isp_dev,
					   HAILO15_ISP_IRQ_EVENT_FE);
		}
	}

	if(isp_dev->fe_enable && isp_dev->irq_status.isp_mis & ISP_MIS_FRAME_OUT){
		tasklet_schedule(&isp_dev->fe_tasklet);
	}


}

irqreturn_t hailo15_isp_irq_process(struct hailo15_isp_device *isp_dev)
{
	hailo15_isp_handle_int(isp_dev);
	return IRQ_HANDLED;
}


void mcm_fe_irq_tasklet(unsigned long arg){
	struct hailo15_isp_device *isp_dev = (struct hailo15_isp_device*)arg;
	isp_dev->fe_dev->fe_isp_irq_work(isp_dev->fe_dev);
}
EXPORT_SYMBOL(mcm_fe_irq_tasklet);


MODULE_LICENSE("GPL v2");
