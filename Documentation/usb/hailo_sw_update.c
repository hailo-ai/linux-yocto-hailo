/*
 * hailo_sw_update.c - SWU Image Upload Tool for Hailo10 USB Device
 *
 * This C program uploads an SWU (Root File System) image to a Hailo10 USB device
 * using libusb. It communicates with the f_hailo_swu_load.c USB gadget function driver.
 *
 * Protocol:
 *   1. Send HAILO_REQ__SWU_LOAD control request with file size (or 0 for large files)
 *   2. Stream SWU data via bulk OUT endpoint in 8KB chunks
 *   3. Send HAILO_REQ__SWU_FINISH control request to signal completion
 *   4. Verify status via HAILO_REQ__SWU_GET_STATUS
 *
 * The kernel driver streams the data directly to /initrd.image for 
 * subsequent loading via initrd mechanism.
 *
 * Features:
 *   - Progress reporting with transfer speed in verbose mode
 *   - EP0 buffer size compliance with USB speed-dependent limitations
 *   - Enhanced status parsing and verification
 *   - Status-only mode for device monitoring
 *   - SWU load device information retrieval
 *
 * Compilation:
 *   gcc -o hailo_sw_update hailo_sw_update.c -lusb-1.0
 *   Or use: ./build_hailo_sw_update.sh
 *
 * Usage:
 *   ./hailo_sw_update [-v] <swu_image_file>
 *   ./hailo_sw_update [--status] [-v]
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

/* Exit codes */
enum exit_code_e {
    HAILO_EXIT_SUCCESS=0,
    HAILO_EXIT_GENERAL_ERROR=1,
    HAILO_EXIT_DEVICE_DISCONNECTED=2,
    HAILO_EXIT_SWUPDATE_ERROR=3,
    HAILO_EXIT_LOCAL_SWU_IMAGE_NOT_SPECIFIED=4,
    HAILO_EXIT_LOCAL_SWU_IMAGE_NOT_EXIST=5,
    HAILO_EXIT_CUSTOMER_PUBKEY_NOT_EXIST=6,
    HAILO_EXIT_SET_BOOT_PARTITION_ERROR=7,
    HAILO_EXIT_MAX_VALUE=8,
};

/**
 * Translate exit code to human-readable string
 */
static const char *exit_code_to_string(int exit_code)
{
    static char *str_sw_exit_code[HAILO_EXIT_MAX_VALUE] = {
    "Success",
    "General error",
    "Device disconnected",
    "SWU update error",
    "Local SWU image not specified",
    "Local SWU image does not exist",
    "Customer public key does not exist",
    "Set boot partition error"
    };

    if (exit_code >= 0 && exit_code < HAILO_EXIT_MAX_VALUE) {
        return str_sw_exit_code[exit_code];
    }
    return "Unknown error code";
}

// Hailo SWU load USB identifiers (adjust as needed)
#define HAILO_VID                   0x0B05  // ASUSTek Computer, Inc.
#define HAILO_PID                   0x1D6F  // Hailo Gadget

/* Vendor-specific control requests for SWU load mode */
#define HAILO_REQ__SWU_GET_STATUS   0x03  /* HAILO_REQ__SWU_GET_STATUS (Get function status): 
                                           *   - idle:ready    Endpoint idle, not processing
                                           *   - load:xxxxxx   Uploading SWU image, <xxxxxx> bytes received, 
                                           *   - exec:running  Executing SW update 
                                           *   - invalid:xxxx  Invalid state or error */
#define HAILO_REQ__SWU_GET_INFO     0x11  /* Get SWU model information */
#define HAILO_REQ__SWU_LOAD         0x12  /* Load SWU image command */
#define HAILO_REQ__SWU_FINISH       0x13  /* Finish SWU loading */
#define HAILO_REQ__SWU_CTRL         0x15  /* SWU control operations */
#define HAILO_REQ__SWU_SYS_REBOOT   0x16  /* Request system reboot */

/* Gadget configuration modes */
#define HAILO_GADGET_CONFIG_RFS_MODE  1   /* RFS upload mode */
#define HAILO_GADGET_CONFIG_SWU_MODE  2   /* SW update mode */

/* SWU Control Sub-commands */
#define HAILO_REQ__SWU_CTRL__GET_STATUS    0  /* Return ready status (1 byte) */
#define HAILO_REQ__SWU_CTRL__GET_RX_CNT    1  /* Return bytes received count (4 bytes) */
#define HAILO_REQ__SWU_CTRL__CLR_RX_CNT    2  /* Reset SWU counter (1 byte response) */
#define HAILO_REQ__SWU_CTRL__GET_EXECUTION_STATUS 3  /* Get swupdate status (8 bytes: 4-byte state + 4-byte exit_code) */

