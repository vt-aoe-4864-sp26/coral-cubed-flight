// uart.c
// CDH Example: USB-C UART Console
//
// Boots the USB CDC console and enables interrupt-driven UART RX.
// Incoming TAB-protocol packets from the USB-C console are processed
// by a command processor thread, which dispatches them through the
// standard CDH command handler and prints responses.

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <cdh.h>

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 20, 4);

void cmd_processor_entry(void)
{
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};

    while (1)
    {
        if (k_msgq_get(&rx_cmd_queue, &local_rx, K_FOREVER) == 0)
        {
            clear_tx_cmd_buff(&local_tx);
            printk("[UART] Packet received — OPCODE: 0x%02x\n",
                   local_rx.data[OPCODE_INDEX]);

            process_rx_packet(&local_rx, &local_tx);

            if (!local_tx.empty)
            {
                printk("[UART] Sending response (size: %d)\n",
                       (int)local_tx.end_index);
                route_tx_packet(&local_tx);
            }
        }
    }
}

K_THREAD_DEFINE(cmd_processor_tid, 2048, cmd_processor_entry,
                NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
    init_leds();
    init_gpio();

    // Boot USB console first
    if (init_usb_console() == 0)
    {
        init_usb_uart();
        printk("\n--- CDH UART Example ---\n");
        printk("USB Console & UART Live. Waiting for commands...\n");
    }

    // Boot hardware UARTs
    init_hardware_uarts();
    printk("Hardware UARTs Live\n");
    printk("Send TAB-protocol packets via USB-C to interact.\n");

    // Heartbeat
    while (1)
    {
        gpio_pin_toggle_dt(&led1);
        k_msleep(1000);
    }

    return 0;
}
