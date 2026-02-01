/*
 * f_hailo_rfs_load.c -- USB function driver for Hailo RFS load Device Mode
 *
 * Hailo RFS load Device Mode:
 * - Single interface with alternate setting 0 only
 * - Bulk OUT, Interrupt IN (status) endpoints
 * - RFS image upload functionality
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

#include "u_hailo_rfs_load.h"

/* External function to get board SKU ID from soc-hailo driver */
extern void board_sku_id_to_str(u32 board_sku_id, char *name, size_t name_size);

/* External function to get board SKU ID via SCMI from hailo gadget driver */
extern int hailo_gadget_get_board_sku_id(u32 *board_sku_id);

#define HAILO_RFS_DRIVER_DESC "Hailo RFS load Device Function"

/* Vendor-specific control requests for RFS load mode */
#define HAILO_REQ__RFS_GET_STATUS       0x03 /* HAILO_REQ__RFS_GET_STATUS (Get function status): 
                                              *   - idle:ready    Endpoint idle, not processing
                                              *   - load:xxxxxx   Uploading RFS image, <xxxxxx> bytes received,
                                              *   - invalid:xxxx  Invalid state or error
                                              */

#define HAILO_REQ__RFS_GET_BUILD_INFO   0x11 /* Get RFS model information */
#define HAILO_REQ__RFS_LOAD             0x12 /* Load RFS image command */
#define HAILO_REQ__RFS_FINISH           0x13 /* Finish RFS loading */
#define HAILO_REQ__RFS_GET_BOARD_SKU_ID 0x14 /* Get board SKU ID */
#define HAILO_REQ__RFS_CTRL             0x15 /* RFS control operations */

/* RFS Control Sub-commands */
#define HAILO_REQ__RFS_CTRL__GET_STATUS    0 /* Return ready status (1 byte) */
#define HAILO_REQ__RFS_CTRL__GET_RX_CNT    1 /* Return bytes received count (4 bytes) */
#define HAILO_REQ__RFS_CTRL__CLR_RX_CNT    2 /* Reset RFS counter (1 byte response) */

/* Sizes */
#define HAILO_RFS_BULK_OUT_BUFFER_SIZE    SZ_64K /* RFS load request max data transfer size: 64 KiB (with ZLP flush) */

#define HAILO_RFS_EP0_BUFFER_SIZE  64           /* EP0 control request buffer size */
#define HAILO_RFS_EP0_STATUS_RESPONSE_SIZE  16  /* EP0 status response buffer size */
#define HAILO_RFS_INTERRUPT_MSG_SIZE  16

#define MAX_RFS_SIZE                   (SZ_256M)  /* Maximum RFS image size: 256MB */

/* Interface number */
#define HAILO_RFS_INTERFACE_NUM      0

/* Alternate setting values */
#define HAILO_RFS_ALT_SETTING_STANDARD  0

/* Bulk OUT endpoint operation modes */
enum hailo_rfs_state {
    HAILO_RFS_STATE__IDLE = 0,         /* Endpoint idle, not processing */
    HAILO_RFS_STATE__UPLOAD,           /* Uploading RFS image */
};

/* RFS file operations for workqueue */
enum hailo_rfs_load_file_operation {
    FILE_OP_ALLOC_VMALLOC = 0,     /* Allocate vmalloc buffer (must be in process context) */
    FILE_OP_WRITE = 1,             /* Write complete vmalloc buffer to RFS file */
    FILE_OP_FREE_VMALLOC = 2,      /* Free vmalloc buffer (must be in process context) */
};

struct f_hailo_rfs_load {
    struct usb_function func;
    
    /* Endpoints */
    struct usb_ep *bulk_out_ep;
    struct usb_ep *intr_in_ep;
    
    /* EP0 control reply buffer */
    struct usb_request *ep0_req;
    
    /* Bulk OUT endpoint operation mode */
    enum hailo_rfs_state state;
    
    /* RFS image streaming */
    size_t rfs_image_received;
    size_t rfs_image_size_expected;
    struct file *rfs_file;  /* Open file for final write */
    
    /* vmalloc buffer for entire RFS image */
    void *rfs_vmalloc_buf;   /* vmalloc buffer for entire RFS image */
    size_t rfs_vmalloc_size; /* Size of vmalloc buffer */
    bool rfs_buffer_ready;   /* Flag indicating vmalloc buffer is ready */
    
    /* Bulk-out request for receiving RFS data and RFS image */
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
    struct work_struct rfs_file_work;
    struct workqueue_struct *file_wq;
    
    /* RFS file operation parameters */
    enum hailo_rfs_load_file_operation rfs_file_operation;  /* File operation type */
    int rfs_file_result;                              /* Result of file operation */
    size_t rfs_vmalloc_requested_size;                /* Size to allocate for vmalloc buffer */
};

/* Helper function to get f_hailo_rfs_load from usb_function pointer */
static inline struct f_hailo_rfs_load *to_f_hailo_rfs_load(struct usb_function *f)
{
    return container_of(f, struct f_hailo_rfs_load, func);
}

/* Helper function to get f_hailo_rfs_load_opts from usb_function_instance pointer */
static inline struct f_hailo_rfs_load_opts *to_f_hailo_rfs_load_opts(const struct usb_function_instance *fi)
{
    return container_of(fi, struct f_hailo_rfs_load_opts, func_inst);
}

/* Helper: convert bulk mode to readable string */
static inline const char *hailo_rfs_state_to_string(int mode)
{
    switch (mode) {
        case HAILO_RFS_STATE__IDLE:          return "IDLE";
        case HAILO_RFS_STATE__UPLOAD:        return "UPLOAD";
        default:                             return "UNKNOWN";
    }
}

/* Helper: get optimal bulk transfer size based on USB speed and endpoint */
static inline size_t hailo_rfs_load_get_bulk_transfer_size(struct f_hailo_rfs_load *rfs)
{
    size_t max_packet_size;
    size_t optimal_size;
    
    if (!rfs->bulk_out_ep || !rfs->bulk_out_ep->desc)
        return HAILO_RFS_BULK_OUT_BUFFER_SIZE;
    
    max_packet_size = usb_endpoint_maxp(rfs->bulk_out_ep->desc);
    
    /* Use multiple packets for better performance, but don't exceed our buffer */
    optimal_size = max_packet_size * 32; /* 32 packets per transfer */
    
    return min_t(size_t, optimal_size, HAILO_RFS_BULK_OUT_BUFFER_SIZE);
}

/* Helper: get EP0 max packet size based on current USB speed */
static inline unsigned int hailo_rfs_load_get_ep0_maxpacket(struct usb_composite_dev *cdev)
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
static struct usb_endpoint_descriptor hailo_rfs_load_fs_bulk_out_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_OUT,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = cpu_to_le16(64),
};


static struct usb_endpoint_descriptor hailo_rfs_load_fs_intr_in_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_INT,
    .wMaxPacketSize = cpu_to_le16(16),
    .bInterval = 10,
};

/* High-Speed endpoints */
static struct usb_endpoint_descriptor hailo_rfs_load_hs_bulk_out_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_OUT,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hailo_rfs_load_hs_intr_in_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_INT,
    .wMaxPacketSize = cpu_to_le16(1024),
    .bInterval = 7, /* 2^(7-1) = 64 microframes */
};

