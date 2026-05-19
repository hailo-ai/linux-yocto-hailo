#include "common.h"
#include <media/v4l2-mediabus.h>
#include <media/v4l2-event.h>
#include "hailo15-isp-hw.h"
#include "hailo15-isp-v4l.h"
#include "hailo15-isp.h"

#define CREATE_TRACE_POINTS
#include <trace/events/hailo15_isp.h>
#include <trace/events/hailo15_dual_sensor.h>

#define SCALE_FACTOR 0x10000
#define MSRZ_SCALE_CALC(in, out)                                               \
	((uint32_t)((((out)-1) * SCALE_FACTOR) / ((in)-1)) + 1)

#define HAILO15_LINE_BUF_CFG_VBLANK_VC_MASK 0xF
#define HAILO15_LINE_BUF_CFG_VBLANK_VC_SHIFT 10
#define HAILO15_LINE_BUF_CFG_FIFO_FILL_MASK 0x1FFF
#define HAILO15_LINE_BUF_CFG_FIFO_FILL_SHIFT 14

/* Raw register access helpers */
static inline uint32_t hailo15_isp_raw_read_reg(struct hailo15_isp_device *isp_dev, uint32_t reg)
{
	return readl(isp_dev->base + reg);
}

static inline void hailo15_isp_raw_write_reg(struct hailo15_isp_device *isp_dev, uint32_t reg, uint32_t val)
{
	writel(val, isp_dev->base + reg);
}

int hailo15_isp_ioctl_read_reg(struct hailo15_isp_device *isp_dev, uint32_t reg, uint32_t *val)
{
	if (!isp_dev || !val)
		return -EINVAL;

	/* Verify FE is disabled - ioctl access requires direct hardware access */
	if (isp_dev->fe_enable) {
		pr_err("%s - Cannot read register 0x%x via ioctl when FE is enabled\n",
		       __func__, reg);
		return -EBUSY;
	}

	*val = hailo15_isp_raw_read_reg(isp_dev, reg);
	return 0;
}
EXPORT_SYMBOL(hailo15_isp_ioctl_read_reg);

int hailo15_isp_ioctl_write_reg(struct hailo15_isp_device *isp_dev, uint32_t reg, uint32_t val)
{
	if (!isp_dev)
		return -EINVAL;

	/* Verify FE is disabled - ioctl access requires direct hardware access */
	if (isp_dev->fe_enable) {
		pr_err("%s - Cannot write register 0x%x via ioctl when FE is enabled\n",
		       __func__, reg);
		return -EBUSY;
	}

	hailo15_isp_raw_write_reg(isp_dev, reg, val);
	return 0;
}
EXPORT_SYMBOL(hailo15_isp_ioctl_write_reg);

