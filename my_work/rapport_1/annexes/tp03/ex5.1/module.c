#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/device.h>          /* needed for sysfs handling */
#include <linux/platform_device.h> /* needed for sysfs handling */
#include <linux/moduleparam.h>  /* needed for module parameters */
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>

//##### /dev #####
#define DEV_BUFFER_SZ 100
static char** dev_buf;
static dev_t driverNb;
static struct cdev driver;
static volatile uint32_t val = 0;
//################

//##### /sys #####
#define SYSFS_BUFFER_SZ 100
static char** sysfs_buf;
static struct platform_device* sysfs_devices;
//###############

static int nbDevices = 1;
module_param(nbDevices, int, 0);

//##### /dev #####
static int driver_open(struct inode* i, struct file* f)
{
    pr_info("Device opened : major:%d, minor:%d\n",imajor(i), iminor(i));
    f->private_data = (void*)iminor(i) - MINOR(driverNb); //Get index
    if ((f->f_mode & (FMODE_READ | FMODE_WRITE)) != 0) {
        return 0;
    }
    pr_info("Have to be opened in write or read mode\n");
    return 1;
}
static int driver_release(struct inode* i, struct file* f)
{
    pr_info("Release device\n");
    return 0;
}

static ssize_t driver_read(struct file* f, char __user* buf, size_t count, loff_t* off)
{
    uint32_t idevice = (uint32_t) f->private_data;
    // compute remaining bytes to copy, update count and pointers
    ssize_t remaining = DEV_BUFFER_SZ - (ssize_t)(*off);
    char* ptr = dev_buf[idevice] + *off;
    if (count > remaining) count = remaining;
    *off += count;
    // copy required number of bytes
    if (copy_to_user(buf, ptr, count) != 0) count = -EFAULT;
    pr_info("(%d) Read from user: %ld bytes\n", idevice, count);
    return count;
}

static ssize_t driver_write(struct file* f, const char __user* buf, size_t count, loff_t* off)
{
    uint32_t idevice = (uint32_t) f->private_data;
    // compute remaining space in buffer and update pointers
    ssize_t remaining = DEV_BUFFER_SZ - (ssize_t)(*off);
    // check if still remaining space to store additional bytes
    if (count >= remaining) count = -EIO;
    // store additional bytes into internal buffer
    if (count > 0) {
        char* ptr = dev_buf[idevice] + *off;
        *off += count;
        ptr[count] = 0;  // make sure string is null terminated
        if (copy_from_user(ptr, buf, count)) count = -EFAULT;
    }
    pr_info("(%d) Write to device: %ld bytes\n", idevice, count);
    return count;
}

static struct file_operations driver_fops = {
    .owner   = THIS_MODULE,
    .open    = driver_open,
    .read    = driver_read,
    .write   = driver_write,
    .release = driver_release,
};
//################

//##### /sys #####
static void sysfs_dev_release(struct device* dev) {}

ssize_t sysfs_show_attr(struct device* dev,
                        struct device_attribute* attr,
                        char* buf)
{
    int idev = MINOR(dev->devt) - MINOR(driverNb); //Get index
    strcpy(buf, sysfs_buf[idev]);
    return strlen(buf);
}
ssize_t sysfs_store_attr(struct device* dev,
                         struct device_attribute* attr,
                         const char* buf,
                         size_t count)
{
    int idev = MINOR(dev->devt) - MINOR(driverNb);
    int len = sizeof(sysfs_buf[idev]) - 1;
    if (len > count) len = count;
    strncpy(sysfs_buf[idev], buf, len);
    sysfs_buf[idev][len] = 0;
    return len;
}

DEVICE_ATTR(val, 0664, sysfs_show_attr, sysfs_store_attr); // Create : struct device_attribute dev_attr_val
//################

static int __init mod_init(void)
{
    int status = 0;
    int idev;
    printk("Allocation of %d devices\n", nbDevices);
    
    //## Memory allocation ##
    dev_buf = kzalloc(sizeof(char*) * nbDevices, GFP_KERNEL); //Array of strings
    sysfs_buf = kzalloc(sizeof(char*) * nbDevices, GFP_KERNEL); //Array of strings
    sysfs_devices = kzalloc(sizeof(struct platform_device) * nbDevices, GFP_KERNEL); // Array of platform device
    for(idev = 0; idev < nbDevices; idev++){
        dev_buf[idev] = kzalloc(DEV_BUFFER_SZ * sizeof(char), GFP_KERNEL);
        sysfs_buf[idev] = kzalloc(SYSFS_BUFFER_SZ * sizeof(char), GFP_KERNEL);
    }
    
    //## Create devices for /dev ##
    status = alloc_chrdev_region(&driverNb, 0, nbDevices, "my_char_dev"); //Several devices
    if (status == 0) {
        cdev_init(&driver, &driver_fops);
        driver.owner = THIS_MODULE;
        status = cdev_add(&driver, driverNb, nbDevices);
        printk("Registered : maj: %d, min: %d\n", MAJOR(driverNb), MINOR(driverNb));
    }
    //## Create /sys/bus/platform/devices/ files and /dev files ##
    for(idev = 0; idev < nbDevices; idev++){
        struct platform_device sysfs_device = {
        .name        = "my_dev_sysfs",
        .id          = idev,
        .dev.devt    = MKDEV(MAJOR(driverNb), MINOR(driverNb) + idev), //Contigous minor numbers
        .dev.release = sysfs_dev_release,
        };
        sysfs_devices[idev] = sysfs_device;
        status = platform_device_register(&sysfs_devices[idev]);
        if (status == 0)
            status = device_create_file(&sysfs_devices[idev].dev, &dev_attr_val);
    }
    
    pr_info("Driver loaded\n");
    return status;
}

static void __exit mod_exit(void)
{
    int idev;

    for(idev = 0; idev < nbDevices; idev++){
        device_remove_file(&sysfs_devices[idev].dev, &dev_attr_val);
        platform_device_unregister(&sysfs_devices[idev]);

        kfree(dev_buf[idev]);
        kfree(sysfs_buf[idev]);
        dev_buf[idev] = NULL;
        sysfs_buf[idev] = NULL;
    }

    kfree(dev_buf);
    kfree(sysfs_devices);
    dev_buf = NULL;
    sysfs_buf = NULL;
    
    cdev_del(&driver);
    unregister_chrdev_region(driverNb, nbDevices);
    pr_info("Driver unloaded\n");
}

module_init (mod_init);
module_exit (mod_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP03 CSEL");
MODULE_LICENSE ("GPL");