/* Super-Speed endpoints */
static struct usb_endpoint_descriptor hailo_rfs_load_ss_bulk_out_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_OUT,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor hailo_rfs_load_ss_bulk_out_comp_desc = {
    .bLength = USB_DT_SS_EP_COMP_SIZE,
    .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
    .bMaxBurst = 0,
    .bmAttributes = 0,
    .wBytesPerInterval = 0,
};

static struct usb_endpoint_descriptor hailo_rfs_load_ss_intr_in_desc = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_INT,
    .wMaxPacketSize = cpu_to_le16(1024),
    .bInterval = 7,
};

static struct usb_ss_ep_comp_descriptor hailo_rfs_load_ss_intr_in_comp_desc = {
    .bLength = USB_DT_SS_EP_COMP_SIZE,
    .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
    .bMaxBurst = 0,
    .bmAttributes = 0,
    .wBytesPerInterval = cpu_to_le16(1024),
};

/* Interface descriptor for RFS mode */
static struct usb_interface_descriptor hailo_rfs_load_intf_desc = {
    .bLength = sizeof(hailo_rfs_load_intf_desc),
    .bDescriptorType = USB_DT_INTERFACE,
    /* .bInterfaceNumber = DYNAMIC */  /* Assigned by composite framework */
    .bAlternateSetting = HAILO_RFS_ALT_SETTING_STANDARD,
    .bNumEndpoints = 2, /* bulk OUT, interrupt IN */
    .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 1, /* RFS mode protocol */
    /* .iInterface = DYNAMIC */
};

/* Descriptor arrays for different USB speeds */
static struct usb_descriptor_header *hailo_rfs_load_fs_function[] = {
    (struct usb_descriptor_header *) &hailo_rfs_load_intf_desc,
    (struct usb_descriptor_header *) &hailo_rfs_load_fs_bulk_out_desc,
    (struct usb_descriptor_header *) &hailo_rfs_load_fs_intr_in_desc,
    NULL,
};

static struct usb_descriptor_header *hailo_rfs_load_hs_function[] = {
    (struct usb_descriptor_header *) &hailo_rfs_load_intf_desc,
    (struct usb_descriptor_header *) &hailo_rfs_load_hs_bulk_out_desc,
    (struct usb_descriptor_header *) &hailo_rfs_load_hs_intr_in_desc,
    NULL,
};

static struct usb_descriptor_header *hailo_rfs_load_ss_function[] = {
    (struct usb_descriptor_header *) &hailo_rfs_load_intf_desc,
    (struct usb_descriptor_header *) &hailo_rfs_load_ss_bulk_out_desc,
    (struct usb_descriptor_header *) &hailo_rfs_load_ss_bulk_out_comp_desc,
    (struct usb_descriptor_header *) &hailo_rfs_load_ss_intr_in_desc,
    (struct usb_descriptor_header *) &hailo_rfs_load_ss_intr_in_comp_desc,
    NULL,
};

/* String descriptors */
#define HAILO_RFS_INTERFACE_IDX  0

static struct usb_string hailo_rfs_load_string_defs[] = {
    [HAILO_RFS_INTERFACE_IDX].s = "Hailo RFS Device Interface",
    { } /* end of list */
};

static struct usb_gadget_strings hailo_rfs_load_string_table = {
    .language = 0x0409, /* en-us */
    .strings = hailo_rfs_load_string_defs,
};

static struct usb_gadget_strings *hailo_rfs_load_strings[] = {
    &hailo_rfs_load_string_table,
    NULL,
};

/* Helper: Updated usb_request Dev -> Host with hailo_swu_state */
static int hailo_rfs_state_update(struct f_hailo_rfs_load *rfs, struct usb_request *req, unsigned length)
{
    req->length = length;
    memset(req->buf, 0, req->length);
    switch (rfs->state) {
        case HAILO_RFS_STATE__IDLE:
            snprintf((char *)req->buf, req->length, "idle:ready");
            break;
        case HAILO_RFS_STATE__UPLOAD:
            snprintf((char *)req->buf, req->length, "load:%06x", (unsigned)rfs->rfs_image_received);
            break;
        default:
            snprintf((char *)req->buf, req->length, "inv:%05x", (unsigned)rfs->state);
            pr_warn("hailo_rfs: invalid state %s\n", hailo_rfs_state_to_string(rfs->state));
            return 1;
    }

    return 0;
}

/* Helper: queue interrupt IN data for RFS load status */
static void hailo_rfs_load_queue_interrupt(struct f_hailo_rfs_load *rfs)
{
    struct usb_request *req = rfs->intr_in_req;
    int ret;

    /* Check if function is configured and endpoints are properly enabled */
    if (!req || !rfs->intr_in_ep || !rfs->intr_in_ep->enabled || !rfs->func.config)
        return;

    /* Status payload based on current RFS load processing state */
    ret = hailo_rfs_state_update(rfs, req, HAILO_RFS_INTERRUPT_MSG_SIZE);
    if (ret) {
        return;
    }

    ret = usb_ep_queue(rfs->intr_in_ep, req, GFP_ATOMIC);
    if (ret) {
        pr_warn("hailo_rfs: intr queue err %d\n", ret);
    } else {
        atomic_inc(&rfs->intr_req_queued);
    }
}

static void hailo_rfs_load_status_timer_fn(struct timer_list *t)
{
    struct f_hailo_rfs_load *rfs;
    
    /* Extra safety: validate timer pointer before from_timer */
    if (!t) 
        return;
        
    rfs = from_timer(rfs, t, status_timer);
    
    /* Safety check: validate rfs pointer and unbinding flag */
    if (!rfs || atomic_read(&rfs->unbinding))
        return;
    
    /* Only send status updates if function is still configured and interrupt endpoint is enabled */
    if (rfs->func.config && rfs->intr_in_ep && rfs->intr_in_ep->enabled) {
        hailo_rfs_load_queue_interrupt(rfs);
        /* Temporarily disable timer rescheduling to debug RCU stall */
        /* Only reschedule if not unbinding and rfs is still valid */
        //if (!atomic_read(&rfs->unbinding) && rfs->func.config)
        //    mod_timer(&rfs->status_timer, jiffies + msecs_to_jiffies(1000));
    }
}

/* EP0 (control endpoint) completion callback for vendor requests */
static void hailo_rfs_load_ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct f_hailo_rfs_load *rfs = req->context;

    pr_debug("hailo_rfs: ep0 control request completed invoked\n");
    /* Safety check: ensure context is valid and function is not being unbound */
    if (!rfs || atomic_read(&rfs->unbinding)) {
        pr_debug("hailo_rfs: ep0 completion during unbind, ignoring\n");
        return;
    }

    if (req->status && req->status != -ESHUTDOWN) {
        pr_warn("hailo_rfs: ep0 req status %d\n", req->status);
        /* Reset request status after connection errors to allow reuse */
        if (req->status == -ECONNRESET || req->status == -ENODEV || req->status == -EPROTO) {
            pr_debug("hailo_rfs: resetting ep0 request status after connection error\n");
            req->status = 0;
        }
    } else {
        pr_debug("hailo_rfs: ep0 control request completed successfully (%d bytes)\n", req->actual);
    }
}

/* Interrupt completion: handle interrupt request completion */
static void hailo_rfs_load_intr_in_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct f_hailo_rfs_load *rfs = req->context;
    
    /* Safety check: ensure context is valid and function is not being unbound */
    if (!rfs || atomic_read(&rfs->unbinding))
        return;
    
    /* Decrement queued request counter */
    if (atomic_read(&rfs->intr_req_queued) > 0)
        atomic_dec(&rfs->intr_req_queued);
    
    if (req->status && req->status != -ESHUTDOWN)
        pr_debug("hailo_rfs: intr_in req status %d\n", req->status);
}

