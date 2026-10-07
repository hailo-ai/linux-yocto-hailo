// SPDX-License-Identifier: GPL-2.0-only
/*
 * Hailo MIPI generator (Introspect SV4E) virtual sensor driver
 *
 * This is a minimal V4L2 subdev for the Introspect SV4E MIPI generator —
 * an external CSI-2 traffic source wired into the Hailo15 CSI bridge. The
 * subdev exists only to advertise the source's frame format / timing to
 * the V4L2 media graph so that downstream (cdns-csi2rx, pixel_mux,
 * rxwrapper, ISP / video-cap) can negotiate the chain. The SV4E itself
 * runs free and is controlled out-of-band over USB from the Introspect
 * host; there is no I2C, no clock/regulator/GPIO/power-domain to manage.
 *
 * Originally cloned from the IMX334 sensor driver; everything related to
 * I2C register writes (exposure, gain, flip) and the v4l2 controls that
 * fed them was inherited then made into no-op shells when the I2C bus
 * was stripped. Those shells (and the writable ctrls that exposed them)
 * have been removed: ISP_media_server's userspace HAILO_SV4E driver
 * intercepts all register R/W in userspace before they reach this
 * subdev (see Sv4e_IsiReadRegIss / Sv4e_IsiWriteRegIss in
 * imaging-sub-system), so the kernel-side wrappers had no callers.
 *
 * What's kept:
 *   - pad/video subdev ops for chain negotiation
 *   - supported_modes[] (4K30/1080p30 in SRGGB12 + SGBRG12)
 *   - read-only V4L2 ctrls PIXEL_RATE (required by cdns-csi2rx),
 *     LINK_FREQ, HBLANK
 *   - parse_hw_config (reads DT csi-id / lane count)
 */
#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

/* Sensor identity to mimic at the v4l2-subdev layer.
 *
 * Default "sv4e" preserves the historical name "mipi_generator,sv4e" so
 * bare-gst / setup_sv4e.sh flows that pin on it keep working untouched.
 *
 * Any other non-empty, whitespace-free value of up to 15 chars is accepted
 * verbatim and rendered as "<value> <mimic_bus_addr>" (e.g.
 * "imx678 0-001a"). The intent is open-ended: the driver makes no
 * assumption about which sensors userspace recognises - it just exposes a
 * name that lets HML's SensorRegistry regex
 * "<prefix>\s+(\d+)-(\w+)" (see hailo-media-library
 * sensor_registry.cpp) match a prefix. Whether the surrounding userspace
 * has matching configs for that identity is a separate concern.
 *
 * Storage is a fixed buffer; sysfs writes update it in place, no alloc.
 */
static char mimic_buffer[16] = "sv4e";
module_param_string(mimic, mimic_buffer, sizeof(mimic_buffer), 0444);
MODULE_PARM_DESC(mimic,
	"Sensor identity to mimic at the v4l2-subdev layer. "
	"Default 'sv4e' = legacy name 'mipi_generator,sv4e'. "
	"Any other non-empty, non-whitespace, <= 15-char value is accepted "
	"and rendered as '<value> <mimic_bus_addr>'.");

/* The bus-address suffix appended to non-legacy mimic names. Cosmetic
 * because the MIPI generator is not on i2c at runtime; chosen to satisfy
 * HML's prefix-bus-addr regex. Default matches the most common real-sensor
 * bus/addr seen in HML's hailo15h/hailo15l device-tree layouts.
 */
static char mimic_bus_addr[16] = "0-001a";
module_param_string(mimic_bus_addr, mimic_bus_addr, sizeof(mimic_bus_addr), 0444);
MODULE_PARM_DESC(mimic_bus_addr,
	"Bus-address suffix appended to non-'sv4e' mimic names (default '0-001a'). "
	"Format expected by HML's regex: '<bus_decimal>-<addr_hex>'.");

/* Validate a mimic string for: non-empty, no whitespace / control / quote /
 * slash, length <= 15 (so "<value> <bus_addr>" fits in V4L2_SUBDEV_NAME_SIZE).
 * Open-ended on character content otherwise.
 */
