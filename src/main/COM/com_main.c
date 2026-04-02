// com_main.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include <com.h>

extern const struct device *uart_gnd_dev;
extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

// 1. Comm Router Queue
K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 20, 4);

// ========== Thread 1: Comm Router (Priority 5) ========== //
void cmd_processor_entry(void) {
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};
    
    while (1) {
        if (k_msgq_get(&rx_cmd_queue, &local_rx, K_FOREVER) == 0) {
            clear_tx_cmd_buff(&local_tx); 

            route_rx_packet(&local_rx, &local_tx); 
            
            if (!local_tx.empty) {
                route_tx_packet(&local_tx); 
            }
        }
    }
}
K_THREAD_DEFINE(cmd_processor_tid, 1024, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void) {
    uint32_t dtr = 0;
    
    init_leds();
    init_gpio();

    // Boot hardware connection to CDH immediately
    init_hardware_uarts();
    printk("Hardware UARTs Live\n");

    // Boot USB connection only if active
    if (device_is_ready(uart_gnd_dev) && usb_enable(NULL) == 0) {
        int timeout = 100; // 10 second wait for Python to connect
        while (!dtr && timeout > 0) {
            uart_line_ctrl_get(uart_gnd_dev, UART_LINE_CTRL_DTR, &dtr);
            k_msleep(100);
            timeout--;
            gpio_pin_toggle_dt(&led1); 
        }
        
        if (dtr) {
            gpio_pin_set_dt(&led1, 1); 
            init_usb_uart(); 
            printk("USB UART Live\n");
        }
    }

    while (1) {
        gpio_pin_toggle_dt(&led2);
        k_msleep(2000); 
    }
    
    return 0;
}