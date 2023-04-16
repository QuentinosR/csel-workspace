#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/device.h>          /* needed for sysfs handling */
#include <linux/platform_device.h> /* needed for sysfs handling */
#define PLATFORM 0
#define CLASS 1
#define MODE PLATFORM

#if MODE == CLASS
static struct class* my_sysfs_class;
static struct device* my_sysfs_device;
#elif MODE == PLATFORM

static void sysfs_dev_release(struct device* dev) {}
static struct platform_device sysfs_device = {
    .name        = "my_dev_sysfs",
    .id          = -1,
    .dev.release = sysfs_dev_release,
};

#endif
static char sysfs_buf[1000];

ssize_t sysfs_show_attr(struct device* dev, struct device_attribute* attr, char* buf)
{
    strcpy(buf, sysfs_buf);
    return strlen(buf);
}
ssize_t sysfs_store_attr(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    int len = sizeof(sysfs_buf) - 1;
    if (len > count) len = count;
    strncpy(sysfs_buf, buf, len);
    sysfs_buf[len] = 0;
    return len;
}

DEVICE_ATTR(val, 0664, sysfs_show_attr, sysfs_store_attr); // Create : struct device_attribute dev_attr_val

static int __init mod_init(void)
{
    int status = 0;
#if MODE == PLATFORM
        status = platform_device_register(&sysfs_device);
    if (status == 0)
        status = device_create_file(&sysfs_device.dev, &dev_attr_val);
#elif MODE == CLASS
    my_sysfs_class = class_create(THIS_MODULE, "my_sysfs_class");
    my_sysfs_device = device_create(my_sysfs_class, NULL, 0, NULL, "my_sysfs_device");
    status = device_create_file(my_sysfs_device, &dev_attr_val);
#endif
    return status;
}
static void __exit mod_exit(void)
{

#if MODE == PLATFORM
    device_remove_file(&sysfs_device.dev, &dev_attr_val);
    platform_device_unregister(&sysfs_device);

#elif MODE == CLASS
    device_remove_file(my_sysfs_device, &dev_attr_val);
    device_destroy(my_sysfs_class, 0);
    class_destroy(my_sysfs_class);
#endif
}

module_init (mod_init);
module_exit (mod_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP03 CSEL");
MODULE_LICENSE ("GPL");
