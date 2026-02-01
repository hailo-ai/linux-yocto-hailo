/*
 * hailo_ai.c -- Hailo RFS loading USB gadget driver
 *
 * Hailo USB gadget with two configurations:
 * 1) Hailo RFS loading mode (using f_hailo_rfs_load.c function)
 * 2) Hailo SW update mode (using f_hailo_swu_load.c function)
 *
 * This is a composite driver that registers separate function drivers
 * for each operational mode, providing proper USB descriptor separation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb/composite.h>
#include <linux/usb/ch9.h>
#include <linux/slab.h>
#include <linux/soc/hailo/scmi_hailo_ops.h>
#include <linux/soc/hailo/scmi_hailo_protocol.h>

/* Compile-time check: ensure at least one function driver is enabled */
#if !IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD) && !IS_ENABLED(USB_F_HAILO_SWU_LOAD)
#error "At least one of CONFIG_USB_F_HAILO_RFS_LOAD, or USB_F_HAILO_SWU_LOAD must be enabled"
#endif

/*
    Device descriptors tree view:
    ------------------

    Device (VID: 0x0B05, PID: 0x1D6F)
    ├── Configuration 1 (Hailo RFS Loading Mode)
    │   └── Interface 0 (f_hailo_rfs_load function)
    │       └── Alternate Setting 0 (RFS Mode)
    │           ├── Bulk OUT Endpoint (RFS upload)
    │           └── Interrupt IN Endpoint (RFS status)
    │
    └── Configuration 2 (Hailo Software Update Mode)
        └── Interface 0 (f_hailo_swu_load function)
            └── Alternate Setting 0 (SW Update Mode)
                ├── Bulk OUT Endpoint (firmware upload)
                └── Interrupt IN Endpoint (upload progress)

    Note: Each configuration now uses separate function drivers with
    dedicated descriptors, vendor requests, and mode-specific behavior.
*/

#define DRIVER_DESC "ASUSTek Computer, Inc. Hailo Composite USB Gadget"

#define HAILO_VENDOR_ID     0x0B05  /* ASUSTek Computer, Inc. */
#define HAILO_PRODUCT_ID    0x1D6F  /* Hailo AI Gadget */
#define HAILO_DEVICE_BCD    0x0100  /* Device release number */

/* USB String Descriptor Indices */
#define STRING_MANUFACTURER_IDX    0
#define STRING_PRODUCT_IDX         1  
#define STRING_SERIAL_IDX          2
#define STRING_CONFIG_RFS_LOAD_MODE_IDX    3
#define STRING_CONFIG_SW_UPDATE_MODE_IDX   4

/* Device Configuration Values */
#define CONFIG_RFS_LOAD_MODE_VALUE   1
#define CONFIG_SW_UPDATE_MODE_VALUE  2

/* Function names for each configuration */
#if IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD)
#define HAILO_RFS_LOAD_FUNCTION_NAME "hailo_rfs_load"
#endif
#if IS_ENABLED(CONFIG_USB_F_HAILO_SWU_LOAD)
#define HAILO_SW_UPDATE_FUNCTION_NAME "hailo_swu_load"
#endif

static bool disabled = false;
module_param(disabled, bool, 0644);
MODULE_PARM_DESC(disabled, "Disable Hailo USB gadget (allows other gadgets to bind)");

/* Global composite device pointer for runtime control */
static struct usb_composite_dev *hailo_cdev = NULL;

/* Device descriptor */
static struct usb_device_descriptor hailo_device_desc = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = cpu_to_le16(0x0300), /* USB 3.0 */
    .bDeviceClass = USB_CLASS_VENDOR_SPEC,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    /* .bMaxPacketSize0 = f(hardware) */
    .idVendor = cpu_to_le16(HAILO_VENDOR_ID),
    .idProduct = cpu_to_le16(HAILO_PRODUCT_ID),
    .bcdDevice = cpu_to_le16(HAILO_DEVICE_BCD),
    /* String indices will be dynamically assigned by usb_gstrings_attach() */
    .iManufacturer = 0,  /* Will be set in bind function */
    .iProduct = 0,       /* Will be set in bind function */
    .iSerialNumber = 0,  /* Will be set in bind function */
    .bNumConfigurations = 0, /* Will be set dynamically based on enabled functions */
};