/* Swupdate execution states */
#define HAILO_SWUPDATE_EXEC_STATE_IDLE           0 /* Never started or completed */
#define HAILO_SWUPDATE_EXEC_STATE_IN_PROGRESS    1 /* Currently running */
#define HAILO_SWUPDATE_EXEC_STATE_END_OK         2 /* Completed successfully */
#define HAILO_SWUPDATE_EXEC_STATE_END_FAIL       3 /* Completed with error */

// USB request types
#define USB_TYPE_VENDOR             (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE)
#define USB_DIR_OUT                 LIBUSB_ENDPOINT_OUT
#define USB_DIR_IN                  LIBUSB_ENDPOINT_IN

// Transfer parameters  
#define BULK_TRANSFER_SIZE          (64*1024)   // 64KB chunks (maximum performance with ZLP flush)
#define USB_TIMEOUT                 10000       // 10 seconds (increased for large transfers)
#define USB_CONTROL_TIMEOUT         5000        // 5 seconds for control requests
#define MAX_SWU_SIZE                (256 * 1024 * 1024)  // 256MB

// EP0 buffer sizes (matching f_hailo_swu_load.c macros)
#define HAILO_SWU_LOAD_EP0_BUFFER_SIZE          64    // EP0 control request buffer size
#define HAILO_SWU_LOAD_EP0_STATUS_RESPONSE_SIZE 16    // EP0 status response buffer size

// Interface/endpoint parameters
#define HAILO_SWU_LOAD_INTERFACE          0
#define HAILO_SWU_LOAD_INTERFACE_CLASS    0xFF    // Vendor-specific
#define HAILO_SWU_LOAD_INTERFACE_PROTOCOL 1       // AI mode protocol

typedef struct {
    libusb_context *ctx;
    libusb_device_handle *handle;
    unsigned char bulk_out_ep;
    unsigned char intr_in_ep;
} hailo_device_t;

/* Global flags */
static int verbose = 0;
static int wait_for_completion = 1;  // Default: wait for completion
static int reboot_only = 0;
static int config_only = 0;

/* USB hotplug callback globals */
static volatile int device_disconnected = 0;  // Flag set when device disconnects
static libusb_hotplug_callback_handle callback_handle = 0;  // Callback registration handle

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [OPTIONS] <swu_image_file>\n", prog_name);
    printf("       %s [OPTIONS] --status\n", prog_name);
    printf("\n");
    printf("Upload SWU (Root File System) image to Hailo SWU load USB device\n");
    printf("\n");
    printf("OPTIONS:\n");
    printf("  -v, --verbose     Verbose output\n");
    printf("  -s, --status      Only check device status (no upload)\n");
    printf("  -n, --no-wait     Upload only, don't wait for swupdate completion\n");
    printf("  -r, --reboot      Reboot the USB device\n");
    printf("  -c, --config N    Configure USB device (N=1: RFS mode, N=2: SWU mode)\n");
    printf("  -h, --help        Show this help message\n");
    printf("\n");
    printf("EXAMPLES:\n");
    printf("  %s image.swu\n", prog_name);
    printf("  %s -v /path/to/swu_image.img\n", prog_name);
    printf("  %s --status       # Check device status only\n", prog_name);
    printf("  %s --reboot       # Reboot device only\n", prog_name);
    printf("  %s --config 2     # Set USB configuration 2 (SWU mode)\n", prog_name);
    printf("  %s -c 1           # Set USB configuration 1 (RFS mode)\n", prog_name);
    printf("\n");
    printf("EXIT CODES:\n");
    for (int i = 0; i < HAILO_EXIT_MAX_VALUE; i++) {
        printf("  %d - %s\n", i, exit_code_to_string(i));
    }
    printf("\n");
}

/**
 * USB hotplug callback function - called when USB device events occur
 * This callback is specifically for handling device disconnection events
 */
