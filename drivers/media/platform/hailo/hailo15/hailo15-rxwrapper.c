#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clk/clk-conf.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>

#include <linux/videodev2.h>
#include <linux/pm_runtime.h>

#include <linux/of_device.h>
#include <linux/sched_clock.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/media-entity.h>
#include <uapi/linux/media-bus-format.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/of_reserved_mem.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/delay.h>
#include <media/v4l2-fwnode.h>
#include "common.h"

#define RXWRAPPER_NUM_PIPES 4

#define RXWRAPPER_CFG_OFFSET 0x0
#define RXWRAPPER_PIPES_INIT_OFFSET 0x4
#define RXWRAPPER_PIPES_DATA_CFG_OFFSET 0x14
#define RXWRAPPER_PIPES_RING_BUFFER_LINE_CNT_OFFSET 0x24
#define RXWRAPPER_PIPES_STRIDE_OFFSET 0x34
#define RXWRAPPER_PIPES_BASE_LOW_OFFSET 0x44
#define RXWRAPPER_PIPES_BASE_HIGH_OFFSET 0x54
#define RXWRAPPER_CSI_IP_CTRL_OFFSET 0x78
#define RXWRAPPER_CSI_OUT_LINE_BUF_CFG_OFFSET 0xcc

#define RXWRAPPER_CFG_CREDIT_HANDLER_EN_OFFSET 0x150
#define RXWRAPPER_CFG_CREDIT_HANDLER_LINE_SIZE_OFFSET 0x160
#define RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_HEIGHT_OFFSET 0x170
#define RXWRAPPER_CFG_CREDIT_HANDLER_BUFFER_FRAMES_OFFSET 0x180
#define RXWRAPPER_CFG_CREDIT_HANDLER_DMA_PAGE_SIZE_OFFSET 0x190
#define RXWRAPPER_CFG_CREDIT_HANDLER_TU_CREDIT_EN_OFFSET 0x1a0
#define RXWRAPPER_CFG_CREDIT_HANDLER_TU_CREDIT_SIZE_OFFSET 0x1b0
#define RXWRAPPER_CFG_CREDIT_HANDLER_TU_SIZE_IN_DMA_PAGES_OFFSET 0x1c0
#define RXWRAPPER_CFG_CREDIT_HANDLER_INT_CREDIT_EN_OFFSET 0x1e0
#define RXWRAPPER_CFG_CREDIT_HANDLER_EXT_UNPROCESSED_CNT_OFFSET 0x1f0
#define RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_DROP_EN_OFFSET 0x200
#define RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_DROP_TH_OFFSET 0x210
#define RXWRAPPER_CFG_CREDIT_HANDLER_ALMOST_FULL_TH_OFFSET 0x220
#define RXWRAPPER_CFG_CREDIT_HANDLER_COUNT_OVERFLOW_FRAMES_OFFSET 0x240
#define RXWRAPPER_STATUS_CREDIT_HANDLER_DROPPED_FRAME_CNT_OFFSET 0x2c0

#define RXWRAPPER_CSI_RX_ERR_IRQ_MASK_OFFSET 0xe0
#define RXWRAPPER_CSI_RX_FSM_ERR_INT_MASK_OFFSET 0xe8
#define RXWRAPPER_CSI_RX_IRQ_MASK_OFFSET 0xf8
#define RXWRAPPER_CSI_RX_FRAME_DROP_INT_MASK_OFFSET 0x100

#define RXWRAPPER_BASE_32BIT_MASK 0xffffffff
#define RXWRAPPER_BASE_16_LSB_MASK 0x0000ffff
#define RXWRAPPER_BASE_P2A_OUTPUT_ADDR 0xf8000000
#define RXWRAPPER_CFG 0x3f101
#define RXWRAPPER_CSI_IP_CTRL_CFG 0x7803e0c8
#define RXWRAPPER_CSI_OUT_LINE_BUF_CFG 0x13

#define RAW12_DT 0x2c
#define RES_4K_FRAME_LINE_NUM 0x870
#define RES_4K_RING_BUFFER_FRAMES HAILO15_NUM_P2A_BUFFERS
#define RES_4K_STRIDE 0x4000
#define RXWRAPPER_BASE_RAW_BUF_SIZE                                            \
	RES_4K_RING_BUFFER_FRAMES *RES_4K_FRAME_LINE_NUM *RES_4K_STRIDE

// size in AXI beats - AXI is 8B -> 3840 pixels * 4B (raw12 padded) = 15360B -> in AXI beats = 1920 (0x780)
#define RXWRAPPER_DEFAULT_CREDITS_LINE_SIZE 0x780

// num of lines in frame = 2160 (0x870)
#define RXWRAPPER_DEFAULT_CREDITS_FRAME_HEIGHT 0x870

// num of frames in the ring buffer
#define RXWRAPPER_DEFAULT_CREDITS_BUFFER_FRAMES RES_4K_RING_BUFFER_FRAMES

// 0 = 512B
#define RXWRAPPER_DEFAULT_CREDITS_DMA_PAGE_SIZE 0x0

// num of lines in tu = 2160 (0x870)
#define RXWRAPPER_DEFAULT_CREDITS_TU_CREDIT_SIZE 0x870

// num of DMA pages in tu -> 1 DMA page = 512B, 1 tu = lines * line size = 2160 * 15360B = 33177600B -> in DMA pages = 64800 (0xfd20)
#define RXWRAPPER_DEFAULT_CREDITS_TU_SIZE_IN_DMA_PAGES 0xfd20

#define RXWRAPPER_DEFAULT_CREDITS_FRAME_DROP_TH RES_4K_RING_BUFFER_FRAMES
#define RXWRAPPER_DEFAULT_CREDITS_ALMOST_FULL_TH RES_4K_RING_BUFFER_FRAMES - 1

#define RXWRAPPER_PIPES_DATA_CFG_DTYPE_SHIFT (3)
#define RXWRAPPER_PIPES_DATA_CFG_DTYPE_WIDTH (6)