static bool mimic_value_is_valid(const char *value)
{
	size_t i, len;

	if (!value || !*value)
		return false;
	len = strlen(value);
	if (len > sizeof(mimic_buffer) - 1)
		return false;
	for (i = 0; i < len; i++) {
		char c = value[i];

		if (c <= ' ' || c >= 0x7f)
			return false;
		if (c == '/' || c == '"' || c == '\'')
			return false;
	}
	return true;
}

/* Render the v4l2_subdev.name string for the chosen mimic.
 * Writes up to `len` bytes (must be V4L2_SUBDEV_NAME_SIZE = 32).
 * Invalid mimic falls back to the legacy name with a warning.
 */
static void render_subdev_name_for_mimic(const char *mimic_value, char *dest, size_t len)
{
	if (strcmp(mimic_value, "sv4e") == 0) {
		snprintf(dest, len, "mipi_generator,sv4e");
	} else if (mimic_value_is_valid(mimic_value)) {
		snprintf(dest, len, "%s %s", mimic_value, mimic_bus_addr);
	} else {
		pr_warn("mipi_generator_sv4e: invalid mimic '%s', using legacy name\n",
			mimic_value);
		snprintf(dest, len, "mipi_generator,sv4e");
	}
}

#define DEFAULT_MODE_IDX 0

/* vmax/vblank ceilings come from the LPFR field width on a real sensor; we
 * retain them here because supported_modes[].vblank_max uses them and they
 * cap the HBLANK ctrl's range.
 */
#define sv4e_MAX_LPFR_4K	((1 << 20) - 2)		/* max even 20-bit value */
#define sv4e_MAX_VBLANK_4K	(sv4e_MAX_LPFR_4K - 2160)
#define sv4e_REG_MIN		0x00
#define sv4e_REG_MAX		0xfffff

/* CSI2 HW configuration — advertised on the source pad */
#define sv4e_LINK_FREQ		891000000
#define sv4e_NUM_DATA_LANES	4

/* Local CID base for sensor-mimicry stub ctrls. HML looks these ctrls up by
 * name string, not CID, so the numeric values are private to this driver.
 * The same +0x2000 offset is used by other drivers in this tree (imx*, cdns-csi2rx);
 * that is benign here because each driver populates its own v4l2_ctrl_handler,
 * and any cross-driver collision would only surface for CID-based lookups -
 * which HML does not perform against this subdev.
 */
/* Local CID base for sensor-mimicry stub ctrls that don't have a standard
 * V4L2_CID. The Wide Dynamic Range ctrl uses the standard
 * V4L2_CID_WIDE_DYNAMIC_RANGE (camera class) directly via
 * v4l2_ctrl_new_std() - same pattern imx664 / imx675 use, see
 * imx664.c:2843 - so the framework picks up the standard "Wide Dynamic
 * Range" name automatically without a hardcoded .name string. The
 * priming and custom_rhs1 ctrls have no standard CID and stay custom.
 */
#define MIPI_GEN_CID_BASE             (V4L2_CID_USER_BASE + 0x2000)
#define MIPI_GEN_CID_CUSTOM_RHS1      (MIPI_GEN_CID_BASE + 0x02)
#define MIPI_GEN_CID_WDR_PRIMING      (MIPI_GEN_CID_BASE + 0x03)

/**
 * struct sv4e_mode - SV4E source-pad mode entry
 */
struct sv4e_mode {
	u32 width;
	u32 height;
	u32 code;
	u32 hblank;
	u64 pclk;
	u32 link_freq_idx;
	struct v4l2_fract frame_interval;
};

/**
 * struct sv4e - SV4E subdev state
 */
struct sv4e {
	struct device *dev;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq_ctrl;
	struct v4l2_ctrl *pclk_ctrl;
	struct v4l2_ctrl *hblank_ctrl;
	const struct sv4e_mode *cur_mode;
	struct mutex mutex;
	bool streaming;
	/* Ephemeral mode populated by permissive set_pad_format for non-table
	 * (w,h,code) requests. cur_mode points here when matched == -1.
	 */
	struct sv4e_mode runtime_mode;
	s64 *link_freqs;
	size_t nr_link_freqs;
};