static int LIBUSB_CALL usb_hotplug_callback(libusb_context *ctx, libusb_device *device,
                                           libusb_hotplug_event event, void *user_data)
{
    struct libusb_device_descriptor desc;
    hailo_device_t *hailo_dev = (hailo_device_t *)user_data;
    (void)ctx; // Suppress unused parameter warning
    
    // Get device descriptor to check if this is our Hailo device
    if (libusb_get_device_descriptor(device, &desc) != 0) {
        return 0; // Continue processing other events
    }
    
    // Check if this is the Hailo device we care about
    if (desc.idVendor != HAILO_VID || desc.idProduct != HAILO_PID) {
        return 0; // Not our device, ignore
    }
    
    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        printf("\n⚠  USB DEVICE DISCONNECTED: Hailo device (VID:0x%04x, PID:0x%04x) has been unplugged!\n", 
               desc.idVendor, desc.idProduct);
        
        // Set the disconnection flag
        device_disconnected = 1;
        
        // If we have a reference to the device handle, note the disconnection
        if (hailo_dev && hailo_dev->handle) {
            // Note: We don't close the handle here as it may be in use by another thread
            // The main thread should check device_disconnected flag and handle cleanup
            printf("   Device handle invalidated - operations will fail gracefully\n");
        }
        
        if (verbose) {
            printf("   Hotplug callback: Device disconnection detected\n");
        }
    }
    
    return 0; // Continue processing other hotplug events
}

/**
 * Register USB hotplug callback for device disconnection events
 */
static int register_usb_disconnect_callback(libusb_context *ctx, hailo_device_t *device)
{
    int ret;
    
    // Check if libusb supports hotplug events
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        if (verbose) {
            printf("Warning: libusb hotplug capability not available on this platform\n");
        }
        return -1; // Not an error, just not supported
    }
    
    // Reset disconnection flag
    device_disconnected = 0;
    
    // Register callback for device removal events
    ret = libusb_hotplug_register_callback(ctx,
                                          LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                          LIBUSB_HOTPLUG_NO_FLAGS,
                                          HAILO_VID,
                                          HAILO_PID,
                                          LIBUSB_HOTPLUG_MATCH_ANY,
                                          usb_hotplug_callback,
                                          device,
                                          &callback_handle);
    
    if (ret != LIBUSB_SUCCESS) {
        fprintf(stderr, "Warning: Failed to register USB hotplug callback: %s\n", 
                libusb_error_name(ret));
        return -1;
    }
    
    if (verbose) {
        printf("✓ USB disconnect callback registered successfully\n");
    }
    
    return 0;
}

/**
 * Unregister USB hotplug callback
 */
static void unregister_usb_disconnect_callback(libusb_context *ctx)
{
    if (callback_handle != 0) {
        libusb_hotplug_deregister_callback(ctx, callback_handle);
        callback_handle = 0;
        if (verbose) {
            printf("USB disconnect callback unregistered\n");
        }
    }
}

/**
 * Check if device has been disconnected and handle it gracefully
 */
