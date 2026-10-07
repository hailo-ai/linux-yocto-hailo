#ifndef HAILO_SHARED_SENSOR_DATA_H
#define HAILO_SHARED_SENSOR_DATA_H

// sensor specific IDs
#include "sensor_id.h"

#include <linux/delay.h>
#include <media/v4l2-ctrls.h>

// Fast toggle data - copied from hailo15/common.h
// we do not include it, as the sensor drivers should be standalone
#define HAILO15_INTERNAL_SENSOR_FAST_TOGGLE_SET_STATUS	_IOW('K', BASE_VIDIOC_PRIVATE + 1, struct fast_toggle_data)

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

/* Transitions on which the regulators stay up and the STANDBY sequence is skipped. */
#define HAILO_FT_SKIP_PREISP	(BIT(TOGGLE_PLUTO_SDR_PREISP) | \
				 BIT(TOGGLE_PLUTO_PREISP_SDR))
#define HAILO_FT_SKIP_2DOL	(BIT(TOGGLE_PLUTO_SDR_HDR) | \
				 BIT(TOGGLE_PLUTO_HDR_SDR) | \
				 BIT(TOGGLE_PLUTO_PREISP_HDR) | \
				 BIT(TOGGLE_PLUTO_HDR_PREISP))

static inline bool hailo_ft_skip_standby(enum fast_toggle_state state,
					 enum fast_toggle_type type,
					 u32 skip_mask)
{
	return state != FAST_TOGGLE_NONE && (skip_mask & BIT(type));
}

/* Record the incoming state and type; APPLY_PRIMING hands off to the sensor. */
static inline int hailo_ft_set_state(struct device *dev,
				     const struct fast_toggle_data *data,
				     enum fast_toggle_state *state,
				     enum fast_toggle_type *type,
				     int (*apply_priming)(void *sensor),
				     void *sensor)
{
	if (data->state < 0 || data->state >= FAST_TOGGLE_STATE_MAX) {
		dev_err(dev, "Invalid fast toggle state %d\n", data->state);
		return -EINVAL;
	}

	*state = data->state;
	*type = data->type;

	if (data->state == FAST_TOGGLE_APPLY_PRIMING && apply_priming)
		return apply_priming(sensor);

	return 0;
}

/*
 * Where a sensor keeps the LEF shutter, frame length and analog gain. LPFR is needed
 * because SHR0 is an offset from the end of the frame, not an exposure: the cache
 * converts the pair into exposure lines (see hailo_ft_cache_exposure).
 */
struct hailo_ft_exp_regs {
	u16 shutter;
	u16 again;
	u16 lpfr;
	u8 shutter_len;
	u8 again_len;
	u8 lpfr_len;
};

typedef int (*hailo_ft_read_reg)(void *sensor, u16 reg, u32 len, u32 *val);
typedef int (*hailo_ft_write_reg)(void *sensor, u16 reg, u32 len, u32 val);

/*
 * Adapters from a driver's typed register accessors to the void * callbacks above.
 * Calling a typed function through an incompatible function pointer is undefined, so
 * every driver needs these; they differ only in the sensor type.
 */
#define HAILO_FT_DEFINE_REG_OPS(sen)						\
static inline int sen##_ft_read_reg(void *sensor, u16 reg, u32 len, u32 *val)		\
{										\
	return sen##_read_reg(sensor, reg, len, val);				\
}										\
										\
static inline int sen##_ft_write_reg(void *sensor, u16 reg, u32 len, u32 val)		\
{										\
	return sen##_write_reg(sensor, reg, len, val);				\
}										\
										\
static inline int sen##_ft_write_regs(void *sensor, const void *regs, u32 num)		\
{										\
	return sen##_write_regs(sensor, regs, num);				\
}

/*
 * Capture the operating point of the regime being left, keyed by that regime, so a
 * later delta switch back resumes it instead of the delta table's default.
 *
 * What gets cached is exposure in lines, not SHR0. SHR0 is an offset from the end of
 * the frame - exposure = dol * LPFR - SHR0 - so the same SHR0 under a different LPFR
 * is a different integration time. 3A drives both SHR0 and LPFR straight over i2c,
 * outside the V4L2 controls and without the driver's vblank following, so both are
 * read back from the sensor here and converted on the spot.
 *
 * Best-effort: a failed read or an out-of-range pair leaves the cache untouched, and
 * the mode then starts from the controls' defaults.
 */
static inline void hailo_ft_cache_exposure(struct device *dev,
					   const struct hailo_ft_exp_regs *regs,
					   u32 *exp_cache, u32 *gain_cache,
					   bool hdr, u32 dol,
					   hailo_ft_read_reg read_reg,
					   void *sensor)
{
	u32 shr0 = 0, lpfr = 0, gain = 0;
	int regime = hdr ? 1 : 0;

	if (read_reg(sensor, regs->shutter, regs->shutter_len, &shr0) ||
	    read_reg(sensor, regs->lpfr, regs->lpfr_len, &lpfr) ||
	    read_reg(sensor, regs->again, regs->again_len, &gain))
		return;

	if (!dol || !lpfr || dol * lpfr <= shr0)
		return;

	exp_cache[regime] = dol * lpfr - shr0;
	gain_cache[regime] = gain;
	dev_info(dev, "fast-toggle: cache %s exp=%u gain=%u (SHR0=%u LPFR=%u dol=%u)",
		 regime ? "HDR" : "SDR", exp_cache[regime], gain, shr0, lpfr, dol);
}

