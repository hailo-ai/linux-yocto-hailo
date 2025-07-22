#ifndef __THERMAL_ENGINE_H
#define __THERMAL_ENGINE_H

#include <stdint.h>

#define MAX_TZ_NAME_LEN 20
#define MAX_NUM_OF_TZ 2
#define MAX_NUM_OF_TZ_TRIPS 10

#ifdef __cplusplus
extern "C" {
#endif

#define HAILO_THERMAL_ENGINE_DATA_PATH "/tmp/thermal_data.bin"
#define HAILO_THERMAL_ENGINE_SEM_PATH  "/thermal_data_sem"

enum trip_direction {
	TRIP_DIR_INVALID = 0,
	TRIP_DIR_UP = 1,
	TRIP_DIR_DOWN = 2,
};

struct hailo_tz_trip {
    int32_t id;
    int32_t type;
    int32_t temp;
    int32_t hyst;
    uint64_t last_cooling_ts;
    uint64_t last_heating_ts;
};

struct hailo_tz {
    char name[MAX_TZ_NAME_LEN];
    char governor[MAX_TZ_NAME_LEN];
    int32_t id;
    int32_t temp;
	int32_t num_trips;
    uint64_t last_trip_info; // Encoding: trip_id, trip_dir, trip_ts.
    struct hailo_tz_trip trip[MAX_NUM_OF_TZ_TRIPS];
};

struct hailo_thermal_data {
	int32_t num_zones;
    struct hailo_tz tz[MAX_NUM_OF_TZ];
};

#ifdef __cplusplus
}
#endif

#endif