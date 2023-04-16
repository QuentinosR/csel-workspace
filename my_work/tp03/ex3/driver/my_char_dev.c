// skeleton.c
#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/moduleparam.h>  /* needed for module parameters */
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>

#define BUFFER_SZ 100
static char** s_buffer;
static dev_t driverNb;
static struct cdev driver;
static volatile uint32_t val = 0;

static int nbDevices = 1;
module_param(nbDevices, int, 0);

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
    ssize_t remaining = BUFFER_SZ - (ssize_t)(*off);
    char* ptr = s_buffer[idevice] + *off;
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
    ssize_t remaining = BUFFER_SZ - (ssize_t)(*off);
    // check if still remaining space to store additional bytes
    if (count >= remaining) count = -EIO;
    // store additional bytes into internal buffer
    if (count > 0) {
        char* ptr = s_buffer[idevice] + *off;
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

static int __init mymodule_init(void)
{
    int idev;
    int status;
    printk("Allocation of %d devices\n", nbDevices);
    s_buffer = kzalloc(sizeof(char*) * nbDevices, GFP_KERNEL); //Array of strings
    for(idev = 0; idev < nbDevices; idev++){
        s_buffer[idev] = kzalloc(BUFFER_SZ * sizeof(char), GFP_KERNEL);
    }
    
    status = alloc_chrdev_region(&driverNb, 0, nbDevices, "my_char_dev"); //Several devices
    if (status == 0) {
        cdev_init(&driver, &driver_fops);
        driver.owner = THIS_MODULE;
        status = cdev_add(&driver, driverNb, nbDevices);
        printk("Registered : maj: %d, min: %d\n", MAJOR(driverNb), MINOR(driverNb));

    }

    
    pr_info("Driver loaded\n");
    return 0;
}

static void __exit mymodule_exit(void)
{
    int idev;

    for(idev = 0; idev < nbDevices; idev++){
        kfree(s_buffer[idev]);
        s_buffer[idev] = NULL;
    }
    kfree(s_buffer);
    s_buffer = NULL;
    
    cdev_del(&driver);
    unregister_chrdev_region(driverNb, nbDevices);
    pr_info("Driver unloaded\n");
}

module_init (mymodule_init);
module_exit (mymodule_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP03 CSEL");
MODULE_LICENSE ("GPL");
