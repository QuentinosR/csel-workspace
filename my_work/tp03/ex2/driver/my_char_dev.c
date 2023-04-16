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
static char s_buffer[BUFFER_SZ];
static dev_t driverNb;
static struct cdev driver;
static volatile uint32_t val = 0;

static int driver_open(struct inode* i, struct file* f)
{
    pr_info("Device opened : major:%d, minor:%d\n",imajor(i), iminor(i));
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
    // compute remaining bytes to copy, update count and pointers
    ssize_t remaining = BUFFER_SZ - (ssize_t)(*off);
    char* ptr = s_buffer + *off;
    if (count > remaining) count = remaining;
    *off += count;
    // copy required number of bytes
    if (copy_to_user(buf, ptr, count) != 0) count = -EFAULT;
    pr_info("Read from user: %ld bytes\n", count);
    return count;
}

static ssize_t driver_write(struct file* f, const char __user* buf, size_t count, loff_t* off)
{
    // compute remaining space in buffer and update pointers
    ssize_t remaining = BUFFER_SZ - (ssize_t)(*off);
    // check if still remaining space to store additional bytes
    if (count >= remaining) count = -EIO;
    // store additional bytes into internal buffer
    if (count > 0) {
        char* ptr = s_buffer + *off;
        *off += count;
        ptr[count] = 0;  // make sure string is null terminated
        if (copy_from_user(ptr, buf, count)) count = -EFAULT;
    }
    pr_info("Write to device: %ld bytes\n", count);
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
    int status = alloc_chrdev_region(&driverNb, 0, 1, "my_char_dev"); //1 device
    if (status == 0) {
        cdev_init(&driver, &driver_fops);
        driver.owner = THIS_MODULE;
        status = cdev_add(&driver, driverNb, 1);
        printk("Registered : maj: %d, min: %d\n", MAJOR(driverNb), MINOR(driverNb));

    }
    pr_info("Linux module skeleton loaded\n");
    return 0;
}

static void __exit mymodule_exit(void)
{
    cdev_del(&driver);
    unregister_chrdev_region(driverNb, 1);
    pr_info("Linux module skeleton unloaded\n");
}

module_init (mymodule_init);
module_exit (mymodule_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP02 CSEL");
MODULE_LICENSE ("GPL");
