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
#include <linux/workqueue.h>
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
#include <linux/media-bus-format.h>
#include <media/v4l2-fwnode.h>
#include "hailo15-media.h"
#include "common.h"

#define RXWRAPPER_NUM_PIPES 4
#define RXWRAPPER_PIPE_0 (0)

#define RXWRAPPER_CFG_OFFSET 0x0
#define RXWRAPPER_PIPES_INIT_OFFSET 0x4
#define RXWRAPPER_PIPES_DATA_CFG_OFFSET 0x14
#define RXWRAPPER_PIPES_RING_BUFFER_LINE_CNT_OFFSET 0x24
#define RXWRAPPER_PIPES_STRIDE_OFFSET 0x34
#define RXWRAPPER_PIPES_BASE_LOW_OFFSET 0x44
#define RXWRAPPER_PIPES_BASE_HIGH_OFFSET 0x54

#define RXWRAPPER_CSI_RX_ERR_IRQ_MASK_OFFSET 0xe0
#define RXWRAPPER_CSI_RX_FSM_ERR_INT_MASK_OFFSET 0xe8
#define RXWRAPPER_CSI_RX_IRQ_MASK_OFFSET 0xf8
#define RXWRAPPER_CSI_RX_FRAME_DROP_INT_MASK_OFFSET 0x100

#define RXWRAPPER_BASE_32BIT_MASK 0xffffffff
#define RXWRAPPER_BASE_16_LSB_MASK 0x0000ffff
#define RXWRAPPER_BASE_P2A_OUTPUT_ADDR 0xf8000000
#define RXWRAPPER_CSI_IP_CTRL_CFG 0x0003e0c8
#define RXWRAPPER_CSI_OUT_LINE_BUF_CFG 0x13

#define RAW12_DT 0x2c
#define RAW10_DT 0x2b
#define YUV422_8b_DT 0x1e
#define RES_4K_FRAME_LINE_NUM 0x870
#define RXWRAPPER_MAX_NUM_EXPOSURES 3
#define RING_BUFFER_FRAMES HAILO15_NUM_P2A_BUFFERS
// 2 PPC -> 2 * 3840 = 7680
#define RES_4K_STRIDE 0x1e00

// size in AXI beats - AXI is 8B -> 3840 pixels * 2B (raw12 2 ppc) = 7680B -> in AXI beats = 960 (0x3c0)
#define RXWRAPPER_RES_4K_DEFAULT_CREDITS_LINE_SIZE 0x3c0

// num of frames in the ring buffer
#define RXWRAPPER_DEFAULT_CREDITS_BUFFER_FRAMES RING_BUFFER_FRAMES

// 0 = 512B
#define RXWRAPPER_DEFAULT_CREDITS_DMA_PAGE_SIZE 0x0
#define RXWRAPPER_DMA_PAGE_SIZE_IN_BYTES 512

// num of lines in tu = 2160 (0x870)
#define RXWRAPPER_RES_4K_DEFAULT_CREDITS_TU_CREDIT_SIZE RES_4K_FRAME_LINE_NUM

// num of DMA pages in tu -> 1 DMA page = 512B, 1 tu = lines * line size = 2160 * 7680B = 16588800B -> in DMA pages = 32400 (0x7E90)
#define RXWRAPPER_RES_4K_DEFAULT_CREDITS_TU_SIZE_IN_DMA_PAGES 0x7e90

#define RXWRAPPER_DEFAULT_CREDITS_FRAME_DROP_TH RING_BUFFER_FRAMES
#define RXWRAPPER_DEFAULT_CREDITS_ALMOST_FULL_TH RING_BUFFER_FRAMES

#define RXWRAPPER_PIPES_CFG_CREDIT_HANDLER_EXT_UNPROCESSED_CNT_SHIFT (0)
#define RXWRAPPER_PIPES_CFG_CREDIT_HANDLER_EXT_UNPROCESSED_CNT_WIDTH (23)

#define RXWRAPPER_PIPES_CFG_CREDIT_HANDLER_COUNT_OVERFLOW_FRAMES_SHIFT (0)
#define RXWRAPPER_PIPES_CFG_CREDIT_HANDLER_COUNT_OVERFLOW_FRAMES_WIDTH (1)

#define RXWRAPPER_PIPES_CTL_CREDIT_HANDLER_SRST_SHIFT (0)
#define RXWRAPPER_PIPES_CTL_CREDIT_HANDLER_SRST_WIDTH (1)

#define RXWRAPPER_PIPES_STATUS_CREDIT_HANDLER_DROPPED_FRAME_CNT_SHIFT (0)
#define RXWRAPPER_PIPES_STATUS_CREDIT_HANDLER_DROPPED_FRAME_CNT_WIDTH (16)

#define RXWRAPPER_PIPES_STATUS_CREDIT_HANDLER_FRAME_CNT_SHIFT (0)
#define RXWRAPPER_PIPES_STATUS_CREDIT_HANDLER_FRAME_CNT_WIDTH (16)

#define RXWRAPPER_PIPES_DATA_CFG_VC_SHIFT (1)
#define RXWRAPPER_PIPES_DATA_CFG_VC_WIDTH (2)

#define RXWRAPPER_PIPES_DATA_CFG_DTYPE_SHIFT (3)
#define RXWRAPPER_PIPES_DATA_CFG_DTYPE_WIDTH (6)

#define RXWRAPPER_PIPES_DATA_CFG_WC_VC_SHIFT (9)
#define RXWRAPPER_PIPES_DATA_CFG_WC_VC_WIDTH (1)

#define RXWRAPPER_PIPES_STRIDE_SHIFT (0)
#define RXWRAPPER_PIPES_STRIDE_WIDTH (32)

#define RXWRAPPER_PIPES_RING_BUFFER_LINE_CNT_SHIFT (0)
#define RXWRAPPER_PIPES_RING_BUFFER_LINE_CNT_WIDTH (32)

#define RXWRAPPER_PIPES_BASE_ADDR_SHIFT (0)
#define RXWRAPPER_PIPES_BASE_ADDR_WIDTH (16)

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

#define RXWRAPPER_REG_MASK(shift, width) \
	(u32)((~((0xffffffff << (width - 1)) << 1)) << (shift))

#define RXWRAPPER_MODIFY_VALUE(current, shift, width, val) \
	((u32)(((current) & ~RXWRAPPER_REG_MASK((shift), (width))) | \
	(((val) << (shift)) & RXWRAPPER_REG_MASK((shift), (width)))))

#define RXWRAPPER_READ_VALUE(val, shift, width) \
	((u32)(((val)&RXWRAPPER_REG_MASK((shift), (width))) >> (shift)))

#define PIPE_VALIDATE_RANGE(pipe)           \
	do {                               	    \
		if (pipe >= RXWRAPPER_NUM_PIPES)    \
			return -EINVAL;                 \
	} while (0);

#define VID_GRP_VALIDATE_RANGE(vid_grp)     \
	do {                                    \
		if (vid_grp >= HAILO15_VID_GRP_MAX) \
			return -EINVAL;                 \
	} while (0);

#define RXWRAPPER_ERR_IRQ_MASK_DEFAULT 0x1 // enable csi_ip
#define RXWRAPPER_FSM_ERR_INT_MASK_DEFAULT 0x0
#define RXWARPPER_RX_IRQ_MASK_DEFAULT 0x0
#define RXWRAPPER_RX_FRAME_DROP_INT_MASK_DEFAULT 0x0

struct rxwrapper_config {
    uint32_t rxwrapper_cfg_reg_value;

    uint8_t rxwrapper_pipes_data_cfg_dtype_shift;
    uint8_t rxwrapper_pipes_data_cfg_wc_vc_shift;

    uint16_t rxwrapper_csi_ip_ctrl_offset;
    uint16_t rxwrapper_cfg_credit_handler_en_offset;
    uint16_t rxwrapper_cfg_credit_handler_line_size_offset;
    uint16_t rxwrapper_cfg_credit_handler_frame_height_offset;
    uint16_t rxwrapper_cfg_credit_handler_buffer_frames_offset;
    uint16_t rxwrapper_cfg_credit_handler_dma_page_size_offset;
    uint16_t rxwrapper_cfg_credit_handler_tu_credit_en_offset;
    uint16_t rxwrapper_cfg_credit_handler_tu_credit_size_offset;
    uint16_t rxwrapper_cfg_credit_handler_tu_size_in_dma_pages_offset;
    uint16_t rxwrapper_cfg_credit_handler_dma_credit_en_offset;
    uint16_t rxwrapper_cfg_credit_handler_int_credit_en_offset;
    uint16_t rxwrapper_pipes_cfg_credit_handler_ext_unprocessed_cnt_offset;
    uint16_t rxwrapper_cfg_credit_handler_frame_drop_en_offset;
    uint16_t rxwrapper_cfg_credit_handler_frame_drop_th_offset;
    uint16_t rxwrapper_cfg_credit_handler_almost_full_th_offset;
    uint16_t rxwrapper_cfg_credit_handler_sram_mode_en_offset;
    uint16_t rxwrapper_pipes_cfg_credit_handler_count_overflow_frames_offset;
    uint16_t rxwrapper_pipes_ctl_credit_handler_srst_offset;
    uint16_t rxwrapper_pipes_status_credit_handler_frame_cnt_offset;
    uint16_t rxwrapper_pipes_status_credit_handler_dropped_frame_cnt_offset;
    struct {
        int exist;
        uint16_t offset;
    } rxwrapper_csi_out_line_buf_cfg;
};

