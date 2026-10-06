
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int number = 10;

module_param(number, int, 0644);

static int __init hello_init(void)
{
    printk(KERN_INFO "Hello from Raspberry Pi kernel module\n");
    printk(KERN_INFO "number = %d\n", number);
    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "Goodbye from Raspberry Pi kernel module\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Course");
MODULE_DESCRIPTION("Simple Raspberry Pi kernel module");