int hailo15_isp_read_vdid_reg(struct hailo15_isp_device *isp_dev, uint8_t vdid,
			      uint32_t reg, uint32_t *val)
{
	int ret;

	/* This function might sleep in FE case. Validate also when not in FE case. */
	might_sleep();

	/* Validate register classification even without FE */
	if (WARN(!isp_fe_is_vdid_register(reg),
		 "hailo15_isp_read_vdid_reg called with non-VDID reg 0x%x\n", reg))
		return -EINVAL;

	if (!isp_dev || !val)
		return -EINVAL;

	if (WARN(vdid == VIV_INVALID_VDID,
		 "%s - VIV_INVALID_VDID for reg 0x%x\n", __func__, reg))
		return -EINVAL;

	/* no fe case */
	if (!isp_dev->fe_enable) {
		if (WARN(vdid != 0,
			 "%s - invalid vdid %d for disabled FE\n", __func__, vdid))
			return -EINVAL;
		*val = hailo15_isp_raw_read_reg(isp_dev, reg);
		return 0;
	}

	/* fe case */
	ret = isp_dev->fe_dev->read_vdid_reg(isp_dev->fe_dev, vdid, reg, val);
	if (ret) {
		pr_err("%s - Failed to read VDID reg 0x%x with vdid %d, ret = %d\n",
		       __func__, reg, vdid, ret);
		isp_dev->fe_enable = 0;
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(hailo15_isp_read_vdid_reg);

int hailo15_isp_read_control_reg(struct hailo15_isp_device *isp_dev,
				 uint32_t reg, uint32_t *val)
{
	int ret;

	/* This function might sleep in FE case. Validate also when not in FE case. */
	might_sleep();

	/* Validate register classification even without FE */
	if (WARN(!isp_fe_is_non_fe_control_register(reg),
		 "hailo15_isp_read_control_reg called with non-control reg 0x%x\n", reg))
		return -EINVAL;

	if (!isp_dev || !val)
		return -EINVAL;

	/* no fe case */
	if (!isp_dev->fe_enable) {
		*val = hailo15_isp_raw_read_reg(isp_dev, reg);
		return 0;
	}

	/* fe case */
	ret = isp_dev->fe_dev->read_control_reg(isp_dev->fe_dev, reg, val);
	if (ret) {
		pr_err("%s - Failed to read control reg 0x%x, ret = %d\n",
		       __func__, reg, ret);
		isp_dev->fe_enable = 0;
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(hailo15_isp_read_control_reg);

void hailo15_isp_irq_read_control_reg(struct hailo15_isp_device *isp_dev,
				     uint32_t reg, uint32_t *val)
{
	int ret;

	if (WARN(!in_interrupt(),
		 "hailo15_isp_irq_read_control_reg called outside IRQ context for reg 0x%x\n", reg))
		return;
	/* Validate register classification even without FE */
	if (WARN(!isp_fe_is_non_fe_control_register(reg),
		 "hailo15_isp_irq_read_control_reg called with FE control reg 0x%x\n", reg))
		return;
	if (WARN(!isp_dev || !val,
		 "hailo15_isp_irq_read_control_reg called with NULL isp_dev or val\n"))
		return;

	if (!isp_dev->fe_enable) {
		/* no fe case */
		*val = hailo15_isp_raw_read_reg(isp_dev, reg);
	} else {
		ret = isp_dev->fe_dev->irq_read_control_reg(isp_dev->fe_dev, reg, val);
		if (WARN(ret, "hailo15_isp_irq_read_control_reg failed for reg 0x%x, ret = %d\n",
			 reg, ret))
			*val = 0;
	}
}
EXPORT_SYMBOL(hailo15_isp_irq_read_control_reg);


int hailo15_isp_write_vdid_reg(struct hailo15_isp_device *isp_dev, uint8_t vdid,
			       uint32_t reg, uint32_t val)
{
	int ret;

	might_sleep();

	/* Validate register classification even without FE */
	if (WARN(!isp_fe_is_vdid_register(reg),
		 "hailo15_isp_write_vdid_reg called with non-VDID reg 0x%x\n", reg))
		return -EINVAL;

	if (!isp_dev)
		return -EINVAL;

	if (WARN(vdid == VIV_INVALID_VDID,
		 "%s - VIV_INVALID_VDID for reg 0x%x\n", __func__, reg))
		return -EINVAL;

	/* no fe case */
	if (!isp_dev->fe_enable) {
		if (WARN(vdid != 0,
			 "%s - invalid vdid %d for disabled FE\n", __func__, vdid))
			return -EINVAL;
		hailo15_isp_raw_write_reg(isp_dev, reg, val);
		return 0;
	}

	/* fe case */
	ret = isp_dev->fe_dev->write_vdid_reg(isp_dev->fe_dev, vdid, reg, val);
	if (ret) {
		pr_err("%s - Failed to write VDID reg 0x%x with vdid %d, ret = %d\n",
		       __func__, reg, vdid, ret);
		isp_dev->fe_enable = 0;
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(hailo15_isp_write_vdid_reg);

int hailo15_isp_write_control_reg(struct hailo15_isp_device *isp_dev,
				  uint32_t reg, uint32_t val)
{
	int ret;

	/* This function might sleep in FE case. Validate also when not in FE case. */
	might_sleep();

	/* Validate register classification even without FE */
	if (WARN(!isp_fe_is_non_fe_control_register(reg),
		 "hailo15_isp_write_control_reg called with non-control reg 0x%x\n", reg))
		return -EINVAL;

	if (!isp_dev)
		return -EINVAL;

	/* no fe case */
	if (!isp_dev->fe_enable) {
		hailo15_isp_raw_write_reg(isp_dev, reg, val);
		return 0;
	}

	/* fe case */
	ret = isp_dev->fe_dev->write_control_reg(isp_dev->fe_dev, reg, val);
	if (ret) {
		pr_err("%s - Failed to write control reg 0x%x, ret = %d\n",
		       __func__, reg, ret);
		isp_dev->fe_enable = 0;
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(hailo15_isp_write_control_reg);

void hailo15_isp_irq_write_control_reg(struct hailo15_isp_device *isp_dev,
				      uint32_t reg, uint32_t val)
{
	int ret;

	if (WARN(!in_interrupt(),
		 "hailo15_isp_irq_write_control_reg called outside IRQ context for reg 0x%x\n", reg))
		return;
	/* Validate register classification even without FE */
	if (WARN(!isp_fe_is_non_fe_control_register(reg),
		 "hailo15_isp_irq_write_control_reg called with FE control reg 0x%x\n", reg))
		return;
	if (WARN(!isp_dev,
		 "hailo15_isp_irq_write_control_reg called with NULL isp_dev\n"))
		return;

	if (!isp_dev->fe_enable) {
		/* no fe case */
		hailo15_isp_raw_write_reg(isp_dev, reg, val);
	} else {
		ret = isp_dev->fe_dev->irq_write_control_reg(isp_dev->fe_dev, reg, val);
		WARN(ret, "hailo15_isp_irq_write_control_reg failed for reg 0x%x, ret = %d\n", reg, ret);
	}
}
EXPORT_SYMBOL(hailo15_isp_irq_write_control_reg);

static inline uint32_t hailo15_isp_wrapper_read_reg(struct hailo15_isp_device *isp_dev, uint32_t reg)
{
	return readl(isp_dev->wrapper_base + reg);
}

static inline void hailo15_isp_wrapper_write_reg(struct hailo15_isp_device *isp_dev,
						 uint32_t reg, uint32_t val)
{
	writel(val, isp_dev->wrapper_base + reg);
}

void hailo15_config_isp_wrapper_interrupts(struct hailo15_isp_device *isp_dev)
{
	const struct isp_wrapper_config *wrapper_cfg = isp_dev->wrapper_cfg;

	pr_debug("%s - writing to isp wrapper interrupt masks\n", __func__);
	hailo15_isp_wrapper_write_reg(isp_dev,
		wrapper_cfg->fatal_asf_int_mask_offset,
		wrapper_cfg->fatal_asf_int_mask_value);
	hailo15_isp_wrapper_write_reg(isp_dev,
		wrapper_cfg->func_int_mask_offset,
		wrapper_cfg->func_int_mask_value);
	// clear error interrupts that might have been left from previous runs
	hailo15_isp_wrapper_write_reg(isp_dev,
		wrapper_cfg->err_int_w1c_offset,
		wrapper_cfg->err_int_w1c_value);
	hailo15_isp_wrapper_write_reg(isp_dev,
		wrapper_cfg->err_int_mask_offset,
		wrapper_cfg->err_int_mask_value);
}

void hailo15_config_isp_wrapper_datapath(struct hailo15_isp_device *isp_dev)
{
	const struct isp_wrapper_config *wrapper_cfg = isp_dev->wrapper_cfg;
	const struct hailo15_hw_shifter_config *shifter_cfg = &wrapper_cfg->shifter_cfg;
	const struct hailo15_isp_line_buf_config *line_buf_cfg = &wrapper_cfg->line_buf_cfg;
	uint32_t pixel_width;
	uint32_t reg_val = 0;
	size_t i = 0;

	// The shifter should only be configured for HDR
	reg_val = isp_dev->hdr_enabled ? shifter_cfg->shift_value : 0;

	dev_dbg(isp_dev->dev, "config isp_wrapper with %ld shifter regs, shift value %d\n",
		shifter_cfg->shifter_regs, reg_val);

	/* Will only be relevant when there are shifter regs being used.0
	 * There is a HW bug in some of the chips, which causes the data to lose percision
	 * when it enter's the ISP stitcher. The shifter is used to fix this issue.
	 * On hardwares where the shifter is not available, a different solution is used.
	 **/
	for (i = 0; i < shifter_cfg->shifter_regs; ++i) {
		uint32_t offset = shifter_cfg->first_shifter_offset + (i * sizeof(reg_val));
		hailo15_isp_wrapper_write_reg(isp_dev, offset, reg_val);
	}

	// Configure line buffer if needed
	if (!line_buf_cfg->enabled) {
		return;
	}

	for (i = 0; i < line_buf_cfg->repeat; ++i) {
		uint32_t channel_offset = i * sizeof(uint32_t);

		pixel_width = isp_dev->input_fmt[i].format.width;
		if (!pixel_width) {
			/* Sink pad not yet configured — datapath will be re-run from the
			 * stream-on path once the format is set, so skip for now. */
			continue;
		}

		// Set vblank_vc, and a fifo fill level, with a value of 1 line of pixels
		reg_val = (line_buf_cfg->values.vblank_vc & HAILO15_LINE_BUF_CFG_VBLANK_VC_MASK) << HAILO15_LINE_BUF_CFG_VBLANK_VC_SHIFT;
		reg_val |= (pixel_width & HAILO15_LINE_BUF_CFG_FIFO_FILL_MASK) << HAILO15_LINE_BUF_CFG_FIFO_FILL_SHIFT;

		hailo15_isp_wrapper_write_reg(isp_dev, line_buf_cfg->offsets.line_buf_cfg + channel_offset, reg_val);

		hailo15_isp_wrapper_write_reg(isp_dev,
			line_buf_cfg->offsets.line_buf_cfg_line_width + channel_offset, pixel_width);
		hailo15_isp_wrapper_write_reg(isp_dev,
			line_buf_cfg->offsets.line_buf_cfg_min_vblank_duration + channel_offset,
			line_buf_cfg->values.line_buf_cfg_min_vblank_duration);
		hailo15_isp_wrapper_write_reg(isp_dev,
			line_buf_cfg->offsets.line_buf_cfg_min_hblank_duration + channel_offset,
			line_buf_cfg->values.line_buf_cfg_min_hblank_duration);

		dev_dbg(isp_dev->dev, "configured line buf cfg for channel %ld (width=%u)\n",
			i, pixel_width);
	}
}

void hailo15_isp_reset_hw(struct hailo15_isp_device* isp_dev){
	hailo15_isp_raw_write_reg(isp_dev, VI_IRCL, VI_IRCL_RESET_ISP);
	mdelay(10);
	hailo15_isp_raw_write_reg(isp_dev, VI_IRCL, VI_IRCL_RESET_ISP_CLEAR);
}
EXPORT_SYMBOL(hailo15_isp_reset_hw);

static enum mcm_rd_fmt hailo15_isp_mcm_rd_cfg(int mcm_mode) {
    switch (mcm_mode) {
        case ISP_MCM_MODE_STITCHING:
            return MCM_RD_FMT_20BIT;
        case ISP_MCM_MODE_INJECTION:
        case ISP_MCM_MODE_MULTI_SENSOR:
        case ISP_MCM_MODE_RAW12_PACKED:
        case ISP_MCM_MODE_RAW_WRITE:
            return MCM_RD_FMT_12BIT;
        case ISP_MCM_MODE_OFF:
        case ISP_MCM_MODE_MAX:
        default:
            return MCM_RD_FMT_INVALID;
    }
}

/*
 * Configure MCM write path compression format in ISP_MCM_CTRL register.
 * Uses mcm_wr0_fmt bits[7:5] for channel 0 (sensor0),
 * or mcm_wr1_fmt bits[10:8] for channel 1 (sensor1).
 */
static void hailo15_isp_set_mcm_write_compression(
	struct hailo15_isp_device *isp_dev, uint8_t vdid, bool compress)
{
	uint32_t mcm_ctrl;
	uint32_t fmt_mask, fmt_16bit, fmt_20bit;

	if (vdid == 0) {
		fmt_mask  = MCM_WR0_FMT_MASK;
		fmt_16bit = MCM_WR0_FMT_16BIT;
		fmt_20bit = MCM_WR0_FMT_20BIT;
	} else {
		fmt_mask  = MCM_WR1_FMT_MASK;
		fmt_16bit = MCM_WR1_FMT_16BIT;
		fmt_20bit = MCM_WR1_FMT_20BIT;
	}

	hailo15_isp_read_control_reg(isp_dev, ISP_MCM_CTRL, &mcm_ctrl);
	mcm_ctrl &= ~fmt_mask;
	mcm_ctrl |= (compress ? fmt_20bit : fmt_16bit);
	hailo15_isp_write_control_reg(isp_dev, ISP_MCM_CTRL, mcm_ctrl);
}

/*
 * Get the MCM read path format based on compression flag.
 * Returns the appropriate mcm_rd_fmt enum value for the MCM_RD_CFG register.
 * Kept as a separate function for symmetry with hailo15_isp_set_mcm_write_compression().
 * TODO - decompression == false assumes that HDR + GTM + HDM is running so it uses 12bit.
 *        this is not generally true but for now it's the only use case. Need to allow setting
 *        it from outside with API.
 */
static enum mcm_rd_fmt hailo15_isp_get_mcm_read_fmt(bool compress)
{
	return compress ? MCM_RD_FMT_20BIT : MCM_RD_FMT_12BIT;
}

static void hailo15_isp_configure_mcm_rdma(struct hailo15_isp_device *isp_dev, int grp_id)
{
	int width, height;
	uint32_t mi_mcm_ctrl, mi_ctrl, mi_mcm_fmt, mi_imsc,
		isp_acq_prop, llength;
	uint8_t vdid = HAILO15_VID_GRP_TO_VDID(grp_id);

	uint32_t rd_cfg_for_mcm_mode = hailo15_isp_mcm_rd_cfg(isp_dev->mcm_mode);

	if (hailo15_isp_is_format_hdr(&isp_dev->input_fmt[HAILO15_VID_GRP_TO_ISP_SINK_PAD(grp_id)])) {
		hailo15_isp_set_mcm_write_compression(isp_dev, vdid, isp_dev->hdr_compression_enabled);
		rd_cfg_for_mcm_mode = hailo15_isp_get_mcm_read_fmt(isp_dev->hdr_decompression_enabled);

		/* When decompression is disabled in hdr mode, assume this is HDR + HDM + GTM flow.
		   In this flow, HMD outputs raw12 padded to 16bit uncompressed (like SDR + HDM),
		   so configuration should be like in SDR */
		if (!isp_dev->hdr_decompression_enabled) {
			hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_ACQ_PROP, &isp_acq_prop);
			/* First clear the current pin_map mode, then set to SDR mode */
			isp_acq_prop &= ~ISP_ACQ_PROP_PINMAP_MASK;
			isp_acq_prop |= ISP_ACQ_PROP_SDR_PINMAP_MASK;
			isp_acq_prop &= ~ISP_ACQ_PROP_HDR_EN_MASK;
			hailo15_isp_write_vdid_reg(isp_dev, vdid, ISP_ACQ_PROP, isp_acq_prop);
		}
	}

	if (rd_cfg_for_mcm_mode == MCM_RD_FMT_INVALID) {
		pr_err("Invalid MCM mode %d\n", isp_dev->mcm_mode);
		return;
	}

	width = isp_dev->input_fmt[HAILO15_VID_GRP_TO_ISP_SINK_PAD(grp_id)].format.width;
	height = isp_dev->input_fmt[HAILO15_VID_GRP_TO_ISP_SINK_PAD(grp_id)].format.height;

	/* In raw12 packed we only have 1.5 bytes per pixel */
	llength = isp_dev->mcm_mode == ISP_MCM_MODE_RAW12_PACKED ?
		(width * 3) / 2 : width * sizeof(uint16_t);

	hailo15_isp_read_vdid_reg(isp_dev, vdid, MI_CTRL, &mi_ctrl);
	mi_ctrl &= ~MI_CTRL_MCM_RAW_RDMA_START_CON;
	mi_ctrl |= MI_CTRL_MCM_RAW_RDMA_PATH_ENABLE;
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_CTRL, mi_ctrl);
	hailo15_isp_read_vdid_reg(isp_dev, vdid, MI_MCM_CTRL, &mi_mcm_ctrl);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_MCM_DMA_RAW_PIC_WIDTH, width);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_MCM_DMA_RAW_PIC_LLENGTH, llength);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_MCM_DMA_RAW_PIC_LVAL, llength);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_MCM_DMA_RAW_PIC_SIZE, llength*height);

	hailo15_isp_write_control_reg(isp_dev, MCM_RD_CFG, rd_cfg_for_mcm_mode);

	hailo15_isp_read_vdid_reg(isp_dev, vdid, MI_MCM_FMT, &mi_mcm_fmt);
	mi_mcm_fmt |= isp_dev->mcm_mode == ISP_MCM_MODE_RAW12_PACKED ? MCM_RD_RAW12_BIT : MCM_RD_RAW16_BIT;
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_MCM_FMT, mi_mcm_fmt);

	/* pin_mapping in MCM raw12 should be 0 (undo append 4 zeros mode)*/
	if (isp_dev->mcm_mode == ISP_MCM_MODE_RAW12_PACKED) {
		hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_ACQ_PROP, &isp_acq_prop);
		isp_acq_prop &= ~BIT(18);
		hailo15_isp_write_vdid_reg(isp_dev, vdid, ISP_ACQ_PROP, isp_acq_prop);
	}

	if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) {
		mi_mcm_ctrl |= MCM_WR_AUTO_UPDATE;
	}
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_MCM_CTRL, mi_mcm_ctrl | MCM_RD_CFG_UPD);
	hailo15_isp_read_vdid_reg(isp_dev, vdid, MI_IMSC, &mi_imsc);
	mi_imsc |= MCM_DMA_RAW_READY;
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_IMSC, mi_imsc);
	if (isp_dev->mcm_mode == ISP_MCM_MODE_STITCHING ||
	    isp_dev->mcm_mode == ISP_MCM_MODE_INJECTION) {
		hailo15_isp_write_control_reg(isp_dev, MCM_RETIMING0, MCM_RETIMING_VSYNC);
		hailo15_isp_write_control_reg(isp_dev, MCM_RETIMING1, MCM_RETIMING_HSYNC);
	}
}

