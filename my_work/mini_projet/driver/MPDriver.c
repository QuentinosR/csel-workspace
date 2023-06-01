#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/device.h>          /* needed for sysfs handling */
#include <linux/platform_device.h> /* needed for sysfs handling */

#include <linux/thermal.h>
#include <linux/gpio.h>
#include <linux/timer.h>

#define ATTR_MAX_VAL_CHARS 2
#define INTERVAL_AUTO_COOL_TIMER_S 5

static struct class* sysfs_class;
static struct device* sysfs_device;
static char coolingMode = 0;
static char blinkingFreq = 5;
static struct thermal_zone_device *thermZone;
static struct timer_list timer_auto_cooling;
static struct timer_list timer_led;
typedef enum {
    NOT_EXIST,MODE, BLINKING
} attribute_t;


//Called
void auto_cooling_callback(struct timer_list *t){
    int temp;
    int retval;

    printk("cooling callback\n");

    retval = thermal_zone_get_temp(thermZone, &temp);

    if (retval < 0){
        printk("Failed to get temperature from thermal zone, %d\n", retval);
    }else{
        printk("Temperature: %d°C\n", temp / 1000); //mC
    }

    //Check temperature
    //Modify blink timer if nec.
    mod_timer(&timer_auto_cooling, jiffies + msecs_to_jiffies(INTERVAL_AUTO_COOL_TIMER_S *1000));
}

void led_timer_callback(struct timer_list *t){
    printk("led callback\n");
}

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

    //Timer setup to control temp.
    //del_timer(struct timer_list * timer);

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
    //int mod_timer(struct timer_list *timer, unsigned long expires);
    return 0;
}
ssize_t sysfs_show_attr(struct device* dev, struct device_attribute* attr, char* buf)
{
    //Impossible to know if destination buffer is wide enough.
    char valWrite;
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
    
    return snprintf(buf, ATTR_MAX_VAL_CHARS, "%d", valWrite); //Return number of chars written
}
ssize_t sysfs_store_attr(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    char safeBuf[ATTR_MAX_VAL_CHARS + 1] = {0};
    int nbCharsBuf;
    char intSafeBuf;

    nbCharsBuf = count <= ATTR_MAX_VAL_CHARS ? count : ATTR_MAX_VAL_CHARS;
    //printk("strlen: %ld, count :%ld\n", retval, count);
    memcpy(safeBuf, buf, nbCharsBuf);
    
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
    //printk("retval : %ld\n", retval);
    return count; //We handled all text
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

    //Thermal zone
    thermZone = thermal_zone_get_zone_by_name("cpu-thermal");
    if (!thermZone) {
        printk("Impossible to set thermal zone\n");
        return 1;
    }

    //Timer setup
    timer_setup(&timer_auto_cooling, auto_cooling_callback, 0);
    timer_setup(&timer_led, led_timer_callback, 0);
    mod_timer(&timer_auto_cooling, jiffies + msecs_to_jiffies(INTERVAL_AUTO_COOL_TIMER_S *1000));
    mod_timer(&timer_led, jiffies + msecs_to_jiffies(1000 / blinkingFreq));
    return status;
}
static void __exit mod_exit(void)
{
    device_remove_file(sysfs_device, &dev_attr_mode);
    device_remove_file(sysfs_device, &dev_attr_blinking);
    device_destroy(sysfs_class, 0);
    class_destroy(sysfs_class);

    del_timer(&timer_auto_cooling);
    del_timer(&timer_led);
}

module_init (mod_init);
module_exit (mod_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Mini Project Driver");
MODULE_LICENSE ("GPL");
