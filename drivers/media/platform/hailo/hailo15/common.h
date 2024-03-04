#ifndef __HAILO15_COMMON__
#define __HAILO15_COMMON__

#include <media/videobuf2-v4l2.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-device.h>

enum hailo15_video_path {
	VID_GRP_ISP_MP,
	VID_GRP_ISP_SP,
	VID_GRP_P2A,
	VID_GRP_MAX,
};

#define STRIDE_ALIGN 16
#define FMT_MAX_PLANES 3

#define HAILO15_MAX_BUFFERS 10
#define HAILO15_NUM_P2A_BUFFERS 1
#define HAILO15_EVENT_RESOURCE_SIZE 4096 * 4

#define VIDEO_FPS_MONITOR_SUBDEV_IOC                                           \
	_IOR('D', BASE_VIDIOC_PRIVATE + 0, uint64_t)
#define VIDEO_GET_VSM_IOC                                                      \
	_IOWR('D', BASE_VIDIOC_PRIVATE + 1, struct hailo15_get_vsm_params)
#define VIDEO_GET_P2A_REGS                                                     \
	_IOR('D', BASE_VIDIOC_PRIVATE + 2, struct hailo15_p2a_buffer_regs_addr)

#define ISPIOC_V4L2_READ_REG                                                   \
	_IOWR('I', BASE_VIDIOC_PRIVATE + 0, struct isp_reg_data)
#define ISPIOC_V4L2_WRITE_REG                                                  \
	_IOWR('I', BASE_VIDIOC_PRIVATE + 1, struct isp_reg_data)
#define ISPIOC_V4L2_RMEM                                                       \
	_IOWR('I', BASE_VIDIOC_PRIVATE + 2, struct hailo15_rmem)
#define ISPIOC_V4L2_MI_START _IOWR('I', BASE_VIDIOC_PRIVATE + 3, uint32_t)
#define ISPIOC_V4L2_MI_STOP _IOWR('I', BASE_VIDIOC_PRIVATE + 4, uint32_t)
#define ISPIOC_V4L2_MCM_DQBUF                                                  \
	_IOWR('I', BASE_VIDIOC_PRIVATE + 5, struct isp_mcm_buf)
#define ISPIOC_V4L2_MCM_BUF_DONE                                               \
	_IOWR('I', BASE_VIDIOC_PRIVATE + 6, struct isp_mcm_buf)
#define ISPIOC_V4L2_MCM_MODE _IOWR('I', BASE_VIDIOC_PRIVATE + 7, uint32_t)
#define ISPIOC_V4L2_REQBUFS                                                    \
	_IOWR('I', BASE_VIDIOC_PRIVATE + 8, struct hailo15_reqbufs)
#define ISPIOC_V4L2_SET_INPUT_FORMAT                                           \
	_IOWR('I', BASE_VIDIOC_PRIVATE + 9, struct v4l2_subdev_format)
#define ISPIOC_V4L2_SET_MCM_MODE _IOWR('I', BASE_VIDIOC_PRIVATE + 10, uint32_t)

#define HAILO15_PAD_REQUBUFS                                                   \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, struct hailo15_pad_reqbufs)
#define HAILO15_PAD_BUF_DONE                                                   \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct hailo15_pad_buf)
#define HAILO15_PAD_BUF_QUEUE                                                  \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct hailo15_pad_buf)
#define HAILO15_PAD_S_STREAM                                                   \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct hailo15_pad_stream_status)

#define HAILO15_PAD_QUERYCTRL                                                  \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct hailo15_pad_queryctrl)
#define HAILO15_PAD_QUERY_EXT_CTRL                                             \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 14, struct hailo15_pad_query_ext_ctrl)
#define HAILO15_PAD_G_CTRL                                                     \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct hailo15_pad_control)
#define HAILO15_PAD_S_CTRL                                                     \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 16, struct hailo15_pad_control)
#define HAILO15_PAD_G_EXT_CTRLS                                                \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 17, struct hailo15_pad_ext_controls)
#define HAILO15_PAD_S_EXT_CTRLS                                                \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 18, struct hailo15_pad_ext_controls)
#define HAILO15_PAD_TRY_EXT_CTRLS                                              \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 19, struct hailo15_pad_ext_controls)
#define HAILO15_PAD_QUERYMENU                                                  \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 20, struct hailo15_pad_querymenu)

