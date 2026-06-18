#ifndef __HAILO15_COMMON__
#define __HAILO15_COMMON__

#include <media/videobuf2-v4l2.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-device.h>
#include <dt-bindings/soc/hailo15_video_path.h>
#include "fe/fe_dev.h"

#define STRIDE_ALIGN 16
#define FMT_MAX_PLANES 3
#define BITS_IN_BYTE 8

#define HAILO15_MAX_BUFFERS 10
#define HAILO15_NUM_P2A_BUFFERS 1
#define HAILO15_EVENT_RESOURCE_DATA_SIZE (4096 * 4)
#define HAILO15_EVENT_RESOURCE_MAX_MAP_SIZE  (HAILO15_EVENT_RESOURCE_DATA_SIZE + PAGE_SIZE)

// Generally Vid-cap (type = 'D') ioctls
#define VIDEO_FPS_MONITOR_SUBDEV_IOC    _IOR('D', BASE_VIDIOC_PRIVATE + 0, uint64_t)
#define VIDEO_GET_VSM_IOC               _IOWR('D', BASE_VIDIOC_PRIVATE + 1, struct hailo15_get_vsm_params)
// HAILO15_INTERNAL_GET_P2A_REGS used to occupy offset 2, but this ioctl is kernel-internal -> moved to type='K' ioctls
#define VIDEO_WAIT_FOR_STREAM_START	_IO('D', BASE_VIDIOC_PRIVATE + 3)
#define VIDEO_TUNING_STATE              _IOWR('D', BASE_VIDIOC_PRIVATE + 4, __u8)
#define VIDEO_HDR_TIME_STAMP_MODE_SET    _IOW('D', BASE_VIDIOC_PRIVATE + 5, __u8)
#define VIDEO_HDR_TIME_STAMP_MODE_GET    _IOR('D', BASE_VIDIOC_PRIVATE + 6, __u8)
#define VIDEO_PIPELINE_STATE_GET         _IOR('D', BASE_VIDIOC_PRIVATE + 7, int)
#define VIDEO_FAST_TOGGLE                _IOR('D', BASE_VIDIOC_PRIVATE + 8, int)
#define VIDEO_FAST_TOGGLE_PRIMING        _IOR('D', BASE_VIDIOC_PRIVATE + 9, int)
#define VIDEO_EVENT_COMPLETE             _IO('D', BASE_VIDIOC_PRIVATE + 10)

// ISP (type = 'I') ioctls
#define ISPIOC_V4L2_READ_REG            _IOWR('I', BASE_VIDIOC_PRIVATE + 0, struct isp_reg_data)
#define ISPIOC_V4L2_WRITE_REG           _IOWR('I', BASE_VIDIOC_PRIVATE + 1, struct isp_reg_data)
#define ISPIOC_V4L2_RMEM                _IOWR('I', BASE_VIDIOC_PRIVATE + 2, struct hailo15_rmem)
#define ISPIOC_V4L2_MI_START            _IOWR('I', BASE_VIDIOC_PRIVATE + 3, uint32_t)
#define ISPIOC_V4L2_MI_STOP             _IOWR('I', BASE_VIDIOC_PRIVATE + 4, uint32_t)

#define ISPIOC_V4L2_MCM_DQBUF           _IOWR('I', BASE_VIDIOC_PRIVATE + 5, struct isp_mcm_buf)
#define ISPIOC_V4L2_MCM_BUF_DONE        _IOWR('I', BASE_VIDIOC_PRIVATE + 6, struct isp_mcm_buf)
#define ISPIOC_V4L2_MCM_MODE            _IOWR('I', BASE_VIDIOC_PRIVATE + 7, uint32_t)
#define ISPIOC_V4L2_REQBUFS             _IOWR('I', BASE_VIDIOC_PRIVATE + 8, struct hailo15_reqbufs)
#define ISPIOC_V4L2_SET_INPUT_FORMAT    _IOWR('I', BASE_VIDIOC_PRIVATE + 9, struct v4l2_subdev_format)
#define ISPIOC_V4L2_SET_MCM_MODE        _IOWR('I', BASE_VIDIOC_PRIVATE + 10, uint32_t)
#define ISPIOC_V4L2_GET_NULL_ADDR       _IOR('I', BASE_VIDIOC_PRIVATE + 11, uint32_t)
#define ISPIOC_V4L2_SET_ENABLE_SP2_ERR  _IOWR('I', BASE_VIDIOC_PRIVATE + 12, __u8)
#define ISPIOC_V4L2_SET_MCM_MODE_PRIMING _IOWR('I', BASE_VIDIOC_PRIVATE + 13, uint32_t)
#define ISPIOC_V4L2_SET_HDR_COMPRESSION  _IOWR('I', BASE_VIDIOC_PRIVATE + 14, struct hdr_comp_ctrl)
#define ISPIOC_V4L2_EVENT_COMPLETE       _IO('I', BASE_VIDIOC_PRIVATE + 15)
#define ISPIOC_V4L2_MCM_MODE_PRIMING    _IOR('I', BASE_VIDIOC_PRIVATE + 16, uint32_t)