/* Helper: schedule vmalloc buffer write to file (async) */
static int hailo_rfs_load_schedule_write_rfs_buffer(struct f_hailo_rfs_load *rfs)
{
    /* Check if we have vmalloc buffer with data */
    if (!rfs->rfs_vmalloc_buf || rfs->rfs_image_received == 0) {
        pr_debug("hailo_rfs: No vmalloc buffer data to write\n");
        return 0;
    }
    
    /* Schedule vmalloc buffer write operation in workqueue (process context) */
    rfs->rfs_file_operation = FILE_OP_WRITE;
    rfs->rfs_file_result = -EINPROGRESS;
    
    if (!queue_work(rfs->file_wq, &rfs->rfs_file_work)) {
        pr_err("hailo_rfs: failed to queue vmalloc buffer write work\n");
        return -EBUSY;
    }
    
    pr_info("hailo_rfs: vmalloc buffer write queued for background processing (%zu bytes)\n", 
            rfs->rfs_image_received);
    return 0;
}

/* Helper: free RFS vmalloc buffer (can be called from any context) */
static void hailo_rfs_image_buf_free(struct f_hailo_rfs_load *rfs)
{
    if (!rfs)
        return;
        
    if (rfs->rfs_vmalloc_buf) {
        pr_info("hailo_rfs: freeing vmalloc buffer (%zu bytes)\n", rfs->rfs_vmalloc_size);
        vfree(rfs->rfs_vmalloc_buf);
        rfs->rfs_vmalloc_buf = NULL;
        rfs->rfs_vmalloc_size = 0;
    }
    
    rfs->rfs_buffer_ready = false;
}

/* Helper: allocate RFS vmalloc buffer (must be called from process context) */
static int hailo_rfs_image_buf_alloc(struct f_hailo_rfs_load *rfs, size_t size)
{
    /* Validate parameters */
    if (!rfs) {
        pr_err("hailo_rfs: invalid rfs pointer for buffer allocation\n");
        return -EINVAL;
    }
    
    if (size == 0 || size > MAX_RFS_SIZE) {
        pr_err("hailo_rfs: invalid RFS buffer size: %zu (max %d)\n", size, MAX_RFS_SIZE);
        return -EINVAL;
    }
    
    /* Free any existing buffer */
    hailo_rfs_image_buf_free(rfs);
    
    /* Allocate new vmalloc buffer */
    rfs->rfs_vmalloc_buf = vmalloc(size);
    if (!rfs->rfs_vmalloc_buf) {
        pr_err("hailo_rfs: failed to allocate vmalloc buffer of size %zu\n", size);
        rfs->rfs_vmalloc_size = 0;
        rfs->rfs_buffer_ready = false;
        return -ENOMEM;
    }
    
    /* Initialize buffer state */
    rfs->rfs_vmalloc_size = size;
    rfs->rfs_buffer_ready = true;
    
    pr_info("hailo_rfs: allocated %zu bytes vmalloc buffer for RFS upload\n", size);
    return 0;
}

/* Helper: schedule vmalloc buffer allocation (interrupt-safe) */
static int hailo_rfs_load_schedule_rfs_buffer_alloc(struct f_hailo_rfs_load *rfs, size_t size)
{
    /* Validate parameters */
    if (!rfs) {
        pr_err("hailo_rfs: invalid rfs pointer for buffer allocation scheduling\n");
        return -EINVAL;
    }
    
    if (size == 0 || size > MAX_RFS_SIZE) {
        pr_warn("hailo_rfs: invalid RFS size %zu, using fallback\n", size);
        rfs->rfs_buffer_ready = false;
        return -EINVAL;
    }
    
    /* Store requested size - existing buffer will be freed in workqueue */
    rfs->rfs_vmalloc_requested_size = size;
    rfs->rfs_file_operation = FILE_OP_ALLOC_VMALLOC;
    rfs->rfs_file_result = -EINPROGRESS;
    
    if (!queue_work(rfs->file_wq, &rfs->rfs_file_work)) {
        pr_err("hailo_rfs: failed to queue vmalloc allocation work\n");
        rfs->rfs_buffer_ready = false;
        return -EBUSY;
    }
    
    pr_info("hailo_rfs: vmalloc allocation queued for %zu bytes\n", size);
    return 0;
}

static int hailo_rfs_load_schedule_rfs_buffer_free(struct f_hailo_rfs_load *rfs)
{
    if (!rfs->rfs_vmalloc_buf) {
        pr_debug("hailo_rfs: no vmalloc buffer to free\n");
        return 0;
    }
    
    /* Schedule vmalloc free in workqueue (process context) */
    rfs->rfs_file_operation = FILE_OP_FREE_VMALLOC;
    rfs->rfs_file_result = -EINPROGRESS;
    
    if (!queue_work(rfs->file_wq, &rfs->rfs_file_work)) {
        pr_err("hailo_rfs: failed to queue vmalloc free work\n");
        return -EBUSY;
    }
    
    pr_info("hailo_rfs: vmalloc free queued\n");
    return 0;
}

/* Helper: cleanup RFS resources (safe from atomic context) */
static void hailo_rfs_load_cleanup_rfs_resources(struct f_hailo_rfs_load *rfs)
{
    /* Schedule vmalloc buffer free - safe from atomic context */
    if (rfs->rfs_vmalloc_buf) {
        hailo_rfs_load_schedule_rfs_buffer_free(rfs);
    }
    
    /* File is now handled in deferred mode - no separate close needed */
    rfs->rfs_file = NULL;
    rfs->rfs_file_result = 0;
}

/* Workqueue function to handle file operations in process context */
static void hailo_rfs_load_file_work_fn(struct work_struct *work)
{
    struct f_hailo_rfs_load *rfs = container_of(work, struct f_hailo_rfs_load, rfs_file_work);

    if (atomic_read(&rfs->unbinding)) {
        pr_debug("hailo_rfs: file work skipped due to unbinding\n");
        return;
    }

    switch (rfs->rfs_file_operation) {
    case FILE_OP_ALLOC_VMALLOC:
        /* Free any existing buffer first, then allocate new one (process context - safe to sleep) */
        hailo_rfs_image_buf_free(rfs);
        rfs->rfs_file_result = hailo_rfs_image_buf_alloc(rfs, rfs->rfs_vmalloc_requested_size);
        break;
        
    case FILE_OP_WRITE:
        /* Open file and write complete vmalloc buffer - called after all chunks received */
        if (rfs->rfs_vmalloc_buf && rfs->rfs_image_received > 0) {
            /* Open file for writing */
            const char *filename = "/initrd.image";
            rfs->rfs_file = filp_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (IS_ERR(rfs->rfs_file)) {
                pr_err("hailo_rfs: failed to open %s: %ld\n", filename, PTR_ERR(rfs->rfs_file));
                rfs->rfs_file = NULL;
                rfs->rfs_file_result = PTR_ERR(rfs->rfs_file);
            } else {
                /* Write complete vmalloc buffer to file */
                loff_t pos = 0;
                ssize_t written = kernel_write(rfs->rfs_file, rfs->rfs_vmalloc_buf, rfs->rfs_image_received, &pos);
                if (written == rfs->rfs_image_received) {
                    pr_debug("hailo_rfs: wrote complete RFS image %zu bytes to %s\n", rfs->rfs_image_received, filename);
                    rfs->rfs_file_result = 0;
                } else {
                    pr_err("hailo_rfs: failed to write RFS image: %zd/%zu\n", written, rfs->rfs_image_received);
                    rfs->rfs_file_result = -EIO;
                }
                
                /* Close file immediately after writing */
                filp_close(rfs->rfs_file, NULL);
                rfs->rfs_file = NULL;
                pr_info("hailo_rfs: closed %s after writing RFS image\n", filename);
            }
        } else {
            pr_err("hailo_rfs: write operation with no vmalloc buffer\n");
            rfs->rfs_file_result = -EINVAL;
        }
        
        /* Free vmalloc buffer after successful write */
        hailo_rfs_image_buf_free(rfs);
        break;
    case FILE_OP_FREE_VMALLOC:
        /* Free vmalloc buffer in process context */
        hailo_rfs_image_buf_free(rfs);
        rfs->rfs_file_result = 0;
        break;
    default:
        pr_err("hailo_rfs: unknown file operation %d\n", rfs->rfs_file_operation);
        rfs->rfs_file_result = -EINVAL;
        break;
    }
}

