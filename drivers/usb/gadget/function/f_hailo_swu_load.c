/*
 * f_hailo_swu_load.c -- USB function driver for Hailo SWU load Device Mode
 *
 * Hailo SWU load Device Mode:
 * - Single interface with alternate setting 0 only
 * - Bulk OUT, Interrupt IN (status) endpoints
 * - SWU image upload functionality
 */

#include <dt-bindings/soc/hailo15_release_version.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/usb/composite.h>
#include <linux/usb/ch9.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sizes.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/umh.h>
#include <linux/usb/gadget.h>

#include "u_hailo_swu_load.h"
#include "u_f.h"

/* External function to get board SKU ID from soc-hailo driver */
extern void board_sku_id_to_str(u32 board_sku_id, char *name, size_t name_size);

/* External function to get board SKU ID via SCMI from hailo gadget driver */
extern int hailo_gadget_get_board_sku_id(u32 *board_sku_id);

#define HAILO_SWU_DRIVER_DESC "Hailo SWU load Device Function"

/* Vendor-specific control requests for SWU load mode */
#define HAILO_REQ__SWU_GET_STATUS   0x03  /* HAILO_REQ__SWU_GET_STATUS (Get function status): 
                                           *   - idle:ready    Endpoint idle, not processing
                                           *   - load:xxxxxx   Uploading SWU image, <xxxxxx> bytes received, 
                                           *   - exec:running  Executing SW update 
                                           *   - invalid:xxxx  Invalid state or error
                                           */
#define HAILO_REQ__SWU_GET_BUILD_INFO   0x11  /* Get SWU model information */
#define HAILO_REQ__SWU_LOAD             0x12  /* Load SWU image command */
#define HAILO_REQ__SWU_FINISH           0x13  /* Finish SWU loading */
#define HAILO_REQ__SWU_GET_BOARD_SKU_ID 0x14 /* Get board SKU ID */
#define HAILO_REQ__SWU_CTRL             0x15  /* SWU control operations */
#define HAILO_REQ__SWU_SYS_REBOOT       0x16  /* System reboot command */

/* SWU Control Sub-commands */
#define HAILO_REQ__SWU_CTRL__GET_STATUS    0  /* Return ready status (1 byte) */
#define HAILO_REQ__SWU_CTRL__GET_RX_CNT    1  /* Return bytes received count (4 bytes) */
#define HAILO_REQ__SWU_CTRL__CLR_RX_CNT    2  /* Reset SWU counter (1 byte response) */
#define HAILO_REQ__SWU_CTRL__GET_EXECUTION_STATUS 3  /* Get swupdate exit code and state (8 bytes) */

/* Swupdate execution states */
#define HAILO_SWUPDATE_EXEC_STATE_IDLE           0 /* Never started or completed */
#define HAILO_SWUPDATE_EXEC_STATE_IN_PROGRESS    1 /* Currently running */
#define HAILO_SWUPDATE_EXEC_STATE_END_OK         2 /* Completed successfully */
#define HAILO_SWUPDATE_EXEC_STATE_END_FAIL       3 /* Completed with error */

/* Sizes */
#define HAILO_SWU_BULK_OUT_BUFFER_SIZE    SZ_64K /* SWU load request max data transfer size: 64 KiB (with ZLP flush) */

#define HAILO_SWU_EP0_BUFFER_SIZE  64           /* EP0 control request buffer size */
#define HAILO_SWU_EP0_STATUS_RESPONSE_SIZE  16  /* EP0 status response buffer size */
#define HAILO_SWU_INTERRUPT_MSG_SIZE  16

#define MAX_SWU_SIZE                   (SZ_256M)  /* Maximum SWU image size: 256MB */

/* Interface number */
#define HAILO_SWU_INTERFACE_NUM      0

/* Alternate setting values */
#define HAILO_SWU_ALT_SETTING_STANDARD  0

/* Bulk OUT endpoint operation modes */
enum hailo_swu_state {
    HAILO_SWU_STATE__IDLE = 0,         /* Endpoint idle, not processing */
    HAILO_SWU_STATE__UPLOAD,           /* Uploading SWU image */
    HAILO_SWU_STATE__EXEC,             /* Executing SW update */
};

/* SWU file operations for workqueue */
enum hailo_swu_load_file_operation {
    FILE_OP_ALLOC_VMALLOC = 0,     /* Allocate vmalloc buffer (must be in process context) */
    FILE_OP_WRITE = 1,             /* Write complete vmalloc buffer to SWU file */
    FILE_OP_FREE_VMALLOC = 2,      /* Free vmalloc buffer (must be in process context) */
};

struct f_hailo_swu_load {
    struct usb_function func;
    
    /* Endpoints */
    struct usb_ep *bulk_out_ep;
    struct usb_ep *intr_in_ep;
    
    /* Bulk OUT endpoint operation mode */
    enum hailo_swu_state state;
    
    /* SWU image streaming */
    size_t swu_image_received;
    size_t swu_image_size_expected;
    struct file *swu_file;  /* Open file for final write */
    
    /* vmalloc buffer for entire SWU image */
    void *swu_vmalloc_buf;   /* vmalloc buffer for entire SWU image */
    size_t swu_vmalloc_size; /* Size of vmalloc buffer */
    bool swu_buffer_ready;   /* Flag indicating vmalloc buffer is ready */
    
    /* Bulk-out request for receiving SWU data and SWU image */
    struct usb_request *bulk_out_req;
    
    /* Interrupt request */
    struct usb_request *intr_in_req;
    
    /* Timer for periodic status updates */
    struct timer_list status_timer;
    
    /* Atomic counter for safe dequeue operations */
    atomic_t intr_req_queued;
    
    /* Flag to indicate function is being unbound */
    atomic_t unbinding;
    
    /* Workqueue for file operations (can't be done in interrupt context) */
    struct work_struct swu_file_work;
    struct workqueue_struct *file_wq;
    
    /* SWU file operation parameters */
    enum hailo_swu_load_file_operation swu_file_operation;  /* File operation type */
    int swu_file_result;                              /* Result of file operation */
    size_t swu_vmalloc_requested_size;                /* Size to allocate for vmalloc buffer */
    
    /* Swupdate execution state */
    struct task_struct *swupdate_task;                /* Kernel thread for swupdate execution */
    int swupdate_exit_code;                          /* Exit code from swupdate (-1 = not started/in progress) */
};

/* Helper function to get f_hailo_swu_load from usb_function pointer */
static inline struct f_hailo_swu_load *to_f_hailo_swu_load(struct usb_function *f)
{
    return container_of(f, struct f_hailo_swu_load, func);
}

/* Helper function to get f_hailo_swu_load_opts from usb_function_instance pointer */
static inline struct f_hailo_swu_load_opts *to_f_hailo_swu_load_opts(const struct usb_function_instance *fi)
{
    return container_of(fi, struct f_hailo_swu_load_opts, func_inst);
}

/* Helper: convert bulk mode to readable string */
static inline const char *hailo_swu_state_to_string(int mode)
{
    switch (mode) {
        case HAILO_SWU_STATE__IDLE:          return "IDLE";
        case HAILO_SWU_STATE__UPLOAD:        return "UPLOAD";
        case HAILO_SWU_STATE__EXEC:          return "EXEC";
        default:                             return "UNKNOWN";
    }
}

/* Helper: get optimal bulk transfer size based on USB speed and endpoint */
static inline size_t hailo_swu_load_get_bulk_transfer_size(struct f_hailo_swu_load *swu)
{
    size_t max_packet_size;
    size_t optimal_size;
    
    if (!swu->bulk_out_ep || !swu->bulk_out_ep->desc)
        return HAILO_SWU_BULK_OUT_BUFFER_SIZE;
    
    max_packet_size = usb_endpoint_maxp(swu->bulk_out_ep->desc);
    
    /* Use multiple packets for better performance, but don't exceed our buffer */
    optimal_size = max_packet_size * 32; /* 32 packets per transfer */
    
    return min_t(size_t, optimal_size, HAILO_SWU_BULK_OUT_BUFFER_SIZE);
}

