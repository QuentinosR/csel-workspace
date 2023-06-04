#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/device.h>          /* needed for sysfs handling */
#include <linux/platform_device.h> /* needed for sysfs handling */

#include <linux/thermal.h>
#include <linux/gpio.h>
#include <linux/timer.h>

#define ATTR_MAX_VAL_CHARS 2
#define FREQ_AUTO_COOL 1
#define FREQ_DEFAULT_LED 2
#define GPIO_LED_STATUS 10
#define CHAR_MAX 255

typedef enum {
    NOT_EXIST, MODE, BLINKING
} attribute_t;

typedef enum {
    MANUAL, AUTOMATIC
} cooling_mode_t;

static struct class* sysfs_class;
static struct device* sysfs_device;
static char coolingMode = AUTOMATIC;
static char blinkingFreq = FREQ_DEFAULT_LED;
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

    timer_set_freq(t, FREQ_AUTO_COOL); //Necessary to set timer at each time end

    retval = thermal_zone_get_temp(thermZone, &temp);
    temp /= 1000; //mC => °C

    if (retval < 0){
        printk("[MPDriver] Failed to get temperature from thermal zone, %d\n", retval);
        return;
    }else{
        //printk("[MPDriver] Temperature: %d°C\n", temp); 
    }

    if(temp < 35){
        blinkingFreq = 2;
    }else if(temp < 40){
        blinkingFreq = 5;
    }else if(temp < 45){
        blinkingFreq = 10;
    }else{
        blinkingFreq = 20;
    }
}

void led_timer_callback(struct timer_list *t){
    static int ledVal = 0;
    ledVal = !ledVal;
    
    gpio_set_value(GPIO_LED_STATUS, ledVal);
    timer_set_freq(t, blinkingFreq * 2);
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
    int status = 1;
    if(mode != MANUAL && mode != AUTOMATIC){
        printk(KERN_INFO"[MPDriver] value mode %d not authorized\n", mode);
        return -1;
    }
    printk(KERN_INFO"[MPDriver] new mode : %d\n", mode);

    if(coolingMode == MANUAL && mode == AUTOMATIC){ // MANUAL => AUTOMATIC
        status = timer_set_freq(&timer_auto_cooling, FREQ_AUTO_COOL);
    }else if(coolingMode == AUTOMATIC && mode == MANUAL){ // AUTOMATIC => MANUAL
        //Next period is in max possible time
        status = mod_timer(&timer_auto_cooling, LONG_MAX);
    }

    coolingMode =  mode; 
    return status > 0 ? 0 : -1;
}

//Return 0 on success
int attr_write_blinking(char freq){
    if(coolingMode == AUTOMATIC){
        printk(KERN_INFO"[MPDriver] Forbidden to manually modify frequency in automatic mode\n");
        return -1;
    }
    if(freq == 0 || freq == CHAR_MAX){
        printk(KERN_INFO"[MPDriver] Impossible to set frequency 0Hz\n");
        return -1;
    }
    printk(KERN_INFO"[MPDriver] new blinking frequency : %d\n", freq);
    blinkingFreq = freq;
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
    return count; //We handled all text
}

DEVICE_ATTR(mode, 0664, sysfs_show_attr, sysfs_store_attr); // Create : struct device_attribute dev_attr_val
DEVICE_ATTR(blinking, 0664, sysfs_show_attr, sysfs_store_attr); // Create : struct device_attribute dev_attr_val

static int __init mod_init(void)
{
    int retVal = 0;
    sysfs_class = class_create(THIS_MODULE, "mpcooling");
    if (IS_ERR(sysfs_class)){
        printk(KERN_ERR"Impossible to create the device class\n");
        return -1;
    }
    sysfs_device = device_create(sysfs_class, NULL, 0, NULL, "controller");
    if (IS_ERR(sysfs_device)){
        printk(KERN_ERR"Impossible to create the device\n");
        return -1;
    }
    retVal = device_create_file(sysfs_device, &dev_attr_mode);
    if(retVal < 0){
        printk(KERN_ERR "Failed to create attribute file mode\n");
        return -1;
    }
    retVal= device_create_file(sysfs_device, &dev_attr_blinking);
    if(retVal < 0){
        printk(KERN_ERR "Failed to create attribute file blinking\n");
        return -1;
    }

    //Thermal zone
    thermZone = thermal_zone_get_zone_by_name("cpu-thermal");
    if (!thermZone) {
        printk(KERN_ERR "Impossible to set thermal zone\n");
        return -1;
    }

    //Leds setup
    retVal = gpio_request(GPIO_LED_STATUS, "gpio_led_status");
    if (retVal < 0) {
        printk(KERN_ERR "Failed to request GPIO pin\n");
        return -1;
    }
    retVal = gpio_direction_output(GPIO_LED_STATUS, 1);
    if (retVal < 0) {
        printk(KERN_ERR "Failed to set GPIO direction\n");
        gpio_free(GPIO_LED_STATUS);  // Free the GPIO pin
        return -1;
    }

    //Timer setup
    timer_setup(&timer_auto_cooling, auto_cooling_callback, 0);
    timer_setup(&timer_led, led_timer_callback, 0);

    retVal = timer_set_freq(&timer_auto_cooling, FREQ_AUTO_COOL);
    if(retVal < 0){
        printk(KERN_ERR "Failed to setup timer for auto coolling\n");
        return 1;
    }
    retVal = timer_set_freq(&timer_led, blinkingFreq); //Divided by two because one period
    if(retVal < 0){
        printk(KERN_ERR "Failed to setup timer for led blinking\n");
        return 1;
    }

    return 0;
}
static void __exit mod_exit(void)
{
    device_remove_file(sysfs_device, &dev_attr_mode);
    device_remove_file(sysfs_device, &dev_attr_blinking);
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