// V4L2 (type = 'V') ioctls
#define HAILO15_PAD_REQBUFS             _IOWR('V', BASE_VIDIOC_PRIVATE + 9, struct hailo15_reqbufs)
#define HAILO15_PAD_BUF_DONE            _IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct hailo15_pad_buf)
#define HAILO15_PAD_BUF_QUEUE           _IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct hailo15_pad_buf)
#define HAILO15_PAD_S_STREAM            _IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct hailo15_pad_stream_status)

#define HAILO15_PAD_QUERYCTRL           _IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct hailo15_pad_queryctrl)
#define HAILO15_PAD_QUERY_EXT_CTRL      _IOWR('V', BASE_VIDIOC_PRIVATE + 14, struct hailo15_pad_query_ext_ctrl)
#define HAILO15_PAD_G_CTRL              _IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct hailo15_pad_control)
#define HAILO15_PAD_S_CTRL              _IOWR('V', BASE_VIDIOC_PRIVATE + 16, struct hailo15_pad_control)
#define HAILO15_PAD_G_EXT_CTRLS         _IOWR('V', BASE_VIDIOC_PRIVATE + 17, struct hailo15_pad_ext_controls)
#define HAILO15_PAD_S_EXT_CTRLS         _IOWR('V', BASE_VIDIOC_PRIVATE + 18, struct hailo15_pad_ext_controls)
#define HAILO15_PAD_TRY_EXT_CTRLS       _IOWR('V', BASE_VIDIOC_PRIVATE + 19, struct hailo15_pad_ext_controls)
#define HAILO15_PAD_QUERYMENU           _IOWR('V', BASE_VIDIOC_PRIVATE + 20, struct hailo15_pad_querymenu)

#define HAILO15_PAD_STAT_SUBSCRIBE      _IOWR('V', BASE_VIDIOC_PRIVATE + 21, struct hailo15_pad_stat_subscribe)
#define HAILO15_PAD_STAT_UNSUBSCRIBE    _IOWR('V', BASE_VIDIOC_PRIVATE + 22, struct hailo15_pad_stat_subscribe)
#define HAILO15_PAD_STAT_DONE           _IOWR('V', BASE_VIDIOC_PRIVATE + 23, struct hailo15_pad_stat)


#define HAILO15_TUNING           		_IOWR('V', BASE_VIDIOC_PRIVATE + 24, __u8)

// Kernel internal ioctl (core ioctl to v4l2_subdev)s (type = 'K'))
#define HAILO15_INTERNAL_GET_P2A_REGS               	_IOR('K', BASE_VIDIOC_PRIVATE + 0, struct hailo15_p2a_buffer_regs_addr)
#define HAILO15_INTERNAL_SENSOR_FAST_TOGGLE_SET_STATUS	_IOW('K', BASE_VIDIOC_PRIVATE + 1, int)
#define HAILO15_INTERNAL_CSI2RX_FAST_TOGGLE_SET_STATUS	_IOW('K', BASE_VIDIOC_PRIVATE + 2, struct fast_toggle_data)
#define HAILO15_INTERNAL_ISP_FAST_TOGGLE_SET_STATUS 	_IOW('K', BASE_VIDIOC_PRIVATE + 3, struct fast_toggle_data)
#define HAILO15_INTERNAL_RXW_FAST_TOGGLE_SET_STATUS 	_IOW('K', BASE_VIDIOC_PRIVATE + 4, struct fast_toggle_data)