void hailo15_isp_configure_frame_size(struct hailo15_isp_device *isp_dev,
				      int grp_id)
{
	const struct hailo15_video_fmt *format;
	uint32_t line_length;
	int bytesperline;
	uint32_t y_size_init_addr, cb_size_init_addr, cr_size_init_addr;
	int path = HAILO15_VID_GRP_TO_ISP_PATH(grp_id);
	uint8_t vdid = HAILO15_VID_GRP_TO_VDID(grp_id);

	if (path >= ISP_MAX_PATH || path < 0) {
		pr_err("%s - invalid path: %d\n", __func__, path);
		return;
	}

	if(isp_dev->mcm_mode){
		hailo15_isp_configure_mcm_rdma(isp_dev, grp_id);
		if(path == ISP_MCM_IN)
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

	hailo15_isp_write_vdid_reg(isp_dev, vdid, y_size_init_addr,
			      hailo15_plane_get_sizeimage(
				      format, isp_dev->fmt[path].format.height,
				      bytesperline, PLANE_Y));

	bytesperline =
		hailo15_plane_get_bytesperline(format, line_length, PLANE_CB);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, cb_size_init_addr,
			      hailo15_plane_get_sizeimage(
				      format, isp_dev->fmt[path].format.height,
				      bytesperline, PLANE_CB));

	bytesperline =
		hailo15_plane_get_bytesperline(format, line_length, PLANE_CR);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, cr_size_init_addr,
			      hailo15_plane_get_sizeimage(
				      format, isp_dev->fmt[path].format.height,
				      bytesperline, PLANE_CR));
}
EXPORT_SYMBOL(hailo15_isp_configure_frame_size);

int hailo15_isp_is_path_enabled(struct hailo15_isp_device *isp_dev, int path)
{
	uint32_t enabled_mask;
	uint32_t mi_ctrl;
	uint8_t vdid;

	if (path >= ISP_MAX_PATH || path < 0)
		return 0;

	if(path == ISP_MCM_IN)
		return isp_dev->rdma_enable;

	vdid = HAILO15_VID_GRP_TO_VDID(isp_dev->cur_buf_path);
	enabled_mask = path == ISP_MP ? MP_YCBCR_PATH_ENABLE_MASK :
					SP2_YCBCR_PATH_ENABLE_MASK;

	hailo15_isp_read_vdid_reg(isp_dev, vdid, MI_CTRL, &mi_ctrl);

	return !!(mi_ctrl & enabled_mask);
}
static inline void
hailo15_isp_configure_rdma_frame_base(struct hailo15_isp_device *isp_dev,
					dma_addr_t addr[FMT_MAX_PLANES], uint8_t vdid,
					struct hailo15_buf_timing *timing)
{
	uint32_t mi_mcm_ctrl;
	int ret;

	hailo15_isp_write_vdid_reg(isp_dev, vdid, MIV2_MCM_DMA_RAW_PIC_START_AD, addr[PLANE_Y]);
	hailo15_isp_read_vdid_reg(isp_dev, vdid, MI_MCM_CTRL, &mi_mcm_ctrl);
	mi_mcm_ctrl |= MCM_RD_CFG_UPD;
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_MCM_CTRL, mi_mcm_ctrl);

	/* FE switch will write to MI_CTRL with MI_CTRL_MCM_RAW_RDMA_START to trigger MCM RDMA start
	   It will do so after the FE is finished (fe_switch wait for completion, and then executes this write)
	*/
	if(isp_dev->fe_enable) {
		if (timing)
			timing->fe_switch_start = ktime_get();

		ret = isp_dev->fe_dev->fe_switch(isp_dev->fe_dev, &isp_dev->fe_switch);
		if (ret) {
			pr_err("%s - failed to switch FE, ret = %d\n", __func__, ret);
			return;
		}
		if (timing)
			timing->fe_switch_end = ktime_get();
	}
}

void
hailo15_isp_configure_mcm_raw_frame_base(struct hailo15_isp_device *isp_dev,
				    dma_addr_t addr[FMT_MAX_PLANES], uint8_t vdid)
{
	uint32_t raw_base_reg = vdid == 0 ? MI_MCM_RAW0_BASE_AD_INIT : MI_MCM_RAW1_BASE_AD_INIT;
	hailo15_isp_write_control_reg(isp_dev, raw_base_reg, addr[PLANE_Y]);
}
EXPORT_SYMBOL(hailo15_isp_configure_mcm_raw_frame_base);

static inline void
hailo15_isp_configure_mp_frame_base(struct hailo15_isp_device *isp_dev,
				    dma_addr_t addr[FMT_MAX_PLANES], uint8_t vdid)
{
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MIV2_MP_Y_BASE_AD_INIT, addr[PLANE_Y]);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MIV2_MP_CB_BASE_AD_INIT, addr[PLANE_CB]);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MIV2_MP_CR_BASE_AD_INIT, addr[PLANE_CR]);
}