/* Device strings */
static struct usb_string hailo_strings_dev[] = {
	[STRING_MANUFACTURER_IDX].s = "Hailo Technologies Ltd.",
	[STRING_PRODUCT_IDX].s = DRIVER_DESC,
	[STRING_SERIAL_IDX].s = "H10-DEV-001",
#if IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD)
	[STRING_CONFIG_RFS_LOAD_MODE_IDX].s = "Hailo RFS Loading Mode",
#endif
#if IS_ENABLED(CONFIG_USB_F_HAILO_SWU_LOAD)
	[STRING_CONFIG_SW_UPDATE_MODE_IDX].s = "Hailo Software Update Mode",
#endif
	{ } /* terminator */
};

static struct usb_gadget_strings hailo_stringtab_dev = {
    .language = 0x0409, /* en-us */
    .strings = hailo_strings_dev,
};

static struct usb_gadget_strings *hailo_dev_strings[] = {
    &hailo_stringtab_dev,
    NULL,
};

/* Function instances for each configuration */
#if IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD)
static struct usb_function_instance *rfs_load_func_inst = NULL;
static struct usb_function *rfs_load_func = NULL;
#endif

#if IS_ENABLED(CONFIG_USB_F_HAILO_SWU_LOAD)
static struct usb_function_instance *sw_update_func_inst = NULL;
static struct usb_function *sw_update_func = NULL;
#endif

#if IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD)
static int hailo_do_rfs_load_config(struct usb_configuration *c)
{
    int ret;

    pr_info("hailo: setting up RFS Loading configuration\n");

    /* Get function from pre-created instance */
    rfs_load_func = usb_get_function(rfs_load_func_inst);
    if (IS_ERR(rfs_load_func)) {
        pr_err("hailo: failed to get RFS Loading function: %ld\n", PTR_ERR(rfs_load_func));
        return PTR_ERR(rfs_load_func);
    }

    ret = usb_add_function(c, rfs_load_func);
    if (ret) {
        pr_err("hailo: failed to add RFS Loading function: %d\n", ret);
        usb_put_function(rfs_load_func);
        rfs_load_func = NULL;
        return ret;
    }

    pr_info("hailo: RFS Loading function successfully added to configuration\n");
    return 0;
}
#endif /* CONFIG_USB_F_HAILO_RFS_LOAD */

#if IS_ENABLED(CONFIG_USB_F_HAILO_SWU_LOAD)
static int hailo_do_sw_update_config(struct usb_configuration *c)
{
    int ret;

    pr_info("hailo: setting up SW Update configuration\n");

    /* Get function from pre-created instance */
    sw_update_func = usb_get_function(sw_update_func_inst);
    if (IS_ERR(sw_update_func)) {
        pr_err("hailo: failed to get SW Update function: %ld\n", PTR_ERR(sw_update_func));
        return PTR_ERR(sw_update_func);
    }

    ret = usb_add_function(c, sw_update_func);
    if (ret) {
        pr_err("hailo: failed to add SW Update function: %d\n", ret);
        usb_put_function(sw_update_func);
        sw_update_func = NULL;
        return ret;
    }

    return 0;
}
#endif /* CONFIG_USB_F_HAILO_SWU_LOAD */
/* Configuration descriptors */
#if IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD)
static struct usb_configuration hailo_rfs_load_config_driver = {
    .label = "Hailo RFS Loading Mode",
    .bConfigurationValue = CONFIG_RFS_LOAD_MODE_VALUE,
    .iConfiguration = STRING_CONFIG_RFS_LOAD_MODE_IDX,
    .MaxPower = 3000,
};
#endif

#if IS_ENABLED(CONFIG_USB_F_HAILO_SWU_LOAD)
static struct usb_configuration hailo_sw_update_config_driver = {
    .label = "Hailo SW Update Mode", 
    .bConfigurationValue = CONFIG_SW_UPDATE_MODE_VALUE,
    .iConfiguration = STRING_CONFIG_SW_UPDATE_MODE_IDX,
    .MaxPower = 3000,
};
#endif