/* Helper: write RFS chunk to vmalloc buffer (interrupt-safe) */
static int hailo_rfs_load_schedule_write_rfs_chunk(struct f_hailo_rfs_load *rfs, const void *data, size_t len)
{
    /* Check unlikely failure conditions first */
    if (unlikely(!rfs->rfs_buffer_ready || !rfs->rfs_vmalloc_buf || 
                 (rfs->rfs_image_received + len > rfs->rfs_vmalloc_size))) {
        /* vmalloc buffer not ready - should not happen with deferred file approach */
        pr_err("hailo_rfs: vmalloc buffer not ready, dropping %zu bytes\n", len);
        return -EAGAIN;
    }
    
    /* Write directly to vmalloc buffer - interrupt-safe! */
    memcpy((char *)rfs->rfs_vmalloc_buf + rfs->rfs_image_received, data, len);
    rfs->rfs_image_received += len;
    pr_debug("hailo_rfs: wrote %zu bytes to vmalloc buffer at offset %zu\n", 
             len, rfs->rfs_image_received - len);
    
    /* Check if upload is complete */
    if (rfs->rfs_image_size_expected > 0 && 
        rfs->rfs_image_received >= rfs->rfs_image_size_expected) {
        pr_info("hailo_rfs: RFS upload to vmalloc buffer complete, writing to file\n");
        rfs->rfs_file_operation = FILE_OP_WRITE;
        queue_work(rfs->file_wq, &rfs->rfs_file_work);
    }
    
    return 0;
}

/* Bulk-out completion: collect RFS data and RFS image from host */
static void hailo_rfs_load_bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
    struct f_hailo_rfs_load *rfs = req->context;
    int ret;

    /* Safety check: ensure context is valid and function is not being unbound */
    if (!rfs || atomic_read(&rfs->unbinding))
        return;

    pr_debug("hailo_rfs: bulk_out_complete called, status=%d, actual=%u, mode=%s\n", 
            req->status, req->actual, hailo_rfs_state_to_string(rfs->state));

    /* Debug: Log all bulk completions regardless of status */
    if (req->status != 0) {
        pr_debug("hailo_rfs: bulk_out_complete with error status=%d, actual=%u\n", 
                req->status, req->actual);
    }

    if (!req->status && req->actual > 0) {
        size_t got = req->actual;
        
        switch (rfs->state) {
        case HAILO_RFS_STATE__UPLOAD:
            /* Handle RFS image upload with streaming writes */
            ret = hailo_rfs_load_schedule_write_rfs_chunk(rfs, req->buf, got);
            if (ret < 0) {
                if (ret == -EAGAIN) {
                    /* File not ready yet - this is a timing issue.
                     * Don't abort upload, just log and continue.
                     * The data will be lost but upload can continue when file is ready. */
                    pr_warn("hailo_rfs: RFS chunk dropped (file opening), continuing...\n");
                } else {
                    pr_err("hailo_rfs: RFS write failed (%d), aborting upload\n", ret);
                    hailo_rfs_load_cleanup_rfs_resources(rfs);
                    rfs->state = HAILO_RFS_STATE__IDLE;
                    return;
                }
            }
            
            pr_debug("hailo_rfs: RFS received %zu bytes, total %zu/%zu\n", 
                    got, rfs->rfs_image_received, rfs->rfs_image_size_expected);
            
            /* Check if RFS upload is complete */
            if (rfs->rfs_image_size_expected > 0 && 
                rfs->rfs_image_received >= rfs->rfs_image_size_expected) {
                pr_info("hailo_rfs: RFS upload complete (%zu bytes), stopping bulk reception\n", 
                       rfs->rfs_image_received);
                /* Complete upload is handled in FINISH_RFS - just mark as idle here */
                rfs->state = HAILO_RFS_STATE__IDLE;
                return;
            } else {
                pr_debug("hailo_rfs: RFS upload continuing, need %zu more bytes\n",
                       rfs->rfs_image_size_expected - rfs->rfs_image_received);
            }
            break;
            
        case HAILO_RFS_STATE__IDLE:
        default:
            /* Ignore data when in idle mode */
            pr_debug("hailo_rfs: ignoring %zu bytes in idle mode\n", got);
            return;
        }
    } else if (req->status) {
        /* Handle USB errors with better diagnostics and recovery */
        if (req->status == -ECONNRESET) {
            pr_info("hailo_rfs: bulk request dequeued (status=-104) - this is expected during setup\n");
            /* This is from usb_ep_dequeue() in LOAD_RFS - don't requeue */
            return;
        } else if (req->status == -ESHUTDOWN) {
            pr_info("hailo_rfs: USB endpoint shutdown, stopping bulk transfers\n");
            return;
        } else {
            pr_warn("hailo_rfs: bulk_out req status %d, continuing...\n", req->status);
        }
    }

    /* Re-submit to keep accepting data if not in idle mode and endpoint is still enabled */
    if (rfs->state != HAILO_RFS_STATE__IDLE && ep && ep->enabled) {
        req->length = hailo_rfs_load_get_bulk_transfer_size(rfs);
        pr_debug("hailo_rfs: requeuing bulk_out request (mode=%s, length=%u)\n", 
                hailo_rfs_state_to_string(rfs->state), req->length);
        ret = usb_ep_queue(ep, req, GFP_ATOMIC);
        if (ret) {
            pr_warn("hailo_rfs: failed to requeue bulk_out request: %d\n", ret);
        } else {
            pr_debug("hailo_rfs: bulk_out request requeued successfully\n");
        }
    } else {
        pr_info("hailo_rfs: not requeuing bulk_out request (mode=%s, ep_enabled=%d)\n", 
                hailo_rfs_state_to_string(rfs->state), ep ? ep->enabled : -1);
    }
}

/* Helper: Send EP0 acknowledgment for vendor requests */
static int hailo_rfs_load_ep0_ack(struct usb_composite_dev *cdev, struct usb_request *req, const char *context)
{
    int ret;
    
    /* Ack (zero-length) */
    req->length = 0;
    ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
    if (ret)
        pr_err("hailo_rfs: ep0 queue fail %s %d\n", context, ret);
    else
        ret = 0;
        
    return ret;
}

