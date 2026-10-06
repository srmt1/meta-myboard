#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>

struct my_device {
    int value;
};

static int my_probe(struct platform_device *pdev)
{
    struct my_device *data;

    dev_info(&pdev->dev, "my_device probe()\n");

    data = devm_kzalloc(&pdev->dev,
                        sizeof(*data),
                        GFP_KERNEL);

    if (!data)
        return -ENOMEM;

    data->value = 1234;

    platform_set_drvdata(pdev, data);

    dev_info(&pdev->dev,
             "driver data value = %d\n",
             data->value);

    return 0;
}

static int my_remove(struct platform_device *pdev)
{
    struct my_device *data;

    data = platform_get_drvdata(pdev);

    dev_info(&pdev->dev,
             "my_device remove(), value = %d\n",
             data->value);

    return 0;
}

static const struct of_device_id my_device_of_match[] = {
    {
        .compatible = "example,my-device",
    },
    { }
};

MODULE_DEVICE_TABLE(of, my_device_of_match);

static struct platform_driver my_device_driver = {
    .probe  = my_probe,
    .remove = my_remove,

    .driver = {
        .name = "my-device",
        .of_match_table = my_device_of_match,
    },
};

module_platform_driver(my_device_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Course");
MODULE_DESCRIPTION("Device Tree driven example driver");