/* Helper: get EP0 max packet size based on current USB speed */
static inline unsigned int hailo_swu_load_get_ep0_maxpacket(struct usb_composite_dev *cdev)
{
    /* EP0 bMaxPacketSize0 is speed-dependent:
     * - Full-Speed: 8, 16, 32, or 64 bytes
     * - High-Speed: 64 bytes
     * - Super-Speed: 512 bytes
     * - Super-Speed Plus: 512 bytes
     */
    if (!cdev || !cdev->gadget || !cdev->gadget->ep0 || !cdev->gadget->ep0->desc)
        return 64; /* Safe default for FS/HS */
        
    return usb_endpoint_maxp(cdev->gadget->ep0->desc);
}

/* Endpoint descriptors */

/* Full-Speed endpoints */
static struct usb_endpoint_descriptor hailo_swu_load_fs_bulk_out_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_OUT,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = cpu_to_le16(64),
};


static struct usb_endpoint_descriptor hailo_swu_load_fs_intr_in_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_INT,
    .wMaxPacketSize = cpu_to_le16(16),
    .bInterval = 10,
};

/* High-Speed endpoints */
static struct usb_endpoint_descriptor hailo_swu_load_hs_bulk_out_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_OUT,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hailo_swu_load_hs_intr_in_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_INT,
    .wMaxPacketSize = cpu_to_le16(1024),
    .bInterval = 7, /* 2^(7-1) = 64 microframes */
};

/* Super-Speed endpoints */
static struct usb_endpoint_descriptor hailo_swu_load_ss_bulk_out_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_OUT,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor hailo_swu_load_ss_bulk_out_comp_desc = {
    .bLength = USB_DT_SS_EP_COMP_SIZE,
    .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
    .bMaxBurst = 0,
    .bmAttributes = 0,
    .wBytesPerInterval = 0,
};

static struct usb_endpoint_descriptor hailo_swu_load_ss_intr_in_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_INT,
    .wMaxPacketSize = cpu_to_le16(1024),
    .bInterval = 7,
};

static struct usb_ss_ep_comp_descriptor hailo_swu_load_ss_intr_in_comp_desc = {
    .bLength = USB_DT_SS_EP_COMP_SIZE,
    .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
    .bMaxBurst = 0,
    .bmAttributes = 0,
    .wBytesPerInterval = cpu_to_le16(1024),
};

/* Interface descriptor for SWU mode */
static struct usb_interface_descriptor hailo_swu_load_intf_desc = {
    .bLength = sizeof(hailo_swu_load_intf_desc),
    .bDescriptorType = USB_DT_INTERFACE,
    /* .bInterfaceNumber = DYNAMIC */  /* Assigned by composite framework */
    .bAlternateSetting = HAILO_SWU_ALT_SETTING_STANDARD,
    .bNumEndpoints = 2, /* bulk OUT, interrupt IN */
    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 1, /* SWU mode protocol */
    /* .iInterface = DYNAMIC */
};

/* Descriptor arrays for different USB speeds */
static struct usb_descriptor_header *hailo_swu_load_fs_function[] = {
    (struct usb_descriptor_header *) &hailo_swu_load_intf_desc,
    (struct usb_descriptor_header *) &hailo_swu_load_fs_bulk_out_desc,
    (struct usb_descriptor_header *) &hailo_swu_load_fs_intr_in_desc,
    NULL,
};

static struct usb_descriptor_header *hailo_swu_load_hs_function[] = {
    (struct usb_descriptor_header *) &hailo_swu_load_intf_desc,
    (struct usb_descriptor_header *) &hailo_swu_load_hs_bulk_out_desc,
    (struct usb_descriptor_header *) &hailo_swu_load_hs_intr_in_desc,
    NULL,
};

static struct usb_descriptor_header *hailo_swu_load_ss_function[] = {
    (struct usb_descriptor_header *) &hailo_swu_load_intf_desc,
    (struct usb_descriptor_header *) &hailo_swu_load_ss_bulk_out_desc,
    (struct usb_descriptor_header *) &hailo_swu_load_ss_bulk_out_comp_desc,
    (struct usb_descriptor_header *) &hailo_swu_load_ss_intr_in_desc,
    (struct usb_descriptor_header *) &hailo_swu_load_ss_intr_in_comp_desc,
    NULL,
};

/* String descriptors */
#define HAILO_SWU_INTERFACE_IDX  0

static struct usb_string hailo_swu_load_string_defs[] = {
    [HAILO_SWU_INTERFACE_IDX].s = "Hailo SWU Device Interface",
    { } /* end of list */
};

static struct usb_gadget_strings hailo_swu_load_string_table = {
    .language = 0x0409, /* en-us */
    .strings = hailo_swu_load_string_defs,
};

static struct usb_gadget_strings *hailo_swu_load_strings[] = {
    &hailo_swu_load_string_table,
    NULL,
};

/* Helper: Updated usb_request Dev -> Host with hailo_swu_state */
static int hailo_swu_state_update(struct f_hailo_swu_load *swu, struct usb_request *req, unsigned length)
{
    req->length = length;
    memset(req->buf, 0, req->length);
    switch (swu->state) {
        case HAILO_SWU_STATE__IDLE:
            snprintf((char *)req->buf, req->length, "idle:ready");
            break;
        case HAILO_SWU_STATE__UPLOAD:
            snprintf((char *)req->buf, req->length, "load:%06x", (unsigned)swu->swu_image_received);
            break;
        case HAILO_SWU_STATE__EXEC:
            snprintf((char *)req->buf, req->length, "exec:running");
            break;
        default:
            snprintf((char *)req->buf, req->length, "inv:%05x", (unsigned)swu->state);
            pr_warn("hailo_swu: invalid state %s\n", hailo_swu_state_to_string(swu->state));
            return 1;
    }

    return 0;
}

/* Helper: queue interrupt IN data for SWU load status */
static void hailo_swu_load_queue_interrupt(struct f_hailo_swu_load *swu)
{
    struct usb_request *req = swu->intr_in_req;
    int ret;

    /* Check if function is configured and endpoints are properly enabled */
    if (!req || !swu->intr_in_ep || !swu->intr_in_ep->enabled || !swu->func.config)
        return;

    /* Status payload based on current SWU load processing state */
    ret = hailo_swu_state_update(swu, req, HAILO_SWU_INTERRUPT_MSG_SIZE);
    if (ret) {
        return;
    }

    ret = usb_ep_queue(swu->intr_in_ep, req, GFP_ATOMIC);
    if (ret) {
        pr_warn("hailo_swu: intr queue err %d\n", ret);
    } else {
        atomic_inc(&swu->intr_req_queued);
    }
}

static void hailo_swu_load_status_timer_fn(struct timer_list *t)
{
    struct f_hailo_swu_load *swu;
    
    /* Extra safety: validate timer pointer before from_timer */
    if (!t) 
        return;
        
    swu = from_timer(swu, t, status_timer);
    
    /* Safety check: validate swu pointer and unbinding flag */
    if (!swu || atomic_read(&swu->unbinding))
        return;
    
    /* Only send status updates if function is still configured and interrupt endpoint is enabled */
    if (swu->func.config && swu->intr_in_ep && swu->intr_in_ep->enabled) {
        hailo_swu_load_queue_interrupt(swu);
        /* Temporarily disable timer rescheduling to debug RCU stall */
        /* Only reschedule if not unbinding and swu is still valid */
        //if (!atomic_read(&swu->unbinding) && swu->func.config)
        //    mod_timer(&swu->status_timer, jiffies + msecs_to_jiffies(1000));
    }
}

/* EP0 (control endpoint) completion callback for vendor requests */
/* Interrupt completion: handle interrupt request completion */
static void hailo_swu_load_intr_in_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct f_hailo_swu_load *swu = req->context;
    
    /* Safety check: ensure context is valid and function is not being unbound */
    if (!swu || atomic_read(&swu->unbinding))
        return;
    
    /* Decrement queued request counter */
    if (atomic_read(&swu->intr_req_queued) > 0)
        atomic_dec(&swu->intr_req_queued);
    
    if (req->status && req->status != -ESHUTDOWN)
        pr_debug("hailo_swu: intr_in req status %d\n", req->status);
}

