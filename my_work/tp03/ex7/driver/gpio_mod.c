#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/moduleparam.h>  /* needed for module parameters */
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#define IO_BUTTON_K1 0
static int k1_irq_nb;
static atomic_t inter_cnt;
DECLARE_WAIT_QUEUE_HEAD(queue);

static ssize_t driver_read(struct file* f, char * buf, size_t count, loff_t* off)
{
    return 0;
}

static unsigned int driver_poll(struct file* f, poll_table* t)
{
    poll_wait(f, &queue, t);
    if (atomic_read(&inter_cnt) != 0) {
        atomic_dec(&inter_cnt); //Reset
        printk("Read is possible by user !\n");
        return EPOLLIN | EPOLLRDNORM; //Data is available
    }
    return 0;
}

static struct file_operations driver_fops = {
    .owner   = THIS_MODULE,
    .read    = driver_read,
    .poll = driver_poll,
};

static struct miscdevice misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "inter_cnt",
    .fops  = &driver_fops,
    .mode  = 0660,
};

static irqreturn_t buttons_isr(int irq, void *dev_id)
{
    atomic_inc(&inter_cnt);
    wake_up_interruptible(&queue);
	printk("Button pushed !\n");
	return IRQ_HANDLED;
}

static int __init mymodule_init(void)
{   

    if(misc_register(&misc_dev) < 0)
        return -1;

    if(gpio_request(IO_BUTTON_K1,"button_k1") < 0) 
        return -1;
  
    if( (k1_irq_nb =  gpio_to_irq(IO_BUTTON_K1)) < 0)
        return -1;

	if(request_irq(k1_irq_nb, buttons_isr ,IRQF_TRIGGER_RISING, "int_k1", "k1")) 
        return -1;

    return 0;
}

static void __exit mymodule_exit(void)
{
    misc_deregister(&misc_dev);
    gpio_free(IO_BUTTON_K1);
    free_irq (k1_irq_nb, "k1");
}

module_init (mymodule_init);
module_exit (mymodule_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP02 CSEL");
MODULE_LICENSE ("GPL");
