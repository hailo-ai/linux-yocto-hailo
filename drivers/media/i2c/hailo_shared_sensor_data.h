#ifndef HAILO_SHARED_SENSOR_DATA_H
#define HAILO_SHARED_SENSOR_DATA_H

// sensor specific IDs
#include "sensor_id.h"

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

#endif // HAILO_SHARED_SENSOR_DATA_H