#define HAILO15_DMA_CTX_CB(ctx, func, ...)                                     \
	({                                                                     \
		int __hailo15_cb_retval;                                       \
		do {                                                           \
			if (!ctx) {                                            \
				__hailo15_cb_retval = -EINVAL;                 \
				break;                                         \
			}                                                      \
			if (!ctx->buf_ctx.ops) {                               \
				__hailo15_cb_retval = -EINVAL;                 \
				break;                                         \
			}                                                      \
			if (!ctx->buf_ctx.ops->func) {                         \
				__hailo15_cb_retval = -EINVAL;                 \
				break;                                         \
			}                                                      \
			__hailo15_cb_retval = ctx->buf_ctx.ops->func(          \
				ctx __VA_OPT__(, ) __VA_ARGS__);               \
		} while (0);                                                   \
		__hailo15_cb_retval;                                           \
	})

#define hailo15_video_node_buffer_process(ctx, buf)                            \
	(HAILO15_DMA_CTX_CB(ctx, buffer_process, buf))
#define hailo15_video_node_get_frame_count(ctx, fc)                            \
	(HAILO15_DMA_CTX_CB(ctx, get_frame_count, fc))
#define hailo15_video_node_get_rmem(ctx, rmem)                                 \
	(HAILO15_DMA_CTX_CB(ctx, get_rmem, rmem))
#define hailo15_video_node_get_event_resource(ctx, resource)                   \
	(HAILO15_DMA_CTX_CB(ctx, get_event_resource, resource))
#define hailo15_video_node_set_private_data(ctx, grp_id, data)                 \
	(HAILO15_DMA_CTX_CB(ctx, set_private_data, grp_id, data))
#define hailo15_video_node_get_private_data(ctx, grp_id, data)                 \
	(HAILO15_DMA_CTX_CB(ctx, get_private_data, grp_id, data))

#define hailo15_dma_buffer_done(ctx, buf, grp_id)                              \
	(HAILO15_DMA_CTX_CB(ctx, buffer_done, buf, grp_id))

#define hailo15_video_node_get_vsm(ctx, grp_id, index, vsm)                    \
	(HAILO15_DMA_CTX_CB(ctx, get_vsm, grp_id, index, vsm))

#define hailo15_video_node_queue_empty(ctx, path)                              \
	(HAILO15_DMA_CTX_CB(ctx, queue_empty, path))

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
	void *buffer_ready_ap_int_mask_addr;
	void *buffer_ready_ap_int_status_addr;
	void *buffer_ready_ap_int_w1c_addr;
	void *buffer_ready_ap_int_w1s_addr;
};

static const struct hailo15_video_fmt __hailo15_formats[] = {
	{
		.fourcc = V4L2_PIX_FMT_YUYV,
		.code = MEDIA_BUS_FMT_YUYV8_1X16,
		.pix_fmt = YUV422,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 2,
		.planes = { {
			.bpp = 2,
			.vscale_ratio = 1,
			.hscale_ratio = 1,
		} },
	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12P,
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 3840,
		.planes = { {
			.bpp = 2,
			.vscale_ratio = 1,
			.hscale_ratio = 1,
		} },
	},
	{
		.fourcc = V4L2_PIX_FMT_RGB24,
		.code = MEDIA_BUS_FMT_RGB888_1X24,
		.pix_fmt = RGB888,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 1,
		.planes = { {
			.bpp = 3,
			.vscale_ratio = 1,
			.hscale_ratio = 1,
		} },

	},
	{
		.fourcc = V4L2_PIX_FMT_NV12M,
		.code = MEDIA_BUS_FMT_YUYV8_2X8,
		.pix_fmt = YUV420,
		.planarity = SEMI_PLANAR,
		.num_planes = 2,
		.width_modulus = 4,
		.planes = { {
				    .bpp = 1,
				    .vscale_ratio = 1,
				    .hscale_ratio = 1,
			    },
			    {
				    .bpp = 1,
				    .vscale_ratio = 2,
				    .hscale_ratio = 1,
			    } },

	},
	{
		.fourcc = V4L2_PIX_FMT_SRGGB12,
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.pix_fmt = RAW12,
		.planarity = INTERLEAVED,
		.num_planes = 1,
		.width_modulus = 3840,
		.planes = { {
			.bpp = 2,
			.vscale_ratio = 1,
			.hscale_ratio = 1,
		} },
	},

};

static inline const struct hailo15_video_fmt *hailo15_get_formats(void)
{
	return __hailo15_formats;
}

static inline unsigned int hailo15_get_formats_count(void)
{
	return (sizeof(__hailo15_formats) / sizeof(struct hailo15_video_fmt));
}

