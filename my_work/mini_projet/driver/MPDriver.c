#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/device.h>          /* needed for sysfs handling */
#include <linux/platform_device.h> /* needed for sysfs handling */
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/mutex.h>

#include "MPDriver.h"

#define FREQ_AUTO_COOL 1
#define FREQ_DEFAULT_LED 2
#define GPIO_LED_STATUS 10
#define CHAR_MAX 255

typedef enum {
    NOT_EXIST, MODE, BLINKING, TEMPERATURE
} attribute_t;

typedef enum {
    MANUAL, AUTOMATIC
} cooling_mode_t;

static struct class* sysfs_class;
static struct device* sysfs_device;
static char coolingMode = AUTOMATIC;
static char blinkingFreq = FREQ_DEFAULT_LED;
static char tempC = 0;
DEFINE_MUTEX(mutAttr);
static struct thermal_zone_device *thermZone;
static struct timer_list timer_auto_cooling;
static struct timer_list timer_led;

int timer_set_freq(struct timer_list* t, int freq){
    return mod_timer(t, jiffies + msecs_to_jiffies(1000 / freq));
}
//Called
void auto_cooling_callback(struct timer_list *t){
    int temp;
    int retval;
    mutex_lock(&mutAttr);
    if(coolingMode == AUTOMATIC)
        timer_set_freq(t, FREQ_AUTO_COOL); //Necessary to set timer at each time end
    else
        mod_timer(&timer_auto_cooling, LONG_MAX);

    mutex_unlock(&mutAttr);

    retval = thermal_zone_get_temp(thermZone, &temp);
    temp /= 1000; //mC => °C

    if (retval < 0){
        printk(KERN_ERR"[MPDriver] Failed to get temperature from thermal zone, %d\n", retval);
        return;
    }else{
        //printk(KERN_INFO"[MPDriver] Temperature: %d°C\n", temp); 
    }
    tempC = temp;

    mutex_lock(&mutAttr);
    if(temp < 35){
        blinkingFreq = 2;
    }else if(temp < 40){
        blinkingFreq = 5;
    }else if(temp < 45){
        blinkingFreq = 10;
    }else{
        blinkingFreq = 20;
    }
    mutex_unlock(&mutAttr);
}

void led_timer_callback(struct timer_list *t){
    static int ledVal = 0;
    ledVal = !ledVal;
    
    gpio_set_value(GPIO_LED_STATUS, ledVal);
    timer_set_freq(t, blinkingFreq * 2);
}

attribute_t get_attr(struct device_attribute* attr){
    if(strncmp(attr->attr.name, ATTR_NAME_MODE, strlen(ATTR_NAME_MODE) ) == 0)
        return MODE;
    else if(strncmp(attr->attr.name, ATTR_NAME_BLINKING, strlen(ATTR_NAME_BLINKING)) == 0)
        return BLINKING;
    else if(strncmp(attr->attr.name, ATTR_NAME_TEMPERATURE, strlen(ATTR_NAME_TEMPERATURE)) == 0)
        return TEMPERATURE;
    return NOT_EXIST;
}

//Return 0 on success
int attr_write_mode(char mode){
    int status = 1;
    if(mode != MANUAL && mode != AUTOMATIC){
        printk(KERN_ERR"[MPDriver] Value mode %d not authorized\n", mode);
        return -1;
    }

    printk(KERN_INFO"[MPDriver] New mode : %d\n", mode);
    if(coolingMode == MANUAL && mode == AUTOMATIC){ // MANUAL => AUTOMATIC
        status = timer_set_freq(&timer_auto_cooling, FREQ_AUTO_COOL);
    }

    coolingMode =  mode; 
    return status > 0 ? 0 : -1;
}