static const struct rxwrapper_config hailo15_rxwrapper_config = {
    .rxwrapper_cfg_reg_value = 0xf101,
    .rxwrapper_pipes_data_cfg_dtype_shift = 3,
    .rxwrapper_pipes_data_cfg_wc_vc_shift = 9,

    .rxwrapper_csi_ip_ctrl_offset = 0x78,
    .rxwrapper_cfg_credit_handler_en_offset = 0x150,
    .rxwrapper_cfg_credit_handler_line_size_offset = 0x160,
    .rxwrapper_cfg_credit_handler_frame_height_offset = 0x170,
    .rxwrapper_cfg_credit_handler_buffer_frames_offset = 0x180,
    .rxwrapper_cfg_credit_handler_dma_page_size_offset = 0x190,
    .rxwrapper_cfg_credit_handler_tu_credit_en_offset = 0x1a0,
    .rxwrapper_cfg_credit_handler_tu_credit_size_offset = 0x1b0,
    .rxwrapper_cfg_credit_handler_tu_size_in_dma_pages_offset = 0x1c0,
    .rxwrapper_cfg_credit_handler_dma_credit_en_offset = 0x1d0,
    .rxwrapper_cfg_credit_handler_int_credit_en_offset = 0x1e0,
    .rxwrapper_pipes_cfg_credit_handler_ext_unprocessed_cnt_offset = 0x1f0,
    .rxwrapper_cfg_credit_handler_frame_drop_en_offset = 0x200,
    .rxwrapper_cfg_credit_handler_frame_drop_th_offset = 0x210,
    .rxwrapper_cfg_credit_handler_almost_full_th_offset = 0x220,
    .rxwrapper_cfg_credit_handler_sram_mode_en_offset = 0x230,
    .rxwrapper_pipes_cfg_credit_handler_count_overflow_frames_offset = 0x240,
    .rxwrapper_pipes_ctl_credit_handler_srst_offset = 0x250,
    .rxwrapper_pipes_status_credit_handler_frame_cnt_offset = 0x280,
    .rxwrapper_pipes_status_credit_handler_dropped_frame_cnt_offset = 0x2c0,
    .rxwrapper_csi_out_line_buf_cfg = {
        .exist = 1,
        .offset = 0xcc,
    },
};

static const struct rxwrapper_config hailo15l_rxwrapper_config = {
    .rxwrapper_cfg_reg_value = 0x7e180,
    .rxwrapper_pipes_data_cfg_dtype_shift = 5,
    .rxwrapper_pipes_data_cfg_wc_vc_shift = 11,

    .rxwrapper_csi_ip_ctrl_offset = 0x98,
    .rxwrapper_cfg_credit_handler_en_offset = 0x190,
    .rxwrapper_cfg_credit_handler_line_size_offset = 0x1a0,
    .rxwrapper_cfg_credit_handler_frame_height_offset = 0x1b0,
    .rxwrapper_cfg_credit_handler_buffer_frames_offset = 0x1c0,
    .rxwrapper_cfg_credit_handler_dma_page_size_offset = 0x1d0,
    .rxwrapper_cfg_credit_handler_tu_credit_en_offset = 0x1e0,
    .rxwrapper_cfg_credit_handler_tu_credit_size_offset = 0x1f0,
    .rxwrapper_cfg_credit_handler_tu_size_in_dma_pages_offset = 0x200,
    .rxwrapper_cfg_credit_handler_dma_credit_en_offset = 0x210,
    .rxwrapper_cfg_credit_handler_int_credit_en_offset = 0x220,
    .rxwrapper_pipes_cfg_credit_handler_ext_unprocessed_cnt_offset = 0x230,
    .rxwrapper_cfg_credit_handler_frame_drop_en_offset = 0x240,
    .rxwrapper_cfg_credit_handler_frame_drop_th_offset = 0x250,
    .rxwrapper_cfg_credit_handler_almost_full_th_offset = 0x260,
    .rxwrapper_cfg_credit_handler_sram_mode_en_offset = 0x270,
    .rxwrapper_pipes_cfg_credit_handler_count_overflow_frames_offset = 0x280,
    .rxwrapper_pipes_ctl_credit_handler_srst_offset = 0x290,
    .rxwrapper_pipes_status_credit_handler_frame_cnt_offset = 0x2c0,
    .rxwrapper_pipes_status_credit_handler_dropped_frame_cnt_offset = 0x300,
    .rxwrapper_csi_out_line_buf_cfg = {
        .exist = 0,
        .offset = 0,
    },
};

static const struct of_device_id hailo15_rxwrapper_of_match[] = {
	{ .compatible = "hailo,hailo15-rxwrapper", .data = &hailo15_rxwrapper_config },
	{ .compatible = "hailo,hailo15l-rxwrapper", .data = &hailo15l_rxwrapper_config },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, hailo15_rxwrapper_of_match);

enum hailo15_rxwrapper_pads {
	RXWRAPPER_SINK_PAD_0,
	RXWRAPPER_SOURCE_PAD_1,
	RXWRAPPER_SOURCE_PAD_2,
	RXWRAPPER_SOURCE_PAD_3,
	RXWRAPPER_SOURCE_PAD_4,
	RXWRAPPER_SOURCE_PAD_5,
	RXWRAPPER_SOURCE_PAD_6,
	RXWRAPPER_PAD_MAX,
};

struct hailo15_rxwrapper_pipe_cfg {
	uint32_t dtype;
	uint32_t stride;
	uint32_t lines_nr;
	uint32_t used_by_grp_id;
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
	struct media_pad pads[RXWRAPPER_PAD_MAX];
	struct v4l2_mbus_framefmt pad_fmts[RXWRAPPER_PAD_MAX];
	int source_pad;
	void *__iomem base;
	struct vm_area_struct vma;
	struct mutex lock;
	struct hailo15_rxwrapper_pipe_cfg pipe_cfg[RXWRAPPER_NUM_PIPES];
	struct clk *rxwrapper_p_clk;
	struct clk *rxwrapper_data_clk;
	struct clk *rxwrapper_xtal_clk;
	struct hailo15_p2a_buffer_regs_addr p2a_buf_regs;
	struct hailo15_buf_ctx *buf_ctx;
	
	spinlock_t buf_lock; /* Protects the following variables: */
	struct hailo15_buffer *cur_buf[HAILO15_VID_GRP_MAX];
	struct hailo15_buffer *next_buf[HAILO15_VID_GRP_MAX];
	struct list_head buf_queue[HAILO15_VID_GRP_MAX];
	atomic_t num_works_processing;
	
	const struct rxwrapper_config *rxwrapper_cfg;
	uint64_t vision_ss_null_addr;
	void *private_data[HAILO15_VID_GRP_MAX];
	int id; /* rxwrapper id: 0/1/... */
	int irq;
	int num_exposures;
	u64 frame_count;

	/* Defer Interrupt handling variables */
	struct list_head irq_work_list;
	spinlock_t irq_work_list_lock;
	struct workqueue_struct* irq_work_wq;
	struct work_struct irq_work;
};

/* Defer Interrupt info element */
struct hailo15_irq_work {
	int grp_id;
	struct hailo15_buffer *dequeued_buf;
	struct list_head list;
};

static const struct v4l2_mbus_framefmt fmt_default = {
	.width		= 3840,
	.height		= 2160,
	.code		= MEDIA_BUS_FMT_SRGGB12_1X12,
	.field		= V4L2_FIELD_NONE,
	.colorspace	= V4L2_COLORSPACE_DEFAULT,
};

struct hailo15_rxwrapper_pipe_cfg rxwrapper_default_pipe_cfg = {
	.dtype = RAW12_DT,
	.lines_nr = RES_4K_FRAME_LINE_NUM * RING_BUFFER_FRAMES,
	.stride = RES_4K_STRIDE,
	.used_by_grp_id = HAILO15_VID_GRP_INVALID,
};

struct hailo15_rxwrapper_credits_cfg rxwrapper_default_credits_cfg = {
	.line_size = RXWRAPPER_RES_4K_DEFAULT_CREDITS_LINE_SIZE,
	.frame_height = RES_4K_FRAME_LINE_NUM,
	.buffer_frames = RXWRAPPER_DEFAULT_CREDITS_BUFFER_FRAMES,
	.dma_page_size = RXWRAPPER_DEFAULT_CREDITS_DMA_PAGE_SIZE,
	.tu_credit_size = RXWRAPPER_RES_4K_DEFAULT_CREDITS_TU_CREDIT_SIZE,
	.tu_size_in_dma_pages = RXWRAPPER_RES_4K_DEFAULT_CREDITS_TU_SIZE_IN_DMA_PAGES,
	.frame_drop_th = RXWRAPPER_DEFAULT_CREDITS_FRAME_DROP_TH,
	.almost_full_th = RXWRAPPER_DEFAULT_CREDITS_ALMOST_FULL_TH,
};

struct hailo15_rxwrapper_pipe_cfg rxwrapper_chosen_pipe_cfg;
struct hailo15_rxwrapper_credits_cfg rxwrapper_chosen_credits_cfg;

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