#define RXWRAPPER_PIPES_STRIDE_SHIFT (0)
#define RXWRAPPER_PIPES_STRIDE_WIDTH (32)

#define RXWRAPPER_PIPES_RING_BUFFER_LINE_CNT_SHIFT (0)
#define RXWRAPPER_PIPES_RING_BUFFER_LINE_CNT_WIDTH (32)

#define RXWRAPPER_PIPES_BASE_ADDR_SHIFT (0)
#define RXWRAPPER_PIPES_BASE_ADDR_WIDTH (32)

#define RXWRAPPER_PIPES_DATA_CFG_ENABLE_SHIFT (0)
#define RXWRAPPER_PIPES_DATA_CFG_ENABLE_WIDTH (1)

#define RXWRAPPER_PIPES_INIT_SHIFT (0)
#define RXWRAPPER_PIPES_INIT_WIDTH (32)

#define RXWRAPPER_CSI_RX_ERR_IRQ_MASK_SHIFT (0)
#define RXWRAPPER_CSI_RX_ERR_IRQ_MASK_WIDTH (32)

#define RXWRAPPER_CSI_RX_FSM_ERR_INT_MASK_SHIFT (0)
#define RXWRAPPER_CSI_RX_FSM_ERR_INT_MASK_WIDTH (32)

#define RXWRAPPER_CSI_RX_IRQ_MASK_SHIFT (0)
#define RXWRAPPER_CSI_RX_IRQ_MASK_WIDTH (32)

#define RXWRAPPER_CSI_RX_FRAME_DROP_INT_MASK_SHIFT (0)
#define RXWRAPPER_CSI_RX_FRAME_DROP_INT_MASK_WIDTH (32)

#define RXWRAPPER_REG_MASK(shift, width)                                       \
	(u32)((~((0xffffffff << (width - 1)) << 1)) << (shift))
#define RXWRAPPER_MODIFY_VALUE(current, shift, width, val)                     \
	((u32)(((current) & ~RXWRAPPER_REG_MASK((shift), (width))) |           \
	       (((val) << (shift)) & RXWRAPPER_REG_MASK((shift), (width)))))
#define RXWRAPPER_READ_VALUE(val, shift, width)                                \
	((u32)(((val)&RXWRAPPER_REG_MASK((shift), (width))) >> (shift)))

#define PIPE_VALIDATE_RANGE(pipe)                                              \
	do {                                                                   \
		if (pipe >= RXWRAPPER_NUM_PIPES)                               \
			return -EINVAL;                                        \
	} while (0);

#define RXWRAPPER_ERR_IRQ_MASK_DEFAULT 0x0
#define RXWRAPPER_FSM_ERR_INT_MASK_DEFAULT 0x0
#define RXWARPPER_RX_IRQ_MASK_DEFAULT 0x0
#define RXWRAPPER_RX_FRAME_DROP_INT_MASK_DEFAULT 0x0

enum hailo15_rxwrapper_pads {
	RXWRAPPER_PAD_SINK0,
	RXWRAPPER_PAD_SOURCE0,
	RXWRAPPER_PAD_SOURCE1,
	RXWRAPPER_PAD_MAX,
};

struct hailo15_rxwrapper_pipe_cfg {
	uint32_t dtype;
	uint32_t stride;
	uint32_t lines_nr;
};

struct hailo15_rxwrapper_credits_cfg {
	uint32_t line_size;
	uint32_t frame_height;
	uint32_t buffer_frames;
	uint32_t dma_page_size;
	uint32_t tu_credit_size;
	uint32_t tu_size_in_dma_pages;
	uint32_t frame_drop_th;
	uint32_t almost_full_th;
};

struct hailo15_rxwrapper_priv {
	struct device *dev;
	struct v4l2_subdev sd;
	struct v4l2_subdev *source_subdev;
	struct v4l2_async_notifier notifier;
	struct media_pad pads[RXWRAPPER_PAD_MAX];
	int source_pad;
	void *__iomem base;
	void *raw_image_buf_virt;
	dma_addr_t raw_image_buf_phys;
	struct vm_area_struct vma;
	struct mutex lock;
	struct hailo15_rxwrapper_pipe_cfg pipe_cfg[RXWRAPPER_NUM_PIPES];
	struct clk *rxwrapper0_p_clk;
	struct clk *rxwrapper0_data_clk;
	struct hailo15_p2a_buffer_regs_addr p2a_buf_regs;
	struct hailo15_buf_ctx *buf_ctx;
	struct hailo15_buffer *cur_buf;
	void *private_data;
	int queue_empty;
	int irq;
};

struct hailo15_rxwrapper_pipe_cfg rxwrapper_default_pipe_cfg = {
	.dtype = RAW12_DT,
	.lines_nr = RES_4K_FRAME_LINE_NUM * RES_4K_RING_BUFFER_FRAMES,
	.stride = RES_4K_STRIDE,
};

struct hailo15_rxwrapper_credits_cfg rxwrapper_default_credits_cfg = {
	.line_size = RXWRAPPER_DEFAULT_CREDITS_LINE_SIZE,
	.frame_height = RXWRAPPER_DEFAULT_CREDITS_FRAME_HEIGHT,
	.buffer_frames = RXWRAPPER_DEFAULT_CREDITS_BUFFER_FRAMES,
	.dma_page_size = RXWRAPPER_DEFAULT_CREDITS_DMA_PAGE_SIZE,
	.tu_credit_size = RXWRAPPER_DEFAULT_CREDITS_TU_CREDIT_SIZE,
	.tu_size_in_dma_pages = RXWRAPPER_DEFAULT_CREDITS_TU_SIZE_IN_DMA_PAGES,
	.frame_drop_th = RXWRAPPER_DEFAULT_CREDITS_FRAME_DROP_TH,
	.almost_full_th = RXWRAPPER_DEFAULT_CREDITS_ALMOST_FULL_TH,
};