/* Helper: schedule vmalloc buffer write to file (async) */
static int hailo_swu_load_schedule_write_swu_buffer(struct f_hailo_swu_load *swu)
{
    /* Check if we have vmalloc buffer with data */
    if (!swu->swu_vmalloc_buf || swu->swu_image_received == 0) {
        pr_debug("hailo_swu: No vmalloc buffer data to write\n");
        return 0;
    }
    
    /* Schedule vmalloc buffer write operation in workqueue (process context) */
    swu->swu_file_operation = FILE_OP_WRITE;
    swu->swu_file_result = -EINPROGRESS;
    
    if (!queue_work(swu->file_wq, &swu->swu_file_work)) {
        pr_err("hailo_swu: failed to queue vmalloc buffer write work\n");
        return -EBUSY;
    }
    
    pr_info("hailo_swu: vmalloc buffer write queued for background processing (%zu bytes)\n", 
            swu->swu_image_received);
    return 0;
}

/* Helper: free SWU vmalloc buffer (can be called from any context) */
static void hailo_swu_image_buf_free(struct f_hailo_swu_load *swu)
{
    if (!swu)
        return;
        
    if (swu->swu_vmalloc_buf) {
        pr_info("hailo_swu: freeing vmalloc buffer (%zu bytes)\n", swu->swu_vmalloc_size);
        vfree(swu->swu_vmalloc_buf);
        swu->swu_vmalloc_buf = NULL;
        swu->swu_vmalloc_size = 0;
    }
    
    swu->swu_buffer_ready = false;
}

/* Helper: allocate SWU vmalloc buffer (must be called from process context) */
static int hailo_swu_image_buf_alloc(struct f_hailo_swu_load *swu, size_t size)
{
    /* Validate parameters */
    if (!swu) {
        pr_err("hailo_swu: invalid swu pointer for buffer allocation\n");
        return -EINVAL;
    }
    
    if (size == 0 || size > MAX_SWU_SIZE) {
        pr_err("hailo_swu: invalid SWU buffer size: %zu (max %d)\n", size, MAX_SWU_SIZE);
        return -EINVAL;
    }
    
    /* Free any existing buffer */
    hailo_swu_image_buf_free(swu);
    
    /* Allocate new vmalloc buffer */
    swu->swu_vmalloc_buf = vmalloc(size);
    if (!swu->swu_vmalloc_buf) {
        pr_err("hailo_swu: failed to allocate vmalloc buffer of size %zu\n", size);
        swu->swu_vmalloc_size = 0;
        swu->swu_buffer_ready = false;
        return -ENOMEM;
    }
    
    /* Initialize buffer state */
    swu->swu_vmalloc_size = size;
    swu->swu_buffer_ready = true;
    
    pr_info("hailo_swu: allocated %zu bytes vmalloc buffer for SWU upload\n", size);
    return 0;
}

/* Helper: schedule vmalloc buffer allocation (interrupt-safe) */
static int hailo_swu_load_schedule_swu_buffer_alloc(struct f_hailo_swu_load *swu, size_t size)
{
    /* Validate parameters */
    if (!swu) {
        pr_err("hailo_swu: invalid swu pointer for buffer allocation scheduling\n");
        return -EINVAL;
    }
    
    if (size == 0 || size > MAX_SWU_SIZE) {
        pr_warn("hailo_swu: invalid SWU size %zu, using fallback\n", size);
        swu->swu_buffer_ready = false;
        return -EINVAL;
    }
    
    /* Store requested size - existing buffer will be freed in workqueue */
    swu->swu_vmalloc_requested_size = size;
    swu->swu_file_operation = FILE_OP_ALLOC_VMALLOC;
    swu->swu_file_result = -EINPROGRESS;
    
    if (!queue_work(swu->file_wq, &swu->swu_file_work)) {
        pr_err("hailo_swu: failed to queue vmalloc allocation work\n");
        swu->swu_buffer_ready = false;
        return -EBUSY;
    }
    
    pr_info("hailo_swu: vmalloc allocation queued for %zu bytes\n", size);
    return 0;
}

static int hailo_swu_load_schedule_swu_buffer_free(struct f_hailo_swu_load *swu)
{
    if (!swu->swu_vmalloc_buf) {
        pr_debug("hailo_swu: no vmalloc buffer to free\n");
        return 0;
    }
    
    /* Schedule vmalloc free in workqueue (process context) */
    swu->swu_file_operation = FILE_OP_FREE_VMALLOC;
    swu->swu_file_result = -EINPROGRESS;
    
    if (!queue_work(swu->file_wq, &swu->swu_file_work)) {
        pr_err("hailo_swu: failed to queue vmalloc free work\n");
        return -EBUSY;
    }
    
    pr_info("hailo_swu: vmalloc free queued\n");
    return 0;
}

/* Helper: cleanup SWU resources (safe from atomic context) */
static void hailo_swu_load_cleanup_swu_resources(struct f_hailo_swu_load *swu)
{
    /* Schedule vmalloc buffer free - safe from atomic context */
    if (swu->swu_vmalloc_buf) {
        hailo_swu_load_schedule_swu_buffer_free(swu);
    }
    
    /* File is now handled in deferred mode - no separate close needed */
    swu->swu_file = NULL;
    swu->swu_file_result = 0;
}


/* Kernel thread function to execute system reboot */
static int reboot_thread_fn(void *data)
{
    static char *envp[] = {
        "HOME=/home/root",
        "PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin",
        "SHELL=/bin/sh",
        NULL
    };
    
    static char *argv[] = {
        "/sbin/reboot",
        NULL
    };

    pr_info("hailo_swu: kernel thread executing system reboot\n");
    pr_info("hailo_swu: calling reboot: %s\n", argv[0]);

    /* Execute reboot command */
    call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
    
    /* This should not return, but if it does, log it */
    pr_info("hailo_swu: reboot command completed (unexpected)\n");
    
    return 0;
}