#define HAILO15_DMA_CTX_CB(ctx, func, grp_id, ...)							\
({																	\
		int __hailo15_cb_retval;									\
		do {														\
			if (!ctx) {												\
				__hailo15_cb_retval = -EINVAL;						\
				break;												\
			}														\
			if (!ctx->buf_ctx[grp_id].ops) {								\
				__hailo15_cb_retval = -EINVAL;						\
				break;												\
			}														\
			if (!ctx->buf_ctx[grp_id].ops->func) {							\
				__hailo15_cb_retval = -EINVAL;						\
				break;												\
			}														\
			__hailo15_cb_retval = ctx->buf_ctx[grp_id].ops->func(			\
				ctx, ##__VA_ARGS__);					\
		} while (0);												\
		__hailo15_cb_retval;										\
	})

#define hailo15_video_node_buffer_process(ctx, grp_id, buf, is_buf_time_synced)             \
	(HAILO15_DMA_CTX_CB(ctx, buffer_process, grp_id, buf, is_buf_time_synced))
#define hailo15_video_node_buffer_queue(ctx, grp_id, buf)                            \
	(HAILO15_DMA_CTX_CB(ctx, buffer_queue, grp_id, buf))
#define hailo15_video_node_get_frame_count(ctx, grp_id, fc)                            \
	(HAILO15_DMA_CTX_CB(ctx, get_frame_count, grp_id, grp_id, fc))
#define hailo15_video_node_fast_toggle_stream(ctx, grp_id, toggle_type)                            \
	(HAILO15_DMA_CTX_CB(ctx, fast_toggle_stream, grp_id, grp_id, toggle_type))
#define hailo15_video_node_get_rmem(ctx, grp_id, rmem)                                 \
	(HAILO15_DMA_CTX_CB(ctx, get_rmem, grp_id, rmem))
#define hailo15_video_node_get_event_resource(ctx, grp_id, resource)                   \
	(HAILO15_DMA_CTX_CB(ctx, get_event_resource, grp_id, resource))
#define hailo15_video_node_set_private_data(ctx, grp_id, data)                 \
	(HAILO15_DMA_CTX_CB(ctx, set_private_data, grp_id, grp_id, data))
#define hailo15_video_node_get_private_data(ctx, grp_id, data)                 \
	(HAILO15_DMA_CTX_CB(ctx, get_private_data, grp_id,grp_id, data))

#define hailo15_dma_buffer_done(ctx, grp_id, buf)                              \
	(HAILO15_DMA_CTX_CB(ctx, buffer_done, grp_id, buf, grp_id))

#define hailo15_dma_buffer_dequeue(ctx, grp_id, buf)                              \
	(HAILO15_DMA_CTX_CB(ctx, buffer_dequeue, grp_id, buf, grp_id))

#define hailo15_video_node_get_vsm(ctx, grp_id, index, vsm)                    \
	(HAILO15_DMA_CTX_CB(ctx, get_vsm, grp_id, grp_id, index, vsm))

#define hailo15_video_node_queue_empty(ctx, grp_id)                              \
	(HAILO15_DMA_CTX_CB(ctx, queue_empty, grp_id,  grp_id))

enum hailo15_fmt_planes {
	PLANE_Y,
	PLANE_CB,
	PLANE_CR,
};

enum hailo15_pix_fmt {
	YUV422,
	RGB888,
	YUV420,
	RAW12,
};

enum hailo15_pix_planarity {
	INTERLEAVED,
	SEMI_PLANAR,
	PLANAR,
};

enum HDR_TIMESTAMP_MODE {
    HDR_TIMESTAMP_MODE_OFF = 0,
    HDR_TIMESTAMP_MODE_ON = 1,
    HDR_TIMESTAMP_MODE_MAX
};

struct hailo15_video_plane {
	uint32_t dma_addr;
	uint32_t size;
};

struct hailo15_vb2_buffer {
    struct vb2_v4l2_buffer vb;
	unsigned int num_planes;
	struct hailo15_video_plane planes[VIDEO_MAX_PLANES];
    struct list_head list;
    uint32_t sequence;
};

struct hailo15_video_plane_fmt {
	int bpp;
	int vscale_ratio;
	int hscale_ratio;
};

struct hailo15_video_fmt {
	int fourcc;
	int code;
	enum hailo15_pix_fmt pix_fmt;
	enum hailo15_pix_planarity planarity;
	unsigned int num_planes;
	unsigned int width_modulus;
	struct hailo15_video_plane_fmt planes[FMT_MAX_PLANES];
};

struct hailo15_p2a_buffer_regs_addr {
	void __iomem *buffer_ready_ap_int_mask_addr;
	void __iomem *buffer_ready_ap_int_status_addr;
	void __iomem *buffer_ready_ap_int_w1c_addr;
	void __iomem *buffer_ready_ap_int_w1s_addr;
};

// struct hailo15_video_plane_fmt
#define HAILO15_CUSTOM_VIDEO_PLANE(_bpp, _vscale_ratio, _hscale_ratio) { \
	.bpp = (_bpp), \
	.vscale_ratio = (_vscale_ratio), \
	.hscale_ratio = (_hscale_ratio), \
}

// Default video plane is a video plane with a default vscale and hscale ratio of 1
#define HAILO15_INTERLEAVED_VIDEO_PLANE(_bpp) HAILO15_CUSTOM_VIDEO_PLANE(_bpp, 1, 1)