struct hailo15_vsm {
	int dx;
	int dy;
};

struct isp_reg_data {
	uint32_t reg;
	uint32_t value;
};

struct hailo15_buffer {
	struct vb2_v4l2_buffer vb;
	struct media_pad *pad;
	struct list_head irqlist;
	dma_addr_t dma[FMT_MAX_PLANES];
	int grp_id;
	int flags;
	struct v4l2_subdev *sd;
};

struct hailo15_buf_ctx {
	struct list_head dmaqueue; /* internal free buffer queue */
	struct hailo15_buf_ops *ops;
	spinlock_t irqlock;
	struct hailo15_buffer *curr_buf;
	int pad; /* the pad the context referring to */
};

struct hailo15_dma_ctx {
	struct hailo15_buf_ctx buf_ctx;
	void *dev;
	void *vid_dev;
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

struct hailo15_pad_reqbufs {
	int pad;
	uint32_t num_buffers;
};

struct hailo15_pad_buf {
	uint32_t pad;
	struct hailo15_buf_ctx *buf;
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

struct hailo15_video_event_resource {
	struct mutex event_lock;
	uint64_t phy_addr;
	void *virt_addr;
	uint32_t size;
};

struct hailo15_subdev_list {
	struct list_head subdev_list;
	struct v4l2_subdev *subdev;
};

struct hailo15_mux_cfg {
	unsigned int pixel_mux_cfg;
	unsigned int isp0_stream0;
	unsigned int isp0_stream1;
	unsigned int isp0_stream2;
	unsigned int isp1_stream0;
	unsigned int isp1_stream1;
	unsigned int isp1_stream2;
	unsigned int vision_buffer_ready_ap_int_mask;
};

struct hailo15_mux_interrupt_cfg {
	unsigned int vision_subsys_asf_int_mask;
	unsigned int vision_asf_int_fatal_mask;
	unsigned int vision_asf_int_nonfatal_mask;
	unsigned int vision_subsys_err_int_mask;
	unsigned int vision_subsys_err_int_agg_mask;
};

struct hailo15_buf_ops {
	int (*buffer_process)(struct hailo15_dma_ctx *ctx,
			      struct hailo15_buffer *buf); /* VIDEO -> DMA */
	int (*buffer_done)(struct hailo15_dma_ctx *ctx,
			   struct hailo15_buffer *buf,
			   int grp_id); /* VIDEO <- DMA */
	int (*get_frame_count)(struct hailo15_dma_ctx *dma_ctx,
			       uint64_t *fc); /* VIDEO -> DMA */
	int (*get_rmem)(struct hailo15_dma_ctx *ctx,
			struct hailo15_rmem *rmem); /* VIDEO -> DMA */
	int (*get_event_resource)(struct hailo15_dma_ctx *ctx,
				  struct hailo15_video_event_resource
					  *event_resource); /* VIDEO -> DMA */
	int (*get_fbuf)(struct hailo15_dma_ctx *ctx,
			struct v4l2_framebuffer *fbuf); /*VIDEO -> DMA */
	int (*set_private_data)(struct hailo15_dma_ctx *ctx, int grp_id,
				void *data);
	int (*get_private_data)(struct hailo15_dma_ctx *ctx, int grp_id,
				void **data);
	int (*get_vsm)(struct hailo15_dma_ctx *ctx, int grp_id, int index,
		       struct hailo15_vsm *vsm); /* VIDEO -> DMA */
	int (*queue_empty)(struct hailo15_dma_ctx *ctx,
			   int path); /* VIDEO -> DMA */
};

#ifndef ALIGN_UP
#define ALIGN_UP(x, align) (((x) + (align)-1) & ~(align - 1))
#endif

int hailo15_v4l2_notifier_bound(struct v4l2_async_notifier *,
				struct v4l2_subdev *,
				struct v4l2_async_subdev *,
				struct media_entity *);
const struct hailo15_video_fmt *hailo15_code_get_format(uint32_t code);
const struct hailo15_video_fmt *hailo15_fourcc_get_format(uint32_t fourcc);
struct v4l2_subdev *hailo15_get_sensor_subdev(struct media_device *mdev);
int hailo15_plane_get_bytesperline(const struct hailo15_video_fmt *format,
				   int width, int plane);
int hailo15_plane_get_sizeimage(const struct hailo15_video_fmt *format,
				int height, int bytesperline, int plane);
int hailo15_fill_planes_fmt(const struct hailo15_video_fmt *format,
			    struct v4l2_pix_format_mplane *mfmt);
#endif