inline void
hailo15_rxwrapper_buffer_done(struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	struct hailo15_buffer *buf;
	struct hailo15_dma_ctx *ctx =
		v4l2_get_subdevdata(&hailo15_rxwrapper->sd);
	uint32_t unprocessed_frames;
	uint32_t overflow_frames;
	uint32_t dropped_frames;

	// read cfg_credit_handler_ext_unprocessed_cnt
	unprocessed_frames =
		readl(hailo15_rxwrapper->base +
		      RXWRAPPER_CFG_CREDIT_HANDLER_EXT_UNPROCESSED_CNT_OFFSET);
	if (unprocessed_frames > 0 &&
	    unprocessed_frames < RXWRAPPER_DEFAULT_CREDITS_FRAME_DROP_TH) {
		buf = hailo15_rxwrapper->cur_buf;
		hailo15_rxwrapper->cur_buf = NULL;

		hailo15_dma_buffer_done(ctx, buf, VID_GRP_P2A);

		writel(1,
		       hailo15_rxwrapper->base +
			       RXWRAPPER_CFG_CREDIT_HANDLER_EXT_UNPROCESSED_CNT_OFFSET);
	} else {
		overflow_frames = readl(
			hailo15_rxwrapper->base +
			RXWRAPPER_CFG_CREDIT_HANDLER_COUNT_OVERFLOW_FRAMES_OFFSET);
		dropped_frames = readl(
			hailo15_rxwrapper->base +
			RXWRAPPER_STATUS_CREDIT_HANDLER_DROPPED_FRAME_CNT_OFFSET);
		pr_warn("%s - unprocessed frames = %u, overflow_frames = %u, dropped_frames = %u\n",
			__func__, unprocessed_frames, overflow_frames,
			dropped_frames);
	}
}

static irqreturn_t hailo15_rxwrapper_irq_handler(int irq, void *arg)
{
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		(struct hailo15_rxwrapper_priv *)arg;

	uint32_t val = readl(
		hailo15_rxwrapper->p2a_buf_regs.buffer_ready_ap_int_status_addr);

	hailo15_rxwrapper_buffer_done(hailo15_rxwrapper);
	writel(val,
	       hailo15_rxwrapper->p2a_buf_regs.buffer_ready_ap_int_w1c_addr);

	return IRQ_HANDLED;
}

static u32
hailo15_rxwrapper_read_reg(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			   u32 offset)
{
	return readl(hailo15_rxwrapper->base + offset);
}

static void
hailo15_rxwrapper_write_reg(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			    u32 offset, u32 data)
{
	writel(data, hailo15_rxwrapper->base + offset);
}

static int
hailo15_rxwrapper_write_field(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			      u32 offset, u32 shift, u32 width, u32 data)
{
	u32 reg;

	if (!hailo15_rxwrapper || !hailo15_rxwrapper->base) {
		return -EINVAL;
	}

	reg = 0;

	if (width != 32 || shift != 0) {
		reg = hailo15_rxwrapper_read_reg(hailo15_rxwrapper, offset);
	}
	hailo15_rxwrapper_write_reg(hailo15_rxwrapper, offset,
				    RXWRAPPER_MODIFY_VALUE(reg, shift, width,
							   data));
	return 0;
}

static u32
hailo15_rxwrapper_read_field(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			     u32 offset, u32 shift, u32 width)
{
	u32 reg;
	reg = hailo15_rxwrapper_read_reg(hailo15_rxwrapper, offset);
	return RXWRAPPER_READ_VALUE(reg, shift, width);
}

static int
hailo15_rxwrapper_pipe_write(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			     u32 pipe, u32 offset, u32 shift, u32 width,
			     u32 data)
{
	PIPE_VALIDATE_RANGE(pipe);

	return hailo15_rxwrapper_write_field(hailo15_rxwrapper,
					     offset + (pipe * sizeof(u32)),
					     shift, width, data);
}

static u32 __maybe_unused
hailo15_rxwrapper_pipe_read(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			    u32 pipe, u32 offset, u32 shift, u32 width)
{
	if (pipe >= RXWRAPPER_NUM_PIPES)
		dev_warn(hailo15_rxwrapper->dev,
			 "tried to read non-existing pipe\n");

	return hailo15_rxwrapper_read_field(
		hailo15_rxwrapper, offset + (pipe * sizeof(u32)), shift, width);
}

static int __maybe_unused hailo15_rxwrapper_pipe_init(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe)
{
	return hailo15_rxwrapper_pipe_write(hailo15_rxwrapper, pipe,
					    RXWRAPPER_PIPES_INIT_OFFSET,
					    RXWRAPPER_PIPES_INIT_SHIFT,
					    RXWRAPPER_PIPES_INIT_WIDTH, 0x1);
}

static int __maybe_unused hailo15_rxwrapper_pipe_reset(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe)
{
	return hailo15_rxwrapper_pipe_write(hailo15_rxwrapper, pipe,
					    RXWRAPPER_PIPES_INIT_OFFSET,
					    RXWRAPPER_PIPES_INIT_SHIFT,
					    RXWRAPPER_PIPES_INIT_WIDTH, 0x0);
}

static int hailo15_rxwrapper_pipe_set_enable(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe, int enable)
{
	return hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe, RXWRAPPER_PIPES_DATA_CFG_OFFSET,
		RXWRAPPER_PIPES_DATA_CFG_ENABLE_SHIFT,
		RXWRAPPER_PIPES_DATA_CFG_ENABLE_WIDTH, !!enable);
}

static int hailo15_rxwrapper_pipe_set_dtype(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe, u32 dtype)
{
	return hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe, RXWRAPPER_PIPES_DATA_CFG_OFFSET,
		RXWRAPPER_PIPES_DATA_CFG_DTYPE_SHIFT,
		RXWRAPPER_PIPES_DATA_CFG_DTYPE_WIDTH, dtype);
}

static int hailo15_rxwrapper_pipe_set_lines_count(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe, u32 lines)
{
	return hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_PIPES_RING_BUFFER_LINE_CNT_OFFSET,
		RXWRAPPER_PIPES_RING_BUFFER_LINE_CNT_SHIFT,
		RXWRAPPER_PIPES_RING_BUFFER_LINE_CNT_WIDTH, lines);
}