static const struct hailo15_video_fmt __hailo15_out_formats[] = {
	{
		.fourcc = V4L2_PIX_FMT_SGBRG12P,
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(12) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12P,
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(12) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12,
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12,
		.code = MEDIA_BUS_FMT_SRGGB12_3X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 3,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12,
		.code = MEDIA_BUS_FMT_SRGGB12_2X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 2,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SGBRG12,
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SGBRG12,
		.code = MEDIA_BUS_FMT_SGBRG12_3X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 3,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SGBRG12,
		.code = MEDIA_BUS_FMT_SGBRG12_2X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 2,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
};

static const struct hailo15_video_fmt __hailo15_formats[] = {
	{
		.fourcc = V4L2_PIX_FMT_YVYU,
		.code = MEDIA_BUS_FMT_YVYU8_2X8,
		.pix_fmt = YUV422,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 2,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_YUYV,
		.code = MEDIA_BUS_FMT_YUYV8_1X16,
		.pix_fmt = YUV422,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 2,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12P,
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(12) },
	},
	{
		.fourcc = V4L2_PIX_FMT_RGB24,
		.code = MEDIA_BUS_FMT_RGB888_1X24,
		.pix_fmt = RGB888,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 1,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(24) },

	},
	{
		.fourcc = V4L2_PIX_FMT_NV12M,
		.code = MEDIA_BUS_FMT_YUYV8_2X8,
		.pix_fmt = YUV420,
		.planarity = SEMI_PLANAR,
		.num_planes = 2,
		.width_modulus = 4,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(8), HAILO15_CUSTOM_VIDEO_PLANE(8, 2, 1) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12,
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12,
		.code = MEDIA_BUS_FMT_SRGGB12_3X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 3,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12,
		.code = MEDIA_BUS_FMT_SRGGB12_2X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 2,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SGBRG12,
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SGBRG12,
		.code = MEDIA_BUS_FMT_SGBRG12_3X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 3,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
	{
		.fourcc = V4L2_PIX_FMT_SGBRG12,
		.code = MEDIA_BUS_FMT_SGBRG12_2X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 2,
		.width_modulus = 16,
		.planes = { HAILO15_INTERLEAVED_VIDEO_PLANE(16), HAILO15_INTERLEAVED_VIDEO_PLANE(16) },
	},
};

struct error_message {
	uint32_t mask;
	const char *message;
};

/* The status register has different error assignments for each bit
between the Hailo15 and Hailo15L platforms. This struct allows for platform-specific handling
when interpreting this register. */
struct err_status_reg {
	char* name;
	uint32_t num_errors;
	struct error_message* errors;
};

const struct hailo15_video_fmt *hailo15_get_out_formats(void);
const struct hailo15_video_fmt *hailo15_get_formats(void);
unsigned int hailo15_get_out_formats_count(void);
unsigned int hailo15_get_formats_count(void);
char *hailo15_fourcc_to_string(uint32_t fourcc, char *buf);

struct hailo15_vsm {
	int dx;
	int dy;
};

struct isp_reg_data {
	uint32_t reg;
	uint32_t value;
};

struct hailo15_buf_timing {
	ktime_t qbuf_start;
	ktime_t fe_switch_start;
	ktime_t fe_switch_end;
	ktime_t rdma_ready;
	ktime_t frame_end;
};

struct hailo15_buffer {
	struct vb2_v4l2_buffer vb;
	struct media_pad *pad;
	struct list_head irqlist;
	dma_addr_t dma[FMT_MAX_PLANES];
	int grp_id;
	int flags;
	struct v4l2_subdev *sd;
	//trace bookkeeping
	uint32_t queue_sequence;

	/* Timing diagnostics for QBUF flow and ISP pipeline */
	struct hailo15_buf_timing timing;
};

/**
 * hailo15_buf_list_init - initialize a hailo15_buffer's irqlist.
 * @buf: the hailo15_buffer to initialize
 *
 * Must be called at buffer init time so that list_empty() checks in
 * hailo15_buf_list_del/add work correctly.
 */
static inline void hailo15_buf_list_init(struct hailo15_buffer *buf)
{
	INIT_LIST_HEAD(&buf->irqlist);
}

/**
 * hailo15_buf_list_add - safely add a hailo15_buffer to the head of a list.
 * @buf: the hailo15_buffer to add
 * @head: the list_head to add to
 *
 * Detects double-add (buffer already linked in a list) and emits a WARN.
 * Requires buf to be initialized via hailo15_buf_list_init.
 */
#define hailo15_buf_list_add(buf, head) \
	_hailo15_buf_list_add(buf, head, true, __func__)

/**
 * hailo15_buf_list_add_tail - safely add a hailo15_buffer to the tail of a list.
 * @buf: the hailo15_buffer to add
 * @head: the list_head to add to
 */