static inline void
hailo15_isp_configure_sp2_frame_base(struct hailo15_isp_device *isp_dev,
				     dma_addr_t addr[FMT_MAX_PLANES], uint8_t vdid)
{
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MIV2_SP2_Y_BASE_AD_INIT, addr[PLANE_Y]);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MIV2_SP2_CB_BASE_AD_INIT,
			      addr[PLANE_CB]);
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MIV2_SP2_CR_BASE_AD_INIT,
			      addr[PLANE_CR]);
}

void hailo15_isp_configure_frame_base(struct hailo15_isp_device *isp_dev,
				      dma_addr_t addr[FMT_MAX_PLANES],
				      int grp_id)
{
	int pad = HAILO15_VID_GRP_TO_ISP_PATH(grp_id);
	uint8_t vdid = HAILO15_VID_GRP_TO_VDID(grp_id);
	struct hailo15_buffer *buf = isp_dev->cur_buf[grp_id];

	if (buf){
		trace_isp_output_buffer_process(buf->vb.vb2_buf.index, grp_id,
						buf->dma[PLANE_Y]);
	}

	if (ISP_MP == pad) {
		hailo15_isp_configure_mp_frame_base(isp_dev, addr, vdid);
	}

	if (ISP_SP2 == pad) {
		hailo15_isp_configure_sp2_frame_base(isp_dev, addr, vdid);
	}

	if(ISP_MCM_IN == pad){
		hailo15_isp_configure_rdma_frame_base(isp_dev, addr, vdid,
			buf ? &buf->timing : NULL);
	}
}
EXPORT_SYMBOL(hailo15_isp_configure_frame_base);

static void hailo15_isp_post_irq_event(struct hailo15_isp_device *isp_dev,
				       int irq_id)
{
	struct video_device *vdev = isp_dev->sd.devnode;
	struct v4l2_event event;
	struct hailo15_isp_irq_status_event *irq_event;
	uint32_t grp_id = isp_dev->cur_buf_path;
	uint8_t vdid = HAILO15_VID_GRP_TO_VDID(grp_id);

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

	irq_event->port = vdid;

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

static inline int __hailo15_isp_frame_rx_rdma_ready(int miv2_mis)
{
	return !!(miv2_mis & MIV2_MCM_DMA_RAW_READY_MASK);
}

static inline int __hailo15_isp_frame_rx_sp2_raw(int miv2_mis)
{
	return !!(miv2_mis & MIV2_SP2_RAW_FRAME_END);
}

/* Drop excess buffers from a sensor's full queue, keeping only 1 buffer */
static void hailo15_isp_drop_excess_buffers(struct hailo15_isp_device *isp_dev,
					     uint8_t sensor_vdid)
{
	struct list_head *raw_full_queue;
	struct list_head *raw_empty_queue;
	struct mutex *raw_full_lock;
	struct mutex *raw_empty_lock;
	struct hailo15_isp_raw_buf *buf, *next_buf;
	LIST_HEAD(temp_list);
	int kept_count = 0;
	int dropped_count = 0;
	int count_before, count_after;
	int other_vdid = (sensor_vdid + 1) % HAILO15_ISP_SINK_PAD_MAX;
	int count_other;

	raw_full_queue = hailo15_isp_get_full_queue(isp_dev, sensor_vdid);
	raw_empty_queue = hailo15_isp_get_empty_queue(isp_dev, sensor_vdid);
	raw_full_lock = hailo15_isp_get_full_lock(isp_dev, sensor_vdid);
	raw_empty_lock = hailo15_isp_get_empty_lock(isp_dev, sensor_vdid);

	if (!raw_full_queue || !raw_empty_queue || !raw_full_lock || !raw_empty_lock) {
		pr_err("%s - failed to get queues/locks for sensor %d\n", __func__, sensor_vdid);
		return;
	}

	/* Lock full queue only to remove excess buffers to the temp list */
	mutex_lock(raw_full_lock);

	/* Get counts before dropping */
	smp_mb();
	count_before = atomic_read(&isp_dev->full_queue_count[sensor_vdid]);
	count_other = atomic_read(&isp_dev->full_queue_count[other_vdid]);

	list_for_each_entry_safe(buf, next_buf, raw_full_queue, list) {
		/* Keep the first buffer, move the rest to temp list */
		if (kept_count == 0) {
			kept_count++;
			continue;
		}
		list_del(&buf->list);
		atomic_dec(&isp_dev->full_queue_count[sensor_vdid]);
		trace_isp_raw_buffer_full_q_out(sensor_vdid, buf->index, buf->phys_addr);
		list_add_tail(&buf->list, &temp_list);
		dropped_count++;
	}
	mutex_unlock(raw_full_lock);

	/* Now transfer from temp_list to empty queue, only need to lock empty lock */
	if (!list_empty(&temp_list)) {
		mutex_lock(raw_empty_lock);
		list_for_each_entry_safe(buf, next_buf, &temp_list, list) {
			list_del(&buf->list);
			trace_isp_raw_buffer_empty_q_in(sensor_vdid, buf->index, buf->phys_addr);
			list_add_tail(&buf->list, raw_empty_queue);
		}
		mutex_unlock(raw_empty_lock);
	}

	/* Get count after dropping */
	count_after = atomic_read(&isp_dev->full_queue_count[sensor_vdid]);

	/* Trace the drop event */
	if (dropped_count > 0) {
		trace_hailo15_dual_sensor_buffers_dropped(sensor_vdid, dropped_count,
							  count_before, count_after,
							  count_other);
	}
}

static void hailo15_isp_handle_multi_sensor_frame_rx(struct hailo15_isp_device *isp_dev)
{
	uint8_t vdid = 0;
	uint8_t next_vdid = 0;
	struct list_head *raw_empty_queue;
	struct list_head *raw_full_queue;
	struct mutex *raw_empty_lock;
	struct mutex *raw_full_lock;
	unsigned long flags;
	bool stop_toggle = false;
	int ret = 0;

	if(!isp_dev->fe_enable) {
		pr_warn("%s - fe_enable is not enabled\n", __func__);
		return;
	}

	/* get the vdid of the current buffer we are done processing */
	vdid = HAILO15_VID_GRP_TO_VDID(isp_dev->cur_buf_path);

	/* get the vdid of the next buffer we want to process as we set in the previous frame */
	next_vdid = isp_dev->fe_switch.next_vdid[1];

	raw_empty_queue = hailo15_isp_get_empty_queue(isp_dev, vdid);
	raw_empty_lock = hailo15_isp_get_empty_lock(isp_dev, vdid);
	raw_full_queue = hailo15_isp_get_full_queue(isp_dev, next_vdid);
	raw_full_lock = hailo15_isp_get_full_lock(isp_dev, next_vdid);
	if (!raw_empty_queue || !raw_empty_lock || !raw_full_queue || !raw_full_lock) {
		pr_err("%s - no empty or full queue or lock for vdid: %d\n", __func__, vdid);
		return;
	}

	/* return current rdma buf to empty list because we are done processing it */
	mutex_lock(raw_empty_lock);
	trace_isp_raw_buffer_empty_q_in(vdid,
					isp_dev->cur_rdma_buf->index,
					isp_dev->cur_rdma_buf->phys_addr);
	list_add_tail(&isp_dev->cur_rdma_buf->list, raw_empty_queue);
	mutex_unlock(raw_empty_lock);

	/* wait until there is an available full buffer from the sensor we want */
	ret = wait_event_interruptible(
		isp_dev->raw_frame_available_wait_q,
		isp_dev->raw_frame_available[next_vdid] ||
		!isp_dev->stream_enabled[next_vdid == 0 ? HAILO15_ISP_SINK_PAD_S0 : HAILO15_ISP_SINK_PAD_S1]);

	if (ret == -ERESTARTSYS) {
		pr_warn("%s - wait_event_interruptible_timeout got interrupted\n", __func__);
		return;
	}

	/* if both sensors are disabled, don't do anything */
	spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
	if (!isp_dev->stream_enabled[HAILO15_ISP_SINK_PAD_S0] && !isp_dev->stream_enabled[HAILO15_ISP_SINK_PAD_S1]) {
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);
		return;
	}
	/* if the sensor we want is disabled, we must push 1 more frame because we already indicated it's the next one (in the previous frame) */
	if (!isp_dev->stream_enabled[next_vdid == 0 ? HAILO15_ISP_SINK_PAD_S0 : HAILO15_ISP_SINK_PAD_S1]) {
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);
		stop_toggle = true;

		/* if there is no raw frame available, we push from the empty queue (because the sensor might already be disabled) */
		if (!isp_dev->raw_frame_available[next_vdid]) {
			raw_full_queue = hailo15_isp_get_empty_queue(isp_dev, next_vdid);
			raw_full_lock = hailo15_isp_get_empty_lock(isp_dev, next_vdid);
			if (!raw_full_queue || !raw_full_lock) {
				pr_err("%s - no empty queue or lock for vdid: %d\n", __func__, next_vdid);
				return;
			}
		}
	} else {
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);
	}

	/* Check if the sensor we're about to process has a lead of more than 1 buffer */
	{
		int count_next, count_other;
		int other_vdid = (next_vdid + 1) % HAILO15_ISP_SINK_PAD_MAX;

		smp_mb(); /* Ensure all atomic writes are visible */
		count_next = atomic_read(&isp_dev->full_queue_count[next_vdid]);
		count_other = atomic_read(&isp_dev->full_queue_count[other_vdid]);

		/* If next_vdid has more than 1 buffer lead, drop excess buffers */
		if (count_next - count_other > 1) {
			hailo15_isp_drop_excess_buffers(isp_dev, next_vdid);
		}
	}

	/* get a full buffer from the raw full queue */
	mutex_lock(raw_full_lock);
	isp_dev->cur_rdma_buf = list_first_entry_or_null(raw_full_queue, struct hailo15_isp_raw_buf, list);
	if (isp_dev->cur_rdma_buf) {
		list_del(&isp_dev->cur_rdma_buf->list);
		{
			int old_count;
			int new_count;
			old_count = atomic_read(&isp_dev->full_queue_count[next_vdid]);
			atomic_dec(&isp_dev->full_queue_count[next_vdid]);
			new_count = atomic_read(&isp_dev->full_queue_count[next_vdid]);
			trace_hailo15_dual_sensor_full_q_count_dec(next_vdid, old_count, new_count);
		}
		trace_isp_raw_buffer_full_q_out(next_vdid,
						isp_dev->cur_rdma_buf->index,
						isp_dev->cur_rdma_buf->phys_addr);
		isp_dev->raw_frame_available[next_vdid] = !list_empty(raw_full_queue);
		mutex_unlock(raw_full_lock);

		/* always set the next vdid to the one we configured to the fe in the previous frame,
			and the second dependent on the toggle_sensors flag */
		isp_dev->fe_switch.next_vdid[0] = next_vdid;
		isp_dev->fe_switch.next_vdid[1] = isp_dev->toggle_sensors ? (next_vdid + 1) % HAILO15_ISP_SINK_PAD_MAX : next_vdid;
		isp_dev->cur_buf_path = isp_dev->fe_switch.next_vdid[0] == 0 ? HAILO15_VID_GRP_SX_CSI0_ISP_MP :
			HAILO15_VID_GRP_SX_CSI1_ISP_MP;
		hailo15_isp_configure_rdma_frame_base(isp_dev, &(isp_dev->cur_rdma_buf->phys_addr),
			isp_dev->fe_switch.next_vdid[0], NULL);

		/* if we were in toggle state before and we need to stop, we just push 1 more frame
			so we can indicate that toggle is disabled and stop the stream */
		if (stop_toggle) {
			isp_dev->toggle_sensors = false;
			wake_up_interruptible(&isp_dev->toggle_sensors_wait_q);
		}
	} else {
		mutex_unlock(raw_full_lock);
		pr_err("%s - no full buffer ready after wait, next_vdid[0]: %d, next_vdid[1]: %d, stream_enabled[0]: %d, stream_enabled[1]: %d\n",
			__func__, isp_dev->fe_switch.next_vdid[0], isp_dev->fe_switch.next_vdid[1], isp_dev->stream_enabled[0], isp_dev->stream_enabled[1]);
	}
}

