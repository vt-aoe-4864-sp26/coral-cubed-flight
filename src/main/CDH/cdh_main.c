// cdh_main.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <cdh.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>

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
    check_com_online();
    printk("Handshake Complete! COM is online.\n");
    
    static tx_cmd_buff_t local_demo_tx = {.size=CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_demo_tx);
    
    // rx_cmd_buff_t dummy_rx = {.route_id = CDH, .bus_msg_id = 0}; 
    // com_start_demo(&dummy_rx, &local_demo_tx);
    route_tx_packet(&local_demo_tx);

    
int cdh_init_nv_memory(void)
{
    static int initialized;
    const struct device *nvm_dev = DEVICE_DT_GET(DT_NODELABEL(is25lp128));
    uint64_t flash_size = 0;
    int rc;

    if (initialized) {
        return 0;
    }

    if (!device_is_ready(nvm_dev)) {
        printk("NVM init failed: is25lp128 device not ready.\n");
        return -1;
    }

    rc = flash_get_size(nvm_dev, &flash_size);
    if (rc != 0) {
        printk("NVM init failed: flash_get_size() error %d\n", rc);
        return -2;
    }

#if defined(CONFIG_FLASH_JESD216_API)
    {
        uint8_t jedec_id[3] = {0};

        rc = flash_read_jedec_id(nvm_dev, jedec_id);
        if (rc != 0) {
            printk("NVM init failed: flash_read_jedec_id() error %d\n", rc);
            return -3;
        }

        if ((jedec_id[0] != 0x9D) || (jedec_id[1] != 0x60) || (jedec_id[2] != 0x18)) {
            printk("NVM init failed: unexpected JEDEC ID %02X %02X %02X\n",
                   jedec_id[0], jedec_id[1], jedec_id[2]);
            return -4;
        }

        printk("NVM JEDEC ID OK: %02X %02X %02X\n",
               jedec_id[0], jedec_id[1], jedec_id[2]);
    }
#endif

    printk("NVM ready: %s, size=%llu bytes, write_block=%zu\n",
           nvm_dev->name,
           (unsigned long long)flash_size,
           flash_get_write_block_size(nvm_dev));

    initialized = 1;
    return 0;
}

    // 5. Blink forever
    while (1) {
        printk("CDH Heartbeat - System Nominal.\n");
        gpio_pin_toggle_dt(&led1);
        k_msleep(1000); 
    }
    
    return 0;
}