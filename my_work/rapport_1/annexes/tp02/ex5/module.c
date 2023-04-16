#include <linux/module.h>  // needed by all modules
#include <linux/init.h>    // needed for macros
#include <linux/kernel.h>  // needed for debugging
#include <linux/moduleparam.h>  /* needed for module parameters */
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/ioport.h>


#define PHYS_ADDR_START_ID 0x01c14200
#define PHYS_ADDR_END_ID 0x01c14210
#define ID_SIZE PHYS_ADDR_END_ID - PHYS_ADDR_START_ID

#define PHYS_ADDR_START_TEMP 0x01c25080
#define PHYS_ADDR_END_TEMP 0x01c25084
#define TEMP_SIZE PHYS_ADDR_END_TEMP - PHYS_ADDR_START_TEMP

#define PHYS_ADDR_START_MAC 0x01c30050
#define PHYS_ADDR_END_MAC 0x01c30058
#define MAC_SIZE PHYS_ADDR_END_MAC - PHYS_ADDR_START_MAC

static int __init mymodule_init(void)
{
    struct resource* r_temp = request_mem_region(PHYS_ADDR_START_TEMP, TEMP_SIZE , "my_reserv_temp");
    if( r_temp == NULL)
        printk("Impossible to reserve temperature space\n");
    else
        release_mem_region(PHYS_ADDR_START_TEMP, TEMP_SIZE);

    struct resource* r_id = request_mem_region(PHYS_ADDR_START_ID, ID_SIZE , "my_reserv_id");
    if( r_id == NULL)
        printk("Impossible to reserve id space\n");
    else
        release_mem_region(PHYS_ADDR_START_ID, ID_SIZE);

    struct resource* r_mac = request_mem_region(PHYS_ADDR_START_MAC, MAC_SIZE , "my_reserv_mac");
    if( r_mac== NULL)
        printk("Impossible to reserve mac space\n");
    else
        release_mem_region(PHYS_ADDR_START_MAC, MAC_SIZE);

    void* temp_ptr = ioremap(PHYS_ADDR_START_TEMP, TEMP_SIZE);
    if(temp_ptr != NULL){
         uint32_t temp = readl(temp_ptr);
        printk("temp : %d\n", -1* 1191 * (temp/10) + 223000);
        iounmap(temp_ptr);
    }

    void* id_ptr_part_1 = ioremap(PHYS_ADDR_START_ID, ID_SIZE / 2);
    if(id_ptr_part_1 != NULL){
         uint64_t id_part_1 = readq(id_ptr_part_1);
         printk("id part 1 : %llx \n", id_part_1);
         iounmap(id_ptr_part_1);
    }
    void* id_ptr_part_2 = ioremap(PHYS_ADDR_START_ID +  ID_SIZE / 2 , ID_SIZE / 2);
     if(id_ptr_part_2 != NULL){
         uint64_t id_part_2 = readq(id_ptr_part_2);
         printk("id part 2 : %llx \n", id_part_2);
         iounmap(id_ptr_part_2);
    }

    void* mac_ptr = ioremap(PHYS_ADDR_START_MAC, MAC_SIZE);
    if(mac_ptr != NULL){
        uint64_t mac = readq(mac_ptr);
        printk("mac : %llx\n", mac);
        iounmap(mac_ptr);
    }

    return 0;
}

static void __exit mymodule_exit(void){}
module_init (mymodule_init);
module_exit (mymodule_exit);
MODULE_AUTHOR ("Quentin Rod <quentin.rod@hes-so.ch>");
MODULE_DESCRIPTION ("Personalized module for TP02 CSEL");
MODULE_LICENSE ("GPL");