static void hailo15_isp_handle_frame_rx_rdma(struct hailo15_isp_device *isp_dev,
					     int irq_status)
{
	int sink_pad;
	struct hailo15_buffer *buf;

	if(!isp_dev->mcm_mode){
		return;
	}

	sink_pad = HAILO15_VID_GRP_TO_ISP_SINK_PAD(isp_dev->cur_buf_path);
	buf = isp_dev->cur_buf[HAILO15_VID_GRP_MCM_IN];

	if(__hailo15_isp_frame_rx_rdma_ready(irq_status)){
		isp_dev->dma_ready = 1;
		if (buf)
			buf->timing.rdma_ready = ktime_get();
	}
	if(__hailo15_isp_frame_rx_mp(irq_status) || __hailo15_isp_frame_rx_sp2(irq_status)){
		isp_dev->frame_end = 1;
		if (buf)
			buf->timing.frame_end = ktime_get();
	}
	if(isp_dev->frame_end){
		atomic_set(&isp_dev->frame_received[sink_pad], 1);
	}
	if(isp_dev->frame_end && isp_dev->dma_ready &&  (!isp_dev->fe_enable || isp_dev->fe_ready)){
		isp_dev->dma_ready = 0;
		isp_dev->frame_end = 0;
		isp_dev->fe_ready = 0;

		switch (isp_dev->mcm_mode) {
			case ISP_MCM_MODE_STITCHING:
			case ISP_MCM_MODE_RAW12_PACKED:
			case ISP_MCM_MODE_RAW_WRITE:
				/* do rx_rdma */
				hailo15_isp_buffer_done(isp_dev, HAILO15_VID_GRP_MCM_IN);
				break;
			case ISP_MCM_MODE_INJECTION:
				mutex_lock(&isp_dev->ready_lock);
				if (isp_dev->output_ready){
					/* do rx_rdma */
					hailo15_isp_buffer_done(isp_dev, HAILO15_VID_GRP_MCM_IN);
				} else {
					/* indicates that the MCM is waiting for the MP to have a buffer ready */
					isp_dev->mcm_waiting = 1;
				}
				mutex_unlock(&isp_dev->ready_lock);
				break;
			case ISP_MCM_MODE_MULTI_SENSOR:
				hailo15_isp_handle_multi_sensor_frame_rx(isp_dev);
				break;
			default:
				pr_err("%s - invalid mcm mode: %d\n", __func__, isp_dev->mcm_mode);
				break;
		}
	}
}

static void hailo15_isp_handle_frame_rx_mp(struct hailo15_isp_device *isp_dev,
					   int irq_status)
{
	int sink_pad;

	if (!__hailo15_isp_frame_rx_mp(irq_status))
		return;

	if (isp_dev->mi_stopped[ISP_MP] && !isp_dev->mcm_mode)
		return;

	sink_pad = HAILO15_VID_GRP_TO_ISP_SINK_PAD(isp_dev->cur_buf_path);
	atomic_set(&isp_dev->frame_received[sink_pad], 1);

	/*do_rx_mp*/
	hailo15_isp_buffer_done(isp_dev, isp_dev->cur_buf_path);
}

static void hailo15_isp_handle_frame_rx_sp2(struct hailo15_isp_device *isp_dev,
					    int irq_status)
{
	int sink_pad;
	if (!__hailo15_isp_frame_rx_sp2(irq_status))
		return;

	if (isp_dev->mi_stopped[ISP_SP2] && !isp_dev->mcm_mode)
		return;

	sink_pad = HAILO15_VID_GRP_TO_ISP_SINK_PAD(isp_dev->cur_buf_path);
	atomic_set(&isp_dev->frame_received[sink_pad], 1);

	/*do_rx_sp2*/
	hailo15_isp_buffer_done(isp_dev, HAILO15_VID_GRP_MCM_IN);
}