static int
hailo15_rxwrapper_write_only_field(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
				  u32 offset, u32 shift, u32 width, u32 data)
{
	u32 reg = 0;

	if (!hailo15_rxwrapper || !hailo15_rxwrapper->base) {
		return -EINVAL;
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

static int
hailo15_rxwrapper_pipe_write_only(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
				 u32 pipe, u32 offset, u32 shift, u32 width, u32 data)
{
	PIPE_VALIDATE_RANGE(pipe);

	return hailo15_rxwrapper_write_only_field(hailo15_rxwrapper,
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
	hailo15_rxwrapper->pipe_cfg[pipe].used_by_grp_id = (enable)
							 ? hailo15_rxwrapper->sd.grp_id
							 : HAILO15_VID_GRP_INVALID;
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
		hailo15_rxwrapper->rxwrapper_cfg->rxwrapper_pipes_data_cfg_dtype_shift,
		RXWRAPPER_PIPES_DATA_CFG_DTYPE_WIDTH, dtype);
}

static int hailo15_rxwrapper_pipe_set_wild_card_vc(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe, bool enable)
{
	return hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe, RXWRAPPER_PIPES_DATA_CFG_OFFSET,
		hailo15_rxwrapper->rxwrapper_cfg->rxwrapper_pipes_data_cfg_wc_vc_shift,
		RXWRAPPER_PIPES_DATA_CFG_WC_VC_WIDTH, enable);
}

static int hailo15_rxwrapper_pipe_set_vc(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe, u32 vc)
{
	return hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe, RXWRAPPER_PIPES_DATA_CFG_OFFSET,
		RXWRAPPER_PIPES_DATA_CFG_VC_SHIFT,
		RXWRAPPER_PIPES_DATA_CFG_VC_WIDTH, vc);
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
	const struct rxwrapper_config *rxwrapper_cfg = hailo15_rxwrapper->rxwrapper_cfg;


	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_line_size_offset, 0, 12,
		credits_cfg->line_size);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_LINE_SIZE failed\n", __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_frame_height_offset, 0, 12,
		credits_cfg->frame_height);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_FRAME_HEIGHT failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_buffer_frames_offset, 0, 16,
		credits_cfg->buffer_frames);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_BUFFER_FRAMES failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_dma_page_size_offset, 0, 3,
		credits_cfg->dma_page_size);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_DMA_PAGE_SIZE failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_tu_credit_en_offset, 0, 1, 0x1);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_TU_CREDIT_EN failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_tu_credit_size_offset, 0, 12,
		credits_cfg->tu_credit_size);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_TU_CREDIT_SIZE failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_tu_size_in_dma_pages_offset, 0, 17,
		credits_cfg->tu_size_in_dma_pages);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_TU_SIZE_IN_DMA_PAGES failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_dma_credit_en_offset, 0, 1, 0x0);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_DMA_CREDIT_EN failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_int_credit_en_offset, 0, 1, 0x1);
	if (ret) {
		pr_err("%s - CFG_CREDIT_HANDLER_TU_SIZE_IN_DMA_PAGES failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_frame_drop_th_offset, 0, 23,
		credits_cfg->frame_drop_th);
	if (ret) {
		pr_err("%s - RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_DROP_TH_OFFSET failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_almost_full_th_offset, 0, 23,
		credits_cfg->almost_full_th);
	if (ret) {
		pr_err("%s - RXWRAPPER_CFG_CREDIT_HANDLER_ALMOST_FULL_TH_OFFSET failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_sram_mode_en_offset, 0, 1, 0x0);
	if (ret) {
		pr_err("%s - RXWRAPPER_CFG_CREDIT_HANDLER_SRAM_MODE_EN_OFFSET failed\n",
			   __func__);
		return ret;
	}

	/* Disable frame drop mechanism */
	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe,
		rxwrapper_cfg->rxwrapper_cfg_credit_handler_frame_drop_en_offset, 0, 1, 0x0);
	if (ret) {
		pr_err("%s - RXWRAPPER_CFG_CREDIT_HANDLER_FRAME_DROP_EN_OFFSET failed\n",
			   __func__);
		return ret;
	}

	ret = hailo15_rxwrapper_pipe_write(
		hailo15_rxwrapper, pipe, rxwrapper_cfg->rxwrapper_cfg_credit_handler_en_offset,
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

/* Should be called with hailo15_rxwrapper->buf_lock locked */
static int __maybe_unused hailo15_rxwrapper_pipe_set_data_address_or_null(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe, u32 grp_id)
{
	u64 config_address;
	struct hailo15_buffer *buf;
	int plane_idx = (hailo15_rxwrapper->num_exposures == 1) ? 0 : pipe;

	VID_GRP_VALIDATE_RANGE(grp_id);
	PIPE_VALIDATE_RANGE(pipe);

	buf = hailo15_rxwrapper->next_buf[grp_id];
	config_address = buf ? 
		buf->dma[plane_idx] : 
		hailo15_rxwrapper->vision_ss_null_addr;
	return hailo15_rxwrapper_pipe_set_data_address(hailo15_rxwrapper, pipe, config_address);
}

static int
hailo15_rxwrapper_pipe_config(struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
				  u32 pipe, u32 dtype, u32 lines, u32 stride, u32 grp_id)
{
	int ret;
	bool is_wildcard_vc = hailo15_is_p2a_wildcard_vc_grp_id(grp_id) && (hailo15_rxwrapper->num_exposures == 1);

	ret = hailo15_rxwrapper_pipe_set_dtype(hailo15_rxwrapper, pipe, dtype);
	if (ret)
		return ret;

	ret = hailo15_rxwrapper_pipe_set_lines_count(hailo15_rxwrapper, pipe, lines);
	if (ret)
		return ret;

	ret = hailo15_rxwrapper_pipe_set_stride(hailo15_rxwrapper, pipe, stride);
	if (ret)
		return ret;

	ret = hailo15_rxwrapper_pipe_set_wild_card_vc(hailo15_rxwrapper, pipe, is_wildcard_vc);
	if (ret)
		return ret;

	if (!is_wildcard_vc) {

		ret = hailo15_rxwrapper_pipe_set_vc(hailo15_rxwrapper, pipe, pipe);
		if (ret)
			return ret;
	}

	return 0;
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
	return hailo15_rxwrapper_write_field(hailo15_rxwrapper,
		hailo15_rxwrapper->rxwrapper_cfg->rxwrapper_csi_ip_ctrl_offset,
		0, 32, conf);
}

static int hailo15_rxwrapper_set_csi_out_line_buf_cfg(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 conf)
{
	if (!hailo15_rxwrapper->rxwrapper_cfg->rxwrapper_csi_out_line_buf_cfg.exist) {
		return 0;
	}

	return hailo15_rxwrapper_write_field(hailo15_rxwrapper,
		hailo15_rxwrapper->rxwrapper_cfg->rxwrapper_csi_out_line_buf_cfg.offset,
		0, 32, conf);
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
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 pipe, u32 grp_id)
{
	struct hailo15_rxwrapper_pipe_cfg *cfg;

	PIPE_VALIDATE_RANGE(pipe);

	cfg = &hailo15_rxwrapper->pipe_cfg[pipe];

	return hailo15_rxwrapper_pipe_config(hailo15_rxwrapper, pipe,
						 cfg->dtype, cfg->lines_nr,
						 cfg->stride, grp_id);
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
    hailo15_rxwrapper_set_cfg(hailo15_rxwrapper,
		hailo15_rxwrapper->rxwrapper_cfg->rxwrapper_cfg_reg_value);
	hailo15_rxwrapper_set_csi_ip_ctrl(hailo15_rxwrapper,
					  RXWRAPPER_CSI_IP_CTRL_CFG);
	hailo15_rxwrapper_set_csi_out_line_buf_cfg(
		hailo15_rxwrapper, RXWRAPPER_CSI_OUT_LINE_BUF_CFG);
}

static inline void hailo15_buffer_ready_int_enable(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, int csi, int pipe)
{
	uint32_t mask;
	mask = readl(hailo15_rxwrapper->p2a_buf_regs.buffer_ready_ap_int_mask_addr);
	mask |= (0x1U << (csi * RXWRAPPER_NUM_PIPES + pipe));
	writel(mask, hailo15_rxwrapper->p2a_buf_regs.buffer_ready_ap_int_mask_addr);
}

static inline void hailo15_buffer_ready_int_disable(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, int csi, int pipe)
{
	uint32_t mask;
	mask = readl(hailo15_rxwrapper->p2a_buf_regs.buffer_ready_ap_int_mask_addr);
	mask &= ~(0x1U << (csi * RXWRAPPER_NUM_PIPES + pipe));
	writel(mask, hailo15_rxwrapper->p2a_buf_regs.buffer_ready_ap_int_mask_addr);
}

static inline void hailo15_buffer_ready_int_clr(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper, int csi, int pipe)
{
	writel((0x1U << (csi * RXWRAPPER_NUM_PIPES + pipe)), hailo15_rxwrapper->p2a_buf_regs.buffer_ready_ap_int_w1c_addr);
}

static inline uint32_t hailo15_buffer_ready_int_status(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	return readl(hailo15_rxwrapper->p2a_buf_regs.buffer_ready_ap_int_status_addr);
}

static inline uint32_t hailo15_get_used_pipes(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	int i;
	uint32_t used_pipes = 0;
	for (i = 0; i < RXWRAPPER_NUM_PIPES; i++) {
		if (hailo15_rxwrapper->pipe_cfg[i].used_by_grp_id != HAILO15_VID_GRP_INVALID) {
			return used_pipes |= BIT(i);
		}
	}
	return used_pipes;
}

int hailo15_rxwrapper_set_stream(struct v4l2_subdev *sd, int enable)
{
	int i, pipe, real_pipe, ret = 0;
	struct v4l2_subdev *remote_src_subdev;
	struct media_pad *remote_subdev_src_pad;
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper = v4l2_subdev_to_hailo15_rxwrapper(sd);
	int csi = hailo15_rxwrapper->id;

	pr_debug("%s: enable=%d\n", __func__, enable);

	if (!hailo15_rxwrapper)
		return -EINVAL;

	mutex_lock(&hailo15_rxwrapper->lock);

	remote_subdev_src_pad = media_entity_remote_pad(&hailo15_rxwrapper->pads[RXWRAPPER_SINK_PAD_0]);
	if (!remote_subdev_src_pad) {
		pr_err("%s - failed to get connected remote pad to subdev %s:pad[%d] , (ctx grp_id %d), aborting...\n",
			__func__, sd->name, RXWRAPPER_SINK_PAD_0, sd->grp_id);
		ret = -ENODEV;
		goto finish;
	}

	if (!is_media_entity_v4l2_subdev(remote_subdev_src_pad->entity)) {
		pr_err("%s - remote entity %s:pad[%d] connected to subdev %s:pad[%d] is not v4l2 subdev, (ctx grp_id %d), aborting...\n",
			__func__, remote_subdev_src_pad->entity->name, remote_subdev_src_pad->index, sd->name, RXWRAPPER_SINK_PAD_0, sd->grp_id);
		ret = -ENODEV;
		goto finish;
	}

	remote_src_subdev = media_entity_to_v4l2_subdev(remote_subdev_src_pad->entity);
	remote_src_subdev->grp_id = sd->grp_id;

	if (!hailo15_is_p2a_grp_id(sd->grp_id)) {
		ret = v4l2_subdev_call(remote_src_subdev, video, s_stream, enable);
		if (ret) {
			dev_err(hailo15_rxwrapper->dev, "%s: failed to enable source subdev\n", __func__);
			goto disable;
		}
		goto finish;
	}

	/* Case Pixel2Axi */
	real_pipe = hailo15_grp_id_to_pipe_id(sd->grp_id);
	if (enable) {
		for (i = 0; i < hailo15_rxwrapper->num_exposures; i++) {
			pipe = real_pipe + i;
			hailo15_rxwrapper_pipe_apply_cfg(hailo15_rxwrapper, pipe, sd->grp_id);
			hailo15_rxwrapper_pipe_enable_credits(hailo15_rxwrapper, pipe, &rxwrapper_chosen_credits_cfg);
			hailo15_rxwrapper_pipe_set_data_address(hailo15_rxwrapper, pipe, hailo15_rxwrapper->vision_ss_null_addr);
			hailo15_rxwrapper_pipe_init(hailo15_rxwrapper, pipe);
		}
		for (i = 0; i < hailo15_rxwrapper->num_exposures; i++) {
			pipe = real_pipe + i;
			hailo15_rxwrapper_pipe_enable(hailo15_rxwrapper, pipe);
			/* Configuring a new address immediately after init & enable pipeline,
			so that when we finished to process the first frame in the ring buffer,
			we will start to process the next frame right away,
			without waiting for the software to finish processing the previous buffer. */
			hailo15_rxwrapper_pipe_set_data_address_or_null(hailo15_rxwrapper, pipe, sd->grp_id);
			if (hailo15_rxwrapper->num_exposures > 1 && i < (hailo15_rxwrapper->num_exposures - 1) ) {
				continue;
			}
			hailo15_buffer_ready_int_enable(hailo15_rxwrapper, csi, pipe);
		}
		ret = v4l2_subdev_call(remote_src_subdev, video, s_stream, enable);
		if (ret) {
			dev_err(hailo15_rxwrapper->dev, "%s: failed to enable source subdev\n", __func__);
			goto disable;
		}
	} else {
		ret = v4l2_subdev_call(remote_src_subdev, video, s_stream, enable);
		if (ret) {
			dev_err(hailo15_rxwrapper->dev, "%s: failed to disable source subdev\n", __func__);
		}
disable:
		for (i = 0; i < hailo15_rxwrapper->num_exposures; i++) {
			pipe = real_pipe + i;
			hailo15_rxwrapper_pipe_disable(hailo15_rxwrapper, pipe);
			hailo15_rxwrapper_pipe_reset(hailo15_rxwrapper, pipe);

			// clean buffer_done irq
			hailo15_buffer_ready_int_clr(hailo15_rxwrapper, csi, pipe);
			hailo15_buffer_ready_int_disable(hailo15_rxwrapper, csi, pipe);
		}
		hailo15_rxwrapper->cur_buf[sd->grp_id] = NULL;
		hailo15_rxwrapper->next_buf[sd->grp_id] = NULL;
		atomic_set(&hailo15_rxwrapper->num_works_processing, 0);

		// Soft reset per channel, clear all internal credits/counter/status
		for (i = 0; i < hailo15_rxwrapper->num_exposures; i++) {
			pipe = real_pipe + i;
			hailo15_rxwrapper_pipe_write_only(hailo15_rxwrapper, pipe,
			hailo15_rxwrapper->rxwrapper_cfg->rxwrapper_pipes_ctl_credit_handler_srst_offset,
			RXWRAPPER_PIPES_CTL_CREDIT_HANDLER_SRST_SHIFT,
			RXWRAPPER_PIPES_CTL_CREDIT_HANDLER_SRST_WIDTH, 0x1);
		}
	}
	hailo15_rxwrapper->frame_count = 0;


	/* TODO: MSW-2716: This code section is unused */
	/*
	goto finish;

err_bad_src_pad:
	dev_err(hailo15_rxwrapper->dev, "%s: bad source pad for source subdev\n", __func__);
finish:
*/
finish:
	mutex_unlock(&hailo15_rxwrapper->lock);
	return ret;
}

static int hailo15_rxwrapper_get_pad_format(struct v4l2_subdev *sd,
						struct v4l2_subdev_state *sd_state,
						struct v4l2_subdev_format *fmt)
{
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		v4l2_subdev_to_hailo15_rxwrapper(sd);

	struct v4l2_mbus_framefmt *src_format;
	struct v4l2_mbus_framefmt *dst_format;
	if (!hailo15_rxwrapper || !fmt || fmt->pad >= RXWRAPPER_PAD_MAX)
		return -EINVAL;

	src_format = &hailo15_rxwrapper->pad_fmts[fmt->pad];
	dst_format = &fmt->format;
	if (!src_format || !dst_format)
		return -EINVAL;

	*dst_format = *src_format;
	return 0;
}

static int hailo15_rxwrapper_change_chosen_pipe_credits_cfg(const struct v4l2_mbus_framefmt *fmt,
	struct hailo15_rxwrapper_pipe_cfg *pipe_cfg, struct hailo15_rxwrapper_credits_cfg *credits_cfg) {

	if (!fmt || !pipe_cfg || !credits_cfg)
		return -EINVAL;

	switch (fmt->code) {
		case MEDIA_BUS_FMT_YVYU8_2X8:
			pipe_cfg->dtype = YUV422_8b_DT;
			break;
		default:
			pipe_cfg->dtype = RAW12_DT;
			break;
	}

	pipe_cfg->lines_nr = fmt->height * RING_BUFFER_FRAMES;
	pipe_cfg->stride = fmt->width * 2; // 2 ppc

	credits_cfg->line_size = fmt->width * 2 / 8; // size in AXI beats- AXI is 8B
	credits_cfg->frame_height = fmt->height;
	credits_cfg->buffer_frames = RXWRAPPER_DEFAULT_CREDITS_BUFFER_FRAMES;
	credits_cfg->dma_page_size = RXWRAPPER_DEFAULT_CREDITS_DMA_PAGE_SIZE;
	credits_cfg->tu_credit_size = fmt->height;
	credits_cfg->tu_size_in_dma_pages = (fmt->height * fmt->width * 2) /
		RXWRAPPER_DMA_PAGE_SIZE_IN_BYTES;
	credits_cfg->frame_drop_th = RXWRAPPER_DEFAULT_CREDITS_FRAME_DROP_TH;
	credits_cfg->almost_full_th = RXWRAPPER_DEFAULT_CREDITS_ALMOST_FULL_TH;
	return 0;
}

static int hailo15_rxwrapper_set_pad_format(struct v4l2_subdev *sd,
						struct v4l2_subdev_state *sd_state,
						struct v4l2_subdev_format *fmt)
{
	struct v4l2_subdev *subdev;
	struct v4l2_subdev *sensor_sd;
	struct media_pad *pad;
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		v4l2_subdev_to_hailo15_rxwrapper(sd);
	const struct v4l2_mbus_framefmt *src_format = &fmt->format;
	struct v4l2_mbus_framefmt *dst_format;
	struct v4l2_subdev_format sensor_fmt = {0};
	int ret, i, pipe, real_pipe;
	int num_exposures;

	dev_dbg(hailo15_rxwrapper->dev, "%s: pad=%d\n", __func__, fmt->pad);

	if (!hailo15_rxwrapper || !fmt)
		return -EINVAL;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		return 0;
	}

	/* set format in hailo15_rxwrapper->pad_fmts */
	dst_format = &hailo15_rxwrapper->pad_fmts[fmt->pad];
	if (!dst_format)
		return -EINVAL;
	*dst_format = *src_format;

	switch (src_format->code) {
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
		num_exposures = 1;
		break;
	case MEDIA_BUS_FMT_SRGGB12_2X12:
	case MEDIA_BUS_FMT_SGBRG12_2X12:
		num_exposures = 2;
		break;
	case MEDIA_BUS_FMT_SRGGB12_3X12:
	case MEDIA_BUS_FMT_SGBRG12_3X12:
		num_exposures = 3;
		break;
	default:
		num_exposures = 1;
		break;
	}

	real_pipe = hailo15_grp_id_to_pipe_id(sd->grp_id);

	if (is_hdr_capable(sd->grp_id)) {
		// HDR capable group_id
		if (num_exposures > RXWRAPPER_MAX_NUM_EXPOSURES) {
			return -ENOTSUPP;
		}
		if (num_exposures > 1 && hailo15_get_used_pipes(hailo15_rxwrapper) != 0) {
			// In HDR mode, no other SDR pipes should be used
			return -EBUSY;
		}
	} else {
		// SDR capable
		if (num_exposures > 1) {
			// Not HDR capable group_id
			return -ENOTSUPP;
		}
		if (hailo15_get_used_pipes(hailo15_rxwrapper) & BIT(real_pipe)) {
			// Already used pipe;
			return -EBUSY;
		}
	}

	// Valid num_exposures, apply it.
	hailo15_rxwrapper->num_exposures = num_exposures;

	/* change chosen pipe_cfg & credits_cfg to match the fmt */
	ret = hailo15_rxwrapper_change_chosen_pipe_credits_cfg(src_format, &rxwrapper_chosen_pipe_cfg, &rxwrapper_chosen_credits_cfg);
	if (ret)
		return ret;

	for (i = 0; i < hailo15_rxwrapper->num_exposures; ++i) {
		pipe = real_pipe + i;
		ret = hailo15_rxwrapper_pipe_set_cfg(hailo15_rxwrapper, pipe, &rxwrapper_chosen_pipe_cfg);
		if (ret)
			return ret;
	}

	memcpy(&sensor_fmt, fmt, sizeof(struct v4l2_subdev_format));

	/* Propagate format to sink */
	pad = &hailo15_rxwrapper->pads[RXWRAPPER_SINK_PAD_0];
	if (pad)
		pad = media_entity_remote_pad(pad);

	if (pad && is_media_entity_v4l2_subdev(pad->entity)) {
		subdev = media_entity_to_v4l2_subdev(pad->entity);
		subdev->grp_id = sd->grp_id;
		ret = v4l2_subdev_call(subdev, pad, set_fmt, NULL, fmt);
		if (ret) {
			pr_err("%s - failed to set format %x on sink subdev %s, ret %d\n",
				__func__, fmt->format.code, subdev->name, ret);
			return ret;
		}
	}

	if(hailo15_is_p2a_grp_id(subdev->grp_id)) {
		/* in isp flow, set_fmt sensor_subdev will be called from daemon */
		sensor_sd = hailo15_get_sensor_subdev(hailo15_rxwrapper->sd.v4l2_dev->mdev);
		if (!sensor_sd) {
			pr_warn("%s - failed to get sensor subdev\n", __func__);
			return -EINVAL;
		}
		ret = v4l2_subdev_call(sensor_sd, pad, set_fmt, NULL, &sensor_fmt);
		if (ret) {
			pr_err("%s - failed to set format %x on sensor %s, ret %d\n",
				__func__, fmt->format.code, sensor_sd->name, ret);
		}
	}
	return ret;
}

/* When stopping stream, we want to be very sure that all deferred work have given all buffers back to user, so sleep if needed */
static void wait_for_deferred_work(struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	int i = 0;
	const int MAX_WAIT_ITERS = 40;
	const int MSECS_SLEEP = 20;
	bool ready = false;
	
	unsigned long flags;
	struct hailo15_irq_work *work, *tmp;
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(&hailo15_rxwrapper->sd);

	for (i = 0; i < MAX_WAIT_ITERS; i++) {
		/* Check no works are waiting to be handled, and that all dispatched works have finished executing */
		spin_lock_irqsave(&hailo15_rxwrapper->irq_work_list_lock, flags);
		ready = list_empty(&hailo15_rxwrapper->irq_work_list) && atomic_read(&hailo15_rxwrapper->num_works_processing) == 0;
		spin_unlock_irqrestore(&hailo15_rxwrapper->irq_work_list_lock, flags);

		if (ready)
			return;
		msleep(MSECS_SLEEP);
	}

	/* The work haven't been called this long? Pro-actively clean the queue */
	spin_lock_irqsave(&hailo15_rxwrapper->irq_work_list_lock, flags);
	if (!list_empty(&hailo15_rxwrapper->irq_work_list)) {
		list_for_each_entry_safe(work, tmp, &hailo15_rxwrapper->irq_work_list, list) {
			hailo15_dma_buffer_dequeue(ctx, work->grp_id, work->dequeued_buf);
			list_del(&work->list); // Remove the work from the list
			kfree(work);           // Free the allocated memory
		}
	}
	spin_unlock_irqrestore(&hailo15_rxwrapper->irq_work_list_lock, flags);

	/* Note we must use memory barrier to make sure this is the last operation in this function. */
	mb();
	if (atomic_read(&hailo15_rxwrapper->num_works_processing) != 0) {
		pr_err("Deferred work not finished after %d iterations of %d ms - should not happen\n", MAX_WAIT_ITERS, MSECS_SLEEP);
	}
}

static int hailo15_rxwrapper_queue_empty(struct hailo15_dma_ctx *ctx,
					 int grp_id)
{
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper = (struct hailo15_rxwrapper_priv *)ctx->dev;
	int i, pipe, real_pipe;
	unsigned long flags;
	struct hailo15_buffer *buf, *nbuf;

	if (!hailo15_is_p2a_grp_id(grp_id))
		return -EINVAL;

	real_pipe = hailo15_grp_id_to_pipe_id(grp_id);

	spin_lock_irqsave(&hailo15_rxwrapper->buf_lock, flags);

	/* clean cur_buf, next_buf and shadow registers (cur_buf can be null - we don't need to track what it was, since we are about to dequeue all buffers here anyway) */
	hailo15_rxwrapper->cur_buf[grp_id] = NULL;
	hailo15_rxwrapper->next_buf[grp_id] = NULL;
	for (i = 0; i < hailo15_rxwrapper->num_exposures; i++) {
		pipe = real_pipe + i;
		hailo15_rxwrapper_pipe_set_data_address(hailo15_rxwrapper, pipe,
			hailo15_rxwrapper->vision_ss_null_addr);
	}

	/* Delete all list elements - and dequeue them back to the userspace */
	list_for_each_entry_safe (buf, nbuf, &hailo15_rxwrapper->buf_queue[grp_id], irqlist) {
		if (buf) {
			list_del(&buf->irqlist);
			hailo15_dma_buffer_dequeue(ctx, grp_id, buf);
		}
	}

	spin_unlock_irqrestore(&hailo15_rxwrapper->buf_lock, flags);

	/* Deferred works might still delay returning buffers to user - wait for all of them to complete */
	wait_for_deferred_work(hailo15_rxwrapper);

	return 0;
}

/* Reads the number of unprocessed frames for each exposure to out parameter.
 * Make sure all exposures have the same number of unprocessed frames.
 * If not - print error message (if ignore_err is false) and return false.
 */
bool check_unprocessed_credits(struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 real_pipe, 
		uint32_t o_unprocessed_frames[RXWRAPPER_NUM_PIPES], bool ignore_err)
{
	int i, pipe;
	bool ret = true;
	const struct rxwrapper_config *rxwrapper_cfg = hailo15_rxwrapper->rxwrapper_cfg;

	/* Read number of unprocessed frames into given array, if 1 unrprocessed for each exposure we're ready */
	for (i = 0; i < hailo15_rxwrapper->num_exposures; i++) {
		pipe = real_pipe + i;
		o_unprocessed_frames[pipe] = hailo15_rxwrapper_pipe_read(hailo15_rxwrapper, pipe,
			rxwrapper_cfg->rxwrapper_pipes_cfg_credit_handler_ext_unprocessed_cnt_offset,
			RXWRAPPER_PIPES_CFG_CREDIT_HANDLER_EXT_UNPROCESSED_CNT_SHIFT,
			RXWRAPPER_PIPES_CFG_CREDIT_HANDLER_EXT_UNPROCESSED_CNT_WIDTH);

		ret |= (o_unprocessed_frames[pipe] > 0) && (o_unprocessed_frames[pipe] == o_unprocessed_frames[real_pipe]);
	}

	/* If we're not ready - print that - this should never happen (print all values of unprocessed frames) */
	if (!ret && !ignore_err) {
		pr_err_ratelimited("Unprocessed frames mismatch. pipe: %u, values: %u %u %u %u\n", real_pipe, o_unprocessed_frames[0],
			o_unprocessed_frames[1], o_unprocessed_frames[2], o_unprocessed_frames[3]);
	}

	return ret;
}

/* Checks HW registers, and prints if there was a frame that was overwritten by HW (warn, as this is not valid) */
void warn_overwritten_frames(struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 real_pipe,
		uint32_t unprocessed_frames[RXWRAPPER_NUM_PIPES])
{
	int i, pipe;
	int dropped;
	const struct rxwrapper_config *rxwrapper_cfg = hailo15_rxwrapper->rxwrapper_cfg;

	for (i = 0; i < hailo15_rxwrapper->num_exposures; i++) {
		pipe = real_pipe + i;
		dropped = hailo15_rxwrapper_pipe_read(hailo15_rxwrapper, pipe,
			rxwrapper_cfg->rxwrapper_pipes_status_credit_handler_dropped_frame_cnt_offset,
			RXWRAPPER_PIPES_STATUS_CREDIT_HANDLER_DROPPED_FRAME_CNT_SHIFT,
			RXWRAPPER_PIPES_STATUS_CREDIT_HANDLER_DROPPED_FRAME_CNT_WIDTH);
		if (dropped > 0 && unprocessed_frames[pipe] > RXWRAPPER_DEFAULT_CREDITS_FRAME_DROP_TH) {
			pr_warn_ratelimited("%s pipe %d overwrite! unprocessed frames = %u,dropped_frames = %u\n",
				__func__, pipe, unprocessed_frames[pipe], dropped);
		}
	}
}

void return_credits(struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 real_pipe,
	uint32_t unprocessed_frames[RXWRAPPER_NUM_PIPES])
{
	int i, pipe;
	u32 creds_to_return;
	const struct rxwrapper_config *rxwrapper_cfg = hailo15_rxwrapper->rxwrapper_cfg;

	for (i = 0; i < hailo15_rxwrapper->num_exposures; i++) {
		pipe = real_pipe + i;
		creds_to_return = (unprocessed_frames[pipe] > 0) ? 1 : 0;
		hailo15_rxwrapper_pipe_write(hailo15_rxwrapper, pipe,
			rxwrapper_cfg->rxwrapper_pipes_cfg_credit_handler_ext_unprocessed_cnt_offset,
			RXWRAPPER_PIPES_CFG_CREDIT_HANDLER_EXT_UNPROCESSED_CNT_SHIFT,
			RXWRAPPER_PIPES_CFG_CREDIT_HANDLER_EXT_UNPROCESSED_CNT_WIDTH, creds_to_return);
	}
}

/* Should be called with hailo15_rxwrapper->buf_lock locked */
void write_next_bufs_to_shadow_regs(struct hailo15_rxwrapper_priv *hailo15_rxwrapper, u32 real_pipe, u32 grp_id)
{
	int i, pipe;

	for (i = 0; i < hailo15_rxwrapper->num_exposures; i++) {
		pipe = real_pipe + i;
		hailo15_rxwrapper_pipe_set_data_address_or_null(hailo15_rxwrapper, pipe, grp_id);
	}
}

static int hailo15_irq_work_enqueue(struct hailo15_rxwrapper_priv *hailo15_rxwrapper, struct hailo15_irq_work *work)
{
	unsigned long flags;

	if (unlikely(!hailo15_rxwrapper->irq_work_wq)) {
		pr_err("%s[%d]: irq_work_wq is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	/* Add work to work queue, increase atomic counter of works that are under process
	 * Place memory barrier to make sure that the increment is done before the work is added to the queue
	 */
	atomic_inc(&hailo15_rxwrapper->num_works_processing);
	mb();
	spin_lock_irqsave(&hailo15_rxwrapper->irq_work_list_lock, flags);
	list_add_tail(&work->list, &hailo15_rxwrapper->irq_work_list);
	spin_unlock_irqrestore(&hailo15_rxwrapper->irq_work_list_lock, flags);
	queue_work(hailo15_rxwrapper->irq_work_wq, &hailo15_rxwrapper->irq_work);

	return 0;
}

void dequeue_buffer_with_deferred_work(struct hailo15_rxwrapper_priv *hailo15_rxwrapper, struct hailo15_dma_ctx *ctx, int grp_id, struct hailo15_buffer *dequeued_buf)
{
	/* Allocate a deferred work */
	struct hailo15_irq_work *work = kzalloc(sizeof(struct hailo15_irq_work), GFP_ATOMIC);
	if (!work) {
		pr_err_ratelimited("%s[%d]: failed to allocate irq work\n", __func__, __LINE__);
		return;
	}
	/* Update interrupted pipes to be handled by defer IRQ work. */
	work->dequeued_buf = dequeued_buf;
	work->grp_id = grp_id;

	hailo15_irq_work_enqueue(hailo15_rxwrapper, work);
}

void hailo15_rxwrapper_buffer_done(struct hailo15_rxwrapper_priv *hailo15_rxwrapper, bool is_first_frame_hdr, u32 real_pipe, u32 grp_id)
{
	uint32_t unprocessed_frames[RXWRAPPER_NUM_PIPES];
	bool unprocessed_ready = true;
	struct hailo15_buffer *dequeued_buf;
	unsigned long flags;
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(&hailo15_rxwrapper->sd);

	unprocessed_ready = check_unprocessed_credits(hailo15_rxwrapper, real_pipe, unprocessed_frames, is_first_frame_hdr);

	if (unprocessed_ready) {
		spin_lock_irqsave(&hailo15_rxwrapper->buf_lock, flags);

		/* cur_buf should be removed from list of rxwrapper bufs and then dequeued back to user (without catching spinlock) */
		dequeued_buf = hailo15_rxwrapper->cur_buf[grp_id];
		if (dequeued_buf)
			list_del(&dequeued_buf->irqlist);

		/* Update cur and next bufs */
		/* If new cur_buf is not null - it's the first entry in the list, so next_buf should be the second list entry */
		hailo15_rxwrapper->cur_buf[grp_id] = hailo15_rxwrapper->next_buf[grp_id];
		hailo15_rxwrapper->next_buf[grp_id] = list_first_entry_or_null(&hailo15_rxwrapper->buf_queue[grp_id], struct hailo15_buffer, irqlist);
		if (hailo15_rxwrapper->cur_buf[grp_id]) {
			if (!list_empty(&hailo15_rxwrapper->buf_queue[grp_id]) && !list_is_singular(&hailo15_rxwrapper->buf_queue[grp_id]))
				hailo15_rxwrapper->next_buf[grp_id] = list_next_entry(hailo15_rxwrapper->next_buf[grp_id], irqlist);
			else
				hailo15_rxwrapper->next_buf[grp_id] = NULL;
		}
		
		write_next_bufs_to_shadow_regs(hailo15_rxwrapper, real_pipe, grp_id);

		/* Pass buffer back to userspace (deferred work - outside of critical section) */
		spin_unlock_irqrestore(&hailo15_rxwrapper->buf_lock, flags);
		if (dequeued_buf) {
			dequeued_buf->vb.vb2_buf.timestamp = ktime_get_ns();
			dequeue_buffer_with_deferred_work(hailo15_rxwrapper, ctx, grp_id, dequeued_buf);
		}
	}
	else if (!is_first_frame_hdr) {
		// Don't print warning if first HDR frame is incomplete. The first frame might be problematic due to HW.
		warn_overwritten_frames(hailo15_rxwrapper, real_pipe, unprocessed_frames);
	}

	return_credits(hailo15_rxwrapper, real_pipe, unprocessed_frames);
}

static struct v4l2_subdev_video_ops hailo15_rxwrapper_v4l2_subdev_video_ops = {
	.s_stream = hailo15_rxwrapper_set_stream,
};

static const struct v4l2_subdev_pad_ops hailo15_rxwrapper_v4l2_subdev_pad_ops = {
	.set_fmt = hailo15_rxwrapper_set_pad_format,
	.get_fmt = hailo15_rxwrapper_get_pad_format,
};

static int rxwrapper_querycap(struct hailo15_rxwrapper_priv *hailo15_rxwrapper, struct v4l2_capability *cap)
{
	strncpy((char *)cap->driver, "hailo-rxwrapper", sizeof(cap->driver));
	memset(cap->bus_info, 0, sizeof(cap->bus_info));
	/* user space needs the id and gets it from here */
	memset(cap->bus_info, hailo15_rxwrapper->id, sizeof(hailo15_rxwrapper->id));
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
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		v4l2_subdev_to_hailo15_rxwrapper(sd);

	dev_dbg(sd->v4l2_dev->dev, "rxwrapper_priv_ioctl\n");

	switch (cmd) {
	case VIDIOC_QUERYCAP:
		ret = rxwrapper_querycap(hailo15_rxwrapper, arg);
		break;
	default:
		pr_debug("rxwrapper: got unsupported ioctl 0x%x\n", cmd);
		ret = -ENOENT;
		break;
	}
	return ret;
}

static int hailo15_rxwrapper_registered(struct v4l2_subdev* sd)
{
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(sd);
	struct hailo15_rxwrapper_priv *rxwrapper_priv = ctx->dev;
	struct v4l2_subdev *remote_subdev = NULL;
	int ret, pad = 0;
	/* TODO: fix crash in ISP when this function fails */
	dev_dbg(rxwrapper_priv->dev, "hailo15_rxwrapper_registered\n");
	ret = hailo15_media_get_subdev(sd->dev, pad, &remote_subdev);
	if (ret) {
		dev_err(sd->dev, "Failed to get remote subdev connected to pad #%d\n", pad);
		return ret;
	}

	if (!remote_subdev->entity.graph_obj.mdev) {
		dev_err(sd->dev, "Failed to get remote subdevice connected to pad #%d\n", pad);
		return -ENODEV;
	}

	ret = v4l2_subdev_call(remote_subdev, core, ioctl, VIDEO_GET_P2A_REGS, &rxwrapper_priv->p2a_buf_regs);
	if (ret) {
		dev_err(sd->dev, "Failed to get P2A registers from subdevice %s\n", remote_subdev->name);
		return ret;
	}

	return hailo15_media_create_links(rxwrapper_priv->dev, &sd->entity, -1);
}

static struct v4l2_subdev_internal_ops rxwrapper_internal_ops = {
	.registered = hailo15_rxwrapper_registered,
};

static struct v4l2_subdev_core_ops rxwrapper_core_ops = {
	.ioctl = rxwrapper_priv_ioctl,
};

struct v4l2_subdev_ops hailo15_rxwrapper_v4l2_subdev_ops = {
	.core = &rxwrapper_core_ops,
	.video = &hailo15_rxwrapper_v4l2_subdev_video_ops,
	.pad = &hailo15_rxwrapper_v4l2_subdev_pad_ops,
};

static int hailo15_rxwrapper_buffer_queue(struct hailo15_dma_ctx *ctx, struct hailo15_buffer *buf) {
	struct hailo15_rxwrapper_priv *rxw_priv;
	struct v4l2_subdev *sd;
	unsigned long flags;
	
	sd = buf->sd;
	rxw_priv = container_of(sd, struct hailo15_rxwrapper_priv, sd);
	
	spin_lock_irqsave(&rxw_priv->buf_lock, flags);
	list_add_tail(&buf->irqlist, &rxw_priv->buf_queue[buf->grp_id]);
	spin_unlock_irqrestore(&rxw_priv->buf_lock, flags);
	return 0;
}

static int hailo15_rxwrapper_set_private_data(struct hailo15_dma_ctx *ctx,
						  int grp_id, void *data)
{
	struct hailo15_rxwrapper_priv *rxwrapper_dev =
		(struct hailo15_rxwrapper_priv *)ctx->dev;

	if (!hailo15_is_p2a_grp_id(grp_id))
		return -EINVAL;

	rxwrapper_dev->private_data[grp_id] = data;

	return 0;
}

static int hailo15_rxwrapper_get_private_data(struct hailo15_dma_ctx *ctx,
						  int grp_id, void **data)
{
	struct hailo15_rxwrapper_priv *rxwrapper_dev =
		(struct hailo15_rxwrapper_priv *)ctx->dev;

	if (!data)
		return -EINVAL;

	if (!hailo15_is_p2a_grp_id(grp_id))
		return -EINVAL;

	*data = rxwrapper_dev->private_data[grp_id];

	return 0;
}

static struct hailo15_irq_work * hailo15_irq_work_dequeue(struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	struct hailo15_irq_work *work;
	unsigned long flags;

	if (unlikely(!hailo15_rxwrapper->irq_work_wq)) {
		pr_err("%s[%d]: irq_work_wq is NULL\n", __func__, __LINE__);
		return NULL;
	}

	spin_lock_irqsave(&hailo15_rxwrapper->irq_work_list_lock, flags);
	work = list_first_entry_or_null(&hailo15_rxwrapper->irq_work_list, struct hailo15_irq_work, list);
	if (work)
		list_del(&work->list);
	spin_unlock_irqrestore(&hailo15_rxwrapper->irq_work_list_lock, flags);

	return work;
}

static int hailo15_irq_work_queue_release(struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	unsigned long flags;
	struct hailo15_irq_work *work, *tmp;
	if (unlikely(!hailo15_rxwrapper)) {
		pr_err("%s[%d]: hailo15_rxwrapper is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	pr_debug("%s[%d]: rxwrapper_work_wq flush and destroy...\n", __FUNCTION__, __LINE__);
	spin_lock_irqsave(&hailo15_rxwrapper->irq_work_list_lock, flags);
	flush_workqueue(hailo15_rxwrapper->irq_work_wq);
	destroy_workqueue(hailo15_rxwrapper->irq_work_wq);
	list_for_each_entry_safe(work, tmp, &hailo15_rxwrapper->irq_work_list, list) {
		list_del(&work->list); // Remove the work from the list
		kfree(work);           // Free the allocated memory
	}
	spin_unlock_irqrestore(&hailo15_rxwrapper->irq_work_list_lock, flags);

	return 0;
}

void hailo15_rxwrapper_irq_work_handle(struct work_struct *work);
static int hailo15_irq_work_queue_setup(struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	if (unlikely(!hailo15_rxwrapper)) {
		pr_err("%s[%d]: hailo15_rxwrapper is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	pr_debug("%s[%d]: rxwrapper_work_wq setup...\n", __FUNCTION__, __LINE__);
	INIT_LIST_HEAD(&hailo15_rxwrapper->irq_work_list);
	spin_lock_init(&hailo15_rxwrapper->irq_work_list_lock);
	hailo15_rxwrapper->irq_work_wq = alloc_ordered_workqueue("rxwrapper_work_wq", WQ_HIGHPRI);
	INIT_WORK(&hailo15_rxwrapper->irq_work, hailo15_rxwrapper_irq_work_handle);

	return 0;
}

static uint32_t calc_hdr_expected_int_status(uint32_t csi, uint32_t num_exposures, bool is_first_frame_hdr) 
{
	uint32_t mask = (1 << num_exposures) -  1;
	if (is_first_frame_hdr) {
		/* Fix for MSW-4889: We don't expect  for HDR LEF frame in VC #0 to be ready. */
		mask &= ~0x1;
	}
    return mask << (csi * RXWRAPPER_NUM_PIPES);
}

static bool hailo15_rxwrapper_check_all_hdr_exposures_rdy(
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper,
	uint32_t csi,
	uint32_t csi_int_status,
	uint32_t *expected_int_status,
	bool *first_hdr_frame)
{
	/* Fix for MSW-4889:
	 * - Due to a BUG, the 1'st HDR frame includes only SEF1, SEF2 exposures.
	 * - Thus, in case of:
	 *   - 1st HDR frame:
	 *     - Only SEF1,SEF2 exposures will be reported due to internal BUG.
	 *     - Pipe #0 ready frames counter is 0.
	 *   - otherwise:
	 *     - All exposures will be reported as expected.
	 * Note:
	 *   - Pipe #0 -> Frame #1 (VC #0): LEF (Long Exposure Frame).
	 *   - Pipe #1 -> Frame #2 (VC #1): SEF1 (Small Exposure Frame #1).
	 *   - Pipe #2 -> Frame #3 (VC #2): SEF2 (Small Exposure Frame #2).
	 */
	/* Read pipe-0 ready frames counter to determine if it's the first HDR frame. */
	uint32_t frame_cnt_pipe_0 = hailo15_rxwrapper_pipe_read(hailo15_rxwrapper, RXWRAPPER_PIPE_0,
		hailo15_rxwrapper->rxwrapper_cfg->rxwrapper_pipes_status_credit_handler_frame_cnt_offset,
		RXWRAPPER_PIPES_STATUS_CREDIT_HANDLER_FRAME_CNT_SHIFT,
		RXWRAPPER_PIPES_STATUS_CREDIT_HANDLER_FRAME_CNT_WIDTH);

	/* Check if first HDR frame exposures are ready */
	*expected_int_status = calc_hdr_expected_int_status(csi, hailo15_rxwrapper->num_exposures, true);
	if(frame_cnt_pipe_0 == 0 && (*expected_int_status & csi_int_status) == *expected_int_status) {
		/* Expected exposures are ready: SEF1, SEF2 */
		*first_hdr_frame = true;
		return true;
	}

	*first_hdr_frame = false;
	
	/* Check if all exposures are ready */
	*expected_int_status = calc_hdr_expected_int_status(csi, hailo15_rxwrapper->num_exposures, false);
	if ((*expected_int_status & csi_int_status) == *expected_int_status) {
		return true;
	}

	return false;
}

#define PIXEL_2_AXI_BUF_RDY_MASK(_csi, _pipe) BIT(((_csi) * RXWRAPPER_NUM_PIPES + (_pipe)))
#define PIXEL_2_AXI_BUF_RDY_MASK__CSI(_csi)  GENMASK(((_csi + 1) * RXWRAPPER_NUM_PIPES-1), (_csi) * RXWRAPPER_NUM_PIPES)

void hailo15_rxwrapper_irq_work_handle(struct work_struct *work)
{
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
		(struct hailo15_rxwrapper_priv *)container_of(work, struct hailo15_rxwrapper_priv, irq_work);
	struct hailo15_dma_ctx *ctx = v4l2_get_subdevdata(&hailo15_rxwrapper->sd);
	struct hailo15_irq_work *h15_irq_work = hailo15_irq_work_dequeue(hailo15_rxwrapper);

	if (!h15_irq_work) {
		pr_warn("%s[%d]: irq_work is NULL - possible if it's the end of stream\n", __func__, __LINE__);
		goto work_done;
	}

	hailo15_rxwrapper->frame_count++;
	hailo15_dma_buffer_dequeue(ctx, h15_irq_work->grp_id, h15_irq_work->dequeued_buf);
	kfree(h15_irq_work);


work_done:
	/* Decrease counter of works under processing.
	 * Note we must use memory barrier to make sure this is the last operation in this function.
	 */
	mb();
	atomic_dec(&hailo15_rxwrapper->num_works_processing);
}

void call_vcs_buffer_done(struct hailo15_rxwrapper_priv *hailo15_rxwrapper, int csi, uint32_t vc_status, bool first_hdr_frame)
{
	int pipe, grp_id, mask;
	bool hdr_mode = (hailo15_rxwrapper->num_exposures > 1) ? true : false;

	if (hdr_mode) {
		/* HDR mode */
		grp_id = (csi == HAILO15_CSI_0) ? HAILO15_VID_GRP_SX_CSI0_P2A : HAILO15_VID_GRP_SX_CSI1_P2A;
		hailo15_rxwrapper_buffer_done(hailo15_rxwrapper, first_hdr_frame, RXWRAPPER_PIPE_0, grp_id);

		return;
	}
	
	/* SDR mode - call buffer_done of each VC */
	for (pipe = 0; pipe < RXWRAPPER_NUM_PIPES; pipe++) {
		mask = PIXEL_2_AXI_BUF_RDY_MASK(csi, pipe);
		if (vc_status & mask) {
			if (hailo15_rxwrapper->pipe_cfg[pipe].used_by_grp_id != HAILO15_VID_GRP_INVALID) {
				grp_id = hailo15_rxwrapper->pipe_cfg[pipe].used_by_grp_id;
				hailo15_rxwrapper_buffer_done(hailo15_rxwrapper, false, pipe, grp_id);
			}
		}
	}
}

static irqreturn_t hailo15_rxwrapper_irq_handler(int irq, void *arg)
{
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper = (struct hailo15_rxwrapper_priv *)arg;
	uint32_t int_status, int_status_per_csi, int_status_for_vc;
	bool hdr_mode = (hailo15_rxwrapper->num_exposures > 1) ? true : false;
	bool first_hdr_frame;

	/* NOTE: each rxwrapper handles its own CSI */
	int csi = hailo15_rxwrapper->id;

	/* The interrupt status for current CSI channel */
	int_status = hailo15_buffer_ready_int_status(hailo15_rxwrapper);
	pr_debug("%s[%d]: int mask %X\n", __func__, __LINE__, int_status);
	int_status_per_csi = (PIXEL_2_AXI_BUF_RDY_MASK__CSI(csi) & int_status);
	if (int_status_per_csi == 0) {
		/* If interrupts are not from this Rx wrapper instance, return (this is a shared IRQ handler!) */
		return IRQ_NONE;
	}

	/* In order to support multi VC (Virtual Channels) of a CSI channel, we extract the interrupt status of VCs */
	if (hdr_mode) {
		/* For first HDR frame, LEF is not expected, we allow int_status to be without it */
		if (!hailo15_rxwrapper_check_all_hdr_exposures_rdy(hailo15_rxwrapper, csi, int_status_per_csi, &int_status_for_vc, &first_hdr_frame)) {
			pr_warn_ratelimited("RXWRAPPER: %s: HDR frame not ready, int_status_per_csi: 0x%x\n", __func__, int_status_per_csi);
			return IRQ_NONE;
		}
	} else {
		int_status_for_vc = int_status_per_csi;
	}

	call_vcs_buffer_done(hailo15_rxwrapper, csi, int_status_for_vc, first_hdr_frame);

	/* Clear the interrupts - required by HW */
	writel(int_status_for_vc, hailo15_rxwrapper->p2a_buf_regs.buffer_ready_ap_int_w1c_addr);

	return IRQ_HANDLED;
}

static int hailo15_rxwrapper_get_frame_count(struct hailo15_dma_ctx *dma_ctx, int grp_id,
                                           uint64_t *fc)
{
        struct hailo15_rxwrapper_priv *hailo15_rxwrapper =
                (struct hailo15_rxwrapper_priv *)dma_ctx->dev;
        if (!fc)
                return -EINVAL;

        *fc = hailo15_rxwrapper->frame_count;
        return 0;
}

static struct hailo15_buf_ops hailo15_rxwrapper_buf_ops = {
	.buffer_queue = hailo15_rxwrapper_buffer_queue,
	.set_private_data = hailo15_rxwrapper_set_private_data,
	.get_private_data = hailo15_rxwrapper_get_private_data,
	.queue_empty = hailo15_rxwrapper_queue_empty,
	.get_frame_count = hailo15_rxwrapper_get_frame_count,
};

static int
hailo15_rxwrapper_dma_ctx_init(struct hailo15_dma_ctx *ctx, int grp_id)
{
	if (!ctx) {
		return -EINVAL;
	}

	VID_GRP_VALIDATE_RANGE(grp_id);

	ctx->buf_ctx[grp_id].ops = kzalloc(sizeof(struct hailo15_buf_ops), GFP_KERNEL);
	if (!ctx->buf_ctx[grp_id].ops)
		return -ENOMEM;
	memcpy(ctx->buf_ctx[grp_id].ops, &hailo15_rxwrapper_buf_ops,
		   sizeof(struct hailo15_buf_ops));
	return 0;
}

static int
hailo15_rxwrapper_dma_ctx_init_all(struct hailo15_dma_ctx *ctx,
				   struct hailo15_rxwrapper_priv *hailo15_rxwrapper)
{
	int i, ret;

	if (!ctx || !hailo15_rxwrapper) {
		return -EINVAL;
	}

	ctx->dev = (void *)hailo15_rxwrapper;
	for (i = 0; i < HAILO15_VID_GRP_MAX; i++) {
		ret = hailo15_rxwrapper_dma_ctx_init(ctx, i);
		if (ret) {
			pr_err("hailo15 rxwrapper: failed to init dma ctx for grp_id-%d, ret %d\n", i, ret);
			return ret;
		}
	}
	v4l2_set_subdevdata(&hailo15_rxwrapper->sd, ctx);

	return 0;
}

static int
hailo15_rxwrapper_dma_ctx_clean(struct hailo15_dma_ctx *ctx, u32 grp_id)
{
	if (!ctx) {
		return -EINVAL;
	}

	VID_GRP_VALIDATE_RANGE(grp_id);

	if (ctx->buf_ctx[grp_id].ops) {
		kfree(ctx->buf_ctx[grp_id].ops);
	}

	return 0;
}

static int
hailo15_rxwrapper_dma_ctx_clean_all(struct hailo15_dma_ctx *ctx)
{
	int i;

	for (i = 0; i < HAILO15_VID_GRP_MAX; i++) {
		hailo15_rxwrapper_dma_ctx_clean(ctx, i);
	}

	return 0;
}

static int hailo15_rxwrapper_parse_null_addr(struct hailo15_rxwrapper_priv* hailo15_rxwrapper) {
	struct fwnode_handle *rxwrapper_node = NULL, *parent_node = NULL;
	uint32_t null_addr = 0;
	int ret = -EINVAL;

	if (!hailo15_rxwrapper || !hailo15_rxwrapper->dev) {
		pr_err("%s: Invalid hailo15_rxwrapper or hailo15_rxwrapper->dev pointer\n", __func__);
		return -EINVAL;
	}

	rxwrapper_node = dev_fwnode(hailo15_rxwrapper->dev);
	if (!rxwrapper_node) {
		dev_err(hailo15_rxwrapper->dev, "Failed to get fwnode for rxwrapper device\n");
		return -ENODEV;
	}

	parent_node = fwnode_get_parent(rxwrapper_node);
	if (!parent_node) {
		dev_err(hailo15_rxwrapper->dev, "Failed to get parent fwnode (vision_subsys)\n");
		return -ENODEV;
	}

	// Read the property from the *parent* node
	ret = fwnode_property_read_u32(parent_node, "null-addr", &null_addr);
	if (ret) {
		dev_err(hailo15_rxwrapper->dev, "Failed to read 'null-addr' from parent node. ret: %d\n", ret);
	} else {
		dev_dbg(hailo15_rxwrapper->dev, "Successfully read null_addr=0x%x from parent node\n", null_addr);
	hailo15_rxwrapper->vision_ss_null_addr = null_addr;
	}

	fwnode_handle_put(parent_node);

	return ret;
}

int hailo15_rxwrapper_probe(struct platform_device *pdev)
{
	int ret, pipe;
	struct hailo15_rxwrapper_priv *hailo15_rxwrapper;
	struct resource *res;
	struct hailo15_dma_ctx *ctx;
	struct clk *clk;
	unsigned int i;

	dev_info(&pdev->dev, "probe started");

	/* Check if all of rxwrapper's dependencies are ready */
	ret = hailo15_media_get_sink_endpoints_status(&pdev->dev);
	if(ret){
		if (ret == -EPROBE_DEFER)
			dev_dbg(&pdev->dev, "Endpoints are not ready, deferring probe\n");
		else
			dev_err(&pdev->dev, "Failed on get endpoints. status: %d\n", ret);
		return ret;
	}

	hailo15_rxwrapper =
		devm_kzalloc(&pdev->dev, sizeof(struct hailo15_rxwrapper_priv), GFP_KERNEL);

	if (!hailo15_rxwrapper) {
		pr_err("Unable to allocate hailo15_rxwrapper struct\n");
		return -ENOMEM;
	}

	hailo15_rxwrapper->rxwrapper_cfg = of_device_get_match_data(&pdev->dev);
	if (!hailo15_rxwrapper->rxwrapper_cfg ) {
		dev_err(&pdev->dev, "No rxwrapper_cfg match found\n");
		return -EINVAL;
	}

	if (device_property_read_u32(&pdev->dev, "id", &hailo15_rxwrapper->id)) {
		dev_notice(&pdev->dev, "id property not found, setting to 0\n");
		hailo15_rxwrapper->id = 0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hailo15_rxwrapper->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hailo15_rxwrapper->base))
		return PTR_ERR(hailo15_rxwrapper->base);

	pr_info("hailo15_rxwrapper base is %llx first 4 bytes \n",
		(u64)hailo15_rxwrapper->base);

	clk = devm_clk_get(&pdev->dev, "rxwrapper_p_clk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to get rxwrapper_p_clk, err = (%pe)\n", clk);
		return PTR_ERR(clk);
	}
	hailo15_rxwrapper->rxwrapper_p_clk = clk;

	clk = devm_clk_get(&pdev->dev, "rxwrapper_data_clk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to get rxwrapper_data_clk, err = (%pe)\n", clk);
		return PTR_ERR(clk);
	}
	hailo15_rxwrapper->rxwrapper_data_clk = clk;

	clk = devm_clk_get(&pdev->dev, "rxwrapper_xtal_clk");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to get rxwrapper_xtal_clk, err = (%pe)\n", clk);
		return PTR_ERR(clk);
	}
	hailo15_rxwrapper->rxwrapper_xtal_clk = clk;

	pm_runtime_get_sync(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = clk_prepare_enable(hailo15_rxwrapper->rxwrapper_p_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed enabling rxwrapper_p_clk, err = (%pe)\n", ERR_PTR(ret));
		return ret;
	}

	ret = clk_prepare_enable(hailo15_rxwrapper->rxwrapper_data_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed enabling rxwrapper_data_clk, err = (%pe)\n", ERR_PTR(ret));
		return ret;
	}

	ret = clk_prepare_enable(hailo15_rxwrapper->rxwrapper_xtal_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed enabling rxwrapper_xtal_clk, err = (%pe)\n", ERR_PTR(ret));
		return ret;
	}

	hailo15_rxwrapper_config_static_default(hailo15_rxwrapper);

	memcpy(&rxwrapper_chosen_pipe_cfg, &rxwrapper_default_pipe_cfg, sizeof(struct hailo15_rxwrapper_pipe_cfg));
	memcpy(&rxwrapper_chosen_credits_cfg, &rxwrapper_default_credits_cfg, sizeof(struct hailo15_rxwrapper_credits_cfg));

	/*@TODO check return value*/
	for (pipe = 0; pipe < RXWRAPPER_NUM_PIPES; ++pipe) {
		hailo15_rxwrapper_pipe_set_cfg(hailo15_rxwrapper, pipe,
						   &rxwrapper_chosen_pipe_cfg);
	}

	platform_set_drvdata(pdev, hailo15_rxwrapper);

	hailo15_rxwrapper->dev = &pdev->dev;
	hailo15_rxwrapper->sd.owner = THIS_MODULE;
	hailo15_rxwrapper->sd.dev = &(pdev->dev);
	hailo15_rxwrapper->sd.fwnode = dev_fwnode(hailo15_rxwrapper->dev);
	v4l2_subdev_init(&hailo15_rxwrapper->sd, &hailo15_rxwrapper_v4l2_subdev_ops);

	hailo15_rxwrapper->sd.internal_ops = &rxwrapper_internal_ops;
	snprintf(hailo15_rxwrapper->sd.name, sizeof(hailo15_rxwrapper->sd.name),
		 "hailo15_rxwrapper.%d", hailo15_rxwrapper->id);
	/*hailo15_rxwrapper->sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;*/
	hailo15_rxwrapper->sd.entity.function = MEDIA_ENT_F_VID_MUX;

    ret = hailo15_rxwrapper_parse_null_addr(hailo15_rxwrapper);
    if (ret) {
        dev_err(&pdev->dev, "Failed to parse null address\n");
        return ret;
    }

	hailo15_rxwrapper->pads[RXWRAPPER_SINK_PAD_0].flags = MEDIA_PAD_FL_SINK;
	for (i = RXWRAPPER_SOURCE_PAD_1; i < RXWRAPPER_PAD_MAX; i++) {
		hailo15_rxwrapper->pad_fmts[i] = fmt_default;
		hailo15_rxwrapper->pads[i].flags = MEDIA_PAD_FL_SOURCE;
	}

	ret = media_entity_pads_init(&hailo15_rxwrapper->sd.entity,
					 RXWRAPPER_PAD_MAX,
					 hailo15_rxwrapper->pads);
	if (ret) {
		pr_err("failed to init entity pads: %d", ret);
		goto error_free_dev;
	}

	hailo15_rxwrapper->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	mutex_init(&hailo15_rxwrapper->lock);

	hailo15_irq_work_queue_setup(hailo15_rxwrapper);

	ctx = kzalloc(sizeof(struct hailo15_dma_ctx), GFP_KERNEL);
	if (!ctx) {
		pr_err("hailo15 rxwrapper: failed to allocate dma ctx\n");
		goto error_alloc_dma_ctx;
	}

	ret = hailo15_rxwrapper_dma_ctx_init_all(ctx, hailo15_rxwrapper);
	if (ret) {
		pr_err("hailo15 rxwrapper: failed to init dma ctx for all groups\n");
		goto error_init_dma_ctx;
	}

	hailo15_rxwrapper->irq = platform_get_irq(pdev, 0);
	if (hailo15_rxwrapper->irq < 0) {
		pr_err("can't get irq resource\n");
		goto error_init_irq;
	}

	ret = devm_request_irq(hailo15_rxwrapper->dev, hailo15_rxwrapper->irq,
				hailo15_rxwrapper_irq_handler, IRQF_SHARED,
				dev_name(hailo15_rxwrapper->dev), hailo15_rxwrapper);

	if (ret) {
		pr_err("%s[%d]: request irq %d error, ret = %d\n", __func__, __LINE__, hailo15_rxwrapper->irq, ret);
		goto error_init_irq;
	}

	ret = hailo15_media_create_connections(hailo15_rxwrapper->dev, &hailo15_rxwrapper->sd);
	if(ret){
		dev_err(hailo15_rxwrapper->dev, "can't create media links!\n");
		goto error_init_irq;
	}

	/* Init buffers FIFOs and their auxiliary variables */
	for (i = 0; i < HAILO15_VID_GRP_MAX; i++) {
		INIT_LIST_HEAD(&hailo15_rxwrapper->buf_queue[i]);
	}
	spin_lock_init(&hailo15_rxwrapper->buf_lock);

	dev_info(hailo15_rxwrapper->dev, "probe finished successfully");
	return 0;

error_init_irq:
	hailo15_rxwrapper_dma_ctx_clean_all(ctx);

error_init_dma_ctx:
	kfree(ctx);
error_alloc_dma_ctx:
	hailo15_irq_work_queue_release(hailo15_rxwrapper);
	mutex_destroy(&hailo15_rxwrapper->lock);
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
	struct v4l2_subdev *subdev;
	struct media_pad *pad;

	mutex_destroy(&hailo15_rxwrapper->lock);
	hailo15_media_entity_clean(&hailo15_rxwrapper->sd.entity);
	v4l2_device_unregister_subdev(&hailo15_rxwrapper->sd);
	hailo15_irq_work_queue_release(hailo15_rxwrapper);

	clk_disable_unprepare(hailo15_rxwrapper->rxwrapper_xtal_clk);
	clk_disable_unprepare(hailo15_rxwrapper->rxwrapper_data_clk);
	clk_disable_unprepare(hailo15_rxwrapper->rxwrapper_p_clk);

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	pad = &hailo15_rxwrapper->pads[RXWRAPPER_SINK_PAD_0];
	if (pad)
		pad = media_entity_remote_pad(pad);
	subdev = media_entity_to_v4l2_subdev(pad->entity);

	kfree(hailo15_rxwrapper);
	hailo15_rxwrapper_dma_ctx_clean_all(ctx);
	kfree(ctx);
	return 0;
}

static struct platform_driver hailo15_rxwrapper_driver = { 
	.probe = hailo15_rxwrapper_probe,
	.remove = hailo15_rxwrapper_remove,
	.driver = {
		.name = "hailo-hailo15_rxwrapper",
		.owner = THIS_MODULE,
		.of_match_table = hailo15_rxwrapper_of_match,
	}
};

module_platform_driver(hailo15_rxwrapper_driver);
MODULE_AUTHOR("Eylon Shabtay <eylons@hailo.ai>");
MODULE_DESCRIPTION("Hailo hailo15_rxwrapper driver");
MODULE_LICENSE("GPL v2");