static int hailo15_rxwrapper_pipe_set_stride(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe, u32 stride)
{
	return hailo15_rxwrapper_pipe_write(hailo15_rxwrapper, pipe,
					    RXWRAPPER_PIPES_STRIDE_OFFSET,
					    RXWRAPPER_PIPES_STRIDE_SHIFT,
					    RXWRAPPER_PIPES_STRIDE_WIDTH,
					    stride);
}

static int __maybe_unused hailo15_rxwrapper_pipe_enable_credits(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe,
	struct hailo15_rxwrapper_credits_cfg *credits_cfg)
{
	int ret;

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_LINE_SIZE_OFFSET, 0, 12,
		credits_cfg->line_size);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_LINE_SIZE failed\n", __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_HEIGHT_OFFSET, 0, 12,
		credits_cfg->frame_height);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_FRAME_HEIGHT failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_BUFFER_FRAMES_OFFSET, 0, 16,
		credits_cfg->buffer_frames);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_BUFFER_FRAMES failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_DMA_PAGE_SIZE_OFFSET, 0, 3,
		credits_cfg->dma_page_size);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_DMA_PAGE_SIZE failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_TU_CREDIT_EN_OFFSET, 0, 1, 0x1);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_TU_CREDIT_EN failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_TU_CREDIT_SIZE_OFFSET, 0, 12,
		credits_cfg->tu_credit_size);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_TU_CREDIT_SIZE failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_TU_SIZE_IN_DMA_PAGES_OFFSET, 0, 17,
		credits_cfg->tu_size_in_dma_pages);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_TU_SIZE_IN_DMA_PAGES failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_INT_CREDIT_EN_OFFSET, 0, 1, 0x1);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_TU_SIZE_IN_DMA_PAGES failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_DROP_TH_OFFSET, 0, 23,
		credits_cfg->frame_drop_th);
	if (ret) {
		pr_err("%s - RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_DROP_TH_OFFSET failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_ALMOST_FULL_TH_OFFSET, 0, 23,
		credits_cfg->almost_full_th);
	if (ret) {
		pr_err("%s - RXWRAPPER_CFG_CREDIT_HANDLER_ALMOST_FULL_TH_OFFSET failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_DROP_EN_OFFSET, 0, 1, 0x1);
	if (ret) {
		pr_err("%s - RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_DROP_EN_OFFSET failed\n",
		       __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe, RXWRAPPER_CFG_CREDIT_HANDLER_EN_OFFSET,
		0, 1, 0x1); // enable
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_EN failed\n", __func__);
		return ret;
	}

	return 0;
}

static int __maybe_unused hailo15_rxwrapper_pipe_set_data_address(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe,
	u64 data_address)
{
	int ret;
	u32 base_data;

	base_data = (u32)(data_address & RXWRAPPER_BASE_16_LSB_MASK);

	ret = hailo15_rxwrapper_pipe_write(hailo15_rxwrapper, pipe,
					   RXWRAPPER_PIPES_BASE_LOW_OFFSET,
					   RXWRAPPER_PIPES_BASE_ADDR_SHIFT,
					   RXWRAPPER_PIPES_BASE_ADDR_WIDTH,
					   base_data);
	if (ret)
		return ret;

	base_data = (u32)((data_address >> 16) & RXWRAPPER_BASE_32BIT_MASK);
	return hailo15_rxwrapper_pipe_write(hailo15_rxwrapper, pipe,
					    RXWRAPPER_PIPES_BASE_HIGH_OFFSET,
					    RXWRAPPER_PIPES_BASE_ADDR_SHIFT,
					    RXWRAPPER_PIPES_BASE_ADDR_WIDTH,
					    base_data);
}

static int
hailo15_rxwrapper_pipe_config(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			      u32 pipe, u32 dtype, u32 lines, u32 stride)
{
	int ret;
	ret = hailo15_rxwrapper_pipe_set_dtype(hailo15_rxwrapper, pipe, dtype);
	if (ret)
		return ret;
	ret = hailo15_rxwrapper_pipe_set_lines_count(hailo15_rxwrapper, pipe,
						     lines);
	if (ret)
		return ret;
	return hailo15_rxwrapper_pipe_set_stride(hailo15_rxwrapper, pipe,
						 stride);
}

static int hailo15_rxwrapper_set_csi_rx_err_irq_mask(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 mask)
{
	return hailo15_rxwrapper_write_field(
		hailo15_rxwrapper, RXWRAPPER_CSI_RX_ERR_IRQ_MASK_OFFSET,
		RXWRAPPER_CSI_RX_ERR_IRQ_MASK_SHIFT,
		RXWRAPPER_CSI_RX_ERR_IRQ_MASK_WIDTH, mask);
}

static int hailo15_rxwrapper_set_csi_rx_fsm_err_int_mask(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 mask)
{
	return hailo15_rxwrapper_write_field(
		hailo15_rxwrapper, RXWRAPPER_CSI_RX_FSM_ERR_INT_MASK_OFFSET,
		RXWRAPPER_CSI_RX_FSM_ERR_INT_MASK_SHIFT,
		RXWRAPPER_CSI_RX_FSM_ERR_INT_MASK_SHIFT, mask);
}

static int hailo15_rxwrapper_set_csi_rx_irq_mask(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 mask)
{
	return hailo15_rxwrapper_write_field(hailo15_rxwrapper,
					     RXWRAPPER_CSI_RX_IRQ_MASK_OFFSET,
					     RXWRAPPER_CSI_RX_IRQ_MASK_SHIFT,
					     RXWRAPPER_CSI_RX_IRQ_MASK_WIDTH,
					     mask);
}

static int hailo15_rxwrapper_set_csi_rx_frame_drop_int_mask(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 mask)
{
	return hailo15_rxwrapper_write_field(
		hailo15_rxwrapper, RXWRAPPER_CSI_RX_FRAME_DROP_INT_MASK_OFFSET,
		RXWRAPPER_CSI_RX_FRAME_DROP_INT_MASK_SHIFT,
		RXWRAPPER_CSI_RX_FRAME_DROP_INT_MASK_WIDTH, mask);
}

static int
hailo15_rxwrapper_set_cfg(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			  u32 conf)
{
	return hailo15_rxwrapper_write_field(hailo15_rxwrapper,
					     RXWRAPPER_CFG_OFFSET, 0, 32, conf);
}

static int hailo15_rxwrapper_set_csi_ip_ctrl(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 conf)
{
	return hailo15_rxwrapper_write_field(
		hailo15_rxwrapper, RXWRAPPER_CSI_IP_CTRL_OFFSET, 0, 32, conf);
}

static int hailo15_rxwrapper_set_csi_out_line_buf_cfg(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 conf)
{
	return hailo15_rxwrapper_write_field(
		hailo15_rxwrapper, RXWRAPPER_CSI_OUT_LINE_BUF_CFG_OFFSET, 0, 32,
		conf);
}

static inline int
hailo15_rxwrapper_pipe_enable(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			      u32 pipe)
{
	return hailo15_rxwrapper_pipe_set_enable(hailo15_rxwrapper, pipe, 1);
}

static inline int
hailo15_rxwrapper_pipe_disable(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			       u32 pipe)
{
	return hailo15_rxwrapper_pipe_set_enable(hailo15_rxwrapper, pipe, 0);
}

static inline struct hailo15_rxwrapper_priv *
v4l2_subdev_to_hailo15_rxwrapper(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct hailo15_rxwrapper_priv, sd);
}

static int hailo15_rxwrapper_pipe_apply_cfg(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe)
{
	struct hailo15_rxwrapper_pipe_cfg *cfg;

	PIPE_VALIDATE_RANGE(pipe);

	cfg = &hailo15_rxwrapper->pipe_cfg[pipe];

	return hailo15_rxwrapper_pipe_config(hailo15_rxwrapper, pipe,
					     cfg->dtype, cfg->lines_nr,
					     cfg->stride);
}

static int
hailo15_rxwrapper_pipe_set_cfg(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
			       u32 pipe, struct hailo15_rxwrapper_pipe_cfg *cfg)
{
	PIPE_VALIDATE_RANGE(pipe);
	memcpy(&hailo15_rxwrapper->pipe_cfg[pipe], cfg,
	       sizeof(struct hailo15_rxwrapper_pipe_cfg));
	return 0;
}

static void hailo15_rxwrapper_config_static_default(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	hailo15_rxwrapper_set_csi_rx_err_irq_mask(
		hailo15_rxwrapper, RXWRAPPER_ERR_IRQ_MASK_DEFAULT);
	hailo15_rxwrapper_set_csi_rx_fsm_err_int_mask(
		hailo15_rxwrapper, RXWRAPPER_FSM_ERR_INT_MASK_DEFAULT);
	hailo15_rxwrapper_set_csi_rx_irq_mask(hailo15_rxwrapper,
					      RXWARPPER_RX_IRQ_MASK_DEFAULT);
	hailo15_rxwrapper_set_csi_rx_frame_drop_int_mask(
		hailo15_rxwrapper, RXWRAPPER_RX_FRAME_DROP_INT_MASK_DEFAULT);
	hailo15_rxwrapper_set_cfg(hailo15_rxwrapper, RXWRAPPER_CFG);
	hailo15_rxwrapper_set_csi_ip_ctrl(hailo15_rxwrapper,
					  RXWRAPPER_CSI_IP_CTRL_CFG);
	hailo15_rxwrapper_set_csi_out_line_buf_cfg(
		hailo15_rxwrapper, RXWRAPPER_CSI_OUT_LINE_BUF_CFG);
}

int hailo15_rxwrapper_set_stream(struct v4l2_subdev *sd, int enable)
{
	int pipe;
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		v4l2_subdev_to_hailo15_rxwrapper(sd);
	struct v4l2_subdev *subdev;
	struct media_pad *pad;
	int ret;

	if (!hailo15_rxwrapper)
		return -EINVAL;

	mutex_lock(&hailo15_rxwrapper->lock);

	pipe = 0;

	if (enable) {
		hailo15_rxwrapper_pipe_apply_cfg(hailo15_rxwrapper, pipe);
		hailo15_rxwrapper_pipe_enable(hailo15_rxwrapper, pipe);
	}

	pad = &hailo15_rxwrapper->pads[0];
	if (pad)
		pad = media_entity_remote_pad(pad);

	if (pad && is_media_entity_v4l2_subdev(pad->entity)) {
		subdev = media_entity_to_v4l2_subdev(pad->entity);
		subdev->grp_id = sd->grp_id;
		if (subdev->grp_id == VID_GRP_P2A) {
			if (enable) {
				hailo15_rxwrapper_pipe_enable_credits(
					hailo15_rxwrapper, pipe,
					&rxwrapper_default_credits_cfg);
				hailo15_rxwrapper_pipe_set_data_address(
					hailo15_rxwrapper, pipe,
					hailo15_rxwrapper->raw_image_buf_phys);
				hailo15_rxwrapper_pipe_init(hailo15_rxwrapper,
							    pipe);
				v4l2_subdev_call(subdev, video, s_stream,
						 enable);
			} else {
				v4l2_subdev_call(subdev, video, s_stream,
						 enable);
				hailo15_rxwrapper_pipe_disable(
					hailo15_rxwrapper, pipe);
				hailo15_rxwrapper_pipe_reset(hailo15_rxwrapper,
							     pipe);
				hailo15_rxwrapper->queue_empty = 0;
				hailo15_rxwrapper->cur_buf = NULL;
				dma_free_coherent(
					hailo15_rxwrapper->dev,
					RXWRAPPER_BASE_RAW_BUF_SIZE,
					hailo15_rxwrapper->raw_image_buf_virt,
					hailo15_rxwrapper->raw_image_buf_phys);
				hailo15_rxwrapper->raw_image_buf_phys = 0;
			}
		} else
			v4l2_subdev_call(subdev, video, s_stream, enable);
	}
	ret = 0;

	/* TODO: MSW-2716: This code section is unused */
	/*
	goto finish;

err_bad_src_pad:
	dev_err(hailo15_rxwrapper->dev, "%s: bad source pad for source subdev\n", __func__);
finish:
*/
	mutex_unlock(&hailo15_rxwrapper->lock);
	return ret;
}

static int hailo15_rxwrapper_get_pad_format(struct v4l2_subdev *sd,
					    struct v4l2_subdev_state *sd_state,
					    struct v4l2_subdev_format *fmt)
{
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		v4l2_subdev_to_hailo15_rxwrapper(sd);

	if (!hailo15_rxwrapper)
		return -EINVAL;

	return 0;
}

static int hailo15_rxwrapper_set_pad_format(struct v4l2_subdev *sd,
					    struct v4l2_subdev_state *sd_state,
					    struct v4l2_subdev_format *fmt)
{
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		v4l2_subdev_to_hailo15_rxwrapper(sd);

	if (!hailo15_rxwrapper)
		return -EINVAL;

	return 0;
}

static int hailo15_rxwrapper_async_bound(struct v4l2_async_notifier *notifier,
					 struct v4l2_subdev *s_subdev,
					 struct v4l2_async_subdev *asd)
{
	struct v4l2_subdev *subdev = notifier->sd;
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		v4l2_subdev_to_hailo15_rxwrapper(subdev);

	hailo15_rxwrapper->source_pad = media_entity_get_fwnode_pad(
		&s_subdev->entity, s_subdev->fwnode, MEDIA_PAD_FL_SOURCE);
	if (hailo15_rxwrapper->source_pad < 0) {
		dev_err(hailo15_rxwrapper->dev,
			"Couldn't find output pad for subdev %s\n",
			s_subdev->name);
		return hailo15_rxwrapper->source_pad;
	}

	hailo15_rxwrapper->source_subdev = s_subdev;
	v4l2_subdev_call(s_subdev, core, ioctl, VIDEO_GET_P2A_REGS,
			 &hailo15_rxwrapper->p2a_buf_regs);

	return media_create_pad_link(
		&hailo15_rxwrapper->source_subdev->entity,
		hailo15_rxwrapper->source_pad, &hailo15_rxwrapper->sd.entity, 0,
		MEDIA_LNK_FL_ENABLED | MEDIA_LNK_FL_IMMUTABLE);
}

static const struct v4l2_async_notifier_operations
	hailo15_rxwrapper_notifier_ops = {
		.bound = hailo15_rxwrapper_async_bound,
	};

static int
hailo15_rxwrapper_parse_dt(struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	struct v4l2_fwnode_endpoint v4l2_ep;
	struct v4l2_async_subdev *asd;
	struct fwnode_handle *fwh;
	struct device_node *ep;
	int ret;

	memset(&v4l2_ep, 0, sizeof(struct v4l2_fwnode_endpoint));
	ep = of_graph_get_endpoint_by_regs(hailo15_rxwrapper->dev->of_node, 0,
					   0);
	if (!ep)
		return -EINVAL;

	fwh = of_fwnode_handle(ep);
	ret = v4l2_fwnode_endpoint_parse(fwh, &v4l2_ep);
	if (ret) {
		of_node_put(ep);
		return ret;
	}

	v4l2_async_notifier_init(&hailo15_rxwrapper->notifier);

	asd = v4l2_async_notifier_add_fwnode_remote_subdev(
		&hailo15_rxwrapper->notifier, fwh, struct v4l2_async_subdev);
	of_node_put(ep);
	if (IS_ERR(asd))
		return PTR_ERR(asd);

	pr_info("hailo15_rxwrapper: match name is %d\n",
		asd->match.fwnode == fwh);

	hailo15_rxwrapper->notifier.ops = &hailo15_rxwrapper_notifier_ops;

	ret = v4l2_async_subdev_notifier_register(&hailo15_rxwrapper->sd,
						  &hailo15_rxwrapper->notifier);
	if (ret)
		v4l2_async_notifier_cleanup(&hailo15_rxwrapper->notifier);
	return ret;
}

static struct v4l2_subdev_video_ops hailo15_rxwrapper_v4l2_subdev_video_ops = {
	.s_stream = hailo15_rxwrapper_set_stream,
};

static const struct v4l2_subdev_pad_ops hailo15_rxwrapper_v4l2_subdev_pad_ops = {
	.set_fmt = hailo15_rxwrapper_set_pad_format,
	.get_fmt = hailo15_rxwrapper_get_pad_format,
};

static int rxwrapper_querycap(struct v4l2_capability *cap)
{
	strncpy((char *)cap->driver, "hailo-rxwrapper", sizeof(cap->driver));
	memset(cap->bus_info, 0, sizeof(cap->bus_info));
	return 0;
}

enum { VVCSIOC_RESET = 0x100,
       VVCSIOC_POWERON,
       VVCSIOC_POWEROFF,
       VVCSIOC_STREAMON,
       VVCSIOC_STREAMOFF,
       VVCSIOC_S_FMT,
       VVCSIOC_S_HDR,
};

static long rxwrapper_priv_ioctl(struct v4l2_subdev *sd, unsigned int cmd,
				 void *arg)
{
	int ret;

	switch (cmd) {
	case VIDIOC_QUERYCAP:
		ret = rxwrapper_querycap(arg);
		break;
	default:
		pr_debug("rxwrapper: got unsupported ioctl 0x%x\n", cmd);
		ret = -ENOENT;
		break;
	}
	return ret;
}

static struct v4l2_subdev_core_ops rxwrapper_core_ops = {
	.ioctl = rxwrapper_priv_ioctl,
};

struct v4l2_subdev_ops hailo15_rxwrapper_v4l2_subdev_ops = {
	.core = &rxwrapper_core_ops,
	.video = &hailo15_rxwrapper_v4l2_subdev_video_ops,
	.pad = &hailo15_rxwrapper_v4l2_subdev_pad_ops,
};

static int hailo15_rxwrapper_get_fbuf(struct hailo15_dma_ctx *ctx,
				      struct v4l2_framebuffer *fbuf)
{
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		(struct hailo15_rxwrapper_priv *)ctx->dev;
	if (!hailo15_rxwrapper->raw_image_buf_phys) {
		hailo15_rxwrapper->raw_image_buf_virt = dma_alloc_coherent(
			hailo15_rxwrapper->dev, RXWRAPPER_BASE_RAW_BUF_SIZE,
			&hailo15_rxwrapper->raw_image_buf_phys, GFP_USER);
		if (!hailo15_rxwrapper->raw_image_buf_virt) {
			pr_err("hailo15_rxwrapper - failed to allocate raw frames buffer\n");
			return -ENOMEM;
		}
	}
	fbuf->base = (void *)hailo15_rxwrapper->raw_image_buf_phys;
	fbuf->fmt.sizeimage =
		RXWRAPPER_BASE_RAW_BUF_SIZE / RES_4K_RING_BUFFER_FRAMES;
	return 0;
}

static int hailo15_rxwrapper_buffer_process(struct hailo15_dma_ctx *ctx,
					    struct hailo15_buffer *buf)
{
	struct v4l2_subdev *sd;
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper;

	sd = buf->sd;
	hailo15_rxwrapper = container_of(sd, struct hailo15_rxwrapper_priv, sd);
	hailo15_rxwrapper->cur_buf = buf;

	return 0;
}

static int hailo15_rxwrapper_set_private_data(struct hailo15_dma_ctx *ctx,
					      int grp_id, void *data)
{
	struct hailo15_rxwrapper_priv *rxwrapper_dev =
		(struct hailo15_rxwrapper_priv *)ctx->dev;

	if (grp_id != VID_GRP_P2A)
		return -EINVAL;

	rxwrapper_dev->private_data = data;

	return 0;
}

static int hailo15_rxwrapper_get_private_data(struct hailo15_dma_ctx *ctx,
					      int grp_id, void **data)
{
	struct hailo15_rxwrapper_priv *rxwrapper_dev =
		(struct hailo15_rxwrapper_priv *)ctx->dev;

	if (!data)
		return -EINVAL;

	if (grp_id != VID_GRP_P2A)
		return -EINVAL;

	*data = rxwrapper_dev->private_data;

	return 0;
}

static int hailo15_rxwrapper_queue_empty(struct hailo15_dma_ctx *ctx,
					 int grp_id)
{
	struct hailo15_rxwrapper_priv *rxwrapper_dev =
		(struct hailo15_rxwrapper_priv *)ctx->dev;

	if (grp_id != VID_GRP_P2A)
		return -EINVAL;

	rxwrapper_dev->queue_empty = 1;
	return 0;
}

static struct hailo15_buf_ops hailo15_rxwrapper_buf_ops = {
	.get_fbuf = hailo15_rxwrapper_get_fbuf,
	.buffer_process = hailo15_rxwrapper_buffer_process,
	.set_private_data = hailo15_rxwrapper_set_private_data,
	.get_private_data = hailo15_rxwrapper_get_private_data,
	.queue_empty = hailo15_rxwrapper_queue_empty,
};

static int
hailo15_rxwrapper_init_dma_ctx(struct hailo15_dma_ctx *ctx,
			       struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	ctx->dev = (void *)hailo15_rxwrapper;
	ctx->buf_ctx.ops = kzalloc(sizeof(struct hailo15_buf_ops), GFP_KERNEL);
	if (!ctx->buf_ctx.ops)
		return -ENOMEM;
	memcpy(ctx->buf_ctx.ops, &hailo15_rxwrapper_buf_ops,
	       sizeof(struct hailo15_buf_ops));
	v4l2_set_subdevdata(&hailo15_rxwrapper->sd, ctx);
	return 0;
}

static void hailo15_rxwrapper_clean_dma_ctx(struct hailo15_dma_ctx *ctx)
{
	kfree(ctx->buf_ctx.ops);
}

/*@TODO: check if mutex needed */
int hailo15_rxwrapper_probe(struct platform_device *pdev)
{
	int ret, pipe;
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper;
	struct resource *res;
	struct hailo15_dma_ctx *ctx;

	pr_info("enter %s\n", __func__);
	hailo15_rxwrapper =
		kzalloc(sizeof(struct hailo15_rxwrapper_priv), GFP_KERNEL);

	if (!hailo15_rxwrapper) {
		pr_err("Unable to allocate hailo15_rxwrapper struct\n");
		return 1;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		goto error_free_dev;

	hailo15_rxwrapper->base = devm_ioremap_resource(&pdev->dev, res);
	pr_info("hailo15_rxwrapper base is %llx first 4 bytes \n",
		(u64)hailo15_rxwrapper->base);

	hailo15_rxwrapper->rxwrapper0_p_clk =
		devm_clk_get(&pdev->dev, "rxwrapper0_p_clk");
	if (IS_ERR(hailo15_rxwrapper->rxwrapper0_p_clk)) {
		pr_err("%s - failed to get hailo15_rxwrapper->rxwrapper0_p_clk clock\n",
		       __func__);
		return PTR_ERR(hailo15_rxwrapper->rxwrapper0_p_clk);
	}

	hailo15_rxwrapper->rxwrapper0_data_clk =
		devm_clk_get(&pdev->dev, "rxwrapper0_data_clk");
	if (IS_ERR(hailo15_rxwrapper->rxwrapper0_data_clk)) {
		pr_err("%s - failed to get hailo15_rxwrapper->rxwrapper0_data_clk clock\n",
		       __func__);
		return PTR_ERR(hailo15_rxwrapper->rxwrapper0_data_clk);
	}

	ret = clk_prepare_enable(hailo15_rxwrapper->rxwrapper0_p_clk);
	if (ret) {
		pr_err("%s - failed enabling rxwrapper0_p_clk\n", __func__);
		return -EAGAIN;
	}

	ret = clk_prepare_enable(hailo15_rxwrapper->rxwrapper0_data_clk);
	if (ret) {
		pr_err("%s - failed enabling rxwrapper0_data_clk\n", __func__);
		return -EAGAIN;
	}

	hailo15_rxwrapper_config_static_default(hailo15_rxwrapper);

	/*@TODO check return value*/
	for (pipe = 0; pipe < RXWRAPPER_NUM_PIPES; ++pipe) {
		hailo15_rxwrapper_pipe_set_cfg(hailo15_rxwrapper, pipe,
					       &rxwrapper_default_pipe_cfg);
	}

	platform_set_drvdata(pdev, hailo15_rxwrapper);

	hailo15_rxwrapper->dev = &pdev->dev;
	hailo15_rxwrapper->sd.owner = THIS_MODULE;
	hailo15_rxwrapper->sd.dev = &(pdev->dev);
	hailo15_rxwrapper->sd.fwnode = dev_fwnode(hailo15_rxwrapper->dev);
	v4l2_subdev_init(&hailo15_rxwrapper->sd,
			 &hailo15_rxwrapper_v4l2_subdev_ops);

	ret = hailo15_rxwrapper_parse_dt(hailo15_rxwrapper);
	if (ret) {
		pr_err("hailo15_rxwrapper: failed to parse dt ret=%d\n", ret);
		goto error_free_dev;
	}

	snprintf(hailo15_rxwrapper->sd.name, sizeof(hailo15_rxwrapper->sd.name),
		 "hailo15_rxwrapper.%d", 0);
	/*hailo15_rxwrapper->sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;*/
	hailo15_rxwrapper->sd.entity.function = MEDIA_ENT_F_VID_MUX;

	hailo15_rxwrapper->pads[RXWRAPPER_PAD_SINK0].flags = MEDIA_PAD_FL_SINK;
	hailo15_rxwrapper->pads[RXWRAPPER_PAD_SOURCE0].flags =
		MEDIA_PAD_FL_SOURCE;
	hailo15_rxwrapper->pads[RXWRAPPER_PAD_SOURCE1].flags =
		MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&hailo15_rxwrapper->sd.entity,
				     RXWRAPPER_PAD_MAX,
				     hailo15_rxwrapper->pads);
	if (ret) {
		pr_err("failed to init entity pads: %d", ret);
		goto error_clean_notifier;
	}

	hailo15_rxwrapper->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ret = v4l2_async_register_subdev(&(hailo15_rxwrapper->sd));
	if (ret < 0) {
		pr_err("failed to register async subdev: %d", ret);
		goto error_clean_notifier;
	}

	mutex_init(&hailo15_rxwrapper->lock);

	ctx = kzalloc(sizeof(struct hailo15_dma_ctx), GFP_KERNEL);
	if (!ctx) {
		pr_err("hailo15 rxwrapper: failed to allocate dma ctx\n");
		goto error_alloc_dma_ctx;
	}

	ret = hailo15_rxwrapper_init_dma_ctx(ctx, hailo15_rxwrapper);
	if (ret) {
		pr_err("hailo15 rxwrapper: failed to init dma ctx\n");
		goto error_init_dma_ctx;
	}

	hailo15_rxwrapper->irq = platform_get_irq(pdev, 0);
	if (hailo15_rxwrapper->irq < 0) {
		pr_err("can't get irq resource\n");
		goto error_init_irq;
	}

	ret = devm_request_irq(hailo15_rxwrapper->dev, hailo15_rxwrapper->irq,
			       hailo15_rxwrapper_irq_handler, 0,
			       dev_name(hailo15_rxwrapper->dev),
			       hailo15_rxwrapper);

	if (ret) {
		pr_err("request irq error\n");
		goto error_init_irq;
	}

	return 0;

error_init_irq:
	hailo15_rxwrapper_clean_dma_ctx(ctx);
error_init_dma_ctx:
	kfree(ctx);
error_alloc_dma_ctx:
	mutex_destroy(&hailo15_rxwrapper->lock);
error_clean_notifier:
	v4l2_async_notifier_cleanup(&hailo15_rxwrapper->notifier);
error_free_dev:
	kfree(hailo15_rxwrapper);
	return -EINVAL;
}

int hailo15_rxwrapper_remove(struct platform_device *pdev)
{
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		platform_get_drvdata(pdev);
	struct hailo15_dma_ctx *ctx =
		v4l2_get_subdevdata(&hailo15_rxwrapper->sd);

	if (hailo15_rxwrapper->raw_image_buf_phys)
		dma_free_coherent(&pdev->dev, RXWRAPPER_BASE_RAW_BUF_SIZE,
				  hailo15_rxwrapper->raw_image_buf_virt,
				  hailo15_rxwrapper->raw_image_buf_phys);
	mutex_destroy(&hailo15_rxwrapper->lock);
	v4l2_async_unregister_subdev(&hailo15_rxwrapper->sd);
	kfree(hailo15_rxwrapper);
	hailo15_rxwrapper_clean_dma_ctx(ctx);
	kfree(ctx);
	return 0;
}

static const struct of_device_id hailo15_rxwrapper_of_match[] = {
	{
		.compatible = "hailo,hailo15-rxwrapper",
	},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, hailo15_rxwrapper_of_match);

static struct platform_driver
	hailo15_rxwrapper_driver = { .probe = hailo15_rxwrapper_probe,
				     .remove = hailo15_rxwrapper_remove,
				     .driver = {
					     .name = "hailo-hailo15_rxwrapper",
					     .owner = THIS_MODULE,
					     .of_match_table =
						     hailo15_rxwrapper_of_match,
				     } };

module_platform_driver(hailo15_rxwrapper_driver);
MODULE_AUTHOR("Eylon Shabtay <eylons@hailo.ai>");
MODULE_DESCRIPTION("Hailo hailo15_rxwrapper driver");
MODULE_LICENSE("GPL v2");
