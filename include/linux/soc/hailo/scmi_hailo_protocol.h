/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef SCMI_HAILO_PROTOCOL_H
#define SCMI_HAILO_PROTOCOL_H

#ifdef __KERNEL__
#    include <linux/types.h>
#else
#    include <stdbool.h>
#    include <stdint.h>
#    ifndef __packed
#        define __packed __attribute__((packed))
#    endif
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
#define SCMI_HAILO_NOC_START_MEASURE_ID 7
#define SCMI_HAILO_NOC_STOP_MEASURE_ID 8
#define SCMI_HAILO_BOOT_SUCCESS_INDICATION_ID 9
#define SCMI_HAILO_SWUPDATE_INDICATION_ID 10
#define SCMI_HAILO_SEND_COMPONENTS_VERSION 11
#define SCMI_HAILO_SET_I2S_SOURCE_CLK_ID 12
#define SCMI_HAILO_SET_SPI_INTERRUPT_FORWARDING_ID 13
#define SCMI_HAILO_GET_MBIST_SUBSERVERS_STATUS_ID 14

/*******************************
 * SCMI-Hailo notification IDs *
 *******************************/

enum scmi_hailo_notification_id {
    SCMI_HAILO_NOC_MEASUREMENT_TRIGGER_NOTIFICATION_ID = 0,
    SCMI_HAILO_NOC_MEASUREMENT_ENDED_NOTIFICATION_ID = 1,
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

/****************************************************
 * SCMI-Hailo get boot info image mode enum *
 ****************************************************/

typedef enum {
    SCMI_HAILO_BOOT_INFO_IMAGE_MODE_NORMAL = 0,
    SCMI_HAILO_BOOT_INFO_IMAGE_MODE_REMOTE_UPDATE = 1,
    SCMI_HAILO_BOOT_INFO_IMAGE_MODE_MAX = 7
} scmi_hailo_boot_info_image_mode_t;

struct scmi_hailo_get_boot_info_p2a {
    struct { // boot status bitmap - each bit set indicates boot success for corresponding component
        uint8_t boot_success_scu_bl : 1;
        uint8_t boot_success_scu_fw : 1;
        uint8_t boot_success_ap_bootloader : 1;
        uint8_t boot_success_ap_software : 1;
        uint8_t reserved : 4;
    } boot_status_bitmap;

    uint8_t boot_count; // Boot attempts of the current image offset & source
    uint8_t active_image_desc_index; // Index of current image descriptor within image_descriptors table
    uint8_t active_boot_image_storage; // Active image storage location - according to the active image descriptor index
    uint32_t active_boot_image_offset; // Memory offset of the actual booted image
    uint8_t bootstrap_image_storage; // Image Storage location - according to hardware bootstrap pads
    uint8_t boot_image_mode; // Boot image mode - normal or remote update
    uint64_t scu_to_ap_timer_offset_ns; // Offset in nanoseconds between SCU and AP timers
} __packed;

struct scmi_hailo_mbist_subservers_status_p2a {
    uint32_t mbist_status_bitmask; // Bitmask of subservers that are currently running MBIST
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
 * NoC Start measure message definitions *
 *****************************************/

struct scmi_hailo_noc_start_measure_a2p_filter {
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

struct scmi_hailo_noc_start_measure_a2p {
    uint32_t sample_time_us;
    uint8_t after_trigger_percentage;
    struct {
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
        uint8_t is_freerunning : 1;
        uint8_t csm_enabled : 1;
        uint8_t dsm_rx_enabled : 1;
        uint8_t dsm_tx_enabled : 1;
        uint8_t reserved : 4;
    };
    uint8_t num_counters;

    struct scmi_hailo_noc_start_measure_a2p_filter filters[4];
} __packed;

/****************************************
 * NoC Stop measure message definitions *
 ***************************************/

struct scmi_hailo_noc_stop_measure_p2a {
    bool was_running;
} __packed;

/* none */

/****************************************************
 * NoC measurement trigger notification definitions *
 ****************************************************/

struct scmi_hailo_noc_measurement_trigger_notification {
    uint16_t sample_index;
} __packed;

/**************************************************
 * NoC measurement ended notification definitions *
 **************************************************/

struct scmi_hailo_noc_measurement_ended_notification {
    uint16_t sample_start_index;
    uint16_t sample_end_index;
} __packed;

/****************************************************
 * SCMI-Hailo boot success indication component IDs *
 ****************************************************/

enum scmi_hailo_boot_success_notification_id {
    SCMI_HAILO_BOOT_SUCCESS_COMPONENT_SCU_FW = 0,
    SCMI_HAILO_BOOT_SUCCESS_COMPONENT_AP_BOOTLOADER = 1,
    SCMI_HAILO_BOOT_SUCCESS_COMPONENT_AP_SOFTWARE = 2,
    SCMI_HAILO_BOOT_SUCCESS_COMPONENT_COUNT = 3
};

/***********************************************
 * Boot success indication message definitions *
 ***********************************************/
struct scmi_hailo_boot_success_indication_a2p {
    /* Boot success indication can be sent by U-Boot or Linux */
    uint8_t component; // 0 - SCU FW, 1 - AP bootloader (uboot), 2 - AP software (Linux)
    uint32_t component_version;
} __packed;

/***********************************************
 * Send components version message definitions *
 ***********************************************/

struct scmi_hailo_send_components_version_p2a {
    uint32_t scu_version;
    uint32_t uboot_version;
} __packed;

/***********************************************
 * Set I2S controller source clock             *
 ***********************************************/
struct scmi_hailo_set_i2s_source_clock_a2p {
    uint32_t i2s_ctrl;
    uint32_t source_clock;
} __packed;


/***********************************************
 * Set SPI Interrupt forwarding                *
 ***********************************************/
 struct scmi_hailo_set_spi_interrupt_forwarding_a2p {
    uint32_t interrupt_id;
    uint32_t forward; /* 0 - mask, 1 - forward to gic for AP handling */
} __packed;
#endif /* SCMI_HAILO_PROTOCOL_H */