/* Helper function to get board SKU ID via SCMI */
int hailo_gadget_get_board_sku_id(u32 *board_sku_id)
{
	const struct scmi_hailo_ops *hailo_ops;
	struct scmi_hailo_get_sku_id_p2a sku_id;
	int ret;

	if (!board_sku_id) {
		pr_err("hailo: invalid board_sku_id pointer\n");
		return -EINVAL;
	}

	/* Get SCMI operations */
	hailo_ops = scmi_hailo_get_ops();
	if (IS_ERR(hailo_ops)) {
		pr_err("hailo: failed to get SCMI ops: %ld\n", PTR_ERR(hailo_ops));
		return PTR_ERR(hailo_ops);
	}

	/* Get SKU ID from SCMI */
	ret = hailo_ops->get_sku_id(&sku_id);
	if (ret) {
		pr_err("hailo: failed to get SKU ID: %d\n", ret);
		return ret;
	}

	*board_sku_id = sku_id.board;
	return 0;
}
EXPORT_SYMBOL_GPL(hailo_gadget_get_board_sku_id);

static int hailo_composite_bind(struct usb_composite_dev *cdev)
{
    struct usb_string *us;
    int num_configs = 0;
    int ret;

    if (disabled) {
        pr_info("Hailo gadget disabled by parameter\n");
        return -ENODEV;
    }

    pr_info("hailo: composite bind\n");

    /* Create function instances once during bind */
#if IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD)
    if (!rfs_load_func_inst) {
        rfs_load_func_inst = usb_get_function_instance(HAILO_RFS_LOAD_FUNCTION_NAME);
        if (IS_ERR(rfs_load_func_inst)) {
            pr_err("hailo: failed to get RFS Loading function instance: %ld\n", PTR_ERR(rfs_load_func_inst));
            return PTR_ERR(rfs_load_func_inst);
        }
    }
#endif

#if IS_ENABLED(CONFIG_USB_F_HAILO_SWU_LOAD)
    if (!sw_update_func_inst) {
        sw_update_func_inst = usb_get_function_instance(HAILO_SW_UPDATE_FUNCTION_NAME);
        if (IS_ERR(sw_update_func_inst)) {
            pr_err("hailo: failed to get SW Update function instance: %ld\n", PTR_ERR(sw_update_func_inst));
            ret = PTR_ERR(sw_update_func_inst);
            goto err_put_instances;
        }
    }
#endif

    /* Allocate string IDs */
    us = usb_gstrings_attach(cdev, hailo_dev_strings, ARRAY_SIZE(hailo_strings_dev));
    if (IS_ERR(us)) {
        ret = PTR_ERR(us);
        goto err_put_instances;
    }

    hailo_device_desc.iManufacturer = us[STRING_MANUFACTURER_IDX].id;
    hailo_device_desc.iProduct = us[STRING_PRODUCT_IDX].id;
    hailo_device_desc.iSerialNumber = us[STRING_SERIAL_IDX].id;

#if IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD)
    {
        int ret;
        hailo_rfs_load_config_driver.iConfiguration = us[STRING_CONFIG_RFS_LOAD_MODE_IDX].id;
        
        /* Add RFS Loading configuration */
        ret = usb_add_config(cdev, &hailo_rfs_load_config_driver, hailo_do_rfs_load_config);
        if (ret) {
            pr_err("hailo: failed to add RFS Loading config: %d\n", ret);
            goto err_put_instances;
        }
        num_configs++;
        pr_info("hailo: RFS Loading configuration added\n");
    }
#endif

#if IS_ENABLED(CONFIG_USB_F_HAILO_SWU_LOAD)
    {
        int ret;
        hailo_sw_update_config_driver.iConfiguration = us[STRING_CONFIG_SW_UPDATE_MODE_IDX].id;
        
        /* Add SW Update configuration */
        ret = usb_add_config(cdev, &hailo_sw_update_config_driver, hailo_do_sw_update_config);
        if (ret) {
            pr_err("hailo: failed to add SW Update config: %d\n", ret);
            goto err_put_instances;
        }
        num_configs++;
        pr_info("hailo: SW Update configuration added\n");
    }
#endif

    /* Update the number of configurations in device descriptor */
    hailo_device_desc.bNumConfigurations = num_configs;
    
    if (num_configs == 0) {
        pr_err("hailo: no function drivers enabled - at least one of CONFIG_USB_F_HAILO_SWU_LOAD or CONFIG_USB_F_HAILO_RFS_LOAD must be set\n");
        ret = -EINVAL;
        goto err_put_instances;
    }

    /* Store composite device pointer for runtime control */
    hailo_cdev = cdev;

    pr_info("hailo: composite bind complete with %d configuration(s)\n", num_configs);
    return 0;

err_put_instances:
#if IS_ENABLED(CONFIG_USB_F_HAILO_SWU_LOAD)
    if (sw_update_func_inst) {
        usb_put_function_instance(sw_update_func_inst);
        sw_update_func_inst = NULL;
    }
#endif
#if IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD)
    if (rfs_load_func_inst) {
        usb_put_function_instance(rfs_load_func_inst);
        rfs_load_func_inst = NULL;
    }
#endif
    return ret;
}


