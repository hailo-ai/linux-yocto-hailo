// SPDX-License-Identifier: GPL-2.0-only
/*
 * Hailo's Ltd modified thermal monitoring tool based on the thermal netlink events.
 *
 * Copyright (C) 2025 Hailo Ltd.
 *
 * Note: Based on original thermal-engine written by Daniel Lezcano <daniel.lezcano@kernel.org>
 *       This version is modified to support Hailo thermal monitoring.
 */

#include "hailo-thermal-engine.h"
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <stdint.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <thermal.h>
#include "thermal-tools.h"

/* write to trace marker */
static int trace_marker_fd = -1;

static void trace_marker_init(void)
{
	/* Try debugfs-based tracefs (older kernels), then standalone tracefs */
	trace_marker_fd = open("/sys/kernel/debug/tracing/trace_marker",
			       O_WRONLY | O_CLOEXEC);
}

static void trace_marker_close(void)
{
	if (trace_marker_fd >= 0) {
		close(trace_marker_fd);
		trace_marker_fd = -1;
	}
}

static void trace_marker_counter(const char *name, int value)
{
	char buf[128];
	int len;

	if (trace_marker_fd < 0)
		return;

	len = snprintf(buf, sizeof(buf), "C|%d|%s|%d", getpid(), name, value);
	(void)write(trace_marker_fd, buf, len);
}

struct options {
	int loglevel;
	int logopt;
	int interactive;
	int daemonize;
};

struct thermal_data {
	struct thermal_zone *tz;
	struct thermal_handler *th;
};

struct hailo_thermal_ctx {
	struct hailo_thermal_data *hailo_td;
	sem_t *sem;
	/* Track which trips are currently active (crossed up) per zone.
	 * Used to emit only the highest active trip,
	 * avoiding sawtooth when multiple trips fire in one update cycle. */
	bool throttle_active[MAX_NUM_OF_TZ][MAX_NUM_OF_TZ_TRIPS];
} ctx;

/*
 * Find the highest active trip for a zone.
 * Returns the trip index, or -1 if no trips are active.
 */
static int get_highest_active_throttle(int tz_id)
{
	int i;

	for (i = MAX_NUM_OF_TZ_TRIPS - 1; i >= 0; i--) {
		if (ctx.throttle_active[tz_id][i])
			return i;
	}
	return -1;
}

/*
 * Emit the current thermal state to trace_marker.
 * The "Throttling State" counter value is highest_active_throttle + 1, so:
 *   0 = no throttle active (full performance)
 *   1 = throttle 0 active
 *   2 = throttle 1 active
 *   ... etc.
 */
static void emit_throttle_state(int tz_id, int temp)
{
	char name[64];
	int highest = get_highest_active_throttle(tz_id);

	snprintf(name, sizeof(name), "Throttling State (Sensor %d)", tz_id);
	trace_marker_counter(name, highest + 1);
	snprintf(name, sizeof(name), "Event Temperature mC (Sensor %d)", tz_id);
	trace_marker_counter(name, temp);
}

/*! 
 * @brief Get monotonic time in msec
 * @note This is not affected by system time changes
 */
