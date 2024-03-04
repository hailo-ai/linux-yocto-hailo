/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef SCMI_HAILO_PROTOCOL_H
#define SCMI_HAILO_PROTOCOL_H

#ifdef __KERNEL__
#	include <linux/types.h>
#else
#	include <stdbool.h>
#	include <stdint.h>
#	ifndef __packed
#		define __packed __attribute__((packed))
#	endif
#endif

/**************************
 * SCMI-Hailo message IDs *
 **************************/

#define SCMI_HAILO_PROTOCOL_VERSION_ID 0
#define SCMI_HAILO_PROTOCOL_ATTRIBUTES_ID 1
#define SCMI_HAILO_PROTOCOL_MESSAGE_ATTRIBUTES_ID 2
#define SCMI_HAILO_GET_BOOT_INFO_ID 3
#define SCMI_HAILO_CONFIGURE_ETH_DELAY_ID 4
#define SCMI_HAILO_GET_FUSE_INFO_ID 5
#define SCMI_HAILO_SET_ETH_RMII_MODE_ID 6
#define SCMI_HAILO_DDR_START_MEASURE_ID 7
#define SCMI_HAILO_DDR_STOP_MEASURE_ID 8

/*******************************
 * SCMI-Hailo notification IDs *
 *******************************/

enum scmi_hailo_notification_id {
	SCMI_HAILO_DDR_MEASUREMENT_TRIGGER_NOTIFICATION_ID = 0,
	SCMI_HAILO_DDR_MEASUREMENT_ENDED_NOTIFICATION_ID = 1,
	SCMI_HAILO_NOTIFICATION_COUNT = 2
};

/*********************************************
 * SCMI protocol version message definitions *
 *********************************************/

struct scmi_hailo_protocol_version_p2a {
	uint32_t version;
} __packed;

/************************************************
 * SCMI protocol attributes message definitions *
 ************************************************/

struct scmi_hailo_protocol_attributes_p2a {
	uint32_t attributes;
} __packed;

/***********************************************
 * SCMI message attributes message definitions *
 ***********************************************/

struct scmi_hailo_protocol_message_attributes_a2p {
	uint32_t message_id;
} __packed;

struct scmi_hailo_protocol_message_attributes_p2a {
	uint32_t attributes;
} __packed;

/*************************************
 * Get Boot Info message definitions *
 *************************************/

struct scmi_hailo_get_boot_info_p2a {
	uint8_t scu_bootstrap;
	uint32_t qspi_flash_ab_offset;
} __packed;

/************************************************
 * Configure ethernet delay message definitions *
 ************************************************/

struct scmi_hailo_eth_delay_configuration_a2p {
	uint8_t tx_bypass_clock_delay;
	uint8_t tx_clock_inversion;
	uint8_t tx_clock_delay;
	uint8_t rx_bypass_clock_delay;
	uint8_t rx_clock_inversion;
	uint8_t rx_clock_delay;
} __packed;

/*************************************
 * Get fuse info message definitions *
 *************************************/

struct scmi_hailo_user_fuse {
#define SCMI_HAILO_PROTOCOL_USER_FUSE_DATA_SIZE (80)
	uint8_t user_fuse_array[SCMI_HAILO_PROTOCOL_USER_FUSE_DATA_SIZE];
} __packed;

struct scmi_hailo_get_fuse_info_p2a {
	struct scmi_hailo_user_fuse user_fuse;
	uint32_t active_clusters;
} __packed;

/*****************************************
 * Set eth rmii mode message definitions *
 *****************************************/

/* none */

/*****************************************
 * DDR Start measure message definitions *
 *****************************************/

struct scmi_hailo_ddr_start_measure_a2p_filter {
	/* entry[0] */
	struct {
		unsigned int window_size : 6;
		unsigned int status : 2;
		unsigned int length : 4;
		unsigned int opcode : 4;
		unsigned int addrbase_high : 4;
		unsigned int urgency : 3;
		unsigned int reserved : 8;
		unsigned int total : 1;
	};

	/* entry[1] */
	uint32_t routeidbase;

	/* entry[2] */
	uint32_t routeidmask;

	/* entry[3] */
	uint32_t addrbase_low;
};

struct scmi_hailo_ddr_start_measure_a2p {
	uint32_t sample_time_us;
	uint8_t after_trigger_percentage;
	/*
	There are 2 running modes:
	- freerunning: the counters start when the measurement starts,
		and keep increasing until the measurement it stopped.
		This means that between samples, the counters only increse.
		This also means that the counters may reach 0xFFFFFFFF and
		won't be able to update again.
	- gtimer: the counters will zero out in each sample.
		This is done by using a hardware feature that stops the counting
		after a certain amount of clock cycles that we configure.
		This mode is more convenient but has the down side that
		we don't have a way to automatically restart the counting,
		so there is a period of time (let's say 3us) between every
		2 samples where we don't measure.
	*/
	uint8_t is_freerunning;
	uint8_t num_counters;

	struct scmi_hailo_ddr_start_measure_a2p_filter filters[4];
} __packed;

/****************************************
 * DDR Stop measure message definitions *
 ***************************************/

struct scmi_hailo_ddr_stop_measure_p2a {
	bool was_running;
} __packed;

/* none */

/****************************************************
 * DDR measurement trigger notification definitions *
 ****************************************************/

struct scmi_hailo_ddr_measurement_trigger_notification {
	uint16_t sample_index;
} __packed;

/**************************************************
 * DDR measurement ended notification definitions *
 **************************************************/

struct scmi_hailo_ddr_measurement_ended_notification {
	uint16_t sample_start_index;
	uint16_t sample_end_index;
} __packed;

#endif /* SCMI_HAILO_PROTOCOL_H */
