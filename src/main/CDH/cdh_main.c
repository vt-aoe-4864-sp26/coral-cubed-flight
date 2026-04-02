// cdh_main.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <cdh.h>

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

// Define Message Queue for ALL Completed RX Commands
K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 20, 4);

void cmd_processor_entry(void) {
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_tx);

    printk("Command Processor Thread Started.\n");

    while (1) {
        if (k_msgq_get(&rx_cmd_queue, &local_rx, K_FOREVER) == 0) {
            
            // Intercept the ACK from COM to wake up the boot sequence
            if (local_rx.data[OPCODE_INDEX] == COMMON_ACK_OPCODE) {
                k_sem_give(&com_awake_semaphore);
            }

            process_rx_packet(&local_rx, &local_tx); 
            
            if (!local_tx.empty) {
                route_tx_packet(&local_tx); 
            }
        }
    }
}

K_THREAD_DEFINE(cmd_processor_tid, 1024, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void) {
    // 1. Hardware Init
    init_leds();
    init_gpio();
    
    // 2. Boot Hardware UARTs immediately
    init_hardware_uarts();
    printk("Hardware UARTs Live\n");
    gpio_pin_toggle_dt(&led2);
    k_msleep(1000);

    // 3. Boots USB-C Console/UART IFF plugged in
    if (init_usb_console() == 0) {
        init_usb_uart(); 
        printk("Console & USB UART Live\n");
    }
    k_msleep(1000);

    // 4. Sync with COM
    gpio_pin_toggle_dt(&led2);
    power_on_com();
    printk("COMEN High\n");

    k_msleep(1000);
    gpio_pin_toggle_dt(&led1);

    check_com_online();
    printk("COM is online! Triggering Master Demo...\n");
    k_msleep(1000);
    gpio_pin_toggle_dt(&led2);
    k_msleep(200);
    gpio_pin_toggle_dt(&led2);
    gpio_pin_toggle_dt(&led2);
    k_msleep(200);
    gpio_pin_toggle_dt(&led2);
    
    tx_cmd_buff_t local_demo_tx = {.size=CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_demo_tx);
    rx_cmd_buff_t dummy_rx = {.route_id = CDH, .bus_msg_id = 0};

    
    gpio_pin_toggle_dt(&led1);
    k_msleep(200);
    gpio_pin_toggle_dt(&led1);
    k_msleep(200);
    gpio_pin_toggle_dt(&led1);
    k_msleep(200);
    //com_start_demo(&dummy_rx, &local_demo_tx);
    //route_tx_packet(&local_demo_tx);

    // 5. Blink forever
    while (1) {
        printk("CDH Heartbeat - System Nominal.\n");
        gpio_pin_toggle_dt(&led1);
        k_msleep(1000); 
    }
    
    return 0;
}