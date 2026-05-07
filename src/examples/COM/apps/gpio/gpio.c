// gpio.c
// COM Example: RF Frontend GPIO Control
//
// Demonstrates control of the COM board's RF Front End Module (FEM).
// Powers on the RF frontend, enables FEM mode, then cycles between
// TX and RX modes with status output.

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "com.h"
#include "radio.h"

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

// Required by libs but unused in this example
K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 1, 4);

int main(void)
{
    init_leds();
    init_radio();

    printk("\n--- COM GPIO Example ---\n");

    // Enable the RF frontend (FEM power-on)
    printk("Enabling RF frontend (FEM PDN)...\n");
    enable_rf();
    gpio_pin_set_dt(&led1, 1);
    printk("RF frontend: ON\n");
    k_msleep(1000);

    // Cycle between TX and RX modes
    printk("Starting TX/RX cycle...\n");
    while (1)
    {
        // TX mode
        printk("[FEM] Switching to TX mode\n");
        enable_tx();
        gpio_pin_set_dt(&led1, 1);
        gpio_pin_set_dt(&led2, 0);
        k_msleep(2000);

        // Disable TX before switching
        disable_tx();
        k_msleep(100);

        // RX mode
        printk("[FEM] Switching to RX mode\n");
        enable_rx();
        gpio_pin_set_dt(&led1, 0);
        gpio_pin_set_dt(&led2, 1);
        k_msleep(2000);

        // Disable RX before switching
        disable_rx();
        k_msleep(100);
    }

    return 0;
}