/* EP0 setup handler for RFS-load-specific vendor requests */
static int hailo_rfs_load_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
    struct f_hailo_rfs_load *rfs = to_f_hailo_rfs_load(f);
    struct usb_composite_dev *cdev = f->config->cdev;
    struct usb_request *req = rfs->ep0_req;
    unsigned value = le16_to_cpu(ctrl->wValue);
    unsigned length = le16_to_cpu(ctrl->wLength);
    unsigned resp_length;
    int ret = -EOPNOTSUPP;

    /* Only support vendor requests */
    if ((ctrl->bRequestType & USB_TYPE_MASK) != USB_TYPE_VENDOR)
        return ret;

    switch (ctrl->bRequest) {
    case HAILO_REQ__RFS_GET_STATUS:
        /* Send processing status - prioritize RFS upload */
        if (!req->buf) {
            pr_err("hailo_rfs: ep0 req buffer missing\n");
            return -ENOMEM;
        }
        
        /* Calculate final response length considering all constraints */
        resp_length = min_t(unsigned, length, HAILO_RFS_EP0_STATUS_RESPONSE_SIZE);
        resp_length = min_t(unsigned, resp_length, hailo_rfs_load_get_ep0_maxpacket(cdev));
        
        /* Now use the resolved length for buffer operations */
        ret = hailo_rfs_state_update(rfs, req, resp_length);
        if (ret) {
            goto rfs_load_ack;
        }
		
        ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
        if (ret)
            pr_err("hailo_rfs: ep0 queue fail status %d\n", ret);
        else
            ret = 0;
        break;

    case HAILO_REQ__RFS_GET_BUILD_INFO:
        /* Send RFS model information */
        if (!req->buf) {
            pr_err("hailo_rfs: ep0 req buffer missing\n");
            return -ENOMEM;
        }
        /* Calculate final response length considering all constraints */
        req->length = min_t(unsigned, length, strlen("Hailo Linux build vXXX.XXX.XXX") + 1);
        req->length = min_t(unsigned, req->length, hailo_rfs_load_get_ep0_maxpacket(cdev));
        
        /* Now use the resolved length for buffer operations */
        memset(req->buf, 0, req->length);
        snprintf((char *)req->buf, req->length, "Hailo Linux build v%d.%d.%d",
                 (HAILO_LINUX_RELEASE_BUILD_VERSION >> 24) & 0xff,
                 (HAILO_LINUX_RELEASE_BUILD_VERSION >> 16) & 0xff,
                 HAILO_LINUX_RELEASE_BUILD_VERSION & 0xffff);
        ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
        if (ret)
            pr_err("hailo_rfs: ep0 queue fail info %d\n", ret);
        else
            ret = 0;
        break;

    case HAILO_REQ__RFS_LOAD:
        /* Reconstruct 32-bit file size from wValue (lower 16 bits) and wIndex (upper 16 bits) */
        {
            unsigned int full_size = (le16_to_cpu(ctrl->wIndex) << 16) | value;
            pr_info("hailo_rfs: LOAD_RFS command, expected size=%u (0x%08x)\n", full_size, full_size);
            rfs->rfs_image_received = 0;
            rfs->rfs_image_size_expected = full_size;
        
            /* Schedule vmalloc buffer allocation (interrupt-safe) */
            ret = hailo_rfs_load_schedule_rfs_buffer_alloc(rfs, full_size);
            if (ret && ret != -EINVAL) {
                /* Only fail on serious errors, continue with fallback for invalid size */
                pr_err("hailo_rfs: failed to schedule buffer allocation: %d\n", ret);
                rfs->state = HAILO_RFS_STATE__IDLE;
                goto rfs_load_ack;
            }
        }
        
        rfs->state = HAILO_RFS_STATE__UPLOAD;
        
        /* Prepare bulk endpoint for RFS data - only dequeue if actually queued */
        pr_debug("hailo_rfs: preparing bulk endpoint, current req status=%d\n", rfs->bulk_out_req->status);
        if (rfs->bulk_out_req->status == -EINPROGRESS) {
            pr_debug("hailo_rfs: dequeuing active bulk_out request\n");
            usb_ep_dequeue(rfs->bulk_out_ep, rfs->bulk_out_req);
            /* Give time for dequeue completion to avoid race */
            udelay(10); // 10 microseconds
        }
        
        /* Configure and queue request for RFS data */
        rfs->bulk_out_req->length = hailo_rfs_load_get_bulk_transfer_size(rfs);
        
        /* Reset request status from any previous dequeue operation */
        rfs->bulk_out_req->status = 0;
        rfs->bulk_out_req->actual = 0;
        
        pr_debug("hailo_rfs: about to queue bulk request, ep enabled=%d, req status=%d, length=%u\n",
                rfs->bulk_out_ep->enabled, rfs->bulk_out_req->status, rfs->bulk_out_req->length);
        
        ret = usb_ep_queue(rfs->bulk_out_ep, rfs->bulk_out_req, GFP_ATOMIC);
        if (ret) {
            pr_err("hailo_rfs: failed to queue bulk request for RFS: %d\n", ret);
            hailo_rfs_load_cleanup_rfs_resources(rfs);
            rfs->state = HAILO_RFS_STATE__IDLE;
        } else {
            pr_debug("hailo_rfs: bulk_out request queued for RFS upload (length=%u)\n", rfs->bulk_out_req->length);
        }

rfs_load_ack:
        
        /* Ack (zero-length) */
        ret = hailo_rfs_load_ep0_ack(cdev, req, "load_rfs");
        break;

    case HAILO_REQ__RFS_FINISH:
        pr_debug("hailo_rfs: FINISH_RFS command, received %zu bytes\n", rfs->rfs_image_received);
        
        rfs->state = HAILO_RFS_STATE__IDLE;
        
        /* Write complete vmalloc buffer to file (includes open, write, close) */
        hailo_rfs_load_schedule_write_rfs_buffer(rfs);
        pr_debug("hailo_rfs: RFS image streaming completed successfully (%zu bytes)\n", rfs->rfs_image_received);
        
        /* Ack (zero-length) */
        ret = hailo_rfs_load_ep0_ack(cdev, req, "finish_rfs");
        break;

    case HAILO_REQ__RFS_GET_BOARD_SKU_ID:
        /* Send board SKU ID information */
        if (!req->buf) {
            pr_err("hailo_rfs: ep0 req buffer missing\n");
            return -ENOMEM;
        }
        {
            u32 board_sku_id;
            const char *board_desc;
            int ret;
            
            /* Get board SKU ID via hailo gadget function */
            ret = hailo_gadget_get_board_sku_id(&board_sku_id);
            if (ret) {
                pr_err("hailo_rfs: failed to get board SKU ID: %d\n", ret);
                snprintf((char *)req->buf, length, "SKU_ID_ERROR");
                req->length = strlen("SKU_ID_ERROR") + 1;
            } else {
                /* Get board description string */
                board_sku_id_to_str(board_sku_id, (char *)req->buf, length);
                req->length = min_t(unsigned, length, strlen((char *)req->buf) + 1);
                req->length = min_t(unsigned, req->length, hailo_rfs_load_get_ep0_maxpacket(cdev));
            }
        }
        
        ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
        if (ret)
            pr_err("hailo_rfs: ep0 queue fail board_sku_id %d\n", ret);
        else
            ret = 0;
        break;

    case HAILO_REQ__RFS_CTRL:
        /* Check if RFS buffer is ready for upload and return bytes received count */
        if (!req->buf) {
            pr_err("hailo_rfs: ep0 req buffer missing for RFS ready check\n");
            return -ENOMEM;
        }
        
        switch (value) {
        case HAILO_REQ__RFS_CTRL__GET_STATUS:
            /* Original behavior: Send single byte: 1 if ready, 0 if not ready */
            req->length = 1;
            *((unsigned char *)req->buf) = rfs->rfs_buffer_ready ? 1 : 0;
            break;
        case HAILO_REQ__RFS_CTRL__GET_RX_CNT:
            /* Enhanced behavior: Return current bytes received count (32-bit) */
            req->length = 4;
            *((unsigned int *)req->buf) = cpu_to_le32((u32)rfs->rfs_image_received);
            pr_debug("hailo_rfs: returning RFS bytes received: %zu\n", rfs->rfs_image_received);
            break;
        case HAILO_REQ__RFS_CTRL__CLR_RX_CNT:
            /* Reset RFS counter (for ping cleanup) */
            pr_debug("hailo_rfs: resetting RFS counter from %zu to 0\n", rfs->rfs_image_received);
            rfs->rfs_image_received = 0;
            req->length = 1;
            *((unsigned char *)req->buf) = 1; /* Return success */
            break;
        default:
            pr_err("hailo_rfs: invalid RFS ready check mode: %d\n", value);
            return -EINVAL;
        }
        
        ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
        if (ret) {
            pr_err("hailo_rfs: ep0 queue fail rfs_ready %d\n", ret);
        } else {
            pr_debug("hailo_rfs: RFS ready status: %s\n", rfs->rfs_buffer_ready ? "ready" : "not ready");
            ret = 0;
        }
        break;

    default:
        pr_info("hailo_rfs: unknown vendor req %02x\n", ctrl->bRequest);
        ret = -EOPNOTSUPP;
    }

    return ret;
}

