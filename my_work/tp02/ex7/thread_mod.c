#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/moduleparam.h>  /* needed for module parameters */
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
//Interesting link to understand https://embetronicx.com/tutorials/linux/device-drivers/waitqueue-in-linux-device-driver-tutorial/

struct task_struct* t_notifier;
struct task_struct* t_receiver;
wait_queue_head_t queue;
int flag = 0;

int thread_receiver (void* data)
{
    while (!kthread_should_stop()) {
        wait_event_interruptible(queue, flag == 1); //Sleep exit condition
        flag = 0; // Reset flag
        printk("Receiver awake\n");
    }
    return 0;
}
int thread_notifier (void* data)
{
    while (!kthread_should_stop()) {
        printk("Notifier awake\n");
        flag = 1; // Sleep exit for receiver
        wake_up_interruptible(&queue); //Wake up queue and evaluates condition
        ssleep(5);
    }
    //To remove module
    flag = 1;
    wake_up_interruptible(&queue);
    return 0;
}

static int __init mymodule_init(void)
{   
    init_waitqueue_head(&queue);
    t_notifier = kthread_run(thread_notifier, NULL, "Notifier thread");
    t_receiver = kthread_run(thread_receiver, NULL, "Receiver thread");
    printk("Run kernel threads\n");
    return 0;
}

static void __exit mymodule_exit(void)
{
    printk("Stop kernel threads\n");
    kthread_stop(t_notifier);
    kthread_stop(t_receiver);

}

module_init (mymodule_init);
module_exit (mymodule_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP02 CSEL");
MODULE_LICENSE ("GPL");