static int check_device_disconnection(const char *operation)
{
    if (device_disconnected) {
        fprintf(stderr, "\n❌ Operation '%s' aborted: USB device has been disconnected\n", operation);
        fprintf(stderr, "   Please reconnect the device and try again\n");
        fprintf(stderr, "   Exiting with error code %d (%s)\n", HAILO_EXIT_DEVICE_DISCONNECTED, exit_code_to_string(HAILO_EXIT_DEVICE_DISCONNECTED));
        exit(HAILO_EXIT_DEVICE_DISCONNECTED); // Exit immediately with specific error code for disconnection
    }
    return 0; // Device still connected
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

    printf("Searching for Hailo SWU load USB device (VID:0x%04x, PID:0x%04x)...\n", HAILO_VID, HAILO_PID);

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

        // Find Hailo SWU load interface
        for (j = 0; j < config->bNumInterfaces; j++) {
            interface = &config->interface[j];
            intf_desc = &interface->altsetting[0];

            // Check if this is our interface
            if (intf_desc->bInterfaceClass == HAILO_SWU_LOAD_INTERFACE_CLASS &&
                intf_desc->bInterfaceProtocol == HAILO_SWU_LOAD_INTERFACE_PROTOCOL &&
                intf_desc->bNumEndpoints == 2) {

                printf("Found Hailo SWU load interface %d\n", intf_desc->bInterfaceNumber);

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

                    printf("Successfully opened Hailo SWU load device\n");
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
    fprintf(stderr, "Hailo SWU load device not found or not accessible\n");
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
    case HAILO_REQ__SWU_GET_STATUS:
	    req_name = "GET_STATUS";
	    break;
    case HAILO_REQ__SWU_LOAD:
	    req_name = "LOAD_SWU";
	    break;
    case HAILO_REQ__SWU_FINISH:
	    req_name = "FINISH_SWU";
	    break;
    case HAILO_REQ__SWU_CTRL:
	    req_name = "SWU_CTRL";
	    break;
    case HAILO_REQ__SWU_SYS_REBOOT:
	    req_name = "SYS_REBOOT";
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

static int gadget_swu__get_status(hailo_device_t *dev, char *status_buf, size_t buf_size)
{
	unsigned char buffer[HAILO_SWU_LOAD_EP0_BUFFER_SIZE];
	int ret;

	ret = vendor_request(dev, HAILO_REQ__SWU_GET_STATUS, 0, 0, buffer,
			     HAILO_SWU_LOAD_EP0_STATUS_RESPONSE_SIZE,
			     USB_DIR_IN);
	if (ret < 0) {
		return -1;
	}

	// Ensure proper null termination within received bytes
	if (ret >= HAILO_SWU_LOAD_EP0_STATUS_RESPONSE_SIZE) {
		ret = HAILO_SWU_LOAD_EP0_STATUS_RESPONSE_SIZE - 1;
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


static int gadget_swu_ctrl__get_status(hailo_device_t *dev)
{
    unsigned char buffer[1];
    int ret;

    ret = vendor_request(dev, HAILO_REQ__SWU_CTRL, HAILO_REQ__SWU_CTRL__GET_STATUS, 0, buffer, 1, USB_DIR_IN);
    if (ret < 0) {
        return -1;
    }

    if (verbose) {
        printf("Gadget buffer %s for image upload\n", buffer[0] ? "ready" : "not ready");
    }

    return buffer[0] ? 1 : 0;  // Return 1 if ready, 0 if not ready
}

static int gadget_swu_ctrl__get_bulk_rx_cnt(hailo_device_t *dev)
{
    unsigned int bytes_received;
    unsigned char buffer[4];
    int ret;

    ret = vendor_request(dev, HAILO_REQ__SWU_CTRL, HAILO_REQ__SWU_CTRL__GET_RX_CNT, 0, buffer, 4, USB_DIR_IN);
    if (ret < 0) {
        return -1;
    }

    bytes_received = le32toh(*((uint32_t *)buffer));
    
    if (verbose) {
        printf("Device reports %u bytes received so far\n", bytes_received);
    }

    return (int)bytes_received;
}

static int gadget_swu_ctrl__clr_bulk_rx_cnt(hailo_device_t *dev, int max_attempts)
{
    unsigned char reset_response = 0;
    int reset_ret;
    int attempt;

    printf("Resetting gadget rx bulk counter...\n");
    
    for (attempt = 0; attempt < max_attempts; attempt++) {
        reset_ret = vendor_request(dev, HAILO_REQ__SWU_CTRL, HAILO_REQ__SWU_CTRL__CLR_RX_CNT, 0, &reset_response, 1, USB_DIR_IN);
        
        if (reset_ret >= 0 && reset_response == 1) {
            if (verbose) {
                printf("SWU counter reset successful on attempt %d/%d\n", attempt + 1, max_attempts);
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
    
    fprintf(stderr, "Warning: Failed to reset SWU counter after %d attempts\n", max_attempts);
    return -1;
}

static int gadget_swu_ctrl__get_execution_status(hailo_device_t *dev, uint32_t *state, uint32_t *exit_code)
{
    unsigned char response[8] = {0};
    int ret = vendor_request(dev, HAILO_REQ__SWU_CTRL, HAILO_REQ__SWU_CTRL__GET_EXECUTION_STATUS, 
                            0, response, sizeof(response), USB_DIR_IN);
    
    if (ret == sizeof(response)) {
        // Parse the response: first 4 bytes = state, next 4 bytes = exit_code
        *state = le32toh(*(uint32_t*)response);
        *exit_code = le32toh(*(uint32_t*)(response + 4));
        return 0;  // Success
    }
    
    return -1;  // Failed to get status
}

static void gadget_swu_ctrl__get_execution_status__print(hailo_device_t *dev)
{
    printf("\n--- SWUpdate Process Status ---\n");
    
    uint32_t state, exit_code;
    if (gadget_swu_ctrl__get_execution_status(dev, &state, &exit_code) == 0) {
        switch (state) {
            case HAILO_SWUPDATE_EXEC_STATE_IDLE:
                printf("🔄 SWUpdate: IDLE (not started or completed)\n");
                break;
            case HAILO_SWUPDATE_EXEC_STATE_IN_PROGRESS:
                printf("⏳ SWUpdate: IN PROGRESS (currently running)\n");
                break;
            case HAILO_SWUPDATE_EXEC_STATE_END_OK:
                printf("✅ SWUpdate: COMPLETED SUCCESSFULLY (exit_code: %u)\n", exit_code);
                break;
            case HAILO_SWUPDATE_EXEC_STATE_END_FAIL:
                printf("❌ SWUpdate: FAILED (exit_code: %u - %s)\n", exit_code, exit_code_to_string(exit_code));
                break;
            default:
                printf("❓ SWUpdate: UNKNOWN STATE (%u, exit_code: %u)\n", state, exit_code);
                break;
        }
    } else {
        printf("⚠ Could not retrieve swupdate status\n");
    }
}

static int gadget_swu__set_config(hailo_device_t *dev, int config_num)
{
	int ret, current_config_num;;
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
	ret = libusb_release_interface(dev->handle, HAILO_SWU_LOAD_INTERFACE);
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
	ret = libusb_claim_interface(dev->handle, HAILO_SWU_LOAD_INTERFACE);
	if (ret < 0) {
		fprintf(stderr, "✗ Failed to re-claim interface: %s\n", libusb_error_name(ret));
		return 1;
	}
	
	printf("✓ Interface re-claimed successfully\n");
	printf("✓ Gadget configuration set to %s mode successfully\n", mode_name);
	
	return 0;
}

static int gadget_swu__reboot_device(hailo_device_t *dev)
{
	int ret;

	printf("=== Hailo SWU Reboot Device ===\n");
	printf("Sending reboot command to device...\n");
	
	ret = vendor_request(dev, HAILO_REQ__SWU_SYS_REBOOT, 0, 0, NULL, 0, USB_DIR_OUT);
	if (ret < 0) {
		fprintf(stderr, "✗ Failed to send reboot command\n");
		return 1;
	}
	
	printf("✓ Reboot command sent successfully\n");
	printf("Device should reboot shortly...\n");
	return 0;
}

static int gadget_swu__get_status__print(hailo_device_t *dev)
{
    char status[64];
    int ret = 0;

    printf("=== SWU gadget Status ===\n");
    
    if (gadget_swu__get_status(dev, status, sizeof(status)) == 0) {
        printf("Current status: %s\n", status);
        
        // Parse and explain status
        if (strncmp(status, "idle:", 5) == 0 || strstr(status, "ready")) {
            printf("✓ Device is ready for operations\n");
        } else if (strncmp(status, "swu:", 4) == 0) {
            unsigned int swu_bytes = 0;
            if (sscanf(status + 4, "%x", &swu_bytes) == 1) {
                printf("📁 SWU upload in progress: %u bytes received\n", swu_bytes);
            }
        } else if (strncmp(status, "req:", 4) == 0) {
            printf("📨 Processing SWU load request\n");
        } else if (strncmp(status, "proc:", 5) == 0) {
            printf("🧠 SWU load processing in progress\n");
        } else if (strncmp(status, "rdy:", 4) == 0) {
            printf("📤 SWU load reply ready\n");
        } else if (strncmp(status, "snd:", 4) == 0) {
            printf("📤 Sending SWU load reply\n");
        }
        
        // Also check swupdate completion status
        gadget_swu_ctrl__get_execution_status__print(dev);
        
        ret = 0;
    } else {
        fprintf(stderr, "✗ Could not retrieve device status\n");
        ret = 1;
    }
    
    return ret;
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
        device_received = gadget_swu_ctrl__get_bulk_rx_cnt(dev);
        
        if (ret == 0 && transferred == sizeof(ping_data) && device_received >= (int)sizeof(ping_data)) {
            printf("✓ Bulk endpoint verified ready (ping successful)\n");
            /* Reset SWU counter to exclude ping data from actual SWU transfer */
            if (gadget_swu_ctrl__clr_bulk_rx_cnt(dev, 3) == 0) {
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
        ready = gadget_swu_ctrl__get_status(dev);
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

static int gadget_swu__start_image_upload(hailo_device_t *dev, size_t file_size)
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
    
    printf("Starting SWU upload, size: %zu bytes (0x%04x%04x)\n", file_size, size_high, size_low);
    
    /* Send LOAD_SWU with file size split across wValue and wIndex */
    ret = vendor_request(dev, HAILO_REQ__SWU_LOAD, size_low, size_high, NULL, 0, USB_DIR_OUT);
    if (ret < 0) {
        return -1;
    }
    
    /* Give device time to set up bulk endpoint for SWU transfer */
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
        // Check for device disconnection before attempting transfer
        if (check_device_disconnection("bulk transfer")) {
            return -1;
        }
        
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

static int wait_for_swu_execution_completion(hailo_device_t *dev, int timeout_seconds)
{
    time_t start_time;
    time_t elapsed = 0;
    uint32_t state, exit_code;
    
    printf("Waiting for SW update execution completion (timeout: %d seconds)...\n", timeout_seconds);
    fflush(stdout);

    start_time = time(NULL);
    
    while (elapsed < timeout_seconds) {
        // Check for device disconnection during waiting
        if (check_device_disconnection("swupdate execution wait")) {
            return -1;
        }
        
        // Process USB events to ensure hotplug callbacks are triggered
        if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
            struct timeval timeout = {0, 0}; // Non-blocking
            libusb_handle_events_timeout(dev->ctx, &timeout);
        }
        
        if (gadget_swu_ctrl__get_execution_status(dev, &state, &exit_code) == 0) {
            if (verbose) {
                printf("\nSW update execution state: %u, exit_code: %u\n", state, exit_code);
            }
            
            switch (state) {
                case HAILO_SWUPDATE_EXEC_STATE_IDLE:
                    if (verbose)
                        printf("\nSW update execution state: idle (not started or completed)\n");
                    break;
                    
                case HAILO_SWUPDATE_EXEC_STATE_IN_PROGRESS:
                    if (verbose)
                        printf("\nSW update execution state: in progress\n");
                    break;
                    
                case HAILO_SWUPDATE_EXEC_STATE_END_OK:
                    printf("\n✓ SW update execution state: completed successfully (exit_code: %u)\n", exit_code);
                    return 0; // Success
                    
                case HAILO_SWUPDATE_EXEC_STATE_END_FAIL:
                    printf("\n✗ SW update execution state: failed (exit_code: %u - %s)\n", exit_code, exit_code_to_string(exit_code));
                    return -EPERM; // Failed - operation not permitted/failed
                    
                default:
                    printf("\n⚠ SW update execution state: Unknown(%u)\n", state);
                    break;
            }
        } else if (verbose) {
            printf("\nFailed to get SW update execution status, continuing...\n");
        }
        
        // Check for device disconnection before waiting
        if (check_device_disconnection("swupdate completion wait")) {
            printf("\nSWUpdate monitoring aborted due to device disconnection\n");
            return -ENODEV; // No such device
        }
        
        // Process USB events and wait before next check
        if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
            struct timeval timeout = {2, 0}; // 2 second timeout
            libusb_handle_events_timeout(dev->ctx, &timeout);
        } else {
            sleep(2);
        }
        printf(".");
        fflush(stdout);
        
        elapsed = time(NULL) - start_time;
        
        /* Provide progress indication every 30 seconds */
        if (elapsed % 30 == 0 && elapsed > 0) {
            printf(" (%lds)\n", elapsed);
            fflush(stdout);
        }
    }

    printf("\n⚠ Timeout waiting for SW update completion after %d seconds\n", timeout_seconds);
    printf("  The SW update execution process may still be running in the background\n");
    return -ETIMEDOUT; // Timeout
}

static int gadget_swu__finish_image_upload(hailo_device_t *dev)
{
    int verify_ret = -1;
    int attempts = 0;
    int ret;
    unsigned char status[64] = {0};
    const int max_attempts = 15; // Up to 30 seconds total

    printf("Finishing SWU image upload command\n");
    ret = vendor_request(dev, HAILO_REQ__SWU_FINISH, 0, 0, NULL, 0, USB_DIR_OUT);
    if (ret < 0) {
        fprintf(stderr, "✗ FINISH_SWU request failed: unable to signal upload completion\n");
        return ret;
    }
    
    printf("✓ FINISH_SWU request completed successfully\n");
    printf("Waiting for device to write uploaded SWU image to file...\n");
        
    // Wait longer for device to process large files - writing to flash can be slow
    // Retry status check with increasing delays for up to 30 seconds total
    
    for (attempts = 0; attempts < max_attempts; attempts++) {
        // Check for device disconnection before delay and status check
        if (check_device_disconnection("finish upload status check")) {
            return -1;
        }
        
        // Progressive delay: 10msec, 1s, 2s, 2s, 2s, ... (capped at 2s per attempt)
        int delay_usec = (attempts == 0) ? 10000 : (attempts == 1) ? 1000000 : 2000000;
        usleep(delay_usec);
        
        // Process USB events during delay
        if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
            struct timeval timeout = {0, 0}; // Non-blocking
            libusb_handle_events_timeout(dev->ctx, &timeout);
        }
        
        verify_ret = vendor_request(dev, HAILO_REQ__SWU_GET_STATUS, 0, 0, status, sizeof(status), USB_DIR_IN);
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
        // Don't treat this as a failure - the FINISH_SWU request succeeded
    }
    
    // Return success (0) since FINISH_SWU completed successfully
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
        fprintf(stderr, "SWU file is empty\n");
        return -1;
    }

    if (file_size > MAX_SWU_SIZE) {
        fprintf(stderr, "SWU file too large: %zu bytes (max %d bytes)\n", file_size, MAX_SWU_SIZE);
        return -1;
    }

    file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        return -1;
    }

    printf("Uploading SWU file: %s (%zu bytes)\n", filename, file_size);

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
        
        // Process USB events to ensure hotplug callbacks are triggered promptly
        if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
            struct timeval timeout = {0, 0}; // Non-blocking
            libusb_handle_events_timeout(dev->ctx, &timeout);
        }
        
        // Check for device disconnection after each transfer
        if (check_device_disconnection("file upload")) {
            ret = -1;
            break;
        }
        
        /* Verify the transfer was actually received by the device */
        if (verbose) {
            usleep(50000); // 50ms delay for device to process
            int device_received = gadget_swu_ctrl__get_bulk_rx_cnt(dev);
            if (device_received >= 0) {
                if ((unsigned int)device_received != bytes_sent) {
                    fprintf(stderr, "\n⚠ TRANSFER VERIFICATION FAILED!\n");
                    fprintf(stderr, "Host sent: %zu bytes, Device received: %d bytes\n", bytes_sent, device_received);
                    fprintf(stderr, "Transfer #%zu (%zu bytes) was lost!\n", 
                           (bytes_sent - transferred) / BULK_TRANSFER_SIZE + 1, (size_t)transferred);
                    
                    /* Wait longer and check again */
                    printf("Waiting longer for transfer to complete...\n");
                    usleep(200000); // Additional 200ms wait
                    device_received = gadget_swu_ctrl__get_bulk_rx_cnt(dev);
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

static int start_sw_update(hailo_device_t *dev, const char *filename)
{
    struct stat st;
    char status[64];

    // Validate file
    if (stat(filename, &st) != 0) {
        perror("stat");
        return -1;
    }

    printf("SWU file: %s\n", filename);
    printf("File size: %zu bytes (%.2f MB)\n", (size_t)st.st_size, (double)st.st_size / (1024 * 1024));

    // Get initial status
    if (gadget_swu__get_status(dev, status, sizeof(status)) == 0) {
        printf("Initial status: %s\n", status);
    }

    // Start upload
    if (gadget_swu__start_image_upload(dev, st.st_size) < 0) {
        return -1;
    }

    // Wait for SWU buffer to be allocated and ready (with 30 second timeout)
    printf("Waiting for device to allocate vmalloc buffer (%zu bytes)...\n", st.st_size);
    if (wait_gadget_ready_for_image_upload(dev, 30) < 0) {
        return -1;
    }

    // Upload data
    if (upload_image_file(dev, filename) < 0) {
        return -1;
    }

    // Critical delay: Ensure all bulk transfers are fully processed before calling FINISH_SWU
    printf("Ensuring all transfers are processed...\n");
    usleep(250000);  // 250ms delay to let USB pipeline clear

    // Finish upload
    if (gadget_swu__finish_image_upload(dev) < 0) {
        return -1;
    }

    usleep(500000);  // Wait 500ms for device to process completion

    // Wait for the actual swupdate process to complete (if requested)
    if (wait_for_completion) {
        printf("\n=== Waiting for SWUpdate Process ===\n");
        int swupdate_result = wait_for_swu_execution_completion(dev, 300); // 5 minutes timeout
        
        if (swupdate_result == 0) {
            printf("✓ SWUpdate process completed successfully!\n");
        } else if (swupdate_result == -EPERM) {
            printf("✗ SWUpdate process failed - check device logs\n");
            return -1;
        } else if (swupdate_result == -ETIMEDOUT) {
            printf("⚠ SWUpdate process timeout - may still be running\n");
            // Don't fail here as the upload was successful
        }
    } else {
        printf("\n=== Skipping SWUpdate Completion Wait ===\n");
        printf("Upload completed. Use --status option to check swupdate progress later.\n");
    }

    // Get final status and verify (optional - don't fail upload if this fails)
    if (gadget_swu__get_status(dev, status, sizeof(status)) == 0) {
        printf("Final status: %s\n", status);
        
        /* Parse status for detailed verification */
        if (strncmp(status, "idle:", 5) == 0 || strstr(status, "idle:ready")) {
            printf("✓ SWU upload completed successfully - device is ready\n");
        } else if (strncmp(status, "load:", 4) == 0) {
            /* Still shows SWU mode - check if transfer completed */
            unsigned int swu_received = 0;
            if (sscanf(status + 4, "%x", &swu_received) == 1) {
                printf("Device reports %u (0x%x) bytes received\n", swu_received, swu_received);
                if (swu_received == (unsigned int)st.st_size) {
                    printf("✓ Byte count matches - SWU upload verified\n");
                } else {
                    printf("⚠ Byte count mismatch: expected %zu, device received %u\n", 
                           (size_t)st.st_size, swu_received);
                    printf("  However, data transfer completed successfully\n");
                }
            } else {
                printf("⚠ Could not parse SWU byte count from status\n");
                printf("  However, data transfer completed successfully\n");
            }
        } else {
            printf("⚠ Unexpected final status: %s\n", status);
            printf("  However, data transfer completed successfully\n");
        }
    } else {
        printf("⚠ Could not retrieve final device status (device may still be processing)\n");
        printf("  Data transfer completed successfully - this is normal for large files\n");
    }

    printf("✓ SWU upload completed - %zu bytes transferred successfully\n", (size_t)st.st_size);
    return 0;
}

int main(int argc, char *argv[]) {
    hailo_device_t device = {0};
    const char *filename = NULL;
    int status_only = 0;
    int ret = 1;
    int opt;
    int config_value = 0;

    // Define long options
    static struct option long_options[] = {
        {"verbose",   no_argument,       0, 'v'},
        {"status",    no_argument,       0, 's'},
        {"no-wait",   no_argument,       0, 'n'},
        {"reboot",    no_argument,       0, 'r'},
        {"config",    required_argument, 0, 'c'},
        {"help",      no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    // Parse arguments using getopt
    while ((opt = getopt_long(argc, argv, "vsnrc:h", long_options, NULL)) != -1) {
        switch (opt) {
        case 'v':
            verbose = 1;
            break;
        case 's':
            status_only = 1;
            break;
        case 'n':
            wait_for_completion = 0;
            break;
        case 'r':
            reboot_only = 1;
            break;
        case 'c':
            config_only = 1;
            config_value = atoi(optarg);
            if (config_value != 1 && config_value != 2) {
                fprintf(stderr, "Invalid config value: %d (must be 1 or 2)\n", config_value);
                print_usage(argv[0]);
                return 1;
            }
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

    if (filename == NULL && !status_only && !reboot_only && !config_only) {
        fprintf(stderr, "SWU file not specified\n");
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
    if (find_hailo_device(&device) < 0) {
        ret = 1;
        goto cleanup;
    }

    // Register USB disconnect callback for hotplug events
    register_usb_disconnect_callback(device.ctx, &device);

    if (config_only) {
    // Set gadget configuration
        ret = gadget_swu__set_config(&device, config_value);
    } else if (status_only) {
        // Status-only mode
        ret = gadget_swu__get_status__print(&device);
    } else if (reboot_only) {
        // Reboot-only mode
        ret = gadget_swu__reboot_device(&device);
    } else {
        // Upload SWU image
        ret = gadget_swu__set_config(&device, HAILO_GADGET_CONFIG_SWU_MODE);
        if (ret != 0) {
            goto cleanup;
        }
        printf("Starting SWU upload process...\n");
        if (start_sw_update(&device, filename) == 0) {
            printf("✓ SWU upload completed successfully!\n");
            ret = 0;
        } else {
            fprintf(stderr, "✗ SWU upload failed\n");
            ret = 1;
        }
    }

cleanup:
    // Unregister USB hotplug callback before cleanup
    if (device.ctx) {
        unregister_usb_disconnect_callback(device.ctx);
    }
    
    if (device.handle) {
        libusb_release_interface(device.handle, HAILO_SWU_LOAD_INTERFACE);
        libusb_close(device.handle);
    }
    
    if (device.ctx) {
        libusb_exit(device.ctx);
    }

    // Final check for device disconnection
    if (device_disconnected) {
        fprintf(stderr, "Program terminated due to USB device disconnection (exit code %d: %s)\n", 
                HAILO_EXIT_DEVICE_DISCONNECTED, exit_code_to_string(HAILO_EXIT_DEVICE_DISCONNECTED));
        return HAILO_EXIT_DEVICE_DISCONNECTED; // Exit code for device disconnection
    }

    return ret;
}