static int hailo_rfs_load_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
    struct f_hailo_rfs_load *rfs = to_f_hailo_rfs_load(f);
    struct usb_composite_dev *cdev = f->config->cdev;
    int ret;

    pr_info("hailo_rfs: set_alt intf %u alt %u speed %d\n", intf, alt, cdev->gadget->speed);

    /* Reset unbinding flag - function is being reconfigured/enabled */
    atomic_set(&rfs->unbinding, 0);

    /* Validate interface and alternate setting */
    if (intf != hailo_rfs_load_intf_desc.bInterfaceNumber) {  /* Use dynamically assigned interface number */
        pr_err("hailo_rfs: invalid interface %u (expected %u)\n", intf, hailo_rfs_load_intf_desc.bInterfaceNumber);
        return -EINVAL;
    }

    if (alt != HAILO_RFS_ALT_SETTING_STANDARD) {
        pr_err("hailo_rfs: unsupported alternate setting %u\n", alt);
        return -EINVAL;
    }

    /* Disable endpoints if they're currently enabled */
    if (rfs->bulk_out_ep && rfs->bulk_out_ep->enabled)
        usb_ep_disable(rfs->bulk_out_ep);
        
    if (rfs->intr_in_ep && rfs->intr_in_ep->enabled)
        usb_ep_disable(rfs->intr_in_ep);

    /* Stop status timer during reconfiguration */
    del_timer_sync(&rfs->status_timer);

    /* Configure bulk OUT endpoint */
    ret = config_ep_by_speed(cdev->gadget, f, rfs->bulk_out_ep);
    if (ret) {
        pr_err("hailo_rfs: failed to configure bulk_out ep: %d\n", ret);
        return ret;
    }

    ret = usb_ep_enable(rfs->bulk_out_ep);
    if (ret) {
        pr_err("hailo_rfs: failed to enable bulk_out ep: %d\n", ret);
        return ret;
    }

    /* Configure interrupt IN endpoint */
    ret = config_ep_by_speed(cdev->gadget, f, rfs->intr_in_ep);
    if (ret) {
        pr_err("hailo_rfs: failed to configure intr_in ep: %d\n", ret);
        goto fail;
    }

    ret = usb_ep_enable(rfs->intr_in_ep);
    if (ret) {
        pr_err("hailo_rfs: failed to enable intr_in ep: %d\n", ret);
        goto fail;
    }

    /* Queue initial bulk-out request to receive data */
    ret = usb_ep_queue(rfs->bulk_out_ep, rfs->bulk_out_req, GFP_ATOMIC);
    if (ret) {
        pr_err("hailo_rfs: failed to queue bulk_out request: %d\n", ret);
        goto fail;
    }

    /* Temporarily disable timer start to debug RCU stall */
    /* Restart status timer only if not unbinding */
    //if (!atomic_read(&rfs->unbinding))
    //    mod_timer(&rfs->status_timer, jiffies + msecs_to_jiffies(1000));

    pr_info("hailo_rfs: endpoints enabled for alt %u speed %d\n", alt, cdev->gadget->speed);
    return 0;

fail:
    if (rfs->bulk_out_ep && rfs->bulk_out_ep->enabled)
        usb_ep_disable(rfs->bulk_out_ep);
    if (rfs->intr_in_ep && rfs->intr_in_ep->enabled)
        usb_ep_disable(rfs->intr_in_ep);
    return ret;
}

static int hailo_rfs_load_get_alt(struct usb_function *f, unsigned intf)
{
    /* Validate interface number */
    if (intf != hailo_rfs_load_intf_desc.bInterfaceNumber) {
        pr_err("hailo_rfs: invalid interface %u in get_alt (expected %u)\n", intf, hailo_rfs_load_intf_desc.bInterfaceNumber);
        return -EINVAL;
    }
    
    /* RFS mode only supports standard alternate setting */
    return HAILO_RFS_ALT_SETTING_STANDARD;
}

static void hailo_rfs_load_disable(struct usb_function *f)
{
    struct f_hailo_rfs_load *rfs = to_f_hailo_rfs_load(f);
    
    /* Early return if already disabled to avoid duplicate operations */
    if (rfs->bulk_out_ep && !rfs->bulk_out_ep->enabled && 
        rfs->intr_in_ep && !rfs->intr_in_ep->enabled) {
        pr_debug("hailo_rfs: disable called again (already disabled)\n");
        return;
    }
    
    /* Set unbinding flag first to stop timer from rescheduling */
    atomic_set(&rfs->unbinding, 1);
    
    /* Stop status timer and ensure it's completely stopped */
    del_timer_sync(&rfs->status_timer);
    
    /* Add memory barrier to ensure timer sees unbinding flag */
    smp_mb();
    
    /* Stop any ongoing operations */
    rfs->state = HAILO_RFS_STATE__IDLE;
    
    /* Note: We avoid manual dequeue during disable to prevent race conditions.
     * The USB controller will handle cleanup when endpoints are disabled. */
    
    /* Disable endpoints with additional safety checks - prevent double disable */
    if (rfs->bulk_out_ep) {
        pr_debug("hailo_rfs: disabling bulk_out_ep (enabled=%d)\n", rfs->bulk_out_ep->enabled);
        if (rfs->bulk_out_ep->enabled) {
            usb_ep_disable(rfs->bulk_out_ep);
        }
    }
    
    if (rfs->intr_in_ep) {
        pr_debug("hailo_rfs: disabling intr_in_ep (enabled=%d)\n", rfs->intr_in_ep->enabled);
        if (rfs->intr_in_ep->enabled) {
            usb_ep_disable(rfs->intr_in_ep);
        }
    }
    
    pr_info("hailo_rfs: disabled\n");
}