/* Supported source-pad modes. 4K30/1080p30 in SRGGB12 + SGBRG12 cover the
 * real IMX sensor modes we replay (imx662/imx678 RGGB, imx715 GBRG) —
 * advertising Bayer modes lets isp_media_server bring up the ISP path
 * automatically (matches how real IMX sensors behave). The legacy YUV422
 * BYD test mode that earlier branches advertised is parked on the
 * MSW-14708-mipi-gen-yuv-bypass kernel branch and intentionally absent
 * here; the supporting csi2rx/pixel_mux/rxwrapper YUV422_8b switch cases
 * on the bypass path are absent too.
 */
/* Permissive bounds for runtime-built modes. The 3840/2160 ceiling comes
 * from rxwrapper hardcoded credits and DMA-page counts; see
 * hailo15-isp-hw-defs.h:4-5 (INPUT_WIDTH/INPUT_HEIGHT) and
 * hailo15-rxwrapper.c:106-127 (RES_4K_FRAME_LINE_NUM, RES_4K_STRIDE,
 * RXWRAPPER_RES_4K_DEFAULT_CREDITS_*). Bit-width ceiling = 12 (RAW12).
 */
static bool sv4e_format_within_bounds(u32 width, u32 height, u32 code)
{
	if (width == 0 || width > 3840)
		return false;
	if (height == 0 || height > 2160)
		return false;
	switch (code) {
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
		return true;
	default:
		return false;
	}
}

static const struct sv4e_mode supported_modes[] = {
	/* idx 0: 4K30 RGGB12 (existing) */
	{
		.width = 3840,
		.height = 2160,
		.hblank = 100,
		.pclk = 594000000,
		.link_freq_idx = 0,
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		},
	},
	/* idx 1: 1080p30 RGGB12 (new — covers imx662 + imx678 1080p) */
	{
		.width = 1920,
		.height = 1080,
		.hblank = 100,
		.pclk = 594000000,
		.link_freq_idx = 0,
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		},
	},
	/* idx 2: 4K30 GBRG12 (new — covers imx715 4K) */
	{
		.width = 3840,
		.height = 2160,
		.hblank = 100,
		.pclk = 594000000,
		.link_freq_idx = 0,
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		},
	},
	/* idx 3: 1080p30 GBRG12 (new — covers imx715 1080p) */
	{
		.width = 1920,
		.height = 1080,
		.hblank = 100,
		.pclk = 594000000,
		.link_freq_idx = 0,
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.frame_interval = {
			.denominator = 30,
			.numerator = 1,
		},
	},
};

static inline struct sv4e *to_sv4e(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct sv4e, sd);
}

/* Ctrl ops shared by all subdev ctrls.
 *
 * Read-only ctrls (PIXEL_RATE / LINK_FREQ / HBLANK) advertise modes to
 * cdns-csi2rx; .s_ctrl is never reached for them. The sensor-mimicry stub
 * ctrls (Wide Dynamic Range, custom_rhs1) ARE writable - HML's
 * IspManager::switch_to_sdr writes them during init. The stub accepts
 * any value (returns 0); the v4l2-ctrl core caches the written value so a
 * subsequent get reads it back, which is all HML observes.
 */
static int sv4e_set_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops sv4e_ctrl_ops = {
	.s_ctrl = sv4e_set_ctrl,
};

/* CUSTOM_RHS1 stub. HML looks this up by exact name "custom_rhs1".
 * Profile-driven value; no hardware effect.
 */
static const struct v4l2_ctrl_config mipi_gen_ctrl_custom_rhs1 = {
	.ops  = &sv4e_ctrl_ops,
	.id   = MIPI_GEN_CID_CUSTOM_RHS1,
	.name = "custom_rhs1",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min  = 0,
	.max  = 65535,
	.step = 1,
	.def  = 0,
};

/* WDR_PRIMING stub. HML looks this up by exact name "wdr_priming" - this is
 * the fast-toggle variant of Wide Dynamic Range, written by HML before a
 * mid-stream profile change so the next frame can switch modes without a
 * full pipeline restart. Without this stub, HCV profile transitions that
 * trigger fast_toggle (e.g. AI-ISP Gen2 on imx678 mimic) fail with -515.
 * Same no-op accept pattern as the other stubs.
 */
static const struct v4l2_ctrl_config mipi_gen_ctrl_wdr_priming = {
	.ops  = &sv4e_ctrl_ops,
	.id   = MIPI_GEN_CID_WDR_PRIMING,
	.name = "wdr_priming",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min  = 0,
	.max  = 1,
	.step = 1,
	.def  = 0,
};

