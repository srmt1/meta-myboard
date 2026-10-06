#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "mydevice"
#define CLASS_NAME  "myclass"
#define BUFFER_SIZE 128

static dev_t dev_number;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;

static char message[BUFFER_SIZE];
static size_t message_length;

static int my_open(struct inode *inode, struct file *file)
{
    pr_info("mydriver: device opened\n");

    return 0;
}

static ssize_t my_read(struct file *file,
                       char __user *buffer,
                       size_t count,
                       loff_t *offset)
{
    size_t bytes_to_copy;

    if (*offset >= message_length)
        return 0;

    bytes_to_copy = message_length - *offset;

    if (count < bytes_to_copy)
        bytes_to_copy = count;

    if (copy_to_user(buffer,
                     message + *offset,
                     bytes_to_copy))
    {
        return -EFAULT;
    }

    *offset += bytes_to_copy;

    return bytes_to_copy;
}

static ssize_t my_write(struct file *file,
                        const char __user *buffer,
                        size_t count,
                        loff_t *offset)
{
    size_t bytes_to_copy;

    bytes_to_copy = count;

    if (bytes_to_copy >= BUFFER_SIZE)
        bytes_to_copy = BUFFER_SIZE - 1;

    if (copy_from_user(message,
                       buffer,
                       bytes_to_copy))
    {
        return -EFAULT;
    }

    message[bytes_to_copy] = '\0';
    message_length = bytes_to_copy;

    pr_info("mydriver: received %zu bytes\n",
            bytes_to_copy);

    return bytes_to_copy;
}

static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .read = my_read,
    .write = my_write,
};

static int __init mydriver_init(void)
{
    int result;

    pr_info("mydriver: loading\n");

    result = alloc_chrdev_region(&dev_number,
                                 0,
                                 1,
                                 DEVICE_NAME);

    if (result < 0)
        return result;

    cdev_init(&my_cdev, &my_fops);

    result = cdev_add(&my_cdev,
                      dev_number,
                      1);

    if (result < 0)
        goto unregister_device;

    //my_class = class_create(THIS_MODULE, CLASS_NAME);
    my_class = class_create(CLASS_NAME);

    if (IS_ERR(my_class))
    {
        result = PTR_ERR(my_class);
        goto delete_cdev;
    }

    my_device = device_create(my_class,
                              NULL,
                              dev_number,
                              NULL,
                              DEVICE_NAME);

    if (IS_ERR(my_device))
    {
        result = PTR_ERR(my_device);
        goto destroy_class;
    }

    pr_info("mydriver: loaded successfully\n");

    return 0;

destroy_class:
    class_destroy(my_class);

delete_cdev:
    cdev_del(&my_cdev);

unregister_device:
    unregister_chrdev_region(dev_number, 1);

    return result;
}

static void __exit mydriver_exit(void)
{
    device_destroy(my_class, dev_number);
    class_destroy(my_class);

    cdev_del(&my_cdev);

    unregister_chrdev_region(dev_number, 1);

    pr_info("mydriver: unloaded\n");
}

module_init(mydriver_init);
module_exit(mydriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Course");
MODULE_DESCRIPTION("Simple character device driver");