/* Kernel thread function to run swupdate and capture exit code */
static int swupdate_thread_fn(void *data)
{
    struct f_hailo_swu_load *swu = (struct f_hailo_swu_load *)data;
    static char *envp[] = {
        "HOME=/home/root",
        "PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin",
        "SHELL=/bin/sh",
        NULL
    };
    
    static char *argv[] = {
        "/etc/run_swupdate.sh",
        "-l", "/tmp/image.swu",
        "-b", 
        "-d",
        NULL
    };

    if (!swu) {
        pr_err("hailo_sw_update: kernel thread started with NULL context\n");
        return -EINVAL;
    }

    pr_info("hailo_sw_update: kernel thread starting swupdate\n");
    pr_info("hailo_sw_update: calling script: %s %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3], argv[4]);

    /* Execute swupdate and wait for completion */
    swu->swupdate_exit_code = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
    swu->swupdate_exit_code = ((swu->swupdate_exit_code >> 8) & 0xff);
    
    pr_info("hailo_sw_update: swupdate completed with exit code %d\n", swu->swupdate_exit_code);
    
    /* Reset state back to idle now that execution is complete */
    swu->state = HAILO_SWU_STATE__IDLE;
    
    return 0;
}

/* Helper: invoke system reboot */
static int hailo_swu_invoke_reboot(void)
{
    struct task_struct *reboot_task;

    pr_info("hailo_swu: initiating system reboot\n");
    
    /* Create and start kernel thread for reboot */
    reboot_task = kthread_run(reboot_thread_fn, NULL, "hailo_reboot");
    if (IS_ERR(reboot_task)) {
        pr_err("hailo_swu: failed to create reboot thread: %ld\n", PTR_ERR(reboot_task));
        return PTR_ERR(reboot_task);
    }
    
    /* Thread started successfully - system will reboot */
    pr_info("hailo_swu: reboot thread started successfully\n");
    return 0;
}

/* Helper: invoke /usr/bin/sw_update to process the image */
static int hailo_sw_update_invoke_swupdate(struct f_hailo_swu_load *swu)
{
    if (!swu) {
        pr_err("hailo_sw_update: NULL swu context\n");
        return -EINVAL;
    }

    /* Check if swupdate is already running */
    if (swu->swupdate_task && !IS_ERR(swu->swupdate_task)) {
        pr_warn("hailo_sw_update: swupdate already running\n");
        return -EBUSY;
    }

    pr_info("hailo_sw_update: starting swupdate in background thread\n");
    
    /* Reset exit code and set state to executing */
    swu->swupdate_exit_code = -MAX_ERRNO - 1; /* Sentinel: outside errno range */
    swu->state = HAILO_SWU_STATE__EXEC;
    
    /* Create and start kernel thread */
    swu->swupdate_task = kthread_run(swupdate_thread_fn, swu, "hailo_swupdate");
    if (IS_ERR(swu->swupdate_task)) {
        swu->swupdate_exit_code = PTR_ERR(swu->swupdate_task);
        pr_err("hailo_sw_update: failed to create swupdate thread: %d\n",
               swu->swupdate_exit_code);
        swu->swupdate_task = NULL;
        return swu->swupdate_exit_code;
    }
    
    /* Thread started successfully - return immediately */
    return 0;
}

/* Workqueue function to handle file operations in process context */
static void hailo_swu_load_file_work_fn(struct work_struct *work)
{
    struct f_hailo_swu_load *swu = container_of(work, struct f_hailo_swu_load, swu_file_work);

    if (atomic_read(&swu->unbinding)) {
        pr_debug("hailo_swu: file work skipped due to unbinding\n");
        return;
    }

    switch (swu->swu_file_operation) {
    case FILE_OP_ALLOC_VMALLOC:
        /* Free any existing buffer first, then allocate new one (process context - safe to sleep) */
        hailo_swu_image_buf_free(swu);
        swu->swu_file_result = hailo_swu_image_buf_alloc(swu, swu->swu_vmalloc_requested_size);
        break;
        
    case FILE_OP_WRITE:
        /* Open file and write complete vmalloc buffer - called after all chunks received */
        if (swu->swu_vmalloc_buf && swu->swu_image_received > 0) {
            /* Open file for writing */
            const char *filename = "/tmp/image.swu";
            swu->swu_file = filp_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (IS_ERR(swu->swu_file)) {
                swu->swu_file_result = PTR_ERR(swu->swu_file);
                pr_err("hailo_swu: failed to open %s: %d\n", filename, swu->swu_file_result);
                swu->swu_file = NULL;
            } else {
                /* Write complete vmalloc buffer to file */
                loff_t pos = 0;
                ssize_t written = kernel_write(swu->swu_file, swu->swu_vmalloc_buf, swu->swu_image_received, &pos);
                if (written == swu->swu_image_received) {
                    pr_debug("hailo_swu: wrote complete SWU image %zu bytes to %s\n", swu->swu_image_received, filename);
                    swu->swu_file_result = 0;
                } else {
                    pr_err("hailo_swu: failed to write SWU image: %zd/%zu\n", written, swu->swu_image_received);
                    swu->swu_file_result = -EIO;
                }
                
                /* Close file immediately after writing */
                filp_close(swu->swu_file, NULL);
                swu->swu_file = NULL;
                pr_info("hailo_swu: closed %s after writing SWU image\n", filename);
            }
        } else {
            pr_err("hailo_swu: write operation with no vmalloc buffer\n");
            swu->swu_file_result = -EINVAL;
        }
        
        /* Free vmalloc buffer after successful write */
        hailo_swu_image_buf_free(swu);

        /* Invoke sw_update to process the uploaded SWU image */
        hailo_sw_update_invoke_swupdate(swu);
        break;
    case FILE_OP_FREE_VMALLOC:
        /* Free vmalloc buffer in process context */
        hailo_swu_image_buf_free(swu);
        swu->swu_file_result = 0;
        break;
    default:
        pr_err("hailo_swu: unknown file operation %d\n", swu->swu_file_operation);
        swu->swu_file_result = -EINVAL;
        break;
    }
}

/* Helper: write SWU chunk to vmalloc buffer (interrupt-safe) */
static int hailo_swu_load_schedule_write_swu_chunk(struct f_hailo_swu_load *swu, const void *data, size_t len)
{
    /* Check unlikely failure conditions first */
    if (unlikely(!swu->swu_buffer_ready || !swu->swu_vmalloc_buf || 
                 (swu->swu_image_received + len > swu->swu_vmalloc_size))) {
        /* vmalloc buffer not ready - should not happen with deferred file approach */
        pr_err("hailo_swu: vmalloc buffer not ready, dropping %zu bytes\n", len);
        return -EAGAIN;
    }
    
    /* Write directly to vmalloc buffer - interrupt-safe! */
    memcpy((char *)swu->swu_vmalloc_buf + swu->swu_image_received, data, len);
    swu->swu_image_received += len;
    pr_debug("hailo_swu: wrote %zu bytes to vmalloc buffer at offset %zu\n", 
             len, swu->swu_image_received - len);
    
    /* Check if upload is complete */
    if (swu->swu_image_size_expected > 0 && 
        swu->swu_image_received >= swu->swu_image_size_expected) {
        pr_info("hailo_swu: SWU upload to vmalloc buffer complete, writing to file\n");
        swu->swu_file_operation = FILE_OP_WRITE;
        queue_work(swu->file_wq, &swu->swu_file_work);
    }
    
    return 0;
}

/* Bulk-out completion: collect SWU data and SWU image from host */
static void hailo_swu_load_bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct f_hailo_swu_load *swu = req->context;
    int ret;

    /* Safety check: ensure context is valid and function is not being unbound */
    if (!swu || atomic_read(&swu->unbinding))
        return;

    pr_debug("hailo_swu: bulk_out_complete called, status=%d, actual=%u, mode=%s\n", 
            req->status, req->actual, hailo_swu_state_to_string(swu->state));

    /* Debug: Log all bulk completions regardless of status */
    if (req->status != 0) {
        pr_debug("hailo_swu: bulk_out_complete with error status=%d, actual=%u\n", 
                req->status, req->actual);
    }

    if (!req->status && req->actual > 0) {
        size_t got = req->actual;
        
        switch (swu->state) {
        case HAILO_SWU_STATE__UPLOAD:
            /* Handle SWU image upload with streaming writes */
            ret = hailo_swu_load_schedule_write_swu_chunk(swu, req->buf, got);
            if (ret < 0) {
                if (ret == -EAGAIN) {
                    /* File not ready yet - this is a timing issue.
                     * Don't abort upload, just log and continue.
                     * The data will be lost but upload can continue when file is ready. */
                    pr_warn("hailo_swu: SWU chunk dropped (file opening), continuing...\n");
                } else {
                    pr_err("hailo_swu: SWU write failed (%d), aborting upload\n", ret);
                    hailo_swu_load_cleanup_swu_resources(swu);
                    swu->state = HAILO_SWU_STATE__IDLE;
                    return;
                }
            }
            
            pr_debug("hailo_swu: SWU received %zu bytes, total %zu/%zu\n", 
                    got, swu->swu_image_received, swu->swu_image_size_expected);
            
            /* Check if SWU upload is complete */
            if (swu->swu_image_size_expected > 0 && 
                swu->swu_image_received >= swu->swu_image_size_expected) {
                pr_info("hailo_swu: SWU upload complete (%zu bytes), stopping bulk reception\n", 
                       swu->swu_image_received);
                /* Complete upload is handled in FINISH_SWU - just mark as idle here */
                swu->state = HAILO_SWU_STATE__IDLE;
                return;
            } else {
                pr_debug("hailo_swu: SWU upload continuing, need %zu more bytes\n",
                       swu->swu_image_size_expected - swu->swu_image_received);
            }
            break;
            
        case HAILO_SWU_STATE__IDLE:
        default:
            /* Ignore data when in idle mode */
            pr_debug("hailo_swu: ignoring %zu bytes in idle mode\n", got);
            return;
        }
    } else if (req->status) {
        /* Handle USB errors with better diagnostics and recovery */
        if (req->status == -ECONNRESET) {
            pr_info("hailo_swu: bulk request dequeued (status=-104) - this is expected during setup\n");
            /* This is from usb_ep_dequeue() in LOAD_SWU - don't requeue */
            return;
        } else if (req->status == -ESHUTDOWN) {
            pr_info("hailo_swu: USB endpoint shutdown, stopping bulk transfers\n");
            return;
        } else {
            pr_warn("hailo_swu: bulk_out req status %d, continuing...\n", req->status);
        }
    }

    /* Re-submit to keep accepting data if not in idle mode and endpoint is still enabled */
    if (swu->state != HAILO_SWU_STATE__IDLE && ep && ep->enabled) {
        req->length = hailo_swu_load_get_bulk_transfer_size(swu);
        pr_debug("hailo_swu: requeuing bulk_out request (mode=%s, length=%u)\n", 
                hailo_swu_state_to_string(swu->state), req->length);
        ret = usb_ep_queue(ep, req, GFP_ATOMIC);
        if (ret) {
            pr_warn("hailo_swu: failed to requeue bulk_out request: %d\n", ret);
        } else {
            pr_debug("hailo_swu: bulk_out request requeued successfully\n");
        }
    } else {
        pr_info("hailo_swu: not requeuing bulk_out request (mode=%s, ep_enabled=%d)\n", 
                hailo_swu_state_to_string(swu->state), ep ? ep->enabled : -1);
    }
}