static int sv4e_enum_mbus_code(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;
	code->code = supported_modes[code->index].code;
	return 0;
}

static int sv4e_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_size_enum *fsize)
{
	size_t i;
	int matched = -1;
	u32 n_for_code = 0;
	const struct sv4e_mode *m;

	for (i = 0; i < ARRAY_SIZE(supported_modes); ++i) {
		if (supported_modes[i].code == fsize->code) {
			if (n_for_code == fsize->index)
				matched = (int)i;
			n_for_code++;
		}
	}
	if (matched < 0)
		return -EINVAL;
	m = &supported_modes[matched];
	fsize->min_width = m->width;
	fsize->max_width = m->width;
	fsize->min_height = m->height;
	fsize->max_height = m->height;
	return 0;
}

static void sv4e_fill_pad_format(struct sv4e *sv4e,
				 const struct sv4e_mode *mode,
				 struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = mode->code;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;
	fmt->format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->format.quantization = V4L2_QUANTIZATION_DEFAULT;
	fmt->format.xfer_func = V4L2_XFER_FUNC_NONE;
}

static int sv4e_get_pad_format(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_format *fmt)
{
	struct sv4e *sv4e = to_sv4e(sd);

	mutex_lock(&sv4e->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *framefmt;
		framefmt = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
		fmt->format = *framefmt;
	} else {
		sv4e_fill_pad_format(sv4e, sv4e->cur_mode, fmt);
	}
	mutex_unlock(&sv4e->mutex);
	return 0;
}

static int sv4e_set_pad_format(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_format *fmt)
{
	struct sv4e *sv4e = to_sv4e(sd);
	const struct sv4e_mode *mode;
	size_t i, matched = (size_t)-1;

	mutex_lock(&sv4e->mutex);

	/*
	 * Three-branch resolution: (1) exact match in supported_modes — use it;
	 * (2) bounds-OK but unmatched — populate sv4e->runtime_mode from
	 * supported_modes[0] defaults; (3) out-of-bounds — return -EINVAL.
	 */
	for (i = 0; i < ARRAY_SIZE(supported_modes); ++i) {
		if (supported_modes[i].code != fmt->format.code)
			continue;
		if (fmt->format.width && fmt->format.width != supported_modes[i].width)
			continue;
		if (fmt->format.height && fmt->format.height != supported_modes[i].height)
			continue;
		matched = i;
		break;
	}
	if (matched != (size_t)-1) {
		mode = &supported_modes[matched];
	} else if (sv4e_format_within_bounds(fmt->format.width,
					     fmt->format.height,
					     fmt->format.code)) {
		/* Build ephemeral mode using defaults from supported_modes[0]
		 * for fields the V4L2 client doesn't supply (pclk, hblank,
		 * link_freq_idx, frame_interval). TODO(F7): per-fps VMAX/HMAX.
		 */
		sv4e->runtime_mode = supported_modes[0];
		sv4e->runtime_mode.width = fmt->format.width;
		sv4e->runtime_mode.height = fmt->format.height;
		sv4e->runtime_mode.code = fmt->format.code;
		mode = &sv4e->runtime_mode;
	} else {
		mutex_unlock(&sv4e->mutex);
		dev_err_ratelimited(sv4e->dev,
			"set_fmt: out-of-bounds %ux%u code=0x%x rejected",
			fmt->format.width, fmt->format.height,
			fmt->format.code);
		return -EINVAL;
	}

	sv4e_fill_pad_format(sv4e, mode, fmt);

	if (fmt->which != V4L2_SUBDEV_FORMAT_TRY)
		sv4e->cur_mode = mode;

	mutex_unlock(&sv4e->mutex);
	return 0;
}

static int sv4e_init_pad_cfg(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *sd_state)
{
	struct sv4e *sv4e = to_sv4e(sd);
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = sd_state ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	sv4e_fill_pad_format(sv4e, &supported_modes[DEFAULT_MODE_IDX], &fmt);
	return sv4e_set_pad_format(sd, sd_state, &fmt);
}

