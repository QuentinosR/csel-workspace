#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/moduleparam.h>  /* needed for module parameters */
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#define IO_BUTTON_K1 0
#define IO_BUTTON_K2 2
#define IO_BUTTON_K3 3

static int k1_irq_nb;
static int k2_irq_nb;
static int k3_irq_nb;

static irqreturn_t buttons_isr(int irq, void *dev_id)
{
	printk("Button pushed %s!\n", (char*)dev_id);

	return IRQ_HANDLED;
}

static int __init mymodule_init(void)
{   
    if(gpio_request(IO_BUTTON_K1,"button_k1") < 0) 
        return -1;
    if(gpio_request(IO_BUTTON_K2,"button_k2") < 0) 
        return -1;
    if(gpio_request(IO_BUTTON_K3,"button_k3") < 0) 
        return -1;

    if( (k1_irq_nb =  gpio_to_irq(IO_BUTTON_K1))  < 0)
        return -1;

    if( (k2_irq_nb =  gpio_to_irq(IO_BUTTON_K2))  < 0)
        return -1;

    if( (k3_irq_nb =  gpio_to_irq(IO_BUTTON_K3))  < 0)
        return -1;

	if( request_irq( k1_irq_nb, buttons_isr ,IRQF_TRIGGER_RISING, "int_k1", "k1")) 
        return -1;

    if( request_irq( k2_irq_nb, buttons_isr ,IRQF_TRIGGER_RISING, "int_k2", "k2")) 
        return -1;

    if( request_irq( k3_irq_nb, buttons_isr ,IRQF_TRIGGER_RISING, "int_k3", "k3")) 
        return -1;

    return 0;
}

static void __exit mymodule_exit(void)
{
    gpio_free(IO_BUTTON_K1);
    gpio_free(IO_BUTTON_K2);
    gpio_free(IO_BUTTON_K3);
    free_irq (k1_irq_nb, "k1");
    free_irq (k2_irq_nb, "k2");
    free_irq (k3_irq_nb, "k3");
}

module_init (mymodule_init);
module_exit (mymodule_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP02 CSEL");
MODULE_LICENSE ("GPL");