/* Helper: Send EP0 acknowledgment for vendor requests */
static int hailo_swu_load_ep0_ack(struct usb_composite_dev *cdev, struct usb_request *req, const char *context)
{
    int ret;
    
    /* Ack (zero-length) */
    req->length = 0;
    ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
    if (ret)
        pr_err("hailo_swu: ep0 queue fail %s %d\n", context, ret);
    else
        ret = 0;
        
    return ret;
}

/* EP0 setup handler for SWU-load-specific vendor requests */
static int hailo_swu_load_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
    struct f_hailo_swu_load *swu = to_f_hailo_swu_load(f);
    struct usb_composite_dev *cdev = f->config->cdev;
    struct usb_request *req = cdev->req;
    unsigned value = le16_to_cpu(ctrl->wValue);
    unsigned length = le16_to_cpu(ctrl->wLength);
    unsigned resp_length;
    int ret = -EOPNOTSUPP;

    /* Only support vendor requests */
    if ((ctrl->bRequestType & USB_TYPE_MASK) != USB_TYPE_VENDOR)
        return ret;

    switch (ctrl->bRequest) {
    case HAILO_REQ__SWU_GET_STATUS:
        /* Send processing status - prioritize SWU upload */
        if (!req->buf) {
            pr_err("hailo_swu: ep0 req buffer missing\n");
            return -ENOMEM;
        }
        
        /* Calculate final response length considering all constraints */
        resp_length = min_t(unsigned, length, HAILO_SWU_EP0_STATUS_RESPONSE_SIZE);
        resp_length = min_t(unsigned, resp_length, hailo_swu_load_get_ep0_maxpacket(cdev));
        
        /* Now use the resolved length for buffer operations */
        ret = hailo_swu_state_update(swu, req, resp_length);
        if (ret) {
            goto swu_load_ack;
        }
		
        ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
        if (ret)
            pr_err("hailo_swu: ep0 queue fail status %d\n", ret);
        else
            ret = 0;
        break;

    case HAILO_REQ__SWU_GET_BUILD_INFO:
        /* Send SWU model information */
        if (!req->buf) {
            pr_err("hailo_swu: ep0 req buffer missing\n");
            return -ENOMEM;
        }
        /* Calculate final response length considering all constraints */
        req->length = min_t(unsigned, length, strlen("Hailo Linux build vXXX.XXX.XXX") + 1);
        req->length = min_t(unsigned, req->length, hailo_swu_load_get_ep0_maxpacket(cdev));
        
        /* Now use the resolved length for buffer operations */
        memset(req->buf, 0, req->length);
        snprintf((char *)req->buf, req->length, "Hailo Linux build v%d.%d.%d",
                 (HAILO_LINUX_RELEASE_BUILD_VERSION >> 24) & 0xff,
                 (HAILO_LINUX_RELEASE_BUILD_VERSION >> 16) & 0xff,
                 HAILO_LINUX_RELEASE_BUILD_VERSION & 0xffff);
        ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
        if (ret)
            pr_err("hailo_swu: ep0 queue fail info %d\n", ret);
        else
            ret = 0;
        break;

    case HAILO_REQ__SWU_LOAD:
        /* Reconstruct 32-bit file size from wValue (lower 16 bits) and wIndex (upper 16 bits) */
        {
            unsigned int full_size = (le16_to_cpu(ctrl->wIndex) << 16) | value;
            pr_info("hailo_swu: LOAD_SWU command, expected size=%u (0x%08x)\n", full_size, full_size);
            swu->swu_image_received = 0;
            swu->swu_image_size_expected = full_size;
        
            /* Schedule vmalloc buffer allocation (interrupt-safe) */
            ret = hailo_swu_load_schedule_swu_buffer_alloc(swu, full_size);
            if (ret && ret != -EINVAL) {
                /* Only fail on serious errors, continue with fallback for invalid size */
                pr_err("hailo_swu: failed to schedule buffer allocation: %d\n", ret);
                swu->state = HAILO_SWU_STATE__IDLE;
                goto swu_load_ack;
            }
        }
        
        swu->state = HAILO_SWU_STATE__UPLOAD;
        
        /* Prepare bulk endpoint for SWU data - only dequeue if actually queued */
        pr_debug("hailo_swu: preparing bulk endpoint, current req status=%d\n", swu->bulk_out_req->status);
        if (swu->bulk_out_req->status == -EINPROGRESS) {
            pr_debug("hailo_swu: dequeuing active bulk_out request\n");
            usb_ep_dequeue(swu->bulk_out_ep, swu->bulk_out_req);
            /* Give time for dequeue completion to avoid race */
            udelay(10); // 10 microseconds
        }
        
        /* Configure and queue request for SWU data */
        swu->bulk_out_req->length = hailo_swu_load_get_bulk_transfer_size(swu);
        
        /* Reset request status from any previous dequeue operation */
        swu->bulk_out_req->status = 0;
        swu->bulk_out_req->actual = 0;
        
        pr_debug("hailo_swu: about to queue bulk request, ep enabled=%d, req status=%d, length=%u\n",
                swu->bulk_out_ep->enabled, swu->bulk_out_req->status, swu->bulk_out_req->length);
        
        ret = usb_ep_queue(swu->bulk_out_ep, swu->bulk_out_req, GFP_ATOMIC);
        if (ret) {
            pr_err("hailo_swu: failed to queue bulk request for SWU: %d\n", ret);
            hailo_swu_load_cleanup_swu_resources(swu);
            swu->state = HAILO_SWU_STATE__IDLE;
        } else {
            pr_debug("hailo_swu: bulk_out request queued for SWU upload (length=%u)\n", swu->bulk_out_req->length);
        }

swu_load_ack:
        
        /* Ack (zero-length) */
        ret = hailo_swu_load_ep0_ack(cdev, req, "load_swu");
        break;

    case HAILO_REQ__SWU_FINISH:
        pr_debug("hailo_swu: FINISH_SWU command, received %zu bytes\n", swu->swu_image_received);
        
        swu->state = HAILO_SWU_STATE__IDLE;
        
        /* Write complete vmalloc buffer to file (includes open, write, close) */
        hailo_swu_load_schedule_write_swu_buffer(swu);
        pr_debug("hailo_swu: SWU image streaming completed successfully (%zu bytes)\n", swu->swu_image_received);
        
        /* Ack (zero-length) */
        ret = hailo_swu_load_ep0_ack(cdev, req, "finish_swu");
        break;

    case HAILO_REQ__SWU_GET_BOARD_SKU_ID:
        /* Send board SKU ID information */
        if (!req->buf) {
            pr_err("hailo_swu: ep0 req buffer missing\n");
            return -ENOMEM;
        }
        {
            u32 board_sku_id;
            int ret;
            
            /* Get board SKU ID via hailo gadget function */
            ret = hailo_gadget_get_board_sku_id(&board_sku_id);
            if (ret) {
                pr_err("hailo_swu: failed to get board SKU ID: %d\n", ret);
                snprintf((char *)req->buf, length, "SKU_ID_ERROR");
                req->length = strlen("SKU_ID_ERROR") + 1;
            } else {
                /* Get board description string */
                board_sku_id_to_str(board_sku_id, (char *)req->buf, length);
                req->length = min_t(unsigned, length, strlen((char *)req->buf) + 1);
                req->length = min_t(unsigned, req->length, hailo_swu_load_get_ep0_maxpacket(cdev));
            }
        }
        
        ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
        if (ret)
            pr_err("hailo_rfs: ep0 queue fail board_sku_id %d\n", ret);
        else
            ret = 0;
        break;

    case HAILO_REQ__SWU_CTRL:
        /* Check if SWU buffer is ready for upload and return bytes received count */
        if (!req->buf) {
            pr_err("hailo_swu: ep0 req buffer missing for SWU ready check\n");
            return -ENOMEM;
        }
        
        switch (value) {
        case HAILO_REQ__SWU_CTRL__GET_STATUS:
            /* Original behavior: Send single byte: 1 if ready, 0 if not ready */
            req->length = 1;
            *((unsigned char *)req->buf) = swu->swu_buffer_ready ? 1 : 0;
            break;
        case HAILO_REQ__SWU_CTRL__GET_RX_CNT:
            /* Enhanced behavior: Return current bytes received count (32-bit) */
            req->length = 4;
            *((unsigned int *)req->buf) = cpu_to_le32((u32)swu->swu_image_received);
            pr_debug("hailo_swu: returning SWU bytes received: %zu\n", swu->swu_image_received);
            break;
        case HAILO_REQ__SWU_CTRL__CLR_RX_CNT:
            /* Reset SWU counter (for ping cleanup) */
            pr_debug("hailo_swu: resetting SWU counter from %zu to 0\n", swu->swu_image_received);
            swu->swu_image_received = 0;
            req->length = 1;
            *((unsigned char *)req->buf) = 1; /* Return success */
            break;
        case HAILO_REQ__SWU_CTRL__GET_EXECUTION_STATUS:
            /* Return swupdate state and exit code */
            req->length = 8; /* 4-byte state + 4-byte exit code */
            /* Determine swupdate state based on task and exit code */
            if (swu->swupdate_exit_code == -MAX_ERRNO - 1) {
                *((int *)req->buf + 1) = cpu_to_le32((u32)(-MAX_ERRNO - 1)); /* no exit code yet */
                if (!swu->swupdate_task) {
                    /* Never started */
                    *((int *)req->buf) = cpu_to_le32(HAILO_SWUPDATE_EXEC_STATE_IDLE);
                    pr_info("hailo_swu: state=idle, exit_code=%d\n", -MAX_ERRNO - 1);
                } else {
                    /* Task is running */
                    *((int *)req->buf) = cpu_to_le32(HAILO_SWUPDATE_EXEC_STATE_IN_PROGRESS);
                    pr_info("hailo_swu: state=in-progress, exit_code=%d\n", -MAX_ERRNO - 1);
                }
            } else if (swu->swupdate_exit_code == 0) {
                /* Completed successfully */
                *((int *)req->buf) = cpu_to_le32(HAILO_SWUPDATE_EXEC_STATE_END_OK);
                *((int *)req->buf + 1) = cpu_to_le32(swu->swupdate_exit_code);
                pr_info("hailo_swu: state=end-ok, exit_code=%d\n", swu->swupdate_exit_code);
            } else {
                /* Completed with error */
                *((int *)req->buf) = cpu_to_le32(HAILO_SWUPDATE_EXEC_STATE_END_FAIL);
                *((int *)req->buf + 1) = cpu_to_le32(swu->swupdate_exit_code);
                pr_info("hailo_swu: state=end-fail, exit_code=%d\n", swu->swupdate_exit_code);
                swu->swupdate_task = NULL; /* Clear task pointer after completion */
            }
            break;
        default:
            pr_err("hailo_swu: invalid SWU control sub-command: %d\n", value);
            return -EINVAL;
        }
        
        ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
        if (ret) {
            pr_err("hailo_swu: ep0 queue fail swu_ready %d\n", ret);
        } else {
            pr_debug("hailo_swu: SWU ready status: %s\n", swu->swu_buffer_ready ? "ready" : "not ready");
            ret = 0;
        }
        break;

    case HAILO_REQ__SWU_SYS_REBOOT:
        pr_info("hailo_swu: SYS_REBOOT command received\n");
        
        /* Invoke system reboot */
        ret = hailo_swu_invoke_reboot();
        if (ret < 0) {
            pr_err("hailo_swu: failed to initiate reboot: %d\n", ret);
        }
        
        /* Ack the request regardless of reboot result */
        ret = hailo_swu_load_ep0_ack(cdev, req, "sys_reboot");
        break;

    default:
        pr_info("hailo_swu: unknown vendor req %02x\n", ctrl->bRequest);
        ret = -EOPNOTSUPP;
    }

    return ret;
}