#define hailo15_buf_list_add_tail(buf, head) \
	_hailo15_buf_list_add(buf, head, false, __func__)

static inline void _hailo15_buf_list_add(struct hailo15_buffer *buf,
					  struct list_head *head,
					  bool to_head, const char *caller)
{
	if (WARN(!list_empty(&buf->irqlist),
		 "%s - attemtped adding buf index %d (grp_id %d) to list, but buf already in a list.",
		 caller, buf->vb.vb2_buf.index, buf->grp_id))
		return;
	if (to_head)
		list_add(&buf->irqlist, head);
	else
		list_add_tail(&buf->irqlist, head);
}

/**
 * hailo15_buf_list_del - safely remove a hailo15_buffer from its irqlist.
 * @buf: the hailo15_buffer to remove
 *
 * Verifies the buffer is actually linked in a list before removing it.
 * If not, a WARN is emitted with the buffer index and grp_id for diagnosis.
 * Requires buf to be initialized via hailo15_buf_list_init.
 */
#define hailo15_buf_list_del(buf) _hailo15_buf_list_del(buf, __func__)

static inline void _hailo15_buf_list_del(struct hailo15_buffer *buf,
					 const char *caller)
{
	if (WARN(list_empty(&buf->irqlist),
		 "%s - attempted deleting buf index %d (grp_id %d), but buf is not in a list.",
		 caller, buf->vb.vb2_buf.index, buf->grp_id))
		return;
	list_del_init(&buf->irqlist);
}

struct hailo15_buf_ctx {
	struct hailo15_buf_ops *ops;
	struct hailo15_buffer *curr_buf;
	int pad; /* the pad the context referring to */
};

struct hailo15_dma_ctx {
	struct hailo15_buf_ctx buf_ctx[HAILO15_VID_GRP_MAX];
	void *dev;
};

struct hailo15_rmem {
	uint8_t port;
	uint64_t addr;
	uint64_t size;
};

struct hailo15_reqbufs {
	int pad;
	uint32_t num_buffers;
};

struct hailo15_pad_buf {
	uint32_t pad;
	struct hailo15_vb2_buffer *buf;
};

struct hailo15_pad_stream_status {
	uint32_t pad;
	uint32_t status;
};

struct hailo15_pad_queryctrl {
	uint32_t pad;
	struct v4l2_queryctrl *query_ctrl;
};

struct hailo15_pad_query_ext_ctrl {
	uint32_t pad;
	struct v4l2_query_ext_ctrl *query_ext_ctrl;
};

struct hailo15_pad_control {
	uint32_t pad;
	struct v4l2_control *control;
};

struct hailo15_pad_ext_controls {
	uint32_t pad;
	struct v4l2_ext_controls *ext_controls;
};

struct hailo15_pad_querymenu {
	uint32_t pad;
	struct v4l2_querymenu *querymenu;
};

struct hailo15_pad_stat_subscribe {
	uint32_t pad;
	uint32_t id;
	uint32_t type;
};

struct hailo15_pad_stat {
	uint32_t pad;
	uint32_t id;
	uint32_t type;
	uint8_t reserved[64];
};

struct hailo15_event_resource {
	struct mutex event_lock;
	uint64_t phy_addr;
	void *virt_addr;
	uint32_t size;
	wait_queue_head_t wait_q;
	uint8_t kernel_seq;
};

struct hailo15_subdev_list {
	struct list_head subdev_list;
	struct v4l2_subdev *subdev;
};

struct hailo15_mux_isp_stream_cfg {
    unsigned int enable;
    unsigned int vc;
    unsigned int dt;
};

struct hailo15_mux_cfg {
	unsigned int pixel_mux_cfg;
	struct hailo15_mux_isp_stream_cfg isp0_stream0;
	struct hailo15_mux_isp_stream_cfg isp0_stream1;
	struct hailo15_mux_isp_stream_cfg isp0_stream2;
	struct hailo15_mux_isp_stream_cfg isp1_stream0;
	struct hailo15_mux_isp_stream_cfg isp1_stream1;
	struct hailo15_mux_isp_stream_cfg isp1_stream2;
	unsigned int vision_buffer_ready_ap_int_mask;
};

struct hailo15_mux_interrupt_cfg {
	unsigned int pixel_mux_vsync_mask;
	unsigned int vision_subsys_asf_int_mask;
	unsigned int vision_asf_int_fatal_mask;
	unsigned int vision_asf_int_nonfatal_mask;
	unsigned int vision_subsys_err_int_mask;
	unsigned int vision_subsys_err_int_agg_mask;
};