static int hailo_composite_unbind(struct usb_composite_dev *cdev)
{
    pr_info("hailo: composite unbind\n");

    /* Clear composite device pointer */
    hailo_cdev = NULL;

#if IS_ENABLED(CONFIG_USB_F_HAILO_RFS_LOAD)
    if (rfs_load_func) {
        usb_put_function(rfs_load_func);
        rfs_load_func = NULL;
    }
    if (rfs_load_func_inst) {
        usb_put_function_instance(rfs_load_func_inst);
        rfs_load_func_inst = NULL;
    }
#endif

#if IS_ENABLED(CONFIG_USB_F_HAILO_SWU_LOAD)
    if (sw_update_func) {
        usb_put_function(sw_update_func);
        sw_update_func = NULL;
    }
    if (sw_update_func_inst) {
        usb_put_function_instance(sw_update_func_inst);
        sw_update_func_inst = NULL;
    }
#endif

    return 0;
}


static struct usb_composite_driver hailo_composite_driver = {
    .name = "hailo-composite-gadget",
    .dev = &hailo_device_desc,
    .strings = hailo_dev_strings,
    .max_speed = USB_SPEED_SUPER_PLUS,
    .bind = hailo_composite_bind,
    .unbind = hailo_composite_unbind,
};


/* Sysfs interface for runtime control */
static ssize_t hailo_gadget_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", (hailo_cdev && !disabled) ? "enable" : "disable");
}


static ssize_t hailo_gadget_store(struct kobject *kobj, struct kobj_attribute *attr,
                                 const char *buf, size_t count)
{
    if (strncmp(buf, "disable", strlen("disable")) == 0 || strncmp(buf, "0", strlen("0")) == 0) {
        if (hailo_cdev) {
            pr_info("Hailo gadget: disabling via sysfs\n");
            /* Set disabled flag first to prevent re-binding */
            disabled = true;
            /* Unregister composite driver to free UDC completely */
            usb_composite_unregister(&hailo_composite_driver);
        }
    } else if (strncmp(buf, "enable", strlen("enable")) == 0 || strncmp(buf, "1", strlen("1")) == 0) {
        if (!hailo_cdev && disabled) {
            pr_info("Hailo gadget: enabling via sysfs\n");
            disabled = false;
            /* Re-register composite driver */
            return usb_composite_probe(&hailo_composite_driver);
        }
    }
    return count;
}

static struct kobj_attribute hailo_gadget_attr = __ATTR(hailo_gadget, 0644, hailo_gadget_show, hailo_gadget_store);
static struct kobject *hailo_gadget_kobj;


static int __init hailo_init(void)
{
    int ret;

    /* Create sysfs interface */
    hailo_gadget_kobj = kobject_create_and_add("hailo_gadget", kernel_kobj);
    if (!hailo_gadget_kobj)
        return -ENOMEM;

    ret = sysfs_create_file(hailo_gadget_kobj, &hailo_gadget_attr.attr);
    if (ret) {
        kobject_put(hailo_gadget_kobj);
        return ret;
    }

    /* Register composite driver */
    ret = usb_composite_probe(&hailo_composite_driver);
    if (ret) {
        sysfs_remove_file(hailo_gadget_kobj, &hailo_gadget_attr.attr);
        kobject_put(hailo_gadget_kobj);
    }

    return ret;
}

static void __exit hailo_exit(void)
{
    usb_composite_unregister(&hailo_composite_driver);
    sysfs_remove_file(hailo_gadget_kobj, &hailo_gadget_attr.attr);
    kobject_put(hailo_gadget_kobj);
}

module_init(hailo_init);
module_exit(hailo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hailo Technologies Ltd.");
MODULE_DESCRIPTION(DRIVER_DESC);
