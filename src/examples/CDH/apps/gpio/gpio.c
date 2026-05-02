// gpio.c
// CDH Example: GPIO Power Enables
//
// Demonstrates CDH's role as power controller. Initializes GPIO and
// enables the COM and PAY power rails, confirming over printk.
// Blinks LEDs as a heartbeat to show the board is alive.

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <cdh.h>

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

// Required by libs but unused in this example
K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 1, 4);

int main(void)
{
    init_leds();
    init_gpio();

    // Boot USB console for printk output
    if (init_usb_console() == 0)
    {
        printk("\n--- CDH GPIO Example ---\n");
    }

    // Enable COM power rail
    printk("Enabling COM power rail...\n");
    power_on_com();
    gpio_pin_set_dt(&led1, 1);
    printk("COM power rail: ON\n");
    k_msleep(500);

    // Enable PAY power rail
    printk("Enabling PAY power rail...\n");
    power_on_pay();
    gpio_pin_set_dt(&led2, 1);
    printk("PAY power rail: ON\n");
    k_msleep(500);

    printk("All power rails enabled. Heartbeat running.\n");

    // Heartbeat blink
    while (1)
    {
        gpio_pin_toggle_dt(&led1);
        gpio_pin_toggle_dt(&led2);
        k_msleep(1000);
    }

    return 0;
}