static uint64_t get_monotonic_time_msec(void)
{
	struct timespec ts;

	// CLOCK_MONOTONIC is not affected by system time changes
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == -1) {
		perror("clock_gettime");
		return -1;
	}

	return (uint64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void hailo_thermal_data_dump(struct hailo_thermal_data *data)
{
	int i, z;

	for (z = 0; z < data->num_zones; z++) {
		struct hailo_tz *tz = &data->tz[z];
		DEBUG("tz[%d]: %s\n", tz->id, tz->name);
		DEBUG("\t- governer %s\n", tz->governor);
		DEBUG("\t- temp %d\n", tz->temp);
		for (i = 0; i < tz->num_trips; i++) {
			struct hailo_tz_trip *tt = &tz->trip[i];
			DEBUG("\t\t- trip[%d]: type %d, temp %d, hyst %d: last_cooling_ts[%19lu] msec, last_heating_ts[%19lu] msec\n",
				   tt->id,
				   tt->type,
			       tt->temp,
			       tt->hyst,
			       tt->last_cooling_ts,
			       tt->last_heating_ts);
		}
	}
}

static int hailo_tz_trip_init(struct thermal_trip *tt, __maybe_unused void *arg)
{
	struct hailo_tz *hailo_tz = arg;
	struct hailo_tz_trip *hailo_tt = &hailo_tz->trip[tt->id];

	// Initialize thermal zones
	hailo_tz->num_trips++;

	hailo_tt->id = tt->id;
	hailo_tt->type = tt->type;
	hailo_tt->temp = tt->temp;
	hailo_tt->hyst = tt->hyst;
	hailo_tt->last_cooling_ts = 0;
	hailo_tt->last_heating_ts = 0;

	return 0;
}

static int hailo_tz_init(struct thermal_zone *tz, __maybe_unused void *arg)
{
	struct hailo_thermal_data *hailo_td = arg;
	struct hailo_tz *hailo_tz = &hailo_td->tz[tz->id];

	// Initialize thermal zones
	snprintf(hailo_tz->name, MAX_TZ_NAME_LEN, "%s", tz->name);
	snprintf(hailo_tz->governor, MAX_TZ_NAME_LEN, "%s", tz->governor);
	hailo_tz->id = tz->id;
	hailo_tz->temp = tz->temp;

	hailo_td->num_zones++;

	for_each_thermal_trip(tz->trip, hailo_tz_trip_init, hailo_tz);

	return 0;
}

static void hailo_thermal_data_init(struct hailo_thermal_data *hailo_td, struct thermal_data *td)
{
	int i, z;
	memset(hailo_td, 0, sizeof(struct hailo_thermal_data));

	hailo_td->num_zones = 0;
	for (z = 0; z < MAX_NUM_OF_TZ; z++) {
		struct hailo_tz *hailo_tz = &hailo_td->tz[z];

		hailo_tz->id = -1;
		for (i = 0; i < MAX_NUM_OF_TZ_TRIPS; i++) {
			struct hailo_tz_trip *hailo_tt = &hailo_tz->trip[i];
			hailo_tt->id = -1;
		}
	}

	for_each_thermal_zone(td->tz, hailo_tz_init, hailo_td);

	hailo_thermal_data_dump(hailo_td);

	INFO("Hailo thermal data initialize successfully.\n");
}

static sem_t* hailo_thermal_sem_open(char *name)
{
	sem_t *sem;

	if(!name) {
		printf("Invalid semaphore name\n");
		return NULL;
	}

	sem = sem_open(name, O_CREAT, 0666, 0);
	if (sem == SEM_FAILED) {
		perror("Failed to open semaphore");
		return NULL;
	}
	return sem;
}

static void* hailo_thermal_data_mmap(char *file_path)
{
	size_t map_size = sizeof(struct hailo_thermal_data);
	void *shared_mem;
	int rc;

	int fd = open(file_path, O_CREAT | O_RDWR, 0644);
	if (fd < 0) {
		printf("Failed to open file %s, err %d\n", file_path, fd);
		return NULL;
	}
	rc = ftruncate(fd, map_size);
	if (rc < 0) {
		printf("Failed to truncate file %s, err %d\n", file_path, rc);
		close(fd);
		return NULL;
	}

	shared_mem = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (shared_mem == MAP_FAILED) {
		printf("Failed to mmap file %s, err %d\n", file_path, errno);
		return NULL;
	}
	close(fd);

	return shared_mem;
}

static void hailo_thermal_data_unmmap(void* shared_mem)
{
	munmap(shared_mem, sizeof(struct hailo_thermal_data) + 8);
}

void hailo_thermal_init(char *file_path, char *sem_path, struct thermal_data *td)
{
	ctx.sem = hailo_thermal_sem_open(sem_path);
	if (!ctx.sem) {
		exit(EXIT_FAILURE);
	}
	ctx.hailo_td = hailo_thermal_data_mmap(file_path);
	if (!ctx.hailo_td) {
		exit(EXIT_FAILURE);
	}
	hailo_thermal_data_init(ctx.hailo_td, td);
	trace_marker_init();
}

void hailo_thermal_uninit(void)
{
	trace_marker_close();
	hailo_thermal_data_unmmap(ctx.hailo_td);
	sem_close(ctx.sem);
	sem_unlink(HAILO_THERMAL_ENGINE_SEM_PATH);
}

static int show_trip(struct thermal_trip *tt, __maybe_unused void *arg)
{
	INFO("trip id=%d, type=%d, temp=%d, hyst=%d\n", tt->id, tt->type,
	     tt->temp, tt->hyst);

	return 0;
}

static int show_temp(struct thermal_zone *tz, __maybe_unused void *arg)
{
	thermal_cmd_get_temp(arg, tz);

	INFO("temperature: %d\n", tz->temp);

	return 0;
}

static int show_governor(struct thermal_zone *tz, __maybe_unused void *arg)
{
	thermal_cmd_get_governor(arg, tz);

	INFO("governor: '%s'\n", tz->governor);

	return 0;
}

static int show_tz(struct thermal_zone *tz, __maybe_unused void *arg)
{
	INFO("thermal zone '%s', id=%d\n", tz->name, tz->id);

	for_each_thermal_trip(tz->trip, show_trip, NULL);

	show_temp(tz, arg);

	show_governor(tz, arg);

	return 0;
}

static int tz_create(const char *name, int tz_id, __maybe_unused void *arg)
{
	INFO("Thermal zone '%s'/%d created\n", name, tz_id);

	return 0;
}

static int tz_delete(int tz_id, __maybe_unused void *arg)
{
	INFO("Thermal zone %d deleted\n", tz_id);

	return 0;
}

static int tz_disable(int tz_id, void *arg)
{
	struct thermal_data *td = arg;
	struct thermal_zone *tz = thermal_zone_find_by_id(td->tz, tz_id);

	INFO("Thermal zone %d ('%s') disabled\n", tz_id, tz->name);

	return 0;
}

static int tz_enable(int tz_id, void *arg)
{
	struct thermal_data *td = arg;
	struct thermal_zone *tz = thermal_zone_find_by_id(td->tz, tz_id);

	INFO("Thermal zone %d ('%s') enabled\n", tz_id, tz->name);

	return 0;
}

static int trip_high(int tz_id, int trip_id, int temp, void *arg)
{
	struct thermal_data *td = arg;
	struct thermal_zone *tz = thermal_zone_find_by_id(td->tz, tz_id);
	uint64_t trip_ts;

	INFO("Thermal zone %d ('%s'): trip point %d crossed way up with %d °C\n",
		tz_id, tz->name, trip_id, temp);

	if (tz_id > ctx.hailo_td->num_zones) {
		printf("invalid thermal zone id %d\n", tz_id);
		return -EINVAL;
	}
	
	trip_ts = get_monotonic_time_msec();
	ctx.hailo_td->tz[tz_id].temp = temp;
	ctx.hailo_td->tz[tz_id].trip[trip_id].last_heating_ts = trip_ts;

	hailo_thermal_data_dump(ctx.hailo_td);

	sem_post(ctx.sem);

	ctx.throttle_active[tz_id][trip_id] = true;
	emit_throttle_state(tz_id, temp);

	return 0;
}


static int trip_low(int tz_id, int trip_id, int temp, void *arg)
{
	struct thermal_data *td = arg;
	struct thermal_zone *tz = thermal_zone_find_by_id(td->tz, tz_id);
	uint64_t trip_ts;
	
	INFO("Thermal zone %d ('%s'): trip point %d crossed way down with %d °C\n",
	     tz_id, tz->name, trip_id, temp);

	if (tz_id > ctx.hailo_td->num_zones) {
		printf("invalid thermal zone id %d\n", tz_id);
		return -EINVAL;
	}
	
	trip_ts = get_monotonic_time_msec();
	ctx.hailo_td->tz[tz_id].temp = temp;
	//ctx.hailo_td->tz[tz_id].last_trip_info = TRIP_INFO_SET(trip_id, TRIP_DIR_DOWN, trip_ts);
	ctx.hailo_td->tz[tz_id].trip[trip_id].last_cooling_ts = trip_ts;

	hailo_thermal_data_dump(ctx.hailo_td);

	sem_post(ctx.sem);

	ctx.throttle_active[tz_id][trip_id] = false;
	emit_throttle_state(tz_id, temp);

	return 0;
}

static int trip_add(int tz_id, int trip_id, int type, int temp, int hyst,
		    __maybe_unused void *arg)
{
	INFO("Trip point added %d: id=%d, type=%d, temp=%d, hyst=%d\n", tz_id,
	     trip_id, type, temp, hyst);

	return 0;
}

static int trip_delete(int tz_id, int trip_id, __maybe_unused void *arg)
{
	INFO("Trip point deleted %d: id=%d\n", tz_id, trip_id);

	return 0;
}

static int trip_change(int tz_id, int trip_id, int type, int temp, int hyst,
		       __maybe_unused void *arg)
{
	struct thermal_data *td = arg;
	struct thermal_zone *tz = thermal_zone_find_by_id(td->tz, tz_id);

	INFO("Trip point changed %d: id=%d, type=%d, temp=%d, hyst=%d\n", tz_id,
	     trip_id, type, temp, hyst);

	tz->trip[trip_id].type = type;
	tz->trip[trip_id].temp = temp;
	tz->trip[trip_id].hyst = hyst;

	return 0;
}

static int cdev_add(const char *name, int cdev_id, int max_state,
		    __maybe_unused void *arg)
{
	INFO("Cooling device '%s'/%d (max state=%d) added\n", name, cdev_id,
	     max_state);

	return 0;
}

static int cdev_delete(int cdev_id, __maybe_unused void *arg)
{
	INFO("Cooling device %d deleted", cdev_id);

	return 0;
}

static int cdev_update(int cdev_id, int cur_state, __maybe_unused void *arg)
{
	INFO("cdev:%d state:%d\n", cdev_id, cur_state);

	return 0;
}

static int gov_change(int tz_id, const char *name, __maybe_unused void *arg)
{
	struct thermal_data *td = arg;
	struct thermal_zone *tz = thermal_zone_find_by_id(td->tz, tz_id);

	INFO("%s: governor changed %s -> %s\n", tz->name, tz->governor, name);

	strcpy(tz->governor, name);

	return 0;
}

static struct thermal_ops ops = { 
	.events.tz_create = tz_create,
	.events.tz_delete = tz_delete,
	.events.tz_disable = tz_disable,
	.events.tz_enable = tz_enable,
	.events.trip_high = trip_high,
	.events.trip_low = trip_low,
	.events.trip_add = trip_add,
	.events.trip_delete = trip_delete,
	.events.trip_change = trip_change,
	.events.cdev_add = cdev_add,
	.events.cdev_delete = cdev_delete,
	.events.cdev_update = cdev_update,
	.events.gov_change = gov_change
};

static int thermal_event(__maybe_unused int fd, __maybe_unused void *arg)
{
	struct thermal_data *td = arg;

	return thermal_events_handle(td->th, td);
}

static void usage(const char *cmd)
{
	printf("%s : A thermal monitoring engine based on notifications\n",
	       cmd);
	printf("Usage: %s [options]\n", cmd);
	printf("\t-h, --help\t\tthis help\n");
	printf("\t-d, --daemonize\n");
	printf("\t-l <level>, --loglevel <level>\tlog level: ");
	printf("DEBUG, INFO, NOTICE, WARN, ERROR\n");
	printf("\t-s, --syslog\t\toutput to syslog\n");
	printf("\n");
	exit(0);
}

static int options_init(int argc, char *argv[], struct options *options)
{
	int opt;

	struct option long_options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "daemonize", no_argument, NULL, 'd' },
		{ "syslog", no_argument, NULL, 's' },
		{ "loglevel", required_argument, NULL, 'l' },
		{ 0, 0, 0, 0 }
	};

	while (1) {
		int optindex = 0;

		opt = getopt_long(argc, argv, "l:dhs", long_options, &optindex);
		if (opt == -1)
			break;

		switch (opt) {
		case 'l':
			options->loglevel = log_str2level(optarg);
			break;
		case 'd':
			options->daemonize = 1;
			break;
		case 's':
			options->logopt = TO_SYSLOG;
			break;
		case 'h':
			usage(basename(argv[0]));
			break;
		default: /* '?' */
			return -1;
		}
	}

	return 0;
}