static int hailo_rfs_load_bind(struct usb_configuration *c, struct usb_function *f)
{
    struct f_hailo_rfs_load_opts *opts = to_f_hailo_rfs_load_opts(f->fi);
    struct f_hailo_rfs_load *rfs = to_f_hailo_rfs_load(f);
    struct usb_composite_dev *cdev = c->cdev;
    struct usb_string *us;
    int ret;

    pr_info("hailo_rfs: bind\n");
    
    mutex_lock(&opts->lock);
    if (opts->bound) {
        mutex_unlock(&opts->lock);
        return -EBUSY;
    }
    opts->bound = true;
    mutex_unlock(&opts->lock);

    /* Allocate string IDs */
    us = usb_gstrings_attach(cdev, hailo_rfs_load_strings, ARRAY_SIZE(hailo_rfs_load_string_defs));
    if (IS_ERR(us))
        return PTR_ERR(us);
    hailo_rfs_load_intf_desc.iInterface = us[HAILO_RFS_INTERFACE_IDX].id;

    /* Allocate dynamic interface ID */
    ret = usb_interface_id(c, f);
    if (ret < 0) {
        pr_err("hailo_rfs: failed to allocate interface ID: %d\n", ret);
        goto fail;
    }
    hailo_rfs_load_intf_desc.bInterfaceNumber = ret;
    pr_info("hailo_rfs: assigned interface ID %d\n", ret);

    /* Allocate EP0 request buffer */
    rfs->ep0_req = usb_ep_alloc_request(cdev->gadget->ep0, GFP_KERNEL);
    if (!rfs->ep0_req)
        return -ENOMEM;
    rfs->ep0_req->buf = kzalloc(HAILO_RFS_EP0_BUFFER_SIZE, GFP_KERNEL);
    if (!rfs->ep0_req->buf) {
        usb_ep_free_request(cdev->gadget->ep0, rfs->ep0_req);
        return -ENOMEM;
    }
    
    /* CRITICAL: Set completion callback for EP0 control requests */
    rfs->ep0_req->complete = hailo_rfs_load_ep0_complete;
    rfs->ep0_req->context = rfs;

    /* Initialize vmalloc buffer fields */
    rfs->rfs_vmalloc_buf = NULL;
    rfs->rfs_vmalloc_size = 0;
    rfs->rfs_buffer_ready = false;

    /* Initialize workqueue for file operations */
    rfs->file_wq = alloc_workqueue("hailo_rfs_load_file", WQ_UNBOUND, 1);
    if (!rfs->file_wq) {
        pr_err("hailo_rfs: failed to create file workqueue\n");
        ret = -ENOMEM;
        goto fail;
    }
    INIT_WORK(&rfs->rfs_file_work, hailo_rfs_load_file_work_fn);

    /* Find endpoints */
    rfs->bulk_out_ep = usb_ep_autoconfig(cdev->gadget, &hailo_rfs_load_fs_bulk_out_desc);
    if (!rfs->bulk_out_ep) {
        pr_err("hailo_rfs: no bulk-out ep\n");
        ret = -ENODEV;
        goto fail;
    }

    rfs->intr_in_ep = usb_ep_autoconfig(cdev->gadget, &hailo_rfs_load_fs_intr_in_desc);
    if (!rfs->intr_in_ep) {
        pr_err("hailo_rfs: no intr-in ep\n");
        ret = -ENODEV;
        goto fail;
    }

    /* Configure endpoints for different speeds */
    hailo_rfs_load_hs_bulk_out_desc.bEndpointAddress = hailo_rfs_load_fs_bulk_out_desc.bEndpointAddress;
    hailo_rfs_load_hs_intr_in_desc.bEndpointAddress = hailo_rfs_load_fs_intr_in_desc.bEndpointAddress;
    hailo_rfs_load_ss_bulk_out_desc.bEndpointAddress = hailo_rfs_load_fs_bulk_out_desc.bEndpointAddress;
    hailo_rfs_load_ss_intr_in_desc.bEndpointAddress = hailo_rfs_load_fs_intr_in_desc.bEndpointAddress;

    /* Assign descriptor arrays */
    ret = usb_assign_descriptors(f, hailo_rfs_load_fs_function, hailo_rfs_load_hs_function,
                                hailo_rfs_load_ss_function, hailo_rfs_load_ss_function);
    if (ret)
        goto fail;

    /* Prepare bulk-out request */
    rfs->bulk_out_req = usb_ep_alloc_request(rfs->bulk_out_ep, GFP_KERNEL);
    if (!rfs->bulk_out_req) { 
        ret = -ENOMEM; 
        goto fail; 
    }
    rfs->bulk_out_req->buf = kzalloc(HAILO_RFS_BULK_OUT_BUFFER_SIZE, GFP_KERNEL);
    if (!rfs->bulk_out_req->buf) {
        ret = -ENOMEM;
        goto fail;
    }
    rfs->bulk_out_req->length = HAILO_RFS_BULK_OUT_BUFFER_SIZE;
    rfs->bulk_out_req->complete = hailo_rfs_load_bulk_out_complete;
    rfs->bulk_out_req->context = rfs;

    /* Prepare interrupt request */
    rfs->intr_in_req = usb_ep_alloc_request(rfs->intr_in_ep, GFP_KERNEL);
    if (!rfs->intr_in_req) {
        ret = -ENOMEM;
        goto fail;
    }
    rfs->intr_in_req->buf = kzalloc(HAILO_RFS_INTERRUPT_MSG_SIZE, GFP_KERNEL);
    if (!rfs->intr_in_req->buf) {
        ret = -ENOMEM;
        goto fail;
    }
    rfs->intr_in_req->complete = hailo_rfs_load_intr_in_complete;
    rfs->intr_in_req->context = rfs;

    /* Initialize atomic counter for interrupt requests */
    atomic_set(&rfs->intr_req_queued, 0);
    atomic_set(&rfs->unbinding, 0);

    /* Initialize status timer */
    timer_setup(&rfs->status_timer, hailo_rfs_load_status_timer_fn, 0);
    
    /* Timer will be started manually when needed, not automatically */

    rfs->state = HAILO_RFS_STATE__IDLE;
    rfs->rfs_image_received = 0;
    rfs->rfs_image_size_expected = 0;
    rfs->rfs_file = NULL;

    pr_info("hailo_rfs: bulk_out ep %s, intr_in ep %s\n",
            rfs->bulk_out_ep->name, rfs->intr_in_ep->name);

    return 0;

fail:
    if (rfs->intr_in_req) {
        if (rfs->intr_in_req->buf) kfree(rfs->intr_in_req->buf);
        usb_ep_free_request(rfs->intr_in_ep, rfs->intr_in_req);
    }
    if (rfs->bulk_out_req) {
        if (rfs->bulk_out_req->buf) kfree(rfs->bulk_out_req->buf);
        usb_ep_free_request(rfs->bulk_out_ep, rfs->bulk_out_req);
    }
    if (rfs->ep0_req) {
        if (rfs->ep0_req->buf) kfree(rfs->ep0_req->buf);
        usb_ep_free_request(cdev->gadget->ep0, rfs->ep0_req);
    }
    return ret;
}

