#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/device.h>          /* needed for sysfs handling */
#include <linux/platform_device.h> /* needed for sysfs handling */
#define ATTR_MAX_VAL_CHARS 2

static struct class* sysfs_class;
static struct device* sysfs_device;
char coolingMode = 0;
char blinkingFreq = 0;

typedef enum {
    NOT_EXIST,MODE, BLINKING
} attribute_t;

attribute_t get_attr(struct device_attribute* attr){
    if(strncmp(attr->attr.name, "mode", 4 ) == 0){
        return MODE;
    }else if(strncmp(attr->attr.name, "blinking", 8) == 0){
        return BLINKING;
    }
    return NOT_EXIST;
}


//Return 0 on success
int attr_write_mode(char mode){
    if(mode != 0 && mode != 1){
        printk("value mode %d not authorized\n", mode);
        return -1;
    }
    printk("new mode : %d\n", mode);
    coolingMode =  mode; 

    return 0;
}

//Return 0 on success
int attr_write_blinking(char freq){
    if( freq != 5 && freq != 10 && freq != 15 && freq != 20){
        printk("value blink %d not authorized\n", freq);
        return -1;
    }

    printk("new blink : %d\n", freq);
    blinkingFreq = freq;
    return 0;
}
ssize_t sysfs_show_attr(struct device* dev, struct device_attribute* attr, char* buf)
{
    ssize_t lenBuf;
    char valWrite;

    lenBuf = strnlen(buf, ATTR_MAX_VAL_CHARS);
    if(lenBuf < ATTR_MAX_VAL_CHARS){ //Not enough memory
        return -1;
    }

    switch(get_attr(attr)){
        case MODE :
            valWrite = coolingMode;
            break;
        case BLINKING :
            valWrite = blinkingFreq;
            break;
        default:
           return -1;
    }
    
    return snprintf(buf, sizeof(buf), "%d", valWrite); //Return number of chars written
}
ssize_t sysfs_store_attr(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    char safeBuf[ATTR_MAX_VAL_CHARS + 1] = {0};
    ssize_t retval;
    char intSafeBuf;
    
    
    retval = strnlen(buf, ATTR_MAX_VAL_CHARS);
    strncpy(safeBuf, buf, ATTR_MAX_VAL_CHARS);
    
    intSafeBuf = simple_strtol(safeBuf, NULL, 10); //Convert on safe buffer
    switch(get_attr(attr)){
        case MODE :
            attr_write_mode(intSafeBuf);
            break;
        case BLINKING :
            attr_write_blinking(intSafeBuf);
            break;
        default:
            break;
    }
    
    return retval + 1; //Otherwise echo redo one write
}

DEVICE_ATTR(mode, 0664, sysfs_show_attr, sysfs_store_attr); // Create : struct device_attribute dev_attr_val
DEVICE_ATTR(blinking, 0664, sysfs_show_attr, sysfs_store_attr); // Create : struct device_attribute dev_attr_val

static int __init mod_init(void)
{
    int status = 0;
    sysfs_class = class_create(THIS_MODULE, "mpcooling");
    sysfs_device = device_create(sysfs_class, NULL, 0, NULL, "controller");
    status = device_create_file(sysfs_device, &dev_attr_mode);
    status |= device_create_file(sysfs_device, &dev_attr_blinking);
    return status;
}
static void __exit mod_exit(void)
{
    device_remove_file(sysfs_device, &dev_attr_mode);
    device_remove_file(sysfs_device, &dev_attr_blinking);
    device_destroy(sysfs_class, 0);
    class_destroy(sysfs_class);
}

module_init (mod_init);
module_exit (mod_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Mini Project Driver");
MODULE_LICENSE ("GPL");
