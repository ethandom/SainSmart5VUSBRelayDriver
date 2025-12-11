#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/idr.h>

#define USB_VENDOR_ID_RELAY      0x0403
#define USB_PRODUCT_ID_RELAY     0x6001
#define FTDI_SIO_SET_BITMODE     0x0B
#define FTDI_BITMODE_BITBANG     0x01
#define FTDI_ALL_PINS_MASK       0xFF
#define USBRELAY_MAX_DEVICES     4   

/* Module metadata */
MODULE_AUTHOR("Ethan Austin-Cruse");
MODULE_DESCRIPTION("SainSmart 5V USB Relay Driver");
MODULE_LICENSE("GPL");

/* Per-device state */
struct usbrelay {
    struct usb_device     *udev;
    struct usb_interface  *intf;
    struct cdev            cdev;
    dev_t                  devt;
    int                    minor;
    u8                     relay_state;
    u8                     bulk_in_ep;
    u8                     bulk_out_ep;
    struct mutex           lock;
};

/* Globals for char devices */
static dev_t usbrelay_first_devt;
static int usbrelay_major;
static struct class *usbrelay_class;
static DEFINE_IDA(usbrelay_ida);  /* allocate minors safely */

/* Fops forward declarations */
static int usbrelay_open(struct inode *inode, struct file *file);
static int usbrelay_release(struct inode *inode, struct file *file);
static ssize_t usbrelay_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static ssize_t usbrelay_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);

static const struct file_operations usbrelay_fops = {
    .owner   = THIS_MODULE,
    .open    = usbrelay_open,
    .release = usbrelay_release,
    .write   = usbrelay_write,
    .read = usbrelay_read,
};

/* Helper: push current relay_state to device via bulk OUT */
static int usbrelay_push_state(struct usbrelay *dev)
{
    int retval;
    int actual_len;
    u8 buf = dev->relay_state;
    retval = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev, dev->bulk_out_ep),
                          &buf,
                          1,
                          &actual_len,
                          1000);
    if (retval < 0 || actual_len != 1) {
        pr_err("usbrelay: bulk write failed: ret=%d len=%d\n",
               retval, actual_len);
        return retval < 0 ? retval : -EIO;
    }
    return 0;
}

static int usbrelay_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    struct usbrelay *dev = NULL;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *ep_desc;
    int retval = 0;
    int i;
    int minor;

    pr_info("usbrelay: probe() called for interface %u\n",
            intf->cur_altsetting->desc.bInterfaceNumber);

    /* 1. Allocate and initialize per-device structure */
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        retval = -ENOMEM;
        goto error;
    }

    dev->udev  = usb_get_dev(interface_to_usbdev(intf));
    dev->intf  = intf;
    dev->relay_state = 0x00;   /* start with all relays off */
    mutex_init(&dev->lock);

    usb_set_intfdata(intf, dev);

    /* 2. Discover bulk IN/OUT endpoints */
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

    /* 3. Allocate a minor and register char device */
    minor = ida_simple_get(&usbrelay_ida, 0, USBRELAY_MAX_DEVICES, GFP_KERNEL);
    if (minor < 0) {
        pr_err("usbrelay: failed to allocate minor: %d\n", minor);
        retval = minor;
        goto error;
    }
    dev->minor = minor;
    dev->devt  = MKDEV(usbrelay_major, minor);

    cdev_init(&dev->cdev, &usbrelay_fops);
    dev->cdev.owner = THIS_MODULE;

    retval = cdev_add(&dev->cdev, dev->devt, 1);
    if (retval) {
        pr_err("usbrelay: cdev_add failed: %d\n", retval);
        goto error_ida;
    }

    if (!usbrelay_class) {
        pr_err("usbrelay: class is NULL, this should not happen\n");
        retval = -ENODEV;
        goto error_cdev;
    }

    if (!device_create(usbrelay_class, &intf->dev, dev->devt,
                       dev, "usbrelay%d", minor)) {
        pr_err("usbrelay: device_create failed for minor %d\n", minor);
        retval = -ENODEV;
        goto error_cdev;
    }

    /* 4. Put FTDI into bit-bang mode */
    retval = usb_control_msg(dev->udev,
                         usb_sndctrlpipe(dev->udev, 0),
                         FTDI_SIO_SET_BITMODE,
                         USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT,
                         (FTDI_BITMODE_BITBANG << 8) | FTDI_ALL_PINS_MASK,
                         iface_desc->desc.bInterfaceNumber,
                         NULL,
                         0,
                         1000);

    if (retval < 0) {
        pr_err("usbrelay: failed to set bit-bang mode: %d\n", retval);
        goto error_device;
    }

    /* Push initial relay_state (all off) */
    retval = usbrelay_push_state(dev);
    if (retval) {
        pr_err("usbrelay: initial state push failed: %d\n", retval);
        goto error_device;
    }

    pr_info("usbrelay: device initialized, /dev/usbrelay%d ready\n", minor);
    return 0;

