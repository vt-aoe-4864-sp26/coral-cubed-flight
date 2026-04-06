// com_main.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include "gnd.h"
#include "uart.h"
#include "radio.h"

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 20, 4);

/* Command Processing Thread */
void cmd_processor_entry(void) {
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};
    
    while (1) {
        if (k_msgq_get(&rx_cmd_queue, &local_rx, K_FOREVER) == 0) {
            clear_tx_cmd_buff(&local_tx); 
            process_rx_packet(&local_rx, &local_tx); 
            if (!local_tx.empty) route_tx_packet(&local_tx); 
        }
    }
}
K_THREAD_DEFINE(cmd_processor_tid, 2048, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void) {
    uint32_t dtr = 0;
    
    init_leds();
    init_radio(); 

    // Boot hardware connection to CDH
    init_hardware_uarts();

    // Only boot usb device/UART if something is connected within 'usb_enumeration_timeout'
    if (device_is_ready(uart_dev) && usb_enable(NULL) == 0) {
        int usb_enumeration_timeout  = 5;
        while (!dtr && usb_enumeration_timeout > 0) {
            uart_line_ctrl_get(uart_dev, UART_LINE_CTRL_DTR, &dtr);
            k_msleep(100);
            usb_enumeration_timeout--;
        }
        init_usb_uart(); // Enable USB interrupts safely!
        if (dtr) {
            gpio_pin_set_dt(&led1, 1); 
        }
    }

    while (1) {
        gpio_pin_toggle_dt(&led2);
        k_msleep(2000); 
    }
    
    return 0;
}