struct hailo15_buf_ops {
	int (*buffer_queue) (struct hailo15_dma_ctx *ctx, struct hailo15_buffer *buf); /* VIDEO -> DMA */
	int (*buffer_dequeue) (struct hailo15_dma_ctx *ctx, struct hailo15_buffer *buf, int grp_id); /* DMA -> VIDEO */
	int (*buffer_process)(struct hailo15_dma_ctx *ctx,
				  struct hailo15_buffer *buf, bool is_buf_time_synced); /* VIDEO -> DMA */
	int (*buffer_done)(struct hailo15_dma_ctx *ctx,
			   struct hailo15_buffer *buf,
			   int grp_id); /* VIDEO <- DMA */
	int (*get_frame_count)(struct hailo15_dma_ctx *dma_ctx, int grp_id,
				   uint64_t *fc); /* VIDEO -> DMA */
	int (*get_rmem)(struct hailo15_dma_ctx *ctx,
			struct hailo15_rmem *rmem); /* VIDEO -> DMA */
	int (*get_event_resource)(struct hailo15_dma_ctx *ctx,
				  struct hailo15_event_resource
					  *event_resource); /* VIDEO -> DMA */
	int (*set_private_data)(struct hailo15_dma_ctx *ctx, int grp_id,
				void *data);
	int (*get_private_data)(struct hailo15_dma_ctx *ctx, int grp_id,
				void **data);
	int (*get_vsm)(struct hailo15_dma_ctx *ctx, int grp_id, int index,
			   struct hailo15_vsm *vsm); /* VIDEO -> DMA */
	int (*queue_empty)(struct hailo15_dma_ctx *ctx,
			   int grp_id); /* VIDEO -> DMA */
	int (*fast_toggle_stream)(struct hailo15_dma_ctx *dma_ctx, int grp_id, int toggle_type); /* VIDEO -> DMA */
};

#ifndef ALIGN_UP
#define ALIGN_UP(x, align) (((x) + (align)-1) & ~(align - 1))
#endif

enum hailo15_isp_path {
	ISP_MP,
	ISP_SP2,
	ISP_MCM_IN,
	ISP_MCM_RAW_OUT,
	ISP_MAX_PATH,
};

enum pixel_mux_pads {
	PIXEL_MUX_SINK_PAD_0,
	PIXEL_MUX_SINK_PAD_1,
	PIXEL_MUX_SINK_PAD_MAX,
	PIXEL_MUX_SOURCE_PAD_0 = PIXEL_MUX_SINK_PAD_MAX,
	PIXEL_MUX_SOURCE_PAD_1,
	PIXEL_MUX_PAD_MAX,
};

/* TODO for version 1.12.0 - change API (together with medialib) to pass more than 1 enum value - pass struct instead! */
enum fast_toggle_type {
	TOGGLE_MERCURY_SDR_SDR = 0,
	TOGGLE_MERCURY_SDR_HDR = 1,
	TOGGLE_MERCURY_HDR_SDR = 2,
	TOGGLE_MERCURY_HDR_HDR = 3, /* unsupported */
	TOGGLE_MERCURY_SDR_PREISP = 4,
	TOGGLE_MERCURY_PREISP_SDR = 5,
	TOGGLE_MERCURY_HDR_PREISP = 6, /* unsupported */
	TOGGLE_MERCURY_PREISP_HDR = 7, /* unsupported */

	TOGGLE_PLUTO_SDR_SDR = 8, /* unsupported */
	TOGGLE_PLUTO_SDR_HDR = 9,
	TOGGLE_PLUTO_HDR_SDR = 10,
	TOGGLE_PLUTO_HDR_HDR = 11, /* unsupported */
	TOGGLE_PLUTO_SDR_PREISP = 12,
	TOGGLE_PLUTO_PREISP_SDR = 13,
	TOGGLE_PLUTO_HDR_PREISP = 14,
	TOGGLE_PLUTO_PREISP_HDR = 15,
	FAST_TOGGLE_TYPE_MAX,
};

enum fast_toggle_state {
	FAST_TOGGLE_NONE,
	FAST_TOGGLE_TEARDOWN,
	FAST_TOGGLE_PRIMING,
	FAST_TOGGLE_APPLY_PRIMING,
	FAST_TOGGLE_ACTIVE,
	FAST_TOGGLE_STATE_MAX,
};

struct fast_toggle_data {
	enum fast_toggle_state state;
	enum fast_toggle_type type;
};

static inline int HAILO15_VID_GRP_TO_ISP_PATH(int grp_id)
{
	switch (grp_id) {
	case HAILO15_VID_GRP_SX_CSI0_ISP_MP:
	case HAILO15_VID_GRP_SX_CSI1_ISP_MP:
		return ISP_MP;
	case HAILO15_VID_GRP_SX_CSI0_ISP_SP:
	case HAILO15_VID_GRP_SX_CSI1_ISP_SP:
		return ISP_SP2;
	case HAILO15_VID_GRP_MCM_IN:
		return ISP_MCM_IN;
	case HAILO15_VID_GRP_MCM_RAW_WR:
		return ISP_MCM_RAW_OUT;
	default:
		return -1;
	}
}

