#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/moduleparam.h>  /* needed for module parameters */
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>

 struct task_struct* t;

int thread (void* data)
{
    while (!kthread_should_stop()) {
        printk("Hello from kernel ! :) \n");
        ssleep(5);
    }
    return 0;
}

static int __init mymodule_init(void)
{
  t= kthread_run(thread, NULL, "my_kernel_thread");
  printk("Run kernel thread\n");
  return 0;
}

static void __exit mymodule_exit(void)
{
    printk("Stop kernel thread\n");
    kthread_stop(t);

}

module_init (mymodule_init);
module_exit (mymodule_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP02 CSEL");
MODULE_LICENSE ("GPL");
