#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mutex.h>


#define USB_VENDOR_ID_RELAY  0x0403
#define USB_PRODUCT_ID_RELAY 0x6001  
#define FTDI_SIO_SET_BITMODE   0x0B
#define FTDI_BITMODE_BITBANG   0x01
#define FTDI_ALL_PINS_MASK     0xFF


// Module metadata
MODULE_AUTHOR("Ethan Austin-Cruse");
MODULE_DESCRIPTION("SainSmart_5V_USB_Driver");
MODULE_LICENSE("GPLv2");

struct usbrelay {
    struct usb_device     *udev;
    struct usb_interface  *intf;

    struct cdev            cdev;
    dev_t                  devt;

    u8                     relay_state;
    u8                     bulk_in_ep;
    u8                     bulk_out_ep;

    struct mutex           lock;
};

static dev_t usbrelay_devt;
static struct class *usbrelay_class;
static const struct file_operations usbrelay_fops;  /* define elsewhere */

static int usbrelay_probe(struct usb_interface *intf, const struct usb_device_id *id) {
    struct usbrelay *dev = NULL;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *ep_desc;
    int retval = 0;
    int i;
    u8 buf;
    int actual_len;

    pr_info("usbrelay: probe() called for interface %u\n", intf->cur_altsetting->desc.bInterfaceNumber);

    // 1. Allocate and initialize per-device structure
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        retval = -ENOMEM;
        goto error;
    }

    dev->udev  = usb_get_dev(interface_to_usbdev(intf));
    dev->intf  = intf;
    dev->relay_state = 0x00;   // start with all relays off
    mutex_init(&dev->lock);

    usb_set_intfdata(intf, dev);

    // 2. Discover bulk IN/OUT endpoint
    iface_desc = intf->cur_altsetting;

    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        ep_desc = &iface_desc->endpoint[i].desc;

        if (usb_endpoint_is_bulk_out(ep_desc)) {
            dev->bulk_out_ep = ep_desc->bEndpointAddress;
        } else if (usb_endpoint_is_bulk_in(ep_desc)) {
            dev->bulk_in_ep = ep_desc->bEndpointAddress;
        }
    }

    if (!dev->bulk_out_ep) {
        pr_err("usbrelay: no bulk OUT endpoint found\n");
        retval = -ENODEV;
        goto error;
    }

    // 3. Register char device and create /dev/usbrelay0

    // Allocate a device number (for a single device)
    retval = alloc_chrdev_region(&usbrelay_devt, 0, 1, "usbrelay");
    if (retval) {
        pr_err("usbrelay: alloc_chrdev_region failed: %d\n", retval);
        goto error;
    }

    dev->devt = usbrelay_devt;

    // Init and add cdev 
    cdev_init(&dev->cdev, &usbrelay_fops);
    dev->cdev.owner = THIS_MODULE;

    retval = cdev_add(&dev->cdev, dev->devt, 1);
    if (retval) {
        pr_err("usbrelay: cdev_add failed: %d\n", retval);
        goto error_unregister_chrdev;
    }

    // Create /sys/class/usbrelay and /dev/usbrelay0 if not already
    if (!usbrelay_class) {
        usbrelay_class = class_create(THIS_MODULE, "usbrelay");
        if (IS_ERR(usbrelay_class)) {
            retval = PTR_ERR(usbrelay_class);
            usbrelay_class = NULL;
            pr_err("usbrelay: class_create failed: %d\n", retval);
            goto error_del_cdev;
        }
    }

    if (!device_create(usbrelay_class, &intf->dev, dev->devt, NULL, "usbrelay0")) {
        pr_err("usbrelay: device_create failed\n");
        retval = -ENODEV;
        goto error_del_cdev;
    }

    // 4. Put FTDI into bit-bang mode 

    // Set all pins as outputs (mask = 0xFF) and enable bit-bang
    retval = usb_control_msg(dev->udev,
                             usb_sndctrlpipe(dev->udev, 0),
                             FTDI_SIO_SET_BITMODE,
                             USB_TYPE_VENDOR | USB_RECIP_DEVICE |
                                 USB_DIR_OUT,
                             (FTDI_ALL_PINS_MASK << 8) | FTDI_BITMODE_BITBANG,
                             iface_desc->desc.bInterfaceNumber,
                             NULL,
                             0,
                             1000);
    if (retval < 0) {
        pr_err("usbrelay: failed to set bit-bang mode: %d\n", retval);
        goto error_destroy_device;
    }

    // 5. Push initial relay_state (all off) via bulk OUT

    buf = dev->relay_state;

    retval = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev, dev->bulk_out_ep),
                          &buf,
                          1,
                          &actual_len,
                          1000);
    if (retval < 0 || actual_len != 1) {
        pr_err("usbrelay: initial bulk write failed: ret=%d len=%d\n",
               retval, actual_len);
        goto error_destroy_device;
    }

    pr_info("usbrelay: device initialized, /dev/usbrelay0 ready\n");
    return 0;

// Error paths
error_destroy_device:
    device_destroy(usbrelay_class, dev->devt);

error_del_cdev:
    cdev_del(&dev->cdev);

error_unregister_chrdev:
    unregister_chrdev_region(usbrelay_devt, 1);

error:
    if (dev) {
        if (dev->udev)
            usb_put_dev(dev->udev);
        kfree(dev);
    }
    usb_set_intfdata(intf, NULL);
    return retval;
}


static void usbrelay_disconnect(struct usb_interface *intf)
{
    struct usbrelay *dev = usb_get_intfdata(intf);

    pr_info("usbrelay: disconnect() called for interface %u\n", intf->cur_altsetting->desc.bInterfaceNumber);

    if (!dev)
        return;

    // 1. Remove the /dev node for this device
    if (usbrelay_class)
        device_destroy(usbrelay_class, dev->devt);

    // 2. Remove the cdev from the kernel
    cdev_del(&dev->cdev);

    // 3. Release the allocated major/minor for this device
    unregister_chrdev_region(dev->devt, 1);

    // 4. Detach our private data from the interface 
    usb_set_intfdata(intf, NULL);

    // 5. Drop the USB device reference and free our state 
    if (dev->udev)
        usb_put_dev(dev->udev);

    kfree(dev);

    pr_info("usbrelay: device disconnected and resources cleaned up\n");
}




static const struct usb_device_id usbrelay_id_table[] = { // Need this table to register usb in Module Device Table
    { USB_DEVICE(USB_VENDOR_ID_RELAY, USB_PRODUCT_ID_RELAY) },
    { }                              // terminating entry
};
MODULE_DEVICE_TABLE(usb, usbrelay_id_table);

static struct usb_driver usbrelay_driver = { // Information to pass to usb_register protocol
    .name       = "usbrelay",
    .id_table   = usbrelay_id_table,
    .probe      = usbrelay_probe,
    .disconnect = usbrelay_disconnect,
};


static int __init custom_init(void) {
    int ret;
    printk(KERN_INFO "SainSmart_5V_USB_Driver inserted.\n");
    ret = usb_register(&usbrelay_driver); // Calls .probe on success
    if (ret) { // Check the status code of usb_register to determine if it was succesfully identified
        printk(KERN_ERR "usbrelay: usb_register failed: %d\n", ret);
        return ret;
    }

    return 0;
}
static void __exit custom_exit(void) {
    usb_deregister(&usbrelay_driver); // Calls .disconnect
    printk(KERN_INFO "SainSmart_5V_USB_Driver ejected.\n");
}
module_init(custom_init);
module_exit(custom_exit);