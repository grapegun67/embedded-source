#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#define GPIO_LED_OUT    5
#define GPIO_SWITCH_IN  6

int     gpio_irq;

static irqreturn_t gpio_interrupt_handler(int irq, void *data)
{
        pr_info(KERN_INFO "interrupted!");

        if (gpio_get_value(GPIO_SWITCH_IN) == 1)
                gpio_set_value(GPIO_LED_OUT, 1);
        else
                gpio_set_value(GPIO_LED_OUT, 0);

        return (IRQ_HANDLED);
}

static int __init gpio_driver_init(void)
{
        int     ret;

        /* gpio initialize      */
        if (gpio_is_valid(GPIO_SWITCH_IN) == false)
                pr_err("GPIO %d is not valid\n", GPIO_SWITCH_IN);

        if (gpio_request(GPIO_SWITCH_IN,"GPIO_6") < 0)
                pr_err("Error: GPIO %d request\n", GPIO_SWITCH_IN);

        gpio_direction_input(GPIO_SWITCH_IN);

        if (gpio_is_valid(GPIO_LED_OUT) == false)
                pr_err("GPIO %d is not valid\n", GPIO_LED_OUT);

        if (gpio_request(GPIO_LED_OUT,"GPIO_LED_OUT") < 0)
                pr_err("Error: GPIO %d request\n", GPIO_LED_OUT);

        gpio_direction_output(GPIO_LED_OUT, 0);

        /* interrtup            */
        gpio_irq = gpio_to_irq(GPIO_SWITCH_IN);
        if (gpio_irq < 0)
                pr_info(KERN_ERR "Failed to set gpio IRQ(%d)\n", gpio_irq);

        ret = request_irq(gpio_irq, gpio_interrupt_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "led-gpio", NULL);
        pr_info(KERN_ERR "request IRQ(%d) (%d)\n", gpio_irq, ret);
        if (ret)
                pr_info(KERN_ERR "Failed to request IRQ(%d)\n", ret);

        pr_info("GPIO driver start\n");
        return 0;
}

static void __exit gpio_driver_exit(void)
{
        free_irq(gpio_irq, NULL);
        gpio_free(GPIO_SWITCH_IN);
        gpio_free(GPIO_LED_OUT);
        pr_info("GPIO driver done\n");
}

module_init(gpio_driver_init);
module_exit(gpio_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("me");
MODULE_DESCRIPTION("switch+led");
MODULE_VERSION("1.0");
