#include <media/v4l2-mediabus.h>
#include <media/v4l2-event.h>
#include "hailo15-isp-hw.h"
#include "hailo15-isp.h"

#define SCALE_FACTOR 0x10000
#define MSRZ_SCALE_CALC(in, out)                                               \
	((uint32_t)((((out)-1) * SCALE_FACTOR) / ((in)-1)) + 1)

/* @TODO get real vdid when calling read/write reg */
uint32_t hailo15_isp_read_reg(struct hailo15_isp_device *isp_dev, uint32_t reg)
{
	uint32_t val = 0;

	if(isp_dev->mcm_mode && isp_dev->fe_dev) {
		isp_dev->fe_dev->fe_read_reg(isp_dev->fe_dev, 0, reg, &val);
		return val;
	}

	return readl(isp_dev->base + reg);
}
EXPORT_SYMBOL(hailo15_isp_read_reg);

void hailo15_isp_write_reg(struct hailo15_isp_device *isp_dev, uint32_t reg,
			   uint32_t val)
{
	if(isp_dev->mcm_mode && isp_dev->fe_dev) {
		isp_dev->fe_dev->fe_write_reg(isp_dev->fe_dev, 0, reg, val);
		return;
	}

	writel(val, isp_dev->base + reg);
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
	hailo15_isp_wrapper_write_reg(isp_dev, ISP_WRAPPER_FATAL_ASF_INT_MASK,
				      FATAL_ASF_INT_SET);
	hailo15_isp_wrapper_write_reg(isp_dev, ISP_FUNC_INT_MASK,
				      FUNC_INT_SET_ALL);
	hailo15_isp_wrapper_write_reg(isp_dev, ISP_ERR_INT_MASK,
				      ERR_INT_SET_ALL);
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

	enabled_mask = path == ISP_MP ? MP_YCBCR_PATH_ENABLE_MASK :
					SP2_YCBCR_PATH_ENABLE_MASK;

	mi_ctrl = hailo15_isp_read_reg(isp_dev, MI_CTRL);

	return !!(mi_ctrl & enabled_mask);
}

static inline void
hailo15_isp_configure_mp_frame_base(struct hailo15_isp_device *isp_dev,
				    dma_addr_t addr[FMT_MAX_PLANES])
{
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
	case HAILO15_ISP_IRQ_ENEVT_ISP_MIS:
		irq_event->irq_status = isp_dev->irq_status.isp_mis;
		break;
	case HAILO15_ISP_IRQ_ENEVT_MI_MIS:
		irq_event->irq_status = isp_dev->irq_status.isp_miv2_mis;
		break;
	case HAILO15_ISP_IRQ_ENEVT_MI_MIS1:
		irq_event->irq_status = isp_dev->irq_status.isp_miv2_mis1;
		break;
	case HAILO15_ISP_IRQ_ENEVT_FE:
		irq_event->irq_status = isp_dev->irq_status.isp_fe;
		break;
	default:
		pr_err("%s - got bad irq_id: %d\n", __func__, irq_id);
		return;
	}

	event.type = HAILO15_ISP_EVENT_IRQ;
	event.id = irq_id;

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

static inline int __hailo15_isp_frame_rx_mp(int miv2_mis)
{
	return !!(miv2_mis & MIV2_MP_YCBCR_FRAME_END_MASK);
}

static inline int __hailo15_isp_frame_rx_sp2(int miv2_mis)
{
	return !!(miv2_mis & MIV2_SP2_YCBCR_FRAME_END_MASK);
}

static void hailo15_isp_handle_frame_rx_mp(struct hailo15_isp_device *isp_dev,
					   int irq_status)
{
	if (!__hailo15_isp_frame_rx_mp(irq_status))
		return;

	if (isp_dev->mi_stopped[ISP_MP] && !isp_dev->mcm_mode)
		return;

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

	/*do_rx_sp2*/
	hailo15_isp_buffer_done(isp_dev, ISP_SP2);
}

static void hailo15_isp_handle_frame_rx(struct hailo15_isp_device *isp_dev,
					int irq_status)
{
	hailo15_isp_handle_frame_rx_mp(isp_dev, irq_status);
	hailo15_isp_handle_frame_rx_sp2(isp_dev, irq_status);
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

	isp_dev->irq_status.isp_miv2_mis =
		hailo15_isp_read_reg(isp_dev, MIV2_MIS);
	hailo15_isp_write_reg(isp_dev, MIV2_ICR,
			      isp_dev->irq_status.isp_miv2_mis);

	if (isp_dev->irq_status.isp_miv2_mis & MIV2_SP2_RAW_FRAME_END) {
		mi_ctrl = hailo15_isp_read_reg(isp_dev, MI_CTRL);
		mi_ctrl |= SP2_RAW_RDMA_START | SP2_RAW_RDMA_START_CON;
		hailo15_isp_write_reg(isp_dev, MI_CTRL, mi_ctrl);
		isp_dev->irq_status.isp_miv2_mis &= ~MIV2_SP2_RAW_FRAME_END;
	}

	if (isp_dev->irq_status.isp_miv2_mis != 0) {
		hailo15_isp_handle_frame_rx(isp_dev,
					    isp_dev->irq_status.isp_miv2_mis);
		raised_irq_count++;
	}

	isp_dev->irq_status.isp_miv2_mis1 =
		hailo15_isp_read_reg(isp_dev, MIV2_MIS1);
	hailo15_isp_write_reg(isp_dev, MIV2_ICR1,
			      isp_dev->irq_status.isp_miv2_mis1);
	if (isp_dev->irq_status.isp_miv2_mis1 != 0) {
		raised_irq_count++;
	}
	
	if(isp_dev->mcm_mode){
		isp_dev->irq_status.isp_fe = hailo15_isp_read_reg(isp_dev, FE_MIS);

		if(isp_dev->irq_status.isp_fe != 0)
			raised_irq_count++;

		if(isp_dev->fe_dev)
			isp_dev->fe_dev->fe_dma_irq(isp_dev->fe_dev);
	}

	event_size = hailo15_isp_get_event_queue_size(isp_dev);

	if (event_size >= HAILO15_ISP_EVENT_QUEUE_SIZE - raised_irq_count) {
		pr_err("hailo15_isp: event queue full! dropping event.");
	} else {
		if (event_size == (HAILO15_ISP_EVENT_QUEUE_SIZE / 2))
			pr_warn("hailo15_isp: event queue reached half of queue size.\n");

		hailo15_isp_post_irq_event(isp_dev,
					   HAILO15_ISP_IRQ_ENEVT_ISP_MIS);
		hailo15_isp_post_irq_event(isp_dev,
					   HAILO15_ISP_IRQ_ENEVT_MI_MIS);
		hailo15_isp_post_irq_event(isp_dev,
					   HAILO15_ISP_IRQ_ENEVT_MI_MIS1);
		if(isp_dev->mcm_mode){
			hailo15_isp_post_irq_event(isp_dev,
					   HAILO15_ISP_IRQ_ENEVT_FE);
		}
	}

	if(isp_dev->mcm_mode && isp_dev->irq_status.isp_mis & ISP_MIS_FRAME_OUT){
		/* @TODO move to tasklet */
		isp_dev->fe_dev->fe_isp_irq_work(isp_dev->fe_dev);
	}
}

irqreturn_t hailo15_isp_irq_process(struct hailo15_isp_device *isp_dev)
{
	hailo15_isp_handle_int(isp_dev);
	return IRQ_HANDLED;
}
MODULE_LICENSE("GPL v2");
