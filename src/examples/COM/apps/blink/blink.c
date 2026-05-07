// blink.c
// COM Example: Blink LED1 and LED2
//
// Simple demonstration that toggles both onboard LEDs in an alternating
// pattern. Radio and UART subsystems are not initialized.

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "com.h"

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

// Required by libs but unused in this example
K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 1, 4);

int main(void)
{
    init_leds();

    printk("\n--- COM Blink Example ---\n");
    printk("Toggling LED1 and LED2...\n");

    gpio_pin_set_dt(&led1, 1);
    gpio_pin_set_dt(&led2, 0);

    while (1)
    {
        gpio_pin_toggle_dt(&led1);
        gpio_pin_toggle_dt(&led2);
        k_msleep(500);
    }

    return 0;
}
