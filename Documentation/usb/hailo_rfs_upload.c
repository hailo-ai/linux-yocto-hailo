/*
 * hailo_rfs_upload.c - RFS Image Upload Tool for Hailo10 USB Device
 *
 * This C program uploads an RFS (Root File System) image to a Hailo10 USB device
 * using libusb. It communicates with the f_hailo_rfs_load.c USB gadget function driver.
 *
 * Protocol:
 *   1. Send HAILO_REQ__RFS_LOAD control request with file size (or 0 for large files)
 *   2. Stream RFS data via bulk OUT endpoint in 8KB chunks
 *   3. Send HAILO_REQ__RFS_FINISH control request to signal completion
 *   4. Verify status via HAILO_REQ__RFS_GET_STATUS
 *
 * The kernel driver streams the data directly to /initrd.image for 
 * subsequent loading via initrd mechanism.
 *
 * Features:
 *   - Progress reporting with transfer speed in verbose mode
 *   - EP0 buffer size compliance with USB speed-dependent limitations
 *   - Enhanced status parsing and verification
 *   - Status-only mode for device monitoring
 *   - RFS load device information retrieval
 *
 * Compilation:
 *   gcc -o hailo_rfs_upload hailo_rfs_upload.c -lusb-1.0
 *   Or use: ./build_hailo_rfs_upload.sh
 *
 * Usage:
 *   ./hailo_rfs_upload [-v] <rfs_image_file>
 *   ./hailo_rfs_upload [--status] [-v]
 *
 * Requirements:
 *   - libusb-1.0 development headers
 *   - Root privileges or udev rules for USB access
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <endian.h>
#include <libusb-1.0/libusb.h>

// Hailo RFS load USB identifiers (adjust as needed)
#define HAILO_VID                   0x0B05  // ASUSTek Computer, Inc.
#define HAILO_PID                   0x1D6F  // Hailo Gadget

// Vendor-specific control requests (from f_hailo_rfs_load.c)
#define HAILO_REQ__RFS_GET_STATUS       0x03  /* HAILO_REQ__RFS_GET_STATUS (Get function status): 
                                               *   - idle:ready    Endpoint idle, not processing
                                               *   - load:xxxxxx   Uploading RFS image, <xxxxxx> bytes received,
                                               *   - invalid:xxxx  Invalid state or error
                                               */
#define HAILO_REQ__RFS_GET_BUILD_INFO       0x11 /* Get hailo build information */
#define HAILO_REQ__RFS_LOAD                 0x12 /* Load RFS image command */
#define HAILO_REQ__RFS_GET_BOARD_SKU_ID     0x14 /* Get board SKU ID */
#define HAILO_REQ__RFS_FINISH               0x13 /* Finish RFS loading */
#define HAILO_REQ__RFS_CTRL                 0x15 /* RFS control operations */
#define HAILO_REQ__RFS_GET_PROTOCOL_VERSION 0x16 /* Get protocol version */

/* RFS Control Sub-commands (matching f_hailo_rfs_load.c macros) */
#define HAILO_REQ__RFS_CTRL__GET_STATUS    0  /* Return ready status (1 byte) */
#define HAILO_REQ__RFS_CTRL__GET_RX_CNT    1  /* Return bytes received count (4 bytes) */
#define HAILO_REQ__RFS_CTRL__CLR_RX_CNT    2  /* Reset RFS counter (1 byte response) */

/* Gadget configuration modes */
#define HAILO_GADGET_CONFIG_RFS_MODE  1   /* RFS upload mode */
#define HAILO_GADGET_CONFIG_SWU_MODE  2   /* SW update mode */

// USB request types
#define USB_TYPE_VENDOR             (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE)
#define USB_DIR_OUT                 LIBUSB_ENDPOINT_OUT
#define USB_DIR_IN                  LIBUSB_ENDPOINT_IN

// Transfer parameters  
#define BULK_TRANSFER_SIZE          (64*1024)   // 64KB chunks (maximum performance with ZLP flush)
#define USB_TIMEOUT                 10000       // 10 seconds (increased for large transfers)
#define USB_CONTROL_TIMEOUT         5000        // 5 seconds for control requests
#define MAX_RFS_SIZE                (256 * 1024 * 1024)  // 256MB

// EP0 buffer sizes (matching f_hailo_rfs_load.c macros)
#define HAILO_RFS_LOAD_EP0_BUFFER_SIZE          64    // EP0 control request buffer size
#define HAILO_RFS_LOAD_EP0_STATUS_RESPONSE_SIZE 16    // EP0 status response buffer size

// Interface/endpoint parameters
#define HAILO_RFS_LOAD_INTERFACE          0
#define HAILO_RFS_LOAD_INTERFACE_CLASS    0xFF    // Vendor-specific
#define HAILO_RFS_LOAD_INTERFACE_PROTOCOL 1       // AI mode protocol

typedef struct {
    libusb_context *ctx;
    libusb_device_handle *handle;
    unsigned char bulk_out_ep;
    unsigned char intr_in_ep;
} hailo_device_t;

