#ifndef SCMI_HAILO_H
#define SCMI_HAILO_H

#include <linux/scmi_protocol.h>

int scmi_hailo_fs_init(struct scmi_device *sdev);
void scmi_hailo_fs_fini(struct scmi_device *sdev);

typedef struct __attribute__((packed)) {
    volatile uint32_t address;
    volatile uint32_t data;
} hailo_address_data_hook_t;

typedef union __attribute__((packed)) {
    uint32_t unique_id_0;
    struct {
        uint32_t fab_id : 1;
        uint32_t rev_product_number : 4;
        uint32_t rev_product_letter : 4;
        uint32_t version_ews_minor : 3;
        uint32_t version_ews_major : 3;
        uint32_t x_location : 6;
        uint32_t y_location : 6;
        uint32_t wafer_id : 5;
    };
} hailo_wafer_info_t;

typedef union __attribute__((packed)) {
    uint32_t unique_id_1;
    struct {
        uint32_t production_step : 8;
        uint32_t production_id : 8;
    };
} hailo_production_info_t;

typedef struct __attribute__((packed)) {
    volatile uint32_t crypto_dummy;
    volatile uint8_t lot_id[8];
    volatile hailo_wafer_info_t wafer_info;
    volatile hailo_production_info_t production_info;
    volatile uint8_t eth_mac_address[6];
    /* Bits 0-2: Version TP minor. */
    /* Bits 3-5: Version TP major. */
    /* Bits 10 : ROM SW flow usage bits. */
    volatile uint16_t diverse_bits;
    volatile uint32_t usb_id;
    /* Bits 0-1: Unused. */
    /* Bits 2-7: Cluster binning. */
    /* Bits 8-17: ROM SW flow usage bits. */
    /* Bits 18-20: SKUs. */
    /* Bits 21-28: Fuse outs. */
    /* Bit 29-31: Device Grade. */
    volatile uint32_t misc_bits;
    volatile hailo_address_data_hook_t address_data_hook[5];
    volatile uint32_t parity_row;
} hailo_user_fuse_t;

#define HAILO_USER_FUSE_BINNING_SHIFT (3)
#define HAILO_USER_FUSE_BINNING_MASK (0x1F)

#define HAILO_USER_FUSE_SKU_SHIFT (18)
#define HAILO_USER_FUSE_SKU_MASK (0x7)

#endif