static int hailo_swu_load_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
    struct f_hailo_swu_load *swu = to_f_hailo_swu_load(f);
    struct usb_composite_dev *cdev = f->config->cdev;
    int ret;

    pr_info("hailo_swu: set_alt intf %u alt %u speed %d\n", intf, alt, cdev->gadget->speed);

    /* Reset unbinding flag - function is being reconfigured/enabled */
    atomic_set(&swu->unbinding, 0);

    /* Validate interface and alternate setting */
    if (intf != hailo_swu_load_intf_desc.bInterfaceNumber) {  /* Use dynamically assigned interface number */
        pr_err("hailo_swu: invalid interface %u (expected %u)\n", intf, hailo_swu_load_intf_desc.bInterfaceNumber);
        return -EINVAL;
    }

    if (alt != HAILO_SWU_ALT_SETTING_STANDARD) {
        pr_err("hailo_swu: unsupported alternate setting %u\n", alt);
        return -EINVAL;
    }

    /* Disable endpoints if they're currently enabled */
    if (swu->bulk_out_ep && swu->bulk_out_ep->enabled)
        usb_ep_disable(swu->bulk_out_ep);
        
    if (swu->intr_in_ep && swu->intr_in_ep->enabled)
        usb_ep_disable(swu->intr_in_ep);

    /* Stop status timer during reconfiguration */
    del_timer_sync(&swu->status_timer);

    /* Configure bulk OUT endpoint */
    ret = config_ep_by_speed(cdev->gadget, f, swu->bulk_out_ep);
    if (ret) {
        pr_err("hailo_swu: failed to configure bulk_out ep: %d\n", ret);
        return ret;
    }

    ret = usb_ep_enable(swu->bulk_out_ep);
    if (ret) {
        pr_err("hailo_swu: failed to enable bulk_out ep: %d\n", ret);
        return ret;
    }

    /* Configure interrupt IN endpoint */
    ret = config_ep_by_speed(cdev->gadget, f, swu->intr_in_ep);
    if (ret) {
        pr_err("hailo_swu: failed to configure intr_in ep: %d\n", ret);
        goto fail;
    }

    ret = usb_ep_enable(swu->intr_in_ep);
    if (ret) {
        pr_err("hailo_swu: failed to enable intr_in ep: %d\n", ret);
        goto fail;
    }

    /* Queue initial bulk-out request to receive data */
    ret = usb_ep_queue(swu->bulk_out_ep, swu->bulk_out_req, GFP_ATOMIC);
    if (ret) {
        pr_err("hailo_swu: failed to queue bulk_out request: %d\n", ret);
        goto fail;
    }

    /* Temporarily disable timer start to debug RCU stall */
    /* Restart status timer only if not unbinding */
    //if (!atomic_read(&swu->unbinding))
    //    mod_timer(&swu->status_timer, jiffies + msecs_to_jiffies(1000));

    pr_info("hailo_swu: endpoints enabled for alt %u speed %d\n", alt, cdev->gadget->speed);
    return 0;

