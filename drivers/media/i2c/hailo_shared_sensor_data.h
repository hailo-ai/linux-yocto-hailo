#ifndef HAILO_SHARED_SENSOR_DATA_H
#define HAILO_SHARED_SENSOR_DATA_H

// sensor specific IDs
#include "sensor_id.h"

#include <media/v4l2-ctrls.h>

// Fast toggle data - copied from hailo15/common.h
// we do not include it, as the sensor drivers should be standalone
#define HAILO15_INTERNAL_SENSOR_FAST_TOGGLE_SET_STATUS	_IOW('K', BASE_VIDIOC_PRIVATE + 1, int)

enum fast_toggle_state {
	FAST_TOGGLE_NONE,
	FAST_TOGGLE_TEARDOWN,
	FAST_TOGGLE_PRIMING,
	FAST_TOGGLE_APPLY_PRIMING,
	FAST_TOGGLE_ACTIVE,
	FAST_TOGGLE_STATE_MAX,
};

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

#endif // HAILO_SHARED_SENSOR_DATA_H