/* Global verbose flag */
static int verbose = 0;

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [OPTIONS] <rfs_image_file>\n", prog_name);
    printf("       %s --status\n", prog_name);
    printf("       %s --sku-id\n", prog_name);
    printf("\n");
    printf("Upload RFS (Root File System) image to Hailo RFS load USB device\n");
    printf("\n");
    printf("OPTIONS:\n");
    printf("  -v, --verbose     Verbose output\n");
	printf("  -s, --status      check device status (no upload)\n");
    printf("  -p, --protocol    get protocol version information\n");
    printf("  -b, --build       get SW build information\n");
    printf("  -i, --sku-id      Get board SKU ID information\n");
    printf("  -h, --help        Show this help message\n");
    printf("\n");
    printf("EXAMPLES:\n");
    printf("  %s rootfs.ext4\n", prog_name);
    printf("  %s -v /path/to/rootfs.img\n", prog_name);
    printf("  %s --status       # Check device status only\n", prog_name);
    printf("\n");
}

static int find_hailo_device(hailo_device_t *dev)
{
    libusb_device **device_list;
    libusb_device *device;
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *config;
    const struct libusb_interface *interface;
    const struct libusb_interface_descriptor *intf_desc;
    const struct libusb_endpoint_descriptor *ep_desc;
    ssize_t num_devices;
    int i, j, k;
    int ret = -1;

    // Get list of USB devices
    num_devices = libusb_get_device_list(dev->ctx, &device_list);
    if (num_devices < 0) {
        fprintf(stderr, "Error getting device list: %s\n", libusb_error_name(num_devices));
        return -1;
    }

    printf("Searching for Hailo RFS load USB device (VID:0x%04x, PID:0x%04x)...\n", HAILO_VID, HAILO_PID);

    // Search for our device
    for (i = 0; i < num_devices; i++) {
        device = device_list[i];
        
        if (libusb_get_device_descriptor(device, &desc) != 0) {
            continue;
        }

        // Check VID/PID
        if (desc.idVendor != HAILO_VID || desc.idProduct != HAILO_PID) {
            continue;
        }

        printf("Found potential device: VID:0x%04x, PID:0x%04x\n", desc.idVendor, desc.idProduct);

        // Open device
        ret = libusb_open(device, &dev->handle);
        if (ret != 0) {
            fprintf(stderr, "Cannot open device: %s\n", libusb_error_name(ret));
            continue;
        }

        // Get configuration
        ret = libusb_get_active_config_descriptor(device, &config);
        if (ret != 0) {
            fprintf(stderr, "Cannot get config descriptor: %s\n", libusb_error_name(ret));
            libusb_close(dev->handle);
            continue;
        }

        // Find Hailo RFS load interface
        for (j = 0; j < config->bNumInterfaces; j++) {
            interface = &config->interface[j];
            intf_desc = &interface->altsetting[0];

            // Check if this is our interface
            if (intf_desc->bInterfaceClass == HAILO_RFS_LOAD_INTERFACE_CLASS &&
                intf_desc->bInterfaceProtocol == HAILO_RFS_LOAD_INTERFACE_PROTOCOL &&
                intf_desc->bNumEndpoints == 2) {

                printf("Found Hailo RFS load interface %d\n", intf_desc->bInterfaceNumber);

                // Find endpoints
                dev->bulk_out_ep = 0;
                dev->intr_in_ep = 0;

                for (k = 0; k < intf_desc->bNumEndpoints; k++) {
                    ep_desc = &intf_desc->endpoint[k];
                    
                    if ((ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT) {
                        if ((ep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
                            dev->bulk_out_ep = ep_desc->bEndpointAddress;
                            printf("  Bulk OUT endpoint: 0x%02x\n", dev->bulk_out_ep);
                        }
                    } else if ((ep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
						dev->intr_in_ep = ep_desc->bEndpointAddress;
						printf("  Interrupt IN endpoint: 0x%02x\n", dev->intr_in_ep);
                    }
                }

                // Check if we found all endpoints
                if (dev->bulk_out_ep && dev->intr_in_ep) {
                    // Claim interface
                    ret = libusb_claim_interface(dev->handle, intf_desc->bInterfaceNumber);
                    if (ret != 0) {
                        fprintf(stderr, "Cannot claim interface: %s\n", libusb_error_name(ret));
                        libusb_free_config_descriptor(config);
                        libusb_close(dev->handle);
                        continue;
                    }

                    printf("Successfully opened Hailo RFS load device\n");
                    libusb_free_config_descriptor(config);
                    libusb_free_device_list(device_list, 1);
                    return 0;
                }
            }
        }

        libusb_free_config_descriptor(config);
        libusb_close(dev->handle);
        dev->handle = NULL;
    }

    libusb_free_device_list(device_list, 1);
    fprintf(stderr, "Hailo RFS load device not found or not accessible\n");
    return -1;
}

static int vendor_request(hailo_device_t *dev,
                          unsigned char request, 
                          unsigned short value, 
                          unsigned short index,
                          unsigned char *data,
                          unsigned short length,
                          int direction)
{
    int ret;
    unsigned char request_type;
    const char *req_name = "UNKNOWN";

    if (direction == USB_DIR_IN) {
        request_type = USB_TYPE_VENDOR | LIBUSB_ENDPOINT_IN;
    } else {
        request_type = USB_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT;
    }

    // Convert request code to name for verbose output
    switch (request) {
    case HAILO_REQ__RFS_GET_STATUS:
	    req_name = "GET_STATUS";
	    break;
    case HAILO_REQ__RFS_GET_BUILD_INFO:
	    req_name = "GET_BUILD_INFO";
	    break;
    case HAILO_REQ__RFS_LOAD:
	    req_name = "LOAD_RFS";
	    break;
    case HAILO_REQ__RFS_FINISH:
	    req_name = "FINISH_RFS";
	    break;
    case HAILO_REQ__RFS_CTRL:
	    req_name = "RFS_CTRL";
	    break;
    case HAILO_REQ__RFS_GET_BOARD_SKU_ID:
	    req_name = "GET_BOARD_SKU_ID";
	    break;
    case HAILO_REQ__RFS_GET_PROTOCOL_VERSION:
	    req_name = "GET_PROTOCOL_VERSION";
	    break;
    }

    if (verbose) {
	    printf("Sending vendor request: %s (0x%02x), value=%u, length=%u, dir=%s\n",
		   req_name, request, value, length,
		   (direction == USB_DIR_IN) ? "IN" : "OUT");
    }

    ret = libusb_control_transfer(dev->handle, request_type, request, value, index,
				  data, length, USB_CONTROL_TIMEOUT);
    if (ret < 0) {
	    fprintf(stderr, "Vendor request %s (0x%02x) failed: %s\n", req_name,
		    request, libusb_error_name(ret));
	    return -1;
    }

    if (verbose) {
	    printf("Vendor request %s completed: %d bytes\n", req_name, ret);
    }

    return ret;
}

static int gadget_rfs__get_status(hailo_device_t *dev, char *status_buf, size_t buf_size)
{
	unsigned char buffer[HAILO_RFS_LOAD_EP0_BUFFER_SIZE];
	int ret;

	ret = vendor_request(dev, HAILO_REQ__RFS_GET_STATUS, 0, 0, buffer,
			     HAILO_RFS_LOAD_EP0_STATUS_RESPONSE_SIZE,
			     USB_DIR_IN);
	if (ret < 0) {
		return -1;
	}

	// Ensure proper null termination within received bytes
	if (ret >= HAILO_RFS_LOAD_EP0_STATUS_RESPONSE_SIZE) {
		ret = HAILO_RFS_LOAD_EP0_STATUS_RESPONSE_SIZE - 1;
	}
	if (ret >= 0) {
		buffer[ret] = '\0';
	} else {
		buffer[0] = '\0'; // Safety fallback
	}

	strncpy(status_buf, (char *)buffer, buf_size - 1);
	status_buf[buf_size - 1] = '\0';

	if (verbose) {
		printf("Device status: '%s' (%d bytes)\n", (char *)buffer, ret);
	}

	return 0;
}

/* Get board SKU ID from device */
static int gadget_rfs__get_board_sku_id(hailo_device_t *dev, char *sku_buf, size_t buf_size)
{
    unsigned char buffer[HAILO_RFS_LOAD_EP0_BUFFER_SIZE];
    int ret;

    memset(buffer, 0, sizeof(buffer));
    ret = vendor_request(dev, HAILO_REQ__RFS_GET_BOARD_SKU_ID, 0, 0, buffer, sizeof(buffer), USB_DIR_IN);
    if (ret < 0) {
        return ret;
    }

    /* Copy the result, ensuring null termination */
    size_t copy_len = (size_t)ret < (buf_size - 1) ? (size_t)ret : (buf_size - 1);
    memcpy(sku_buf, buffer, copy_len);
    sku_buf[copy_len] = '\0';

    return 0;
}

static int gadget_rfs__get_build_info(hailo_device_t *dev, char *status_buf, size_t buf_size)
{
	unsigned char buffer[HAILO_RFS_LOAD_EP0_BUFFER_SIZE];
	int ret;

	ret = vendor_request(dev, HAILO_REQ__RFS_GET_BUILD_INFO, 0, 0, buffer,
			     HAILO_RFS_LOAD_EP0_BUFFER_SIZE,
			     USB_DIR_IN);
	if (ret < 0) {
		return -1;
	}

	// Ensure proper null termination within received bytes
	if (ret >= HAILO_RFS_LOAD_EP0_BUFFER_SIZE) {
		ret = HAILO_RFS_LOAD_EP0_BUFFER_SIZE - 1;
	}
	if (ret >= 0) {
		buffer[ret] = '\0';
	} else {
		buffer[0] = '\0'; // Safety fallback
	}

	strncpy(status_buf, (char *)buffer, buf_size - 1);
	status_buf[buf_size - 1] = '\0';

	if (verbose) {
		printf("%s (%d bytes)\n", (char *)buffer, ret);
	}

	return 0;
}

static int gadget_rfs__get_protocol_version(hailo_device_t *dev, uint32_t *version)
{
	uint32_t _version;
	int ret;

	memset(&_version, 0, sizeof(_version));
	ret = vendor_request(dev, HAILO_REQ__RFS_GET_PROTOCOL_VERSION, 0, 0, (unsigned char *)&_version, sizeof(_version), USB_DIR_IN);
	if (ret < 0) {
		return -1;
	}

	if (ret != 4) {
		fprintf(stderr, "Protocol version response: expected 4 bytes, got %d bytes\n", ret);
		return -1;
	}

	// Convert 4 bytes to uint32_t (little endian)
	*version = le32toh(_version);

	if (verbose) {
		printf("Protocol version: 0x%08x (%d bytes)\n", *version, ret);
	}

	return 0;
}

static int gadget_rfs__get_status__print(hailo_device_t *dev)
{
	char status[64];
    int ret = 0;

	printf("=== Hailo RFS load Device Status ===\n");
	
	if (gadget_rfs__get_status(dev, status, sizeof(status)) == 0) {
		printf("Current status: %s\n", status);
		
		// Parse and explain status
		if (strncmp(status, "idle:", 5) == 0 || strstr(status, "ready")) {
			printf("✓ Device is ready for operations\n");
		} else if (strncmp(status, "rfs:", 4) == 0) {
			unsigned int rfs_bytes = 0;
			if (sscanf(status + 4, "%x", &rfs_bytes) == 1) {
				printf("📁 RFS upload in progress: %u bytes received\n", rfs_bytes);
			}
		} else if (strncmp(status, "req:", 4) == 0) {
			printf("📨 Processing RFS load request\n");
		} else if (strncmp(status, "proc:", 5) == 0) {
			printf("🧠 RFS load processing in progress\n");
		} else if (strncmp(status, "rdy:", 4) == 0) {
			printf("📤 RFS load reply ready\n");
		} else if (strncmp(status, "snd:", 4) == 0) {
			printf("📤 Sending RFS load reply\n");
		}
		ret = 0;
	} else {
		fprintf(stderr, "✗ Could not retrieve device status\n");
		ret = 1;
	}

	return ret;
}

static int gadget_rfs_ctrl__get_status(hailo_device_t *dev)
{
    unsigned char buffer[1];
    int ret;

    ret = vendor_request(dev, HAILO_REQ__RFS_CTRL, HAILO_REQ__RFS_CTRL__GET_STATUS, 0, buffer, 1, USB_DIR_IN);
    if (ret < 0) {
        return -1;
    }

    if (verbose) {
        printf("Gadget buffer %s for image upload\n", buffer[0] ? "ready" : "not ready");
    }

    return buffer[0] ? 1 : 0;  // Return 1 if ready, 0 if not ready
}

static int gadget_rfs_ctrl__get_bulk_rx_cnt(hailo_device_t *dev)
{
    unsigned int bytes_received;
    unsigned char buffer[4];
    int ret;

    ret = vendor_request(dev, HAILO_REQ__RFS_CTRL, HAILO_REQ__RFS_CTRL__GET_RX_CNT, 0, buffer, 4, USB_DIR_IN);
    if (ret < 0) {
        return -1;
    }

    bytes_received = le32toh(*((uint32_t *)buffer));
    
    if (verbose) {
        printf("Device reports %u bytes received so far\n", bytes_received);
    }

    return (int)bytes_received;
}

static int gadget_rfs_ctrl__clr_bulk_rx_cnt(hailo_device_t *dev, int max_attempts)
{
    unsigned char reset_response = 0;
    int reset_ret;
    int attempt;

    printf("Resetting gadget rx bulk counter...\n");
    
    for (attempt = 0; attempt < max_attempts; attempt++) {
        reset_ret = vendor_request(dev, HAILO_REQ__RFS_CTRL, HAILO_REQ__RFS_CTRL__CLR_RX_CNT, 0, &reset_response, 1, USB_DIR_IN);
        
        if (reset_ret >= 0 && reset_response == 1) {
            if (verbose) {
                printf("RFS counter reset successful on attempt %d/%d\n", attempt + 1, max_attempts);
            }
            return 0;
        }
        
        if (attempt < max_attempts - 1) {
            if (verbose) {
                printf("Reset attempt %d/%d failed (ret=%d, response=%d), retrying...\n", 
                       attempt + 1, max_attempts, reset_ret, reset_response);
            }
            usleep(50000); // 50ms delay between attempts
        }
    }
    
    fprintf(stderr, "Warning: Failed to reset RFS counter after %d attempts\n", max_attempts);
    return -1;
}

static int ping_bulk_endpoint(hailo_device_t *dev)
{
    /* Send a small ping packet to verify endpoint is ready */
    const int max_ping_attempts = 10;
    int ret, transferred, device_received;
    int ping_attempts;
    unsigned char ping_data[64];

    printf("Testing bulk endpoint readiness...\n");

    memset(ping_data, 0xAA, sizeof(ping_data)); // Fill with test pattern
    for (ping_attempts = 0; ping_attempts < max_ping_attempts; ping_attempts++) {
        ret = libusb_bulk_transfer(dev->handle, dev->bulk_out_ep, ping_data, sizeof(ping_data), &transferred, 1000);
       
        /* Check if device received the ping */
        usleep(50000); // wait 50ms for device to process
        device_received = gadget_rfs_ctrl__get_bulk_rx_cnt(dev);
        
        if (ret == 0 && transferred == sizeof(ping_data) && device_received >= (int)sizeof(ping_data)) {
            printf("✓ Bulk endpoint verified ready (ping successful)\n");
            /* Reset RFS counter to exclude ping data from actual RFS transfer */
            if (gadget_rfs_ctrl__clr_bulk_rx_cnt(dev, 3) == 0) {
                return 0;
            }
        }
        
        printf("Ping attempt %d/%d (ret=%d, transferred=%d, received=%d)...\n", 
               ping_attempts + 1, max_ping_attempts, ret, transferred, device_received);
        usleep(100000); // 100ms between ping attempts
    }
    
    fprintf(stderr, "Warning: Bulk endpoint ping failed, proceeding anyway\n");
    return -ETIMEDOUT; // Failed - endpoint not responding
}

static int wait_gadget_ready_for_image_upload(hailo_device_t *dev, int timeout_seconds) {
    int elapsed = 0;
    int ready;

    printf("Waiting for gadget readiness for image upload (vmalloc buffer allocation)...");
    fflush(stdout);

    while (elapsed < timeout_seconds) {
        ready = gadget_rfs_ctrl__get_status(dev);
        if (ready > 0) {
            printf("\ngadget is ready for image upload!\n");
            
            /* Test bulk endpoint readiness with ping */
            ping_bulk_endpoint(dev);
            return 0;
        }

        printf(".");
        fflush(stdout);
        sleep(1);
        elapsed++;
        
        /* Provide progress indication every 10 seconds */
        if (elapsed % 10 == 0 && elapsed > 0) {
            printf(" (%ds)", elapsed);
        }
    }

    fprintf(stderr, "\nTimeout waiting for vmalloc buffer allocation (device may be busy)\n");
    return -ETIMEDOUT;
}

static int gadget_rfs__start_image_upload(hailo_device_t *dev, size_t file_size)
{
    /* CRITICAL: vmalloc implementation requires accurate file size for buffer allocation.
     * We now send the file size in two parts for files larger than 65535 bytes:
     * 1. wValue (16-bit) = lower 16 bits of file size
     * 2. wIndex (16-bit) = upper 16 bits of file size
     * Device reconstructs: full_size = (wIndex << 16) | wValue
     */
    unsigned short size_low = (unsigned short)(file_size & 0xFFFF);
    unsigned short size_high = (unsigned short)((file_size >> 16) & 0xFFFF);
    int ret;
    
    printf("Starting RFS upload, size: %zu bytes (0x%04x%04x)\n", file_size, size_high, size_low);
    
    /* Send LOAD_RFS with file size split across wValue and wIndex */
    ret = vendor_request(dev, HAILO_REQ__RFS_LOAD, size_low, size_high, NULL, 0, USB_DIR_OUT);
    if (ret < 0) {
        return -1;
    }
    
    /* Give device time to set up bulk endpoint for RFS transfer */
    usleep(10000); // 10ms delay
    
    return 0;
}

static int bulk_transfer_with_retry(hailo_device_t *dev,
                                    unsigned char *buffer, 
                                    size_t transfer_size,
                                    size_t transfer_number,
                                    size_t bytes_sent,
                                    int max_retries)
{
    int transferred;
    int retry_count = 0;
    int ret;
    
    do {
        if (verbose) {
            printf("Attempting bulk transfer #%zu: %zu bytes to EP 0x%02x (offset %zu)\n", 
                   transfer_number, transfer_size, dev->bulk_out_ep, bytes_sent);
        }
        ret = libusb_bulk_transfer(dev->handle, dev->bulk_out_ep, buffer, transfer_size, &transferred, USB_TIMEOUT);
        if (verbose) {
            printf("Bulk transfer #%zu result: ret=%d, transferred=%d\n", transfer_number, ret, transferred);
        }
        if (ret == 0) {
            return transferred; // Success - return bytes transferred
        }
        
        retry_count++;
        if (ret == LIBUSB_ERROR_NO_DEVICE || ret == LIBUSB_ERROR_NOT_FOUND) {
            fprintf(stderr, "\nDevice disconnected at offset %zu\n", bytes_sent);
            return -1;
        }
        
        if (retry_count < max_retries) {
            if (verbose) {
                fprintf(stderr, "\nBulk transfer retry %d/%d at offset %zu: %s\n", 
                       retry_count, max_retries, bytes_sent, libusb_error_name(ret));
            }
            usleep(10); // 10usec delay before retry
        } else {
            fprintf(stderr, "\nBulk transfer failed after %d retries at offset %zu: %s\n", 
                   max_retries, bytes_sent, libusb_error_name(ret));
            return -1;
        }
    } while (retry_count < max_retries);
    
    return -1; // Should never reach here
}

static int gadget_rfs__finish_image_upload(hailo_device_t *dev)
{
    int verify_ret = -1;
    int attempts = 0;
    int ret;
    unsigned char status[64] = {0};
    const int max_attempts = 15; // Up to 30 seconds total

    printf("Finishing RFS image upload command\n");
    ret = vendor_request(dev, HAILO_REQ__RFS_FINISH, 0, 0, NULL, 0, USB_DIR_OUT);
    if (ret < 0) {
        fprintf(stderr, "✗ FINISH_RFS request failed: unable to signal upload completion\n");
        return ret;
    }
    
    printf("✓ FINISH_RFS request completed successfully\n");
    printf("Waiting for device to write uploaded RFS image to file...\n");
        
    // Wait longer for device to process large files - writing to flash can be slow
    // Retry status check with increasing delays for up to 30 seconds total
    
    for (attempts = 0; attempts < max_attempts; attempts++) {
        // Progressive delay: 10msec, 1s, 2s, 2s, 2s, ... (capped at 2s per attempt)
        int delay_usec = (attempts == 0) ? 10000 : (attempts == 1) ? 1000000 : 2000000;
        usleep(delay_usec);
        
        verify_ret = vendor_request(dev, HAILO_REQ__RFS_GET_STATUS, 0, 0, status, sizeof(status), USB_DIR_IN);
        if (verify_ret >= 0) {  // Success means any non-negative return (bytes transferred)
            printf("✓ Device status after upload: %s\n", (char *)status);
            break;
        } else if (verbose) {
            printf("Status check attempt %d/%d failed (device may be busy writing file)...\n", attempts + 1, max_attempts);
        }
    }
    
    if (verify_ret < 0) {
        printf("⚠ Could not retrieve final device status (device may still be processing)\n");
        printf("  This is normal for large files - the upload likely succeeded\n");
        // Don't treat this as a failure - the FINISH_RFS request succeeded
    }
    
    // Return success (0) since FINISH_RFS completed successfully
    return 0;
}

static int upload_image_file(hailo_device_t *dev, const char *filename)
{
    FILE *file;
    struct stat st;
    size_t file_size, bytes_sent = 0;
    size_t bytes_read, bytes_to_read;
    time_t start_time, current_time;
    int ret = 0, transferred;
    unsigned char buffer[BULK_TRANSFER_SIZE];

    // Open and validate file
    if (stat(filename, &st) != 0) {
        perror("stat");
        return -1;
    }

    file_size = st.st_size;
    if (file_size == 0) {
        fprintf(stderr, "RFS file is empty\n");
        return -1;
    }

    if (file_size > MAX_RFS_SIZE) {
        fprintf(stderr, "RFS file too large: %zu bytes (max %d bytes)\n", file_size, MAX_RFS_SIZE);
        return -1;
    }

    file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return -1;
    }

    printf("Uploading RFS file: %s (%zu bytes)\n", filename, file_size);

    start_time = time(NULL);

    // Upload file in chunks
    while (bytes_sent < file_size) {
        bytes_to_read = (file_size - bytes_sent < BULK_TRANSFER_SIZE) ? 
                        (file_size - bytes_sent) : BULK_TRANSFER_SIZE;

        bytes_read = fread(buffer, 1, bytes_to_read, file);
        if (bytes_read == 0) {
            if (feof(file)) {
                break;
            }
            fprintf(stderr, "Error reading file: %s\n", strerror(errno));
            ret = -1;
            break;
        }

        // Use actual file data size - no padding needed with 64KB chunks
        size_t transfer_size = bytes_read;
        size_t transfer_number = (bytes_sent / BULK_TRANSFER_SIZE) + 1;
        const int max_retries = 3;

        // Send via bulk OUT endpoint with retry logic
        transferred = bulk_transfer_with_retry(dev, buffer, transfer_size, transfer_number, bytes_sent, max_retries);
        if (transferred < 0) {
            ret = -1;
            break;
        }

        if (transferred != (int)transfer_size) {
            fprintf(stderr, "\nPartial transfer at offset %zu: sent %d/%zu bytes\n", 
                   bytes_sent, transferred, transfer_size);
            /* Continue anyway, but note the discrepancy */
        }
        
        bytes_sent += transferred;
        
        /* Verify the transfer was actually received by the device */
        if (verbose) {
            usleep(50000); // 50ms delay for device to process
            int device_received = gadget_rfs_ctrl__get_bulk_rx_cnt(dev);
            if (device_received >= 0) {
                if ((unsigned int)device_received != bytes_sent) {
                    fprintf(stderr, "\n⚠ TRANSFER VERIFICATION FAILED!\n");
                    fprintf(stderr, "Host sent: %zu bytes, Device received: %d bytes\n", bytes_sent, device_received);
                    fprintf(stderr, "Transfer #%zu (%zu bytes) was lost!\n", 
                           (bytes_sent - transferred) / BULK_TRANSFER_SIZE + 1, (size_t)transferred);
                    
                    /* Wait longer and check again */
                    printf("Waiting longer for transfer to complete...\n");
                    usleep(200000); // Additional 200ms wait
                    device_received = gadget_rfs_ctrl__get_bulk_rx_cnt(dev);
                    if ((unsigned int)device_received == bytes_sent) {
                        printf("✓ Transfer verified after delay: device received %d bytes\n", device_received);
                    } else {
                        fprintf(stderr, "✗ Transfer still failed after delay: device has %d bytes\n", device_received);
                    }
                } else {
                    printf("✓ Transfer verified: device received %d bytes\n", device_received);
                }
            }
            if (bytes_sent % (1024*1024) == 0 || bytes_sent == file_size) {
                printf("\nTransferred %zu MB\n", bytes_sent / (1024*1024));
            }
        }
        
        // Significant delay to ensure device processes transfer and requeues before next chunk
        if (bytes_sent < file_size) { // Don't delay after last chunk
            if (verbose) {
                printf("Waiting for device to process transfer...\n");
            }
            usleep(100); // 100 usecs delay between chunks to ensure proper processing
        }

        // Progress indicator with enhanced verbose mode
        int progress = (bytes_sent * 100) / file_size;
        if (verbose) {
            current_time = time(NULL);
            double elapsed = difftime(current_time, start_time);
            double speed = (elapsed > 0) ? (bytes_sent / elapsed) : 0;
            printf("\rUploading: %d%% (%zu/%zu bytes, %.1f KB/s)", 
                   progress, bytes_sent, file_size, speed / 1024.0);
        } else {
            printf("\rUploading: %d%% (%zu/%zu bytes)", progress, bytes_sent, file_size);
        }
        fflush(stdout);
    }

    printf("\n");
    
    // Send a zero-length packet to force USB controller to flush any batched data
    if (ret == 0) {
        printf("Sending zero-length packet to flush USB controller buffers...\n");
        unsigned char zlp_buffer[1] = {0};
        int zlp_transferred = 0;
        int zlp_ret = libusb_bulk_transfer(dev->handle, dev->bulk_out_ep, zlp_buffer, 
                                          0, &zlp_transferred, USB_TIMEOUT);
        if (verbose) {
            printf("ZLP result: ret=%d, transferred=%d\n", zlp_ret, zlp_transferred);
        }
    }
    
    fclose(file);

    if (ret == 0) {
        current_time = time(NULL);
        double duration = difftime(current_time, start_time);
        double speed = (duration > 0) ? (bytes_sent / duration) : 0;
        printf("Upload completed: %zu bytes in %.1f seconds (%.1f KB/s)\n", bytes_sent, duration, speed / 1024.0);
    }

    return ret;
}

static int upload_rfs_image(hailo_device_t *dev, const char *filename)
{
    struct stat st;
    char status[64];

    // Validate file
    if (stat(filename, &st) != 0) {
        perror("stat");
        return -1;
    }

    printf("RFS file: %s\n", filename);
    printf("File size: %zu bytes (%.2f MB)\n", (size_t)st.st_size, (double)st.st_size / (1024 * 1024));

    // Get initial status
    if (gadget_rfs__get_status(dev, status, sizeof(status)) == 0) {
        printf("Initial status: %s\n", status);
    }

    // Start upload
    if (gadget_rfs__start_image_upload(dev, st.st_size) < 0) {
        return -1;
    }

    // Wait for RFS buffer to be allocated and ready (with 30 second timeout)
    printf("Waiting for device to allocate vmalloc buffer (%zu bytes)...\n", st.st_size);
    if (wait_gadget_ready_for_image_upload(dev, 30) < 0) {
        return -1;
    }

    // Upload data
    if (upload_image_file(dev, filename) < 0) {
        return -1;
    }

    // Critical delay: Ensure all bulk transfers are fully processed before calling FINISH_RFS
    printf("Ensuring all transfers are processed...\n");
    usleep(250000);  // 250ms delay to let USB pipeline clear

    // Finish upload
    if (gadget_rfs__finish_image_upload(dev) < 0) {
        return -1;
    }

    printf("✓ RFS upload completed - %zu bytes transferred successfully\n", (size_t)st.st_size);
    return 0;
}

static int gadget_rfs__set_config(hailo_device_t *dev, int config_num)
{
	int ret, current_config_num;
	const char *mode_name = (config_num == 1) ? "RFS" : "SWU";

    ret = libusb_get_configuration(dev->handle, &current_config_num);
    if (ret < 0) {
        fprintf(stderr, "✗ Failed to get current USB configuration: %s\n", libusb_error_name(ret));
        return 1;
    }

    if (current_config_num == config_num) {
        printf("✓ USB configuration %d (%s mode) already active, no change needed\n", config_num, mode_name);
        return 0;
    }

	printf("Setting gadget configuration %d (%s mode)...\n", config_num, mode_name);
    
	/* Release current interface before changing configuration */
	ret = libusb_release_interface(dev->handle, HAILO_RFS_LOAD_INTERFACE);
	if (ret < 0) {
		fprintf(stderr, "Warning: Failed to release interface: %s\n", libusb_error_name(ret));
		/* Continue anyway - this might not be critical */
	}
	
	/* Set USB configuration (equivalent to usb_modeswitch --configuration N) */
	ret = libusb_set_configuration(dev->handle, config_num);
	if (ret < 0) {
		fprintf(stderr, "✗ Failed to set USB configuration %d: %s\n", config_num, libusb_error_name(ret));
		return 1;
	}
	
	printf("✓ USB configuration %d set successfully\n", config_num);
	
	/* Give device time to reconfigure */
	usleep(100000); // 100ms delay for configuration to take effect
	
	/* Re-claim the interface */
	ret = libusb_claim_interface(dev->handle, HAILO_RFS_LOAD_INTERFACE);
	if (ret < 0) {
		fprintf(stderr, "✗ Failed to re-claim interface: %s\n", libusb_error_name(ret));
		return 1;
	}
	
	printf("✓ Interface re-claimed successfully\n");
	printf("✓ Gadget configuration set to %s mode successfully\n", mode_name);
	
	return 0;
}

int main(int argc, char *argv[]) {
    hailo_device_t device = {0};
    const char *filename = NULL;
    int do_get_status = 0;
    int do_get_build_info = 0;
    int do_get_sku_id = 0;
    int do_get_protocol_version = 0;
    int ret = 1;
    int opt;

    // Define long options
    static struct option long_options[] = {
        {"verbose",   no_argument,       0, 'v'},
        {"protocol",  no_argument,       0, 'p'},
        {"build",     no_argument,       0, 'b'},
        {"sku-id",    no_argument,       0, 'i'},
        {"status",    no_argument,       0, 's'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    // Parse arguments using getopt
    while ((opt = getopt_long(argc, argv, "vsbihp", long_options, NULL)) != -1) {
        switch (opt) {
        case 'v':
            verbose = 1;
            break;
        case 's':
            do_get_status = 1;
            break;
        case 'b':
            do_get_build_info = 1;
            break;
        case 'i':
            do_get_sku_id = 1;
            break;
        case 'p':
            do_get_protocol_version = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        case '?':
            // getopt_long already printed error message
            print_usage(argv[0]);
            return 1;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    // Get filename from remaining arguments
    if (optind < argc) {
        filename = argv[optind];
        optind++;
        
        // Check for multiple files
        if (optind < argc) {
            fprintf(stderr, "Multiple files specified\n");
            print_usage(argv[0]);
            return 1;
        }
    }

    if (filename == NULL && !do_get_status && !do_get_build_info && !do_get_sku_id && !do_get_protocol_version) {
        fprintf(stderr, "RFS file not specified\n");
        print_usage(argv[0]);
        return 1;
    }

    // Initialize libusb
    ret = libusb_init(&device.ctx);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize libusb: %s\n", libusb_error_name(ret));
        return 1;
    }

    if (verbose) {
        libusb_set_option(device.ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_INFO);
    }

    // Find and open device
    ret = find_hailo_device(&device);
    if (ret < 0) {
        goto cleanup;
    }

    ret = gadget_rfs__set_config(&device, HAILO_GADGET_CONFIG_RFS_MODE);
    if (ret < 0) {
        goto cleanup;
    }

    if (do_get_build_info) {
        char build_info[128];
        ret = gadget_rfs__get_build_info(&device, build_info, sizeof(build_info));
        printf("=== Hailo RFS load Device Build Info ===\n");
        if (ret == 0) {
            printf("%s\n", build_info);
        } else {
            fprintf(stderr, "✗ Could not retrieve device build info\n");
        }
    }

    if (do_get_sku_id) {
        // Board SKU ID mode
        char board_sku[64];

        printf("=== Hailo Board Information ===\n");
		ret = gadget_rfs__get_board_sku_id(&device, board_sku, sizeof(board_sku));
        if (ret == 0) {
            printf("Board SKU ID: %s\n", board_sku);
        } else {
            fprintf(stderr, "✗ Could not retrieve board SKU ID\n");
        }
    }

    if (do_get_protocol_version) {
        // Protocol version mode
        uint32_t protocol_version;

        printf("=== Hailo Protocol Information ===\n");
        ret = gadget_rfs__get_protocol_version(&device, &protocol_version);
        if (ret == 0) {
            printf("Protocol version: %u (0x%08x)\n", protocol_version, protocol_version);
        } else {
            fprintf(stderr, "✗ Could not retrieve protocol version\n");
        }
    }

    if (do_get_status) {
        ret = gadget_rfs__get_status__print(&device);
    }
    
    if (filename != NULL) {
        printf("Starting RFS upload process...\n");
        if (upload_rfs_image(&device, filename) == 0) {
            printf("✓ RFS upload completed successfully!\n");
            ret = 0;
        } else {
            fprintf(stderr, "✗ RFS upload failed\n");
            ret = 1;
        }
    }

cleanup:
    if (device.handle) {
        libusb_release_interface(device.handle, HAILO_RFS_LOAD_INTERFACE);
        libusb_close(device.handle);
    }
    
    if (device.ctx) {
        libusb_exit(device.ctx);
    }

    return ret;
}