fail:
    if (swu->bulk_out_ep && swu->bulk_out_ep->enabled)
        usb_ep_disable(swu->bulk_out_ep);
    if (swu->intr_in_ep && swu->intr_in_ep->enabled)
        usb_ep_disable(swu->intr_in_ep);
    return ret;
}

static int hailo_swu_load_get_alt(struct usb_function *f, unsigned intf)
{
    /* Validate interface number */
    if (intf != hailo_swu_load_intf_desc.bInterfaceNumber) {
        pr_err("hailo_swu: invalid interface %u in get_alt (expected %u)\n", intf, hailo_swu_load_intf_desc.bInterfaceNumber);
        return -EINVAL;
    }
    
    /* SWU mode only supports standard alternate setting */
    return HAILO_SWU_ALT_SETTING_STANDARD;
}

static void hailo_swu_load_disable(struct usb_function *f)
{
    struct f_hailo_swu_load *swu = to_f_hailo_swu_load(f);
    int rc;

    /* Early return if already disabled to avoid duplicate operations */
    if (swu->bulk_out_ep && !swu->bulk_out_ep->enabled && 
        swu->intr_in_ep && !swu->intr_in_ep->enabled) {
        pr_info("hailo_swu: disable called again (already disabled)\n");
        return;
    }
    
    /* Set unbinding flag first to stop timer from rescheduling */
    atomic_set(&swu->unbinding, 1);
    
    /* Stop status timer and ensure it's completely stopped */
    del_timer_sync(&swu->status_timer);
    
    /* Add memory barrier to ensure timer sees unbinding flag */
    smp_mb();
    
    /* Stop any ongoing operations */
    swu->state = HAILO_SWU_STATE__IDLE;
    
    /* Note: We avoid manual dequeue during disable to prevent race conditions.
     * The USB controller will handle cleanup when endpoints are disabled. */
    
    /* Disable endpoints with additional safety checks - prevent double disable */
    if (swu->bulk_out_ep && swu->bulk_out_ep->enabled) {
        pr_info("hailo_swu: bulk_out_ep disabling...\n");
        rc = usb_ep_disable(swu->bulk_out_ep);
        if (rc) {
            pr_err("hailo_swu: bulk_out_ep disabling failed, rc = %d\n", rc);
        } else {
            pr_info("hailo_swu: bulk_out_ep disabled successfully\n");
        }
    }
    
    if (swu->intr_in_ep && swu->intr_in_ep->enabled) {
        pr_info("hailo_swu: intr_in_ep disabling...\n");
        rc = usb_ep_disable(swu->intr_in_ep);
        if (rc) {
            pr_err("hailo_swu: intr_in_ep disabling failed, rc = %d\n", rc);
        } else {
            pr_info("hailo_swu: intr_in_ep disabled successfully\n");
        }
    }
    
    pr_info("hailo_swu: disabled\n");
}

static int hailo_swu_load_bind(struct usb_configuration *c, struct usb_function *f)
{
    struct f_hailo_swu_load_opts *opts = to_f_hailo_swu_load_opts(f->fi);
    struct f_hailo_swu_load *swu = to_f_hailo_swu_load(f);
    struct usb_composite_dev *cdev = c->cdev;
    struct usb_string *us;
    int ret;

    pr_info("hailo_swu: bind...\n");
    
    mutex_lock(&opts->lock);
    if (opts->bound) {
        mutex_unlock(&opts->lock);
        return -EBUSY;
    }
    opts->bound = true;
    mutex_unlock(&opts->lock);

    /* Allocate string IDs */
    us = usb_gstrings_attach(cdev, hailo_swu_load_strings, ARRAY_SIZE(hailo_swu_load_string_defs));
    if (IS_ERR(us))
        return PTR_ERR(us);
    hailo_swu_load_intf_desc.iInterface = us[HAILO_SWU_INTERFACE_IDX].id;

    /* Allocate dynamic interface ID */
    ret = usb_interface_id(c, f);
    if (ret < 0) {
        pr_err("hailo_swu: failed to allocate interface ID, rc = %d\n", ret);
        goto fail;
    }
    hailo_swu_load_intf_desc.bInterfaceNumber = ret;
    pr_info("hailo_swu: assigned interface ID %d\n", ret);

    /* Initialize vmalloc buffer fields */
    swu->swu_vmalloc_buf = NULL;
    swu->swu_vmalloc_size = 0;
    swu->swu_buffer_ready = false;

    /* Initialize workqueue for file operations */
    swu->file_wq = alloc_workqueue("hailo_swu_load_file", WQ_UNBOUND, 1);
    if (!swu->file_wq) {
        pr_err("hailo_swu: failed to create file workqueue\n");
        ret = -ENOMEM;
        goto fail;
    }
    INIT_WORK(&swu->swu_file_work, hailo_swu_load_file_work_fn);

    /* Find endpoints */
    swu->bulk_out_ep = usb_ep_autoconfig(cdev->gadget, &hailo_swu_load_fs_bulk_out_desc);
    if (!swu->bulk_out_ep) {
        pr_err("hailo_swu: no bulk-out ep\n");
        ret = -ENODEV;
        goto fail;
    }

    swu->intr_in_ep = usb_ep_autoconfig(cdev->gadget, &hailo_swu_load_fs_intr_in_desc);
    if (!swu->intr_in_ep) {
        pr_err("hailo_swu: no intr-in ep\n");
        ret = -ENODEV;
        goto fail;
    }

    /* Configure endpoints for different speeds */
    hailo_swu_load_hs_bulk_out_desc.bEndpointAddress = hailo_swu_load_fs_bulk_out_desc.bEndpointAddress;
    hailo_swu_load_hs_intr_in_desc.bEndpointAddress = hailo_swu_load_fs_intr_in_desc.bEndpointAddress;
    hailo_swu_load_ss_bulk_out_desc.bEndpointAddress = hailo_swu_load_fs_bulk_out_desc.bEndpointAddress;
    hailo_swu_load_ss_intr_in_desc.bEndpointAddress = hailo_swu_load_fs_intr_in_desc.bEndpointAddress;

    /* Assign descriptor arrays */
    ret = usb_assign_descriptors(f, hailo_swu_load_fs_function, hailo_swu_load_hs_function,
                                hailo_swu_load_ss_function, hailo_swu_load_ss_function);
    if (ret)
        goto fail;

    /* Prepare bulk-out request */
    swu->bulk_out_req = usb_ep_alloc_request(swu->bulk_out_ep, GFP_KERNEL);
    if (!swu->bulk_out_req) { 
        ret = -ENOMEM; 
        goto fail; 
    }
    swu->bulk_out_req->buf = kzalloc(HAILO_SWU_BULK_OUT_BUFFER_SIZE, GFP_KERNEL);
    if (!swu->bulk_out_req->buf) {
        ret = -ENOMEM;
        goto fail;
    }
    swu->bulk_out_req->length = HAILO_SWU_BULK_OUT_BUFFER_SIZE;
    swu->bulk_out_req->complete = hailo_swu_load_bulk_out_complete;
    swu->bulk_out_req->context = swu;

    /* Prepare interrupt request */
    swu->intr_in_req = usb_ep_alloc_request(swu->intr_in_ep, GFP_KERNEL);
    if (!swu->intr_in_req) {
        ret = -ENOMEM;
        goto fail;
    }
    swu->intr_in_req->buf = kzalloc(HAILO_SWU_INTERRUPT_MSG_SIZE, GFP_KERNEL);
    if (!swu->intr_in_req->buf) {
        ret = -ENOMEM;
        goto fail;
    }
    swu->intr_in_req->complete = hailo_swu_load_intr_in_complete;
    swu->intr_in_req->context = swu;

    /* Initialize atomic counter for interrupt requests */
    atomic_set(&swu->intr_req_queued, 0);
    atomic_set(&swu->unbinding, 0);

    /* Initialize status timer */
    timer_setup(&swu->status_timer, hailo_swu_load_status_timer_fn, 0);
    
    /* Timer will be started manually when needed, not automatically */

    swu->state = HAILO_SWU_STATE__IDLE;
    swu->swu_image_received = 0;
    swu->swu_image_size_expected = 0;
    swu->swu_file = NULL;

    pr_info("hailo_swu: bulk_out ep %s, intr_in ep %s\n",
            swu->bulk_out_ep->name, swu->intr_in_ep->name);
    pr_info("hailo_swu: bind completed\n");

    return 0;

fail:
    free_ep_req(swu->intr_in_ep, swu->intr_in_req);
    free_ep_req(swu->bulk_out_ep, swu->bulk_out_req);
    return ret;
}