static int sv4e_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct sv4e *sv4e = to_sv4e(sd);

	mutex_lock(&sv4e->mutex);
	pr_info("sv4e: subdev %s: set_streaming %d\n", sd->name, enable);
	sv4e->streaming = enable;
	mutex_unlock(&sv4e->mutex);
	return 0;
}

static int
sv4e_find_nearest_frame_interval_mode(struct sv4e *sv4e,
				      struct v4l2_subdev_frame_interval *fi,
				      struct sv4e_mode const **mode)
{
	struct sv4e_mode const *curr_mode;
	int min_diff = INT_MAX;
	int curr_diff;
	int i;

	if (!sv4e || !fi)
		return -EINVAL;
	if (fi->interval.denominator == 0 || fi->interval.numerator == 0) {
		*mode = &supported_modes[DEFAULT_MODE_IDX];
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(supported_modes); ++i) {
		curr_mode = &supported_modes[i];
		curr_diff = abs(curr_mode->frame_interval.denominator -
				(int)(fi->interval.denominator /
				      fi->interval.numerator));
		if (curr_diff == 0) {
			*mode = curr_mode;
			return 0;
		}
		if (curr_diff < min_diff) {
			min_diff = curr_diff;
			*mode = curr_mode;
		}
	}
	return 0;
}

static int sv4e_s_frame_interval(struct v4l2_subdev *sd,
				 struct v4l2_subdev_frame_interval *fi)
{
	struct sv4e *sv4e = to_sv4e(sd);
	struct sv4e_mode const *mode;
	int ret;

	mutex_lock(&sv4e->mutex);
	ret = sv4e_find_nearest_frame_interval_mode(sv4e, fi, &mode);
	if (ret == 0) {
		fi->interval = mode->frame_interval;
		sv4e->cur_mode = mode;
	}
	mutex_unlock(&sv4e->mutex);
	return ret;
}

static int sv4e_g_frame_interval(struct v4l2_subdev *sd,
				 struct v4l2_subdev_frame_interval *fi)
{
	struct sv4e *sv4e = to_sv4e(sd);

	mutex_lock(&sv4e->mutex);
	fi->interval = sv4e->cur_mode->frame_interval;
	mutex_unlock(&sv4e->mutex);
	return 0;
}

/* Parse DT for CSI lane count + link frequency match. */
static int sv4e_parse_hw_config(struct sv4e *sv4e)
{
	struct fwnode_handle *fwnode = dev_fwnode(sv4e->dev);
	struct v4l2_fwnode_endpoint bus_cfg = { .bus_type = V4L2_MBUS_CSI2_DPHY };
	struct fwnode_handle *ep;
	int ret;
	int i;

	if (!fwnode)
		return -ENXIO;
	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -ENXIO;
	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;
	if (bus_cfg.bus.mipi_csi2.num_data_lanes != sv4e_NUM_DATA_LANES) {
		dev_err(sv4e->dev,
			"number of CSI2 data lanes %d is not supported",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto done_endpoint_free;
	}
	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(sv4e->dev, "no link frequencies defined");
		ret = -EINVAL;
		goto done_endpoint_free;
	}
	for (i = 0; i < bus_cfg.nr_of_link_frequencies; i++)
		if (bus_cfg.link_frequencies[i] == sv4e_LINK_FREQ)
			goto done_endpoint_free;
	ret = -EINVAL;
done_endpoint_free:
	v4l2_fwnode_endpoint_free(&bus_cfg);
	return ret;
}

