// cdh_main.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <cdh.h>

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 20, 4);

void cmd_processor_entry(void) {
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};

    while (1) {
        if (k_msgq_get(&rx_cmd_queue, &local_rx, K_FOREVER) == 0) {
            clear_tx_cmd_buff(&local_tx);

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

K_THREAD_DEFINE(cmd_processor_tid, 2048, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void) {
    init_leds();
    init_gpio();
    
    // 1. HARDWARE POWER SEQUENCING (Prevent USB Drops!)
    // Power on COM immediately. The inrush current will cause a brownout, 
    // but since USB isn't started yet, it won't crash the terminal!
    power_on_com();
    k_msleep(1500); // Wait for voltage rails to fully recover
    
    // 2. NOW it is safe to boot USB!
    if (init_usb_console() == 0) {
        init_usb_uart(); 
        printk("\n--- CDH BOOT SEQUENCE START ---\n");
        printk("Console & USB UART Live\n");
    }

    // 3. Boot Hardware UARTs
    init_hardware_uarts();
    printk("Hardware UARTs Live\n");

    // 4. Sync with COM
    printk("COM is powered. Starting Handshake...\n");
    check_com_online(); // TODO: overcome COM delay for alive ACK.
    printk("Handshake Complete! COM is online.\n");
    
    static tx_cmd_buff_t local_demo_tx = {.size=CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_demo_tx);
    
    // rx_cmd_buff_t dummy_rx = {.route_id = CDH, .bus_msg_id = 0}; 
    // com_start_demo(&dummy_rx, &local_demo_tx);
    route_tx_packet(&local_demo_tx);

    // 5. Blink forever
    while (1) {
        printk("CDH Heartbeat - System Nominal.\n");
        gpio_pin_toggle_dt(&led1);
        k_msleep(1000); 
    }
    
    return 0;
}