/*
 * Set the LEF exposure/gain controls for the incoming mode: the exposure and gain it
 * last streamed at (SDR and HDR are cached separately), or the controls' defaults
 * when it has not streamed yet. Both fit the incoming frame, so the handler_setup
 * that follows never replays an HDR exposure into an SDR frame (or the reverse).
 */
static inline int hailo_ft_restore_exposure(struct device *dev,
					    const u32 *exp_cache,
					    const u32 *gain_cache, bool hdr,
					    struct v4l2_ctrl *exp_ctrl,
					    struct v4l2_ctrl *again_ctrl)
{
	int regime = hdr ? 1 : 0;
	bool cached = exp_cache[regime] != 0;
	s32 exp, gain;
	int ret;

	if (!exp_ctrl || !again_ctrl)
		return 0;

	exp = cached ? exp_cache[regime] : exp_ctrl->default_value;
	gain = cached ? gain_cache[regime] : again_ctrl->default_value;

	dev_info(dev, "fast-toggle: restore %s exp=%d gain=%d%s",
		 regime ? "HDR" : "SDR", exp, gain, cached ? "" : " (default)");

	ret = __v4l2_ctrl_s_ctrl(exp_ctrl, exp);
	if (ret) {
		dev_err(dev, "fail to restore exposure (%d)", ret);
		return ret;
	}

	ret = __v4l2_ctrl_s_ctrl(again_ctrl, gain);
	if (ret)
		dev_err(dev, "fail to restore gain (%d)", ret);

	return ret;
}

/*
 * An exposure needs a frame of at least (exposure + gap) / dol lines, so that
 * SHR0 = dol * LPFR - exposure does not drop below gap. Return the mode's LPFR
 * when it is long enough, otherwise that minimum.
 */
static inline u32 hailo_ft_lpfr_for_exposure(u32 mode_lpfr, u32 exposure,
					     u32 gap, u32 dol)
{
	u32 required_lpfr;

	if (!dol)
		return mode_lpfr;

	required_lpfr = DIV_ROUND_UP(exposure + gap, dol);
	return max(mode_lpfr, required_lpfr);
}

/*
 * What a driver must supply for the shared mode-apply step below.
 */
struct hailo_ft_mode_ops {
	struct device *dev;
	void *sensor;
	int (*write_regs)(void *sensor, const void *regs, u32 num);
	/* LEF cluster: exposure is the master, gain its member */
	struct v4l2_ctrl *exp_ctrl;
	struct v4l2_ctrl *again_ctrl;
	u32 *exp_cache;
	u32 *gain_cache;
};

/*
 * Program the incoming mode: nothing on a same-mode restart, the delta when the
 * transition has one, the full table otherwise. Pass delta_regs = NULL when the
 * transition has no delta.
 */
static inline int hailo_ft_apply_mode(const struct hailo_ft_mode_ops *ops,
				      bool same_mode, bool hdr,
				      const void *delta_regs, u32 delta_num,
				      const void *full_regs, u32 full_num)
{
	int ret;

	/*
	 * Same-mode restart (SDR<->SDR, SDR<->PreISP): the registers survived the
	 * pm_runtime skip, so only exposure/gain/vblank need pushing - nothing to do
	 * here. prev_mode is cleared on a non-fast-toggle stop, so this only fires
	 * when they really did survive.
	 */
	if (same_mode)
		return 0;

	if (delta_regs) {
		/*
		 * Delta switch (SDR<->2DOL): write only the registers that differ.
		 * Reachable only on a fast toggle - a non-fast-toggle stop clears
		 * prev_mode, the delta lookup then returns NULL, and such restarts fall
		 * through to the full reprogram below.
		 */
		ret = ops->write_regs(ops->sensor, delta_regs, delta_num);
		if (ret) {
			dev_err(ops->dev, "fail to write delta registers");
			return ret;
		}
	} else {
		/* Full reprogram (first boot or unsupported transition) */
		ret = ops->write_regs(ops->sensor, full_regs, full_num);
		if (ret) {
			dev_err(ops->dev, "fail to write initial registers");
			return ret;
		}
	}

	/*
	 * SDR and HDR share the exposure control, so after a switch between them it
	 * still holds the exposure of the mode being left. Set it for the incoming
	 * mode before the control push below replays it into the new frame.
	 */
	ret = hailo_ft_restore_exposure(ops->dev, ops->exp_cache, ops->gain_cache,
					hdr, ops->exp_ctrl, ops->again_ctrl);
	if (ret) {
		dev_err(ops->dev, "fail to restore exposure/gain on mode change");
		return ret;
	}

	return 0;
}