void hailo15_isp_handle_mcm_raw_frame_rx(struct work_struct *work)
{
	struct hailo15_irq_deffered_work *irq_deffered_work =
		(struct hailo15_irq_deffered_work *)container_of(
			work, struct hailo15_irq_deffered_work, irq_deffered_w);
	struct hailo15_isp_device *isp_dev = irq_deffered_work->isp_dev;
	struct hailo15_isp_raw_buf *first_sensor_buf;
	struct list_head *raw_full_queue;
	struct list_head *raw_empty_queue;
	struct mutex *raw_empty_lock;
	struct mutex *raw_full_lock;
	unsigned int isp_port;
	uint32_t irq_status = irq_deffered_work->irq_status;
	bool raw_frame_end[HAILO15_ISP_SINK_PAD_MAX] = {false};

	if (irq_status & (MIV2_MCM_RAW0_FRAME_END | MIV2_MCM_RAW1_FRAME_END)) {
		if (irq_status & MIV2_MCM_RAW0_FRAME_END) {
			raw_frame_end[HAILO15_ISP_SINK_PAD_S0] = true;
		}
		if (irq_status & MIV2_MCM_RAW1_FRAME_END) {
			raw_frame_end[HAILO15_ISP_SINK_PAD_S1] = true;
		}
		for (isp_port = 0; isp_port < HAILO15_ISP_SINK_PAD_MAX; isp_port++) {
			if (!raw_frame_end[isp_port])
				continue;

			raw_full_queue = hailo15_isp_get_full_queue(isp_dev, isp_port);
			raw_empty_queue = hailo15_isp_get_empty_queue(isp_dev, isp_port);
			raw_empty_lock = hailo15_isp_get_empty_lock(isp_dev, isp_port);
			raw_full_lock = hailo15_isp_get_full_lock(isp_dev, isp_port);
			if (!raw_full_queue || !raw_empty_queue || !raw_empty_lock || !raw_full_lock) {
				pr_err("%s - no full or empty queues for raw %d\n", __func__, isp_port);
				continue;
			}

			/* add cur raw buf to full queue and setup next empty */
			mutex_lock(raw_full_lock);
			if (!isp_dev->cur_raw_buf[isp_port]) {
				mutex_unlock(raw_full_lock);
				pr_err_ratelimited("%s - no cur_raw_buf for raw %d, frame will be dropped\n", __func__, isp_port);
				continue;
			}
			trace_isp_raw_buffer_full_q_in(isp_port,
							 isp_dev->cur_raw_buf[isp_port]->index,
							 isp_dev->cur_raw_buf[isp_port]->phys_addr);
			list_add_tail(&isp_dev->cur_raw_buf[isp_port]->list, raw_full_queue);
			atomic_inc(&isp_dev->full_queue_count[isp_port]);
			isp_dev->raw_frame_available[isp_port] = true;
			mutex_unlock(raw_full_lock);

			/* incase rdma is waiting for raw frame to be availble */
			if (atomic_read(&isp_dev->first_rdma_done))
				wake_up_interruptible(&isp_dev->raw_frame_available_wait_q);

			/* setup an empty buf for the next raw frame from the sensor */
			mutex_lock(raw_empty_lock);
			isp_dev->cur_raw_buf[isp_port] = list_first_entry_or_null(raw_empty_queue, struct hailo15_isp_raw_buf, list);
			if (!isp_dev->cur_raw_buf[isp_port]) {
				mutex_unlock(raw_empty_lock);
				pr_err_ratelimited("%s - no empty buffers for raw %d, cur_buf_path: %d\n", __func__, isp_port, isp_dev->cur_buf_path);
			} else {
				/* remove the buffer from the empty queue */
				list_del(&isp_dev->cur_raw_buf[isp_port]->list);
				trace_isp_raw_buffer_empty_q_out(isp_port,
								 isp_dev->cur_raw_buf[isp_port]->index,
								 isp_dev->cur_raw_buf[isp_port]->phys_addr);
				mutex_unlock(raw_empty_lock);
				hailo15_isp_configure_mcm_raw_frame_base(isp_dev, &isp_dev->cur_raw_buf[isp_port]->phys_addr, isp_port);
			}

			if (!atomic_cmpxchg(&isp_dev->first_rdma_done, 0, 1)) {
				mutex_lock(raw_full_lock);
				first_sensor_buf = list_first_entry_or_null(raw_full_queue, struct hailo15_isp_raw_buf, list);
				if (!first_sensor_buf) {
					mutex_unlock(raw_full_lock);
					pr_err_ratelimited("%s - no full buffers for first sensor\n", __func__);
				} else {
					list_del(&first_sensor_buf->list);
					atomic_dec(&isp_dev->full_queue_count[isp_port]);
					trace_isp_raw_buffer_full_q_out(isp_port,
									first_sensor_buf->index,
									first_sensor_buf->phys_addr);
					isp_dev->raw_frame_available[isp_port] = false;
					mutex_unlock(raw_full_lock);

					/* first rdma, no race condition with mcm_wr_raw_wq */
					isp_dev->cur_rdma_buf = first_sensor_buf;
					isp_dev->fe_switch.next_vdid[0] = isp_port;
					isp_dev->fe_switch.next_vdid[1] = isp_port;
					isp_dev->cur_buf_path = isp_port == HAILO15_ISP_SINK_PAD_S0 ? HAILO15_VID_GRP_SX_CSI0_ISP_MP :
						HAILO15_VID_GRP_SX_CSI1_ISP_MP;
					hailo15_isp_configure_rdma_frame_base(isp_dev, &first_sensor_buf->phys_addr, isp_port, NULL);
				}
			}
		}
	}
	kfree(irq_deffered_work);
}

static void hailo15_isp_mcm_raw_wr_buffer_done_work(struct work_struct *work)
{
	struct hailo15_mcm_raw_wr_buffer_done_work *buf_done_work =
		container_of(work, struct hailo15_mcm_raw_wr_buffer_done_work, work);
	struct hailo15_isp_device *isp_dev = buf_done_work->isp_dev;
	unsigned long flags;

	if (!isp_dev) {
		pr_err_ratelimited("%s: invalid isp_dev\n", __func__);
		goto out;
	}

	/* Trace that work started processing */
	trace_isp_mcm_raw_wr_frame_irq_work(
		buf_done_work->grp_id,
		ktime_get_ns());

	/* Check if stream is still active by verifying stream_enabled for sensor 0's sink pad,
	 * sensor 1 not supported yet.
	 */
	spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
	if (!isp_dev->stream_enabled[HAILO15_ISP_SINK_PAD_S0]) {
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);
		pr_debug_ratelimited("%s: stream stopped, skipping buffer_done\n", __func__);
		goto out;
	}
	spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);

	hailo15_isp_buffer_done(isp_dev, buf_done_work->grp_id);

out:
	kfree(buf_done_work);
}

static void hailo15_isp_mis_work(struct work_struct *work)
{
	struct hailo15_irq_deffered_work *irq_deffered_work =
		(struct hailo15_irq_deffered_work *)container_of(
			work, struct hailo15_irq_deffered_work, irq_deffered_w);
	struct hailo15_isp_device *isp_dev = irq_deffered_work->isp_dev;
	uint8_t vdid = 0;
	int ret;
	uint32_t isp_imsc;
	uint32_t delta_h, delta_v;

	if (irq_deffered_work->irq_status & ISP_MIS_DATA_LOSS) {
		if (!isp_dev->mcm_mode) {
			pr_err("fatal: isp data loss detected!\n");
			ret = hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_IMSC, &isp_imsc);
			if (ret)
				goto exit;
			isp_imsc &= ~(ISP_MIS_DATA_LOSS);
			hailo15_isp_write_vdid_reg(isp_dev, vdid, ISP_IMSC, isp_imsc);
		} else {
			pr_err_ratelimited("isp data loss detected in mcm mode %d\n", isp_dev->mcm_mode);
		}
	}

	if (isp_dev->irq_status.isp_mis & (ISP_MIS_VSM_DONE)) {
		// currently only supported on vdid 0
		ret = hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_VSM_DELTA_H, &delta_h);
		if (ret)
			goto exit;
		ret = hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_VSM_DELTA_V, &delta_v);
		if (ret)
			goto exit;

		isp_dev->current_vsm.dx = hailo15_isp_process_delta(delta_h);
		isp_dev->current_vsm.dy = hailo15_isp_process_delta(delta_v);
	}

exit:
	kfree(irq_deffered_work);
}

void hailo15_isp_stitcher_stats_work(struct work_struct *work)
{
	struct hailo15_isp_device *isp_dev =
		container_of(work, struct hailo15_isp_device, stitcher_stats_work);
	uint8_t local_buf[ISP_HDR_EXP_STATISTICS_MAX];
	uint32_t n = READ_ONCE(isp_dev->stitcher_stats_n);
	uint32_t i;

	if (!n)
		return;

	for (i = 0; i < n; i++)
		local_buf[i] = hailo15_isp_raw_read_reg(
			isp_dev, ISP_HDR_EXP_STATISTICS_BASE + (i * 4)) & 0xFF;

	mutex_lock(&isp_dev->stitcher_stats_lock);
	memcpy(isp_dev->stitcher_stats_buf, local_buf, n);
	mutex_unlock(&isp_dev->stitcher_stats_lock);
}

static bool hailo15_isp_any_stream_enabled(struct hailo15_isp_device *isp_dev)
{
	unsigned long flags;
	bool streaming = false;
	int i;

	spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
	for (i = 0; i < HAILO15_ISP_SINK_PAD_MAX; i++)
		streaming |= isp_dev->stream_enabled[i];
	spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);
	return streaming;
}

/* Configure the HW ISP stitcher statistics path: 5x5 grid window from the
 * given input size, start the measurement block, unmask the per-exposure
 * ready bits in stitching IMSC. Must be called before streaming begins so
 * the writes don't race with the FE stitcher-IMSC save/restore.
 *
 * Example windows (offset always (0,0)):
 *   2592x1944 -> cell 516x384
 *   3840x2160 -> cell 764x428
 */
int hailo15_isp_stitcher_hw_enable(struct hailo15_isp_device *isp_dev,
				    uint32_t width, uint32_t height)
{
	uint32_t cell_w, cell_h, conf, imsc;

	if (width < 20 || height < 20)
		return -EINVAL;

	if (hailo15_isp_any_stream_enabled(isp_dev)) {
		pr_err("%s: refused, stream already enabled\n", __func__);
		return -EBUSY;
	}

	cell_w = ((width / 5) - 1) & ~3U;
	cell_h = ((height / 5) - 1) & ~3U;

	hailo15_isp_raw_write_reg(isp_dev, ISP_HDR_EXP_H_OFFSET, 0);
	hailo15_isp_raw_write_reg(isp_dev, ISP_HDR_EXP_V_OFFSET, 0);
	hailo15_isp_raw_write_reg(isp_dev, ISP_HDR_EXP_H_SIZE,
				   cell_w & ISP_HDR_EXP_H_SIZE_MASK);
	hailo15_isp_raw_write_reg(isp_dev, ISP_HDR_EXP_V_SIZE,
				   cell_h & ISP_HDR_EXP_V_SIZE_MASK);

	conf = hailo15_isp_raw_read_reg(isp_dev, ISP_HDR_EXP_CONF);
	conf |= ISP_HDR_EXP_CONF_START | ISP_HDR_EXP_CONF_SRC_SEL |
		ISP_HDR_EXP_CONF_MEAS_MODE;
	hailo15_isp_raw_write_reg(isp_dev, ISP_HDR_EXP_CONF, conf);

	imsc = hailo15_isp_raw_read_reg(isp_dev, ISP_STITCHING_IMSC);
	imsc |= ISP_STITCHING_IMSC_EXP_STAT_MASK;
	hailo15_isp_raw_write_reg(isp_dev, ISP_STITCHING_IMSC, imsc);
	return 0;
}