/* ---------- Error paths ---------- */
error_device:
    device_destroy(usbrelay_class, dev->devt);

error_cdev:
    cdev_del(&dev->cdev);

error_ida:
    ida_simple_remove(&usbrelay_ida, minor);

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

    device_destroy(usbrelay_class, dev->devt);
    cdev_del(&dev->cdev);
    ida_simple_remove(&usbrelay_ida, dev->minor);

    usb_set_intfdata(intf, NULL);

    if (dev->udev)
        usb_put_dev(dev->udev);

    kfree(dev);

    pr_info("usbrelay: device disconnected and resources cleaned up\n");
}

/* Match table: this driver only handles your relay's VID/PID */
static const struct usb_device_id usbrelay_id_table[] = {
    { USB_DEVICE(USB_VENDOR_ID_RELAY, USB_PRODUCT_ID_RELAY) },
    { } /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, usbrelay_id_table);

/* USB driver registration struct */
static struct usb_driver usbrelay_driver = {
    .name       = "usbrelay",
    .id_table   = usbrelay_id_table,
    .probe      = usbrelay_probe,
    .disconnect = usbrelay_disconnect,
};

/* ---------- file_operations implementations ---------- */

static int usbrelay_open(struct inode *inode, struct file *file)
{
    struct usbrelay *dev;

    /* Get back to our usbrelay struct from the cdev */
    dev = container_of(inode->i_cdev, struct usbrelay, cdev);
    if (!dev)
        return -ENODEV;
    file->private_data = dev;
    return 0;
}

static int usbrelay_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t usbrelay_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct usbrelay *dev = file->private_data;
    u8 mask;

    if (!dev)
        return -ENODEV;
    if (*ppos > 0)
        return 0;  // EOF on second read
    if (count < 1)
        return -EINVAL;

    mutex_lock(&dev->lock);
    mask = dev->relay_state;
    mutex_unlock(&dev->lock);

    if (copy_to_user(buf, &mask, 1))
        return -EFAULT;

    *ppos = 1;
    return 1;
}

static ssize_t usbrelay_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct usbrelay *dev = file->private_data;
    u8 mask;
    int retval;

    if (!dev)
        return -ENODEV;

    if (count < 1)
        return 0;              // nothing to do

    if (copy_from_user(&mask, buf, 1))
        return -EFAULT;

    mutex_lock(&dev->lock);

    dev->relay_state = mask;
    retval = usbrelay_push_state(dev);

    mutex_unlock(&dev->lock);

    if (retval)
        return retval;

    // Pretend we consumed everything user wrote
    return count;
}


/* ---------- module init/exit ---------- */

static int __init usbrelay_init(void)
{
    int ret;

    pr_info("usbrelay: module init\n");

    /* Reserve a range of char device numbers for up to USBRELAY_MAX_DEVICES */
    ret = alloc_chrdev_region(&usbrelay_first_devt, 0,
                              USBRELAY_MAX_DEVICES, "usbrelay");
    if (ret) {
        pr_err("usbrelay: alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }

    usbrelay_major = MAJOR(usbrelay_first_devt);

    usbrelay_class = class_create("usbrelay");
    if (IS_ERR(usbrelay_class)) {
        ret = PTR_ERR(usbrelay_class);
        usbrelay_class = NULL;
        pr_err("usbrelay: class_create failed: %d\n", ret);
        unregister_chrdev_region(usbrelay_first_devt, USBRELAY_MAX_DEVICES);
        return ret;
    }

    ret = usb_register(&usbrelay_driver);
    if (ret) {
        pr_err("usbrelay: usb_register failed: %d\n", ret);
        class_destroy(usbrelay_class);
        usbrelay_class = NULL;
        unregister_chrdev_region(usbrelay_first_devt, USBRELAY_MAX_DEVICES);
        return ret;
    }

    pr_info("usbrelay: driver registered, major=%d\n", usbrelay_major);
    return 0;
}

static void __exit usbrelay_exit(void)
{
    pr_info("usbrelay: module exit\n");

    usb_deregister(&usbrelay_driver);

    if (usbrelay_class) {
        class_destroy(usbrelay_class);
        usbrelay_class = NULL;
    }

    unregister_chrdev_region(usbrelay_first_devt, USBRELAY_MAX_DEVICES);
}

module_init(usbrelay_init);
module_exit(usbrelay_exit);