static const struct v4l2_subdev_video_ops sv4e_video_ops = {
	.s_stream = sv4e_set_stream,
	.s_frame_interval = sv4e_s_frame_interval,
	.g_frame_interval = sv4e_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops sv4e_pad_ops = {
	.init_cfg = sv4e_init_pad_cfg,
	.enum_mbus_code = sv4e_enum_mbus_code,
	.enum_frame_size = sv4e_enum_frame_size,
	.get_fmt = sv4e_get_pad_format,
	.set_fmt = sv4e_set_pad_format,
};

static const struct v4l2_subdev_ops sv4e_subdev_ops = {
	.video = &sv4e_video_ops,
	.pad = &sv4e_pad_ops,
};

/* Register the three read-only ctrls that downstream blocks query for
 * timing (cdns-csi2rx looks up PIXEL_RATE; LINK_FREQ + HBLANK round out
 * the standard CSI sensor advertisement).
 */
static int sv4e_init_controls(struct sv4e *sv4e)
{
	struct v4l2_ctrl_handler *ctrl_hdlr = &sv4e->ctrl_handler;
	const struct sv4e_mode *mode = sv4e->cur_mode;
	int ret;

	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 6);
	if (ret)
		return ret;
	ctrl_hdlr->lock = &sv4e->mutex;

	sv4e->pclk_ctrl = v4l2_ctrl_new_std(ctrl_hdlr, &sv4e_ctrl_ops,
					    V4L2_CID_PIXEL_RATE, mode->pclk,
					    mode->pclk, 1, mode->pclk);
	sv4e->link_freq_ctrl = v4l2_ctrl_new_int_menu(
		ctrl_hdlr, &sv4e_ctrl_ops, V4L2_CID_LINK_FREQ,
		sv4e->nr_link_freqs - 1, mode->link_freq_idx,
		sv4e->link_freqs);
	if (sv4e->link_freq_ctrl)
		sv4e->link_freq_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	sv4e->hblank_ctrl =
		v4l2_ctrl_new_std(ctrl_hdlr, &sv4e_ctrl_ops, V4L2_CID_HBLANK,
				  sv4e_REG_MIN, sv4e_REG_MAX, 1, mode->hblank);
	if (sv4e->hblank_ctrl)
		sv4e->hblank_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* Sensor-mimicry stubs. HML's IspManager::switch_to_sdr writes Wide
	 * Dynamic Range / custom_rhs1; the fast-toggle path additionally
	 * writes wdr_priming. The generator accepts any value (no hardware
	 * effect). WDR uses the standard V4L2_CID (same pattern as imx664/
	 * imx675 - see imx664.c:2843) so v4l2-ctrls-defs.c supplies the
	 * standard "Wide Dynamic Range" name. The priming + custom_rhs1
	 * ctrls have no standard CID and stay custom-config. Return
	 * pointers are discarded - registration errors surface through
	 * ctrl_hdlr->error checked below.
	 */
	v4l2_ctrl_new_std(ctrl_hdlr, &sv4e_ctrl_ops,
			  V4L2_CID_WIDE_DYNAMIC_RANGE, 0, 1, 1, 0);
	v4l2_ctrl_new_custom(ctrl_hdlr, &mipi_gen_ctrl_custom_rhs1, NULL);
	v4l2_ctrl_new_custom(ctrl_hdlr, &mipi_gen_ctrl_wdr_priming, NULL);

	if (ctrl_hdlr->error) {
		dev_err(sv4e->dev, "control init failed: %d", ctrl_hdlr->error);
		v4l2_ctrl_handler_free(ctrl_hdlr);
		return ctrl_hdlr->error;
	}
	sv4e->sd.ctrl_handler = ctrl_hdlr;
	return 0;
}

/* Sysfs attribute for runtime mimic switching.
 * Located under the subdev's class dir (parented to sv4e->sd.dev).
 * Writes update mimic_buffer in place and re-render sd.name.
 */
static ssize_t mimic_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s\n", mimic_buffer);
}

static ssize_t mimic_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	char trimmed[sizeof(mimic_buffer)];
	size_t n;
	struct v4l2_subdev *sd = dev_get_drvdata(dev);

	n = min(count, sizeof(trimmed) - 1);
	memcpy(trimmed, buf, n);
	trimmed[n] = '\0';
	/* strip trailing newline */
	if (n > 0 && trimmed[n - 1] == '\n')
		trimmed[n - 1] = '\0';

	if (strcmp(trimmed, "sv4e") != 0 && !mimic_value_is_valid(trimmed)) {
		pr_warn("mipi_generator_sv4e: rejected mimic '%s' (empty / contains whitespace or special chars / too long)\n",
			trimmed);
		return -EINVAL;
	}

	/* Update the module-param-backed buffer in place; no allocation. */
	strscpy(mimic_buffer, trimmed, sizeof(mimic_buffer));

	if (sd)
		render_subdev_name_for_mimic(mimic_buffer, sd->name, sizeof(sd->name));

	return count;
}