/* Counterpart of hailo15_isp_stitcher_hw_enable. Must be called after the
 * last stream stops — same FE stitcher-IMSC race window applies. */
int hailo15_isp_stitcher_hw_disable(struct hailo15_isp_device *isp_dev)
{
	uint32_t conf, imsc;

	if (hailo15_isp_any_stream_enabled(isp_dev)) {
		pr_err("%s: refused, stream still enabled\n", __func__);
		return -EBUSY;
	}

	imsc = hailo15_isp_raw_read_reg(isp_dev, ISP_STITCHING_IMSC);
	imsc &= ~ISP_STITCHING_IMSC_EXP_STAT_MASK;
	hailo15_isp_raw_write_reg(isp_dev, ISP_STITCHING_IMSC, imsc);

	conf = hailo15_isp_raw_read_reg(isp_dev, ISP_HDR_EXP_CONF);
	conf &= ~ISP_HDR_EXP_CONF_START;
	hailo15_isp_raw_write_reg(isp_dev, ISP_HDR_EXP_CONF, conf);
	return 0;
}

static void hailo15_isp_handle_frame_rx_sp2_raw(struct hailo15_isp_device *isp_dev,
					    int irq_status)
{
	uint32_t mi_ctrl;
	uint8_t vdid;

	if (!__hailo15_isp_frame_rx_sp2_raw(irq_status))
		return;

	vdid = HAILO15_VID_GRP_TO_VDID(isp_dev->cur_buf_path);
	hailo15_isp_read_vdid_reg(isp_dev, vdid, MI_CTRL, &mi_ctrl);
	mi_ctrl |= SP2_RAW_RDMA_START | SP2_RAW_RDMA_START_CON;
	hailo15_isp_write_vdid_reg(isp_dev, vdid, MI_CTRL, mi_ctrl);
}

void hailo15_isp_handle_frame_rx(struct work_struct *work)
{
	struct hailo15_irq_deffered_work *irq_deffered_work =
		(struct hailo15_irq_deffered_work *)container_of(
			work, struct hailo15_irq_deffered_work, irq_deffered_w);
	struct hailo15_isp_device *isp_dev = irq_deffered_work->isp_dev;
	uint32_t irq_status = irq_deffered_work->irq_status;
	int sink_pad;
	unsigned long flags;

	sink_pad = HAILO15_VID_GRP_TO_ISP_SINK_PAD(isp_dev->cur_buf_path);

	spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
	if (!isp_dev->stream_enabled[sink_pad]) {
		spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);
		pr_debug_ratelimited("%s: stream stopped for pad %d, skipping buffer_done\n",
				     __func__, sink_pad);
		goto out;
	}
	spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);

	hailo15_isp_handle_frame_rx_sp2_raw(isp_dev, irq_status);
	hailo15_isp_handle_frame_rx_mp(isp_dev, irq_status);
	hailo15_isp_handle_frame_rx_sp2(isp_dev, irq_status);
	hailo15_isp_handle_frame_rx_rdma(isp_dev, irq_status);

out:
	atomic_set(&isp_dev->buf_done_ready, 1);
	wake_up_interruptible_all(&isp_dev->buf_done_wait_q);
	kfree(irq_deffered_work);
}

static void hailo15_isp_handle_afm_int(struct work_struct *work)
{
	struct hailo15_irq_deffered_work *irq_deffered_work =
		(struct hailo15_irq_deffered_work *)container_of(
			work, struct hailo15_irq_deffered_work, irq_deffered_w);
	struct hailo15_isp_device *isp_dev = irq_deffered_work->isp_dev;
	uint32_t sum_a, sum_b, sum_c, lum_a, lum_b, lum_c;
	uint8_t vdid;

	vdid = HAILO15_VID_GRP_TO_VDID(isp_dev->cur_buf_path);

	if (hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_AFM_SUM_A, &sum_a) ||
	    hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_AFM_SUM_B, &sum_b) ||
	    hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_AFM_SUM_C, &sum_c) ||
	    hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_AFM_LUM_A, &lum_a) ||
	    hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_AFM_LUM_B, &lum_b) ||
	    hailo15_isp_read_vdid_reg(isp_dev, vdid, ISP_AFM_LUM_C, &lum_c)) {
		pr_err("%s - Failed to read AFM registers\n", __func__);
		goto exit;
	}

	mutex_lock(&isp_dev->af_kevent->data_lock);
	if (isp_dev->af_kevent->ready == 1) {
		pr_debug("%s - AF event not handled in time, dropping measurements\n",
			__func__);
		mutex_unlock(&isp_dev->af_kevent->data_lock);
		goto exit;
	}

	isp_dev->af_kevent->sum_a = sum_a;
	isp_dev->af_kevent->sum_b = sum_b;
	isp_dev->af_kevent->sum_c = sum_c;
	isp_dev->af_kevent->lum_a = lum_a;
	isp_dev->af_kevent->lum_b = lum_b;
	isp_dev->af_kevent->lum_c = lum_c;
	isp_dev->af_kevent->ready = 1;
	isp_dev->af_kevent->vdid = vdid;
	mutex_unlock(&isp_dev->af_kevent->data_lock);

	wake_up_interruptible_all(&isp_dev->af_kevent->wait_q);

exit:
	kfree(irq_deffered_work);
}

static void hailo15_isp_handle_mcm_multi_sensor_irq(struct hailo15_isp_device *isp_dev,
						     uint32_t masked_mis)
{
	struct hailo15_irq_deffered_work *irq_deffered_work;

	irq_deffered_work = kzalloc(sizeof(struct hailo15_irq_deffered_work), GFP_ATOMIC);
	if (!irq_deffered_work) {
		pr_err_ratelimited("%s[%d]: failed to allocate irq_deffered_work\n", __func__, __LINE__);
		return;
	}
	INIT_WORK(&irq_deffered_work->irq_deffered_w, hailo15_isp_handle_mcm_raw_frame_rx);
	irq_deffered_work->irq_status = masked_mis;
	irq_deffered_work->isp_dev = isp_dev;
	if (!queue_work(isp_dev->mcm_wr_raw_wq, &irq_deffered_work->irq_deffered_w)) {
		pr_err_ratelimited("%s[%d]: failed to queue mcm_wr_raw work\n", __func__, __LINE__);
		kfree(irq_deffered_work);
	}
}

static void hailo15_isp_handle_mcm_raw_write_irq(struct hailo15_isp_device *isp_dev,
						  uint32_t masked_mis)
{
	struct hailo15_mcm_raw_wr_buffer_done_work *buf_done_work;

	/* Currently only handling MIV2_MCM_RAW0_FRAME_END */
	if (!(masked_mis & MIV2_MCM_RAW0_FRAME_END))
		return;

	/* Trace frame end interrupt */
	trace_isp_mcm_raw_wr_frame_end_irq(
		HAILO15_VID_GRP_MCM_RAW_WR,
		ktime_get_ns());

	buf_done_work = kzalloc(sizeof(struct hailo15_mcm_raw_wr_buffer_done_work), GFP_ATOMIC);
	if (!buf_done_work) {
		pr_err_ratelimited("%s[%d]: failed to allocate buffer_done work\n", __func__, __LINE__);
		return;
	}
	buf_done_work->isp_dev = isp_dev;
	buf_done_work->grp_id = HAILO15_VID_GRP_MCM_RAW_WR;
	INIT_WORK(&buf_done_work->work, hailo15_isp_mcm_raw_wr_buffer_done_work);
	if (!queue_work(isp_dev->mcm_wr_raw_wq, &buf_done_work->work)) {
		pr_err_ratelimited("%s[%d]: failed to queue buffer_done work\n", __func__, __LINE__);
		kfree(buf_done_work);
	}
}