//Return 0 on success
int attr_write_blinking(char freq){
    if(coolingMode == AUTOMATIC){
        printk(KERN_ERR"[MPDriver] Forbidden to manually modify frequency in automatic mode\n");
        return -1;
    }
    if(freq == 0 || freq == CHAR_MAX){
        printk(KERN_ERR"[MPDriver] Impossible to set frequency 0Hz\n");
        return -1;
    }
    printk(KERN_INFO"[MPDriver] New blinking frequency : %dHz\n", freq);
    blinkingFreq = freq;
    return 0;
}
ssize_t sysfs_show_attr(struct device* dev, struct device_attribute* attr, char* buf)
{
    //Impossible to know if destination buffer is wide enough.
    char valWrite;
    int nbWritten;
    
    switch(get_attr(attr)){
        case MODE :
            mutex_lock(&mutAttr);
            valWrite = coolingMode;
            mutex_unlock(&mutAttr);
            break;
        case BLINKING :
            mutex_lock(&mutAttr);
            valWrite = blinkingFreq;
            mutex_unlock(&mutAttr);
            break;
        case TEMPERATURE :
            mutex_lock(&mutAttr);
            valWrite = tempC;
            mutex_unlock(&mutAttr);
            break;
        default:
           return -1;
    }

    nbWritten = snprintf(buf, ATTR_MAX_VAL_CHARS + 1 , "%d", valWrite); //Size has to include null terminated
    return  nbWritten;
}
ssize_t sysfs_store_attr(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    char safeBuf[ATTR_MAX_VAL_CHARS + 1] = {0};
    int nbCharsBuf;
    char intSafeBuf;
    int ret;

    nbCharsBuf = count <= ATTR_MAX_VAL_CHARS ? count : ATTR_MAX_VAL_CHARS;
    //printk("strlen: %ld, count :%ld\n", retval, count);
    memcpy(safeBuf, buf, nbCharsBuf);
    
    intSafeBuf = simple_strtol(safeBuf, NULL, 10); //Convert on safe buffer
    
    switch(get_attr(attr)){
        case MODE :
            mutex_lock(&mutAttr);
            attr_write_mode(intSafeBuf);
            mutex_unlock(&mutAttr);
            break;
        case BLINKING :
            mutex_lock(&mutAttr);
            ret = attr_write_blinking(intSafeBuf);
            mutex_unlock(&mutAttr);
            if( ret < 0)
                return -1;
            break;
        default:
            return -1;
    }
    return count; //We handled all text
}


DEVICE_ATTR(mode, 0664, sysfs_show_attr, sysfs_store_attr); // Create : struct device_attribute dev_attr_val
DEVICE_ATTR(blinking, 0664, sysfs_show_attr, sysfs_store_attr); // Create : struct device_attribute dev_attr_val
DEVICE_ATTR(temperature, 0444, sysfs_show_attr, sysfs_store_attr); // Create : struct device_attribute dev_attr_val

static int __init mod_init(void)
{
    int retVal = 0;
    console_loglevel = 0;

    sysfs_class = class_create(THIS_MODULE, "mpcooling");
    if (IS_ERR(sysfs_class)){
        printk(KERN_ERR"[MPDriver] Impossible to create the device class\n");
        return -1;
    }
    sysfs_device = device_create(sysfs_class, NULL, 0, NULL, "controller");
    if (IS_ERR(sysfs_device)){
        printk(KERN_ERR"[MPDriver] Impossible to create the device\n");
        return -1;
    }
    retVal = device_create_file(sysfs_device, &dev_attr_mode);
    if(retVal < 0){
        printk(KERN_ERR"[MPDriver] Failed to create attribute file mode\n");
        return -1;
    }
    retVal= device_create_file(sysfs_device, &dev_attr_blinking);
    if(retVal < 0){
        printk(KERN_ERR"[MPDriver] Failed to create attribute file blinking\n");
        return -1;
    }
    retVal= device_create_file(sysfs_device, &dev_attr_temperature);
    if(retVal < 0){
        printk(KERN_ERR"[MPDriver] Failed to create attribute file temperature\n");
        return -1;
    }

    //Thermal zone
    thermZone = thermal_zone_get_zone_by_name("cpu-thermal");
    if (!thermZone) {
        printk(KERN_ERR"[MPDriver] Impossible to set thermal zone\n");
        return -1;
    }

    //Leds setup
    retVal = gpio_request(GPIO_LED_STATUS, "gpio_led_status");
    if (retVal < 0) {
        printk(KERN_ERR"[MPDriver] Failed to request GPIO pin\n");
        return -1;
    }
    retVal = gpio_direction_output(GPIO_LED_STATUS, 1);
    if (retVal < 0) {
        printk(KERN_ERR"[MPDriver] Failed to set GPIO direction\n");
        gpio_free(GPIO_LED_STATUS);  // Free the GPIO pin
        return -1;
    }

    //Timer setup
    timer_setup(&timer_auto_cooling, auto_cooling_callback, 0);
    timer_setup(&timer_led, led_timer_callback, 0);

    retVal = timer_set_freq(&timer_auto_cooling, FREQ_AUTO_COOL);
    if(retVal < 0){
        printk(KERN_ERR"[MPDriver] Failed to setup timer for auto coolling\n");
        return 1;
    }
    retVal = timer_set_freq(&timer_led, blinkingFreq); //Divided by two because one period
    if(retVal < 0){
        printk(KERN_ERR"[MPDriver] Failed to setup timer for led blinking\n");
        return 1;
    }

    return 0;
}
static void __exit mod_exit(void)
{
    device_remove_file(sysfs_device, &dev_attr_mode);
    device_remove_file(sysfs_device, &dev_attr_blinking);
    device_remove_file(sysfs_device, &dev_attr_temperature);
    device_destroy(sysfs_class, 0);
    class_destroy(sysfs_class);

    //Free leds
    gpio_free(GPIO_LED_STATUS);

    del_timer(&timer_auto_cooling);
    del_timer(&timer_led);
}

module_init (mod_init);
module_exit (mod_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Mini Project Driver");
MODULE_LICENSE ("GPL");