static void hailo_swu_load_unbind(struct usb_configuration *c, struct usb_function *f)
{
    struct f_hailo_swu_load_opts *opts = to_f_hailo_swu_load_opts(f->fi);
    struct f_hailo_swu_load *swu = to_f_hailo_swu_load(f);
    int rc;

    pr_info("hailo_swu: unbind...\n");
    
    /* Set unbinding flag to prevent completion callbacks from accessing freed memory */
    atomic_set(&swu->unbinding, 1);
    
    /* Stop status timer to prevent further interrupt queuing during cleanup */
    del_timer_sync(&swu->status_timer);
    
    /* Reset atomic counter and ensure no more interrupt requests are queued */
    atomic_set(&swu->intr_req_queued, 0);
    
    /* Clear configuration reference to prevent timer from queuing requests */
    swu->func.config = NULL;
    
    /* Mark the function as unbound to prevent timer from rescheduling */
    mutex_lock(&opts->lock);
    opts->bound = false;
    mutex_unlock(&opts->lock);
    
    usb_free_all_descriptors(f);
    
    /* Cleanup workqueue and wait for any pending file operations */
    if (swu->file_wq) {
        flush_workqueue(swu->file_wq);
        destroy_workqueue(swu->file_wq);
        swu->file_wq = NULL;
    }
    
    /* Dequeue any remaining requests as a safety measure (after disable) */
    if (swu->bulk_out_req && swu->bulk_out_ep) {
        rc = usb_ep_dequeue(swu->bulk_out_ep, swu->bulk_out_req);
        if (rc && rc != -EINVAL) /* -EINVAL means request wasn't queued, which is fine */
            pr_err("hailo_swu: bulk_out_req dequeue failed, rc = %d\n", rc);
        else
            pr_debug("hailo_swu: bulk_out_req dequeue completed\n");
    }
    if (swu->intr_in_req && swu->intr_in_ep) {
        rc = usb_ep_dequeue(swu->intr_in_ep, swu->intr_in_req);
        if (rc && rc != -EINVAL) /* -EINVAL means request wasn't queued, which is fine */
            pr_err("hailo_swu: intr_in_req dequeue failed, rc = %d\n", rc);
        else
            pr_debug("hailo_swu: intr_in_req dequeue completed\n");
    }
          
    /* DO NOT set completion callbacks to NULL - this causes race conditions! 
     * The USB controller may still call these callbacks even after dequeue.
     * Instead, rely on the atomic unbinding flag to make callbacks no-op.
     * The completion callbacks will safely check atomic_read(&swu->unbinding) 
     * and return early if unbinding is in progress.
     */
    
    /* Free USB requests and their buffers */
    free_ep_req(swu->intr_in_ep, swu->intr_in_req);
    free_ep_req(swu->bulk_out_ep, swu->bulk_out_req);

    /* Free vmalloc buffer if allocated */
    hailo_swu_image_buf_free(swu);
    pr_info("hailo_swu: unbind completed\n");
}

static void hailo_swu_load_free_func(struct usb_function *f)
{
    struct f_hailo_swu_load_opts *opts;
    struct f_hailo_swu_load *swu = to_f_hailo_swu_load(f);

    if (!swu) {
        pr_err("hailo_swu: free_func called with NULL swu pointer\n");
        return;
    }

    /* Timer should already be stopped by unbind(), but ensure it's flagged */
    atomic_set(&swu->unbinding, 1);

    opts = container_of(f->fi, struct f_hailo_swu_load_opts, func_inst);
    mutex_lock(&opts->lock);
    opts->refcnt--;
    mutex_unlock(&opts->lock);
    
    pr_debug("hailo_swu: freeing function structure\n");
    kfree(swu);
}

/* Function instance management */
static void hailo_swu_load_attr_release(struct config_item *item)
{
    struct f_hailo_swu_load_opts *opts = container_of(to_config_group(item),
                                               struct f_hailo_swu_load_opts, 
                                               func_inst.group);
    usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations hailo_swu_load_item_ops = {
    .release = hailo_swu_load_attr_release,
};

static struct config_item_type hailo_swu_load_func_type = {
    .ct_item_ops = &hailo_swu_load_item_ops,
    .ct_owner = THIS_MODULE,
};

static void hailo_swu_load_free_inst(struct usb_function_instance *fi)
{
    struct f_hailo_swu_load_opts *opts = to_f_hailo_swu_load_opts(fi);

    mutex_destroy(&opts->lock);
    kfree(opts);
}

static struct usb_function_instance *hailo_swu_load_alloc_inst(void)
{
    struct f_hailo_swu_load_opts *opts;

    opts = kzalloc(sizeof(*opts), GFP_KERNEL);
    if (!opts)
        return ERR_PTR(-ENOMEM);

    mutex_init(&opts->lock);
    opts->func_inst.set_inst_name = NULL;
    opts->func_inst.free_func_inst = hailo_swu_load_free_inst;
    
    /* Initialize default values */
    opts->status_interval_ms = 1000;
    opts->bound = false;
    opts->refcnt = 0;

    config_group_init_type_name(&opts->func_inst.group, "", &hailo_swu_load_func_type);

    return &opts->func_inst;
}

static struct usb_function *hailo_swu_load_alloc_func(struct usb_function_instance *fi)
{
    struct f_hailo_swu_load_opts *opts = to_f_hailo_swu_load_opts(fi);
    struct f_hailo_swu_load *swu;

    mutex_lock(&opts->lock);
    opts->refcnt++;
    mutex_unlock(&opts->lock);

    swu = kzalloc(sizeof(*swu), GFP_KERNEL);
    if (!swu) {
        mutex_lock(&opts->lock);
        opts->refcnt--;
        mutex_unlock(&opts->lock);
        return ERR_PTR(-ENOMEM);
    }

    /* Initialize swupdate state */
    swu->swupdate_task = NULL;
    swu->swupdate_exit_code = -MAX_ERRNO - 1; /* Sentinel: never started/in progress */

    swu->func.name = "hailo_swu_load";
    swu->func.strings = hailo_swu_load_strings;
    swu->func.bind = hailo_swu_load_bind;
    swu->func.unbind = hailo_swu_load_unbind;
    swu->func.setup = hailo_swu_load_setup;
    swu->func.set_alt = hailo_swu_load_set_alt;
    swu->func.get_alt = hailo_swu_load_get_alt;
    swu->func.disable = hailo_swu_load_disable;
    swu->func.free_func = hailo_swu_load_free_func;

    return &swu->func;
}

DECLARE_USB_FUNCTION_INIT(hailo_swu_load, hailo_swu_load_alloc_inst, hailo_swu_load_alloc_func);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(HAILO_SWU_DRIVER_DESC);
MODULE_AUTHOR("Hailo Technologies Ltd.");