static void hailo15_isp_handle_int(struct hailo15_isp_device *isp_dev)
{
	int event_size = 0;
	int raised_irq_count = 0;
	int ret;
	bool fe_requests_skip_other_irqs;
	unsigned long flags;
	struct hailo15_irq_deffered_work *irq_deffered_work;
	uint32_t masked_mis;
	bool stream_exists = false;

	spin_lock_irqsave(&isp_dev->stream_state_lock, flags);
	if (isp_dev->stream_enabled[HAILO15_ISP_SINK_PAD_S0] ||
		isp_dev->stream_enabled[HAILO15_ISP_SINK_PAD_S1]) {
		stream_exists = true;
	}
	spin_unlock_irqrestore(&isp_dev->stream_state_lock, flags);

	memset(&isp_dev->irq_status, 0, sizeof(isp_dev->irq_status));

	ret = isp_dev->fe_dev->fe_dma_irq(isp_dev->fe_dev,
					   &isp_dev->irq_status.isp_fe,
					   &fe_requests_skip_other_irqs);
	if (ret)
		pr_err_ratelimited("fe_dma_irq failed, ret = %d\n", ret);
	if (isp_dev->irq_status.isp_fe) {
		raised_irq_count++;
		if (!isp_dev->fe_enable)
			pr_warn_ratelimited("Received FE interrupt while FE not enabled\n");
		// set status only if fe_dma_irq succeeded
		if (!ret) {
			if (isp_dev->irq_status.isp_fe & FE_INT_CFG_END)
				isp_dev->fe_ready = 1;
		}
	}
	if (fe_requests_skip_other_irqs)
		goto post_irq_events;

	/* clear the hw interrupt - mis is raw cpu read, icr is raw cpu write */
	hailo15_isp_irq_read_control_reg(isp_dev, ISP_MIS, &isp_dev->irq_status.isp_mis);
	hailo15_isp_irq_write_control_reg(isp_dev, ISP_ICR, isp_dev->irq_status.isp_mis);

	if (isp_dev->irq_status.isp_mis != 0) {
		raised_irq_count++;

		if (stream_exists) {
			irq_deffered_work = kzalloc(sizeof(struct hailo15_irq_deffered_work), GFP_ATOMIC);
			if (!irq_deffered_work) {
				pr_err_ratelimited("%s[%d]: failed to allocate isp_mis work\n", __func__, __LINE__);
				return;
			}

			irq_deffered_work->isp_dev = isp_dev;
			irq_deffered_work->irq_status = isp_dev->irq_status.isp_mis;
			INIT_WORK(&irq_deffered_work->irq_deffered_w, hailo15_isp_mis_work);
			if (!queue_work(isp_dev->isp_mis_wq, &irq_deffered_work->irq_deffered_w)) {
				pr_err_ratelimited("%s[%d]: failed to queue isp_mis work\n", __func__, __LINE__);
				kfree(irq_deffered_work);
			}
		}
	}

	if (isp_dev->irq_status.isp_mis &
	    (ISP_MIS_AFM_SUM_OF | ISP_MIS_AFM_LUM_OF)) {
		pr_warn("%s - AFM overflow - mis = 0x%x\n", __func__,
			isp_dev->irq_status.isp_mis);
	} else if (isp_dev->irq_status.isp_mis & (ISP_MIS_AFM_FIN)) {
		if (stream_exists) {
			irq_deffered_work = kzalloc(sizeof(struct hailo15_irq_deffered_work), GFP_ATOMIC);
			if (!irq_deffered_work) {
				pr_err_ratelimited("%s[%d]: failed to allocate isp_mis afm work\n", __func__, __LINE__);
				return;
			}

			irq_deffered_work->isp_dev = isp_dev;
			irq_deffered_work->irq_status = isp_dev->irq_status.isp_mis;
			INIT_WORK(&irq_deffered_work->irq_deffered_w, hailo15_isp_handle_afm_int);
			queue_work(isp_dev->af_wq, &irq_deffered_work->irq_deffered_w);
		}
	}

	hailo15_process_irq_stats_events(isp_dev, HAILO15_ISP_IRQ_EVENT_ISP_MIS, isp_dev->irq_status.isp_mis);

	hailo15_isp_irq_read_control_reg(isp_dev, ISP_STITCHING_MIS,
					 &isp_dev->irq_status.isp_stitching_mis);
	hailo15_isp_irq_write_control_reg(isp_dev, ISP_STITCHING_ICR,
					  isp_dev->irq_status.isp_stitching_mis);

	if (isp_dev->irq_status.isp_stitching_mis & ISP_STITCHING_MIS_ERR_MASK) {
		pr_err_ratelimited("hailo15_isp: stitching error IRQ, mis=0x%x (exp_err1=%d exp_err2=%d fifo_empty=%d)\n",
			isp_dev->irq_status.isp_stitching_mis,
			!!(isp_dev->irq_status.isp_stitching_mis & BIT(0)),
			!!(isp_dev->irq_status.isp_stitching_mis & BIT(1)),
			!!(isp_dev->irq_status.isp_stitching_mis & BIT(2)));
	}

	if (READ_ONCE(isp_dev->stitcher_stats_enable) &&
	    (isp_dev->irq_status.isp_stitching_mis &
	     isp_dev->stitcher_ready_mask))
		queue_work(isp_dev->stitcher_stats_wq,
			   &isp_dev->stitcher_stats_work);

	hailo15_isp_irq_read_control_reg(isp_dev, MIV2_MIS, &isp_dev->irq_status.isp_miv2_mis);
	hailo15_isp_irq_write_control_reg(isp_dev, MIV2_ICR, isp_dev->irq_status.isp_miv2_mis);

	/* queue raw frame mcm work */
	masked_mis = isp_dev->irq_status.isp_miv2_mis & (MIV2_MCM_RAW0_FRAME_END | MIV2_MCM_RAW1_FRAME_END);
	if (masked_mis != 0 && stream_exists) {
		/* Route interrupt based on MCM mode */
		if (isp_dev->mcm_mode == ISP_MCM_MODE_MULTI_SENSOR) {
			hailo15_isp_handle_mcm_multi_sensor_irq(isp_dev, masked_mis);
		} else if (isp_dev->mcm_mode == ISP_MCM_MODE_RAW_WRITE) {
			hailo15_isp_handle_mcm_raw_write_irq(isp_dev, masked_mis);
		}
	}

	/* queue rest of miv2 work */
	masked_mis = isp_dev->irq_status.isp_miv2_mis &
		(MIV2_MP_YCBCR_FRAME_END_MASK | MIV2_SP2_YCBCR_FRAME_END_MASK |
			MIV2_MCM_DMA_RAW_READY_MASK | MIV2_SP2_RAW_FRAME_END);
	if (masked_mis != 0 && stream_exists) {
		irq_deffered_work = kzalloc(sizeof(struct hailo15_irq_deffered_work), GFP_ATOMIC);
		if (!irq_deffered_work) {
			pr_err_ratelimited("%s[%d]: failed to allocate irq_deffered_work\n", __func__, __LINE__);
			return;
		}
		INIT_WORK(&irq_deffered_work->irq_deffered_w, hailo15_isp_handle_frame_rx);
		irq_deffered_work->irq_status = masked_mis;
		irq_deffered_work->isp_dev = isp_dev;
		if (!queue_work(isp_dev->miv2_mis_wq, &irq_deffered_work->irq_deffered_w)) {
			pr_err_ratelimited("%s[%d]: failed to queue miv2_mis work\n", __func__, __LINE__);
			kfree(irq_deffered_work);
		}
		raised_irq_count++;
	}

	if (isp_dev->irq_status.isp_miv2_mis & MIV2_SP2_RAW_FRAME_END) {
		/* clear sp2 raw frame end irq before posting event to user space */
		isp_dev->irq_status.isp_miv2_mis &= ~MIV2_SP2_RAW_FRAME_END;
	}

	hailo15_process_irq_stats_events(isp_dev, HAILO15_ISP_IRQ_EVENT_MI_MIS, isp_dev->irq_status.isp_miv2_mis);

	/* Clear mcm frame irqs so user space doens't try to handle them */
	if(isp_dev->rdma_enable){
		isp_dev->irq_status.isp_miv2_mis &= ~MIV2_MCM_RAW0_FRAME_END;
		isp_dev->irq_status.isp_miv2_mis &= ~MIV2_MCM_RAW1_FRAME_END;
		isp_dev->irq_status.isp_miv2_mis &= ~MIV2_MCM_DMA_RAW_READY_MASK;
	}

	hailo15_isp_irq_read_control_reg(isp_dev, MIV2_MIS1, &isp_dev->irq_status.isp_miv2_mis1);
	hailo15_isp_irq_write_control_reg(isp_dev, MIV2_ICR1, isp_dev->irq_status.isp_miv2_mis1);
	if (isp_dev->irq_status.isp_miv2_mis1 != 0) {
		raised_irq_count++;
	}

post_irq_events:
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


}

irqreturn_t hailo15_isp_err_irq_process(struct hailo15_isp_device *isp_dev, int irq) {
	u32 errors;
	u32 mask;
	const struct isp_wrapper_config *wrapper_cfg;

	if (!isp_dev) {
		pr_err("isp_dev is null\n");
		return IRQ_NONE;
	}
	wrapper_cfg = isp_dev->wrapper_cfg;
	if (!wrapper_cfg) {
		pr_err("wrapper_cfg is null\n");
		return IRQ_NONE;
	}
	errors = hailo15_isp_wrapper_read_reg(isp_dev, wrapper_cfg->err_int_status_offset);
	/* Ignore if there are no isp errors */
	if (!errors) {
		return IRQ_NONE;
	}
	mask = hailo15_isp_wrapper_read_reg(isp_dev, wrapper_cfg->err_int_mask_offset);
	hailo15_print_irq_error_message((struct err_status_reg *)&isp_dev->wrapper_cfg->isp_err_interrupt_reg, errors & mask, irq);

	/* Clear the error IRQs */
	hailo15_isp_wrapper_write_reg(isp_dev, wrapper_cfg->err_int_w1c_offset, errors);

	/* Clear the current error bits in the mask */
	hailo15_isp_wrapper_write_reg(isp_dev, wrapper_cfg->err_int_mask_offset, mask & ~errors);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL(hailo15_isp_err_irq_process);

irqreturn_t hailo15_isp_irq_process(struct hailo15_isp_device *isp_dev)
{
	hailo15_isp_handle_int(isp_dev);
	return IRQ_HANDLED;
}
EXPORT_SYMBOL(hailo15_isp_irq_process);



MODULE_LICENSE("GPL v2");