enum {
	THERMAL_ENGINE_SUCCESS = 0,
	THERMAL_ENGINE_OPTION_ERROR,
	THERMAL_ENGINE_DAEMON_ERROR,
	THERMAL_ENGINE_LOG_ERROR,
	THERMAL_ENGINE_THERMAL_ERROR,
	THERMAL_ENGINE_MAINLOOP_ERROR,
};

int main(int argc, char *argv[])
{
	struct thermal_data td;
	struct options options = {
		.loglevel = LOG_INFO,
		.logopt = TO_STDOUT,
	};

	if (options_init(argc, argv, &options)) {
		ERROR("Usage: %s --help\n", argv[0]);
		return THERMAL_ENGINE_OPTION_ERROR;
	}

	if (options.daemonize && daemon(0, 0)) {
		ERROR("Failed to daemonize: %p\n");
		return THERMAL_ENGINE_DAEMON_ERROR;
	}

	if (log_init(options.loglevel, basename(argv[0]), options.logopt)) {
		ERROR("Failed to initialize logging facility\n");
		return THERMAL_ENGINE_LOG_ERROR;
	}

	td.th = thermal_init(&ops);
	if (!td.th) {
		ERROR("Failed to initialize the thermal library\n");
		return THERMAL_ENGINE_THERMAL_ERROR;
	}

	td.tz = thermal_zone_discover(td.th);
	if (!td.tz) {
		ERROR("No thermal zone available\n");
		return THERMAL_ENGINE_THERMAL_ERROR;
	}

	for_each_thermal_zone(td.tz, show_tz, td.th);

	hailo_thermal_init(HAILO_THERMAL_ENGINE_DATA_PATH, HAILO_THERMAL_ENGINE_SEM_PATH, &td);

	if (mainloop_init()) {
		ERROR("Failed to initialize the mainloop\n");
		hailo_thermal_uninit();
		return THERMAL_ENGINE_MAINLOOP_ERROR;
	}

	if (mainloop_add(thermal_events_fd(td.th), thermal_event, &td)) {
		ERROR("Failed to setup the mainloop\n");
		hailo_thermal_uninit();
		return THERMAL_ENGINE_MAINLOOP_ERROR;
	}

	INFO("Waiting for thermal events ...\n");

	if (mainloop(-1)) {
		ERROR("Mainloop failed\n");
		hailo_thermal_uninit();
		return THERMAL_ENGINE_MAINLOOP_ERROR;
	}

	hailo_thermal_uninit();

	return THERMAL_ENGINE_SUCCESS;
}
