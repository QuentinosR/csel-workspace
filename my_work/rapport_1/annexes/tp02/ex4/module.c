// skeleton.c
#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/moduleparam.h>  /* needed for module parameters */
#include <linux/slab.h>

struct l_element {
    char string[100];
    int32_t ID;
    struct list_head node;
};

static char* initString = "";
module_param(initString, charp, 0);

static int nbElements = 0;
module_param(nbElements, int, 0);

static LIST_HEAD (globList); // definition of the global list
static uint32_t cpt = 0; //Counter of number of elements in list


// process all elements of the list
static void print_list(struct list_head *head) {
    struct l_element* ele;
    list_for_each_entry(ele, head, node) { 
      pr_info("[ID : %d, string : %s]\n", ele->ID, ele->string);
    }
}

static int __init mymodule_init(void)
{
    int i;
    pr_info ("My module loaded\n");
    for(i = 0; i < nbElements; i++){
        
        struct l_element* ele;
        ele = kzalloc(sizeof(*ele), GFP_KERNEL);
        if (ele != NULL)
        {
            ele->ID = cpt++;
            strcpy(ele->string, initString); 
            // add element at the end of the list 
            list_add_tail(&ele->node, &globList); 
        }
    }

    print_list(&globList); //Debugging
    return 0;
}

static void __exit mymodule_exit(void)
{
    struct l_element* ele;
    pr_info ("My module unloaded\n");

    list_for_each_entry(ele, &globList, node) { 
      kfree(ele);
    }
}

module_init (mymodule_init);
module_exit (mymodule_exit);

MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP02 CSEL");
MODULE_LICENSE ("GPL");