/*
 * Leave STANDBY and wait for the internal regulator, the sequence every driver runs
 * when the switch is not skipping standby. reg/val differ per sensor.
 */
static inline int hailo_ft_cancel_standby(struct device *dev, u16 reg, u32 val,
					  hailo_ft_write_reg write_reg, void *sensor)
{
	int ret;

	ret = write_reg(sensor, reg, 1, val);
	if (ret) {
		dev_err(dev, "Failed to cancel standby on stream start: %d\n", ret);
		return ret;
	}

	/* 24 ms for internal regulator stabilization */
	usleep_range(24000, 25000);

	return 0;
}

/*
 * Which register list turns the "from" mode into the "to" mode. The lists stay
 * per-sensor: besides the differing values, they carry the 2DOL-only registers that
 * must be explicitly cleared on the way back, which the mode tables do not hold.
 */
struct hailo_ft_mode_delta {
	const void *from_regs;
	const void *to_regs;
	const void *delta;
};

static inline const void *hailo_ft_find_delta(const struct hailo_ft_mode_delta *table,
					      size_t num, const void *from_regs,
					      const void *to_regs)
{
	size_t i;

	if (!from_regs || !to_regs || from_regs == to_regs)
		return NULL;

	for (i = 0; i < num; i++)
		if (table[i].from_regs == from_regs && table[i].to_regs == to_regs)
			return table[i].delta;

	return NULL;
}

/*
 * Generic snapshot-and-restore for V4L2 controls across handler_setup.
 *
 * __v4l2_ctrl_handler_setup() iterates all controls and calls s_ctrl to push
 * cur.val to hardware. However, one control's s_ctrl may call
 * __v4l2_ctrl_modify_range() on another control as a side effect (e.g. VBLANK
 * resets exposure to default). This corrupts user-set values.
 *
 * The snapshot saves all writable integer control values before handler_setup,
 * then restores any that were changed (i.e. corrupted by side effects).
 */

#define HAILO_SENSOR_MAX_CTRLS 32

struct hailo_ctrl_snapshot_entry {
	struct v4l2_ctrl *ctrl;
	s32 saved_val;
};

struct hailo_ctrl_snapshot {
	struct hailo_ctrl_snapshot_entry entries[HAILO_SENSOR_MAX_CTRLS];
	int count;
};

/**
 * hailo_ctrl_snapshot_save() - Save current values of all writable integer controls
 * @hdl: V4L2 control handler
 * @snap: snapshot to fill
 *
 * Walks hdl->ctrls and saves cur.val for every non-read-only, non-button,
 * integer control (is_int flag set).
 */
static inline void hailo_ctrl_snapshot_save(struct v4l2_ctrl_handler *hdl,
					    struct hailo_ctrl_snapshot *snap)
{
	struct v4l2_ctrl *ctrl;

	snap->count = 0;
	list_for_each_entry(ctrl, &hdl->ctrls, node) {
		if (snap->count >= HAILO_SENSOR_MAX_CTRLS)
			break;
		/* Skip non-integer, read-only, and button controls */
		if (!ctrl->is_int)
			continue;
		if (ctrl->flags & V4L2_CTRL_FLAG_READ_ONLY)
			continue;
		if (ctrl->type == V4L2_CTRL_TYPE_BUTTON)
			continue;

		snap->entries[snap->count].ctrl = ctrl;
		snap->entries[snap->count].saved_val = ctrl->cur.val;
		snap->count++;
	}
}

/**
 * hailo_ctrl_snapshot_restore() - Restore controls that were changed during handler_setup
 * @snap: previously saved snapshot
 *
 * Compares saved values to current cur.val. For any control that changed
 * (corrupted by a side effect), restores via __v4l2_ctrl_s_ctrl which
 * validates and clamps to the current valid range.
 * Skips inactive controls as they cannot be written.
 */
static inline void hailo_ctrl_snapshot_restore(struct hailo_ctrl_snapshot *snap)
{
	int i;

	for (i = 0; i < snap->count; i++) {
		struct v4l2_ctrl *ctrl = snap->entries[i].ctrl;
		s32 saved = snap->entries[i].saved_val;

		if (ctrl->flags & V4L2_CTRL_FLAG_INACTIVE)
			continue;
		if (ctrl->cur.val != saved)
			__v4l2_ctrl_s_ctrl(ctrl, saved);
	}
}

/*
 * Push every control to hardware, bracketed by the snapshot so values corrupted as a
 * side effect of handler_setup (VBLANK's s_ctrl calling __v4l2_ctrl_modify_range on
 * exposure, for one) are put back.
 */
static inline int hailo_ft_push_controls(struct device *dev,
					 struct v4l2_ctrl_handler *hdl)
{
	struct hailo_ctrl_snapshot snap;
	int ret;

	hailo_ctrl_snapshot_save(hdl, &snap);

	ret = __v4l2_ctrl_handler_setup(hdl);
	if (ret) {
		dev_err(dev, "fail to setup handler (%d)", ret);
		return ret;
	}

	hailo_ctrl_snapshot_restore(&snap);

	return 0;
}

#endif // HAILO_SHARED_SENSOR_DATA_H