static inline uint8_t HAILO15_VID_GRP_TO_VDID(int grp_id)
{
	switch (grp_id) {
	case HAILO15_VID_GRP_SX_CSI0_ISP_MP:
	case HAILO15_VID_GRP_SX_CSI0_ISP_SP:
	case HAILO15_VID_GRP_MCM_RAW_WR:
		return 0;
	case HAILO15_VID_GRP_SX_CSI1_ISP_MP:
	case HAILO15_VID_GRP_SX_CSI1_ISP_SP:
		return 1;
	case HAILO15_VID_GRP_MCM_IN:
		// currently MCM-IN path is only in use
		// for VDID 0.
		return 0;
	default:
		return VIV_INVALID_VDID;
	}
}

static inline bool hailo15_is_p2a_wildcard_vc_grp_id(int grp_id)
{
	switch(grp_id) {
		case HAILO15_VID_GRP_SX_CSI0_P2A:
		case HAILO15_VID_GRP_SX_CSI1_P2A:
			return true;
		default:
			return false;
	}
}

static inline bool hailo15_is_p2a_grp_id(int grp_id)
{
	switch(grp_id) {
		case HAILO15_VID_GRP_SX_CSI0_P2A:
		case HAILO15_VID_GRP_S0_CSI0_P2A:
		case HAILO15_VID_GRP_S1_CSI0_P2A:
		case HAILO15_VID_GRP_S2_CSI0_P2A:
		case HAILO15_VID_GRP_S3_CSI0_P2A:
		case HAILO15_VID_GRP_SX_CSI1_P2A:
		case HAILO15_VID_GRP_S0_CSI1_P2A:
		case HAILO15_VID_GRP_S1_CSI1_P2A:
		case HAILO15_VID_GRP_S2_CSI1_P2A:
		case HAILO15_VID_GRP_S3_CSI1_P2A:
			return true;
		default:
			return false;
	}
}

static inline bool hailo15_is_mcm_raw_wr_grp_id(int grp_id)
{
	return grp_id == HAILO15_VID_GRP_MCM_RAW_WR;
}

static inline bool hailo15_is_mcm_in_grp_id(int grp_id)
{
	return grp_id == HAILO15_VID_GRP_MCM_IN;
}

static inline bool hailo15_is_isp_grp_id(int grp_id)
{
	switch(grp_id) {
		case HAILO15_VID_GRP_SX_CSI0_ISP_MP:
		case HAILO15_VID_GRP_SX_CSI0_ISP_SP:
		case HAILO15_VID_GRP_SX_CSI1_ISP_MP:
		case HAILO15_VID_GRP_SX_CSI1_ISP_SP:
		case HAILO15_VID_GRP_MCM_IN:
		case HAILO15_VID_GRP_MCM_RAW_WR:
			return true;
		default:
			return false;
	}
}

static inline int hailo15_grp_id_to_pipe_id(int grp_id)
{
	switch(grp_id) {
		case HAILO15_VID_GRP_SX_CSI0_ISP_MP:
		case HAILO15_VID_GRP_SX_CSI0_ISP_SP:
		case HAILO15_VID_GRP_SX_CSI1_ISP_MP:
		case HAILO15_VID_GRP_SX_CSI1_ISP_SP:
		case HAILO15_VID_GRP_SX_CSI0_P2A:
		case HAILO15_VID_GRP_SX_CSI1_P2A:
		case HAILO15_VID_GRP_MCM_IN:
		case HAILO15_VID_GRP_MCM_RAW_WR:
			return 0;
		case HAILO15_VID_GRP_S0_CSI0_P2A:
		case HAILO15_VID_GRP_S1_CSI0_P2A:
		case HAILO15_VID_GRP_S2_CSI0_P2A:
		case HAILO15_VID_GRP_S3_CSI0_P2A:
			return grp_id - HAILO15_VID_GRP_S0_CSI0_P2A;
		case HAILO15_VID_GRP_S0_CSI1_P2A:
		case HAILO15_VID_GRP_S1_CSI1_P2A:
		case HAILO15_VID_GRP_S2_CSI1_P2A:
		case HAILO15_VID_GRP_S3_CSI1_P2A:
			return grp_id - HAILO15_VID_GRP_S0_CSI1_P2A;
		default:
			return HAILO15_VID_GRP_INVALID;
	}
}

static inline bool is_hdr_capable(int grp_id)
{
	switch(grp_id) {
		case HAILO15_VID_GRP_SX_CSI0_P2A:
		case HAILO15_VID_GRP_SX_CSI1_P2A:
			return true;
		default:
			return false;
	}
}

static inline char* hailo15_grp_id_to_str(int grp_id)
{
	switch(grp_id) {
		case HAILO15_VID_GRP_SX_CSI0_ISP_MP:
			return "sx-csi0-isp-mp";
		case HAILO15_VID_GRP_SX_CSI0_ISP_SP:
			return "sx-csi0-isp-sp";
		case HAILO15_VID_GRP_SX_CSI1_ISP_MP:
			return "sx-csi1-isp-mp";
		case HAILO15_VID_GRP_SX_CSI1_ISP_SP:
			return "sx-csi1-isp-sp";
		case HAILO15_VID_GRP_SX_CSI0_P2A:
			return "sx-csi0-p2a";
		case HAILO15_VID_GRP_SX_CSI1_P2A:
			return "sx-csi1-p2a";
		case HAILO15_VID_GRP_MCM_RAW_WR:
			return "mcm-raw-wr";
		case HAILO15_VID_GRP_MCM_IN:
			return "mcm-in";
		case HAILO15_VID_GRP_S0_CSI0_P2A:
			return "s0-csi0-p2a";
		case HAILO15_VID_GRP_S1_CSI0_P2A:
			return "s1-csi0-p2a";
		case HAILO15_VID_GRP_S2_CSI0_P2A:
			return "s2-csi0-p2a";
		case HAILO15_VID_GRP_S3_CSI0_P2A:
			return "s3-csi0-p2a";
		case HAILO15_VID_GRP_S0_CSI1_P2A:
			return "s0-csi1-p2a";
		case HAILO15_VID_GRP_S1_CSI1_P2A:
			return "s1-csi1-p2a";
		case HAILO15_VID_GRP_S2_CSI1_P2A:
			return "s2-csi1-p2a";
		case HAILO15_VID_GRP_S3_CSI1_P2A:
			return "s3-csi1-p2a";
		default:
			return "invalid-vid";
	}
}

static inline int pixel_mux_grp_id_to_sink_pad_index(int grp_id)
{
	switch (grp_id) {
		case HAILO15_VID_GRP_SX_CSI0_ISP_MP:
		case HAILO15_VID_GRP_SX_CSI0_ISP_SP:
		case HAILO15_VID_GRP_SX_CSI0_P2A:
		case HAILO15_VID_GRP_S0_CSI0_P2A:
		case HAILO15_VID_GRP_S1_CSI0_P2A:
		case HAILO15_VID_GRP_S2_CSI0_P2A:
		case HAILO15_VID_GRP_S3_CSI0_P2A:
		case HAILO15_VID_GRP_MCM_RAW_WR:
			return PIXEL_MUX_SINK_PAD_0;
		case HAILO15_VID_GRP_SX_CSI1_ISP_MP:
		case HAILO15_VID_GRP_SX_CSI1_ISP_SP:
		case HAILO15_VID_GRP_SX_CSI1_P2A:
		case HAILO15_VID_GRP_S0_CSI1_P2A:
		case HAILO15_VID_GRP_S1_CSI1_P2A:
		case HAILO15_VID_GRP_S2_CSI1_P2A:
		case HAILO15_VID_GRP_S3_CSI1_P2A:
			return PIXEL_MUX_SINK_PAD_1;
		default:
			return -EINVAL;
	}
}

int hailo15_v4l2_notifier_bound(struct v4l2_async_notifier *,
				struct v4l2_subdev *,
				struct v4l2_async_subdev *,
				struct media_entity *);
const struct hailo15_video_fmt *hailo15_code_get_format(uint32_t code);
const struct hailo15_video_fmt *hailo15_fourcc_get_format(uint32_t fourcc, __u8 num_planes);
const struct hailo15_video_fmt *hailo15_fourcc_get_out_format(uint32_t fourcc, __u8 num_planes);
struct v4l2_subdev *hailo15_get_csi2rx_subdev(struct media_device *mdev, int grp_id);
struct v4l2_subdev *hailo15_get_sensor_subdev(struct media_device *mdev, int grp_id);
int hailo15_plane_get_bytesperline(const struct hailo15_video_fmt *format,
				   int width, int plane);
int hailo15_plane_get_sizeimage(const struct hailo15_video_fmt *format,
				int height, int bytesperline, int plane);
int hailo15_fill_planes_fmt(const struct hailo15_video_fmt *format,
				struct v4l2_pix_format_mplane *mfmt);
void hailo15_print_irq_error_message(struct err_status_reg *err_status_reg, u32 errors, int irq);

#endif