/* Single-instance driver: mimic_buffer is intentionally module-scoped, and
 * mimic_store does not lock against concurrent writers. The attribute is a
 * debug knob expected to be set once at boot via the module param or once
 * interactively from a single root shell.
 */
static DEVICE_ATTR_RW(mimic);

static int sv4e_probe(struct platform_device *pdev)
{
	struct sv4e *sv4e;
	struct device *dev = &pdev->dev;
	int ret;

	sv4e = devm_kzalloc(&pdev->dev, sizeof(*sv4e), GFP_KERNEL);
	if (!sv4e)
		return -ENOMEM;
	sv4e->dev = dev;
	dev_info(sv4e->dev, "probe started");

	v4l2_subdev_init(&sv4e->sd, &sv4e_subdev_ops);
	sv4e->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	render_subdev_name_for_mimic(mimic_buffer, sv4e->sd.name, sizeof(sv4e->sd.name));

	ret = sv4e_parse_hw_config(sv4e);
	if (ret) {
		dev_err(sv4e->dev, "HW configuration is not supported");
		return ret;
	}

	mutex_init(&sv4e->mutex);
	sv4e->cur_mode = &supported_modes[DEFAULT_MODE_IDX];

	sv4e->link_freqs = devm_kcalloc(dev, 1, sizeof(s64), GFP_KERNEL);
	if (!sv4e->link_freqs) {
		ret = -ENOMEM;
		goto error_destroy_mutex;
	}
	sv4e->link_freqs[0] = sv4e_LINK_FREQ;
	sv4e->nr_link_freqs = 1;

	ret = sv4e_init_controls(sv4e);
	if (ret) {
		dev_err(sv4e->dev, "failed to init controls: %d", ret);
		goto error_destroy_mutex;
	}

	sv4e->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	sv4e->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sv4e->sd.entity, 1, &sv4e->pad);
	if (ret) {
		dev_err(sv4e->dev, "failed to init entity pads: %d", ret);
		goto error_handler_free;
	}

	sv4e->sd.dev = sv4e->dev;
	platform_set_drvdata(pdev, &sv4e->sd);

	/* Expose runtime mimic switching via sysfs.
	 * platform_set_drvdata above stores &sv4e->sd so mimic_store can
	 * recover the subdev via dev_get_drvdata.
	 */
	ret = device_create_file(sv4e->sd.dev, &dev_attr_mimic);
	if (ret)
		dev_warn(sv4e->sd.dev, "failed to create mimic sysfs attr: %d\n", ret);

	ret = v4l2_async_register_subdev_sensor(&sv4e->sd);
	if (ret < 0) {
		dev_err(sv4e->dev, "failed to register async subdev: %d", ret);
		goto error_media_entity;
	}

	dev_info(sv4e->dev, "probe finished successfully");
	return 0;

error_media_entity:
	device_remove_file(sv4e->sd.dev, &dev_attr_mimic);
	media_entity_cleanup(&sv4e->sd.entity);
error_handler_free:
	v4l2_ctrl_handler_free(sv4e->sd.ctrl_handler);
error_destroy_mutex:
	mutex_destroy(&sv4e->mutex);
	if (ret == -ENXIO)
		dev_info(sv4e->dev, "exit probe, sensor not connected");
	else
		dev_err(sv4e->dev, "probe failed with %d", ret);
	return ret;
}

static int sv4e_remove(struct platform_device *pdev)
{
	struct v4l2_subdev *sd = platform_get_drvdata(pdev);
	struct sv4e *sv4e = to_sv4e(sd);

	device_remove_file(sd->dev, &dev_attr_mimic);
	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	mutex_destroy(&sv4e->mutex);
	return 0;
}

static const struct of_device_id sv4e_of_match[] = {
	{ .compatible = "mipi_generator,sv4e" },
	{}
};
MODULE_DEVICE_TABLE(of, sv4e_of_match);

static struct platform_driver sv4e_driver = {
	.probe = sv4e_probe,
	.remove = sv4e_remove,
	.driver = {
		.name = "mipi_generator,sv4e",
		.of_match_table = sv4e_of_match,
	},
};
module_platform_driver(sv4e_driver);

MODULE_DESCRIPTION("Hailo MIPI generator (Introspect SV4E) virtual sensor driver");
MODULE_LICENSE("GPL");