static void hailo_rfs_load_unbind(struct usb_configuration *c, struct usb_function *f)
{
    struct f_hailo_rfs_load_opts *opts = to_f_hailo_rfs_load_opts(f->fi);
    struct f_hailo_rfs_load *rfs = to_f_hailo_rfs_load(f);

    pr_info("hailo_rfs: unbind\n");
    
    /* Set unbinding flag to prevent completion callbacks from accessing freed memory */
    atomic_set(&rfs->unbinding, 1);
    
    /* Stop status timer to prevent further interrupt queuing during cleanup */
    del_timer_sync(&rfs->status_timer);
    
    /* Reset atomic counter and ensure no more interrupt requests are queued */
    atomic_set(&rfs->intr_req_queued, 0);
    
    /* Clear configuration reference to prevent timer from queuing requests */
    rfs->func.config = NULL;
    
    /* Mark the function as unbound to prevent timer from rescheduling */
    mutex_lock(&opts->lock);
    opts->bound = false;
    mutex_unlock(&opts->lock);
    
    usb_free_all_descriptors(f);
    
    /* Cleanup any RFS resources */
    hailo_rfs_load_cleanup_rfs_resources(rfs);
    
    /* Cleanup workqueue and wait for any pending file operations */
    if (rfs->file_wq) {
        flush_workqueue(rfs->file_wq);
        destroy_workqueue(rfs->file_wq);
        rfs->file_wq = NULL;
    }
    
    /* CRITICAL FIX: Dequeue all pending requests FIRST while completion callbacks are still valid */
    if (rfs->bulk_out_req && rfs->bulk_out_ep) {
        usb_ep_dequeue(rfs->bulk_out_ep, rfs->bulk_out_req);
    }
    if (rfs->intr_in_req && rfs->intr_in_ep) {
        usb_ep_dequeue(rfs->intr_in_ep, rfs->intr_in_req);
    }
    
    /* Ensure all endpoints are disabled after dequeuing */
    if (rfs->bulk_out_ep && rfs->bulk_out_ep->enabled) {
        usb_ep_disable(rfs->bulk_out_ep);
    }
    if (rfs->intr_in_ep && rfs->intr_in_ep->enabled) {
        usb_ep_disable(rfs->intr_in_ep);
    }
    
    /* Force synchronization barriers to ensure all dequeue operations complete */
    synchronize_rcu();
    
    /* Wait for all USB controller operations to complete */
    msleep(500);
    
    /* DO NOT set completion callbacks to NULL - this causes race conditions! 
     * The USB controller may still call these callbacks even after dequeue.
     * Instead, rely on the atomic unbinding flag to make callbacks no-op.
     * The completion callbacks will safely check atomic_read(&rfs->unbinding) 
     * and return early if unbinding is in progress.
     */
    
    /* Final wait to ensure dequeue operations complete */
    msleep(100);
    
    /* Free USB requests and their buffers */
    if (rfs->intr_in_req && rfs->intr_in_ep) {
        if (rfs->intr_in_req->buf)
            kfree(rfs->intr_in_req->buf);
        usb_ep_free_request(rfs->intr_in_ep, rfs->intr_in_req);
    }

    if (rfs->bulk_out_req && rfs->bulk_out_ep) {
        if (rfs->bulk_out_req->buf)
            kfree(rfs->bulk_out_req->buf);
        usb_ep_free_request(rfs->bulk_out_ep, rfs->bulk_out_req);
    }

    /* Free vmalloc buffer if allocated */
    hailo_rfs_image_buf_free(rfs);

    if (rfs->ep0_req && f->config && f->config->cdev && f->config->cdev->gadget) {
        if (rfs->ep0_req->buf)
            kfree(rfs->ep0_req->buf);
        usb_ep_free_request(f->config->cdev->gadget->ep0, rfs->ep0_req);
    }
}

static void hailo_rfs_load_free_func(struct usb_function *f)
{
    struct f_hailo_rfs_load_opts *opts;
    struct f_hailo_rfs_load *rfs = to_f_hailo_rfs_load(f);

    if (!rfs) {
        pr_err("hailo_rfs: free_func called with NULL rfs pointer\n");
        return;
    }

    /* Timer should already be stopped by unbind(), but ensure it's flagged */
    atomic_set(&rfs->unbinding, 1);

    opts = container_of(f->fi, struct f_hailo_rfs_load_opts, func_inst);
    mutex_lock(&opts->lock);
    opts->refcnt--;
    mutex_unlock(&opts->lock);
    
    pr_debug("hailo_rfs: freeing function structure\n");
    kfree(rfs);
}

/* Function instance management */
static void hailo_rfs_load_attr_release(struct config_item *item)
{
    struct f_hailo_rfs_load_opts *opts = container_of(to_config_group(item),
                                               struct f_hailo_rfs_load_opts, 
                                               func_inst.group);
    usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations hailo_rfs_load_item_ops = {
    .release = hailo_rfs_load_attr_release,
};

static struct config_item_type hailo_rfs_load_func_type = {
    .ct_item_ops = &hailo_rfs_load_item_ops,
    .ct_owner = THIS_MODULE,
};

static void hailo_rfs_load_free_inst(struct usb_function_instance *fi)
{
    struct f_hailo_rfs_load_opts *opts = to_f_hailo_rfs_load_opts(fi);

    mutex_destroy(&opts->lock);
    kfree(opts);
}

static struct usb_function_instance *hailo_rfs_load_alloc_inst(void)
{
    struct f_hailo_rfs_load_opts *opts;

    opts = kzalloc(sizeof(*opts), GFP_KERNEL);
    if (!opts)
        return ERR_PTR(-ENOMEM);

    mutex_init(&opts->lock);
    opts->func_inst.set_inst_name = NULL;
    opts->func_inst.free_func_inst = hailo_rfs_load_free_inst;
    
    /* Initialize default values */
    opts->status_interval_ms = 1000;
    opts->bound = false;
    opts->refcnt = 0;

    config_group_init_type_name(&opts->func_inst.group, "", &hailo_rfs_load_func_type);

    return &opts->func_inst;
}

static struct usb_function *hailo_rfs_load_alloc_func(struct usb_function_instance *fi)
{
    struct f_hailo_rfs_load_opts *opts = to_f_hailo_rfs_load_opts(fi);
    struct f_hailo_rfs_load *rfs;

    mutex_lock(&opts->lock);
    opts->refcnt++;
    mutex_unlock(&opts->lock);

    rfs = kzalloc(sizeof(*rfs), GFP_KERNEL);
    if (!rfs) {
        mutex_lock(&opts->lock);
        opts->refcnt--;
        mutex_unlock(&opts->lock);
        return ERR_PTR(-ENOMEM);
    }

    rfs->func.name = "hailo_rfs_load";
    rfs->func.strings = hailo_rfs_load_strings;
    rfs->func.bind = hailo_rfs_load_bind;
    rfs->func.unbind = hailo_rfs_load_unbind;
    rfs->func.setup = hailo_rfs_load_setup;
    rfs->func.set_alt = hailo_rfs_load_set_alt;
    rfs->func.get_alt = hailo_rfs_load_get_alt;
    rfs->func.disable = hailo_rfs_load_disable;
    rfs->func.free_func = hailo_rfs_load_free_func;

    return &rfs->func;
}

DECLARE_USB_FUNCTION_INIT(hailo_rfs_load, hailo_rfs_load_alloc_inst, hailo_rfs_load_alloc_func);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(HAILO_RFS_DRIVER_DESC);
MODULE_AUTHOR("Hailo Technologies Ltd.");
