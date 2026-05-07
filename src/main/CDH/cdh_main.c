// cdh_main.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <cdh.h>

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 20, 4);

void tab_nvs_init(void);
void tab_reset_nvs_msg_id(void);
int nvs_queue_push(rx_cmd_buff_t* cmd);
int nvs_queue_pop(rx_cmd_buff_t* cmd);
int nvs_queue_peek(rx_cmd_buff_t* cmd);
void nvs_queue_clear(void);
int nvs_queue_get_count(void);

void cmd_processor_entry(void)
{
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};
    
    bool waiting_for_ack = false;
    uint16_t expected_ack_id = 0;

    while (1)
    {
        // Execute stored commands first
        if (nvs_queue_get_count() > 0 && !waiting_for_ack)
        {
            if (nvs_queue_peek(&local_rx) == 0)
            {
                clear_tx_cmd_buff(&local_tx);
                process_rx_packet(&local_rx, &local_tx);
                if (!local_tx.empty)
                {
                    route_tx_packet(&local_tx);
                }
                
                uint8_t dest_id = (local_rx.data[ROUTE_INDEX] & 0xF0) >> 4;
                if (dest_id == CDH) {
                    // CDH handled it locally. We can pop it.
                    rx_cmd_buff_t dummy;
                    nvs_queue_pop(&dummy);
                } else if (dest_id == PLD) {
                    // We routed it to PLD. Wait for its ACK.
                    waiting_for_ack = true;
                    expected_ack_id = local_rx.bus_msg_id;
                }
            }
        }

        // Check for new incoming commands (non-blocking if queue has items and we are not waiting, otherwise wait)
        k_timeout_t wait_time = (nvs_queue_get_count() > 0 && !waiting_for_ack) ? K_NO_WAIT : K_FOREVER;
        
        if (k_msgq_get(&rx_cmd_queue, &local_rx, wait_time) == 0)
        {
            if (local_rx.data[OPCODE_INDEX] == COMMON_CLEAR_QUEUE_OPCODE)
            {
                nvs_queue_clear();
                waiting_for_ack = false;
                continue;
            }
            if (local_rx.data[OPCODE_INDEX] == COMMON_RESET_MSG_ID_OPCODE)
            {
                tab_reset_nvs_msg_id();
                continue;
            }

            if (local_rx.data[OPCODE_INDEX] == COMMON_ACK_OPCODE || 
                local_rx.data[OPCODE_INDEX] == COMMON_NACK_OPCODE)
            {
                // Is this the ACK we are waiting for?
                if (waiting_for_ack && local_rx.bus_msg_id == expected_ack_id) {
                    rx_cmd_buff_t dummy;
                    nvs_queue_pop(&dummy);
                    waiting_for_ack = false;
                }
                // Also give the semaphore if it's an ACK
                if (local_rx.data[OPCODE_INDEX] == COMMON_ACK_OPCODE) {
                    k_sem_give(&com_awake_semaphore);
                }
                
                // Route this ACK back to whoever sent the command
                clear_tx_cmd_buff(&local_tx);
                process_rx_packet(&local_rx, &local_tx);
                if (!local_tx.empty)
                {
                    route_tx_packet(&local_tx);
                }
                continue;
            }

            // Determine if command is meant for CDH or PLD to enqueue
            uint8_t dest_id = (local_rx.data[ROUTE_INDEX] & 0xF0) >> 4;
            if (dest_id == CDH || dest_id == PLD)
            {
                nvs_queue_push(&local_rx);
            }
            else
            {
                // Unhandled route, just process immediately (or it gets dropped)
                clear_tx_cmd_buff(&local_tx);
                process_rx_packet(&local_rx, &local_tx);
                if (!local_tx.empty)
                {
                    route_tx_packet(&local_tx);
                }
            }
        }
    }
}

K_THREAD_DEFINE(cmd_processor_tid, 2048, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
    init_leds();
    init_gpio();
    tab_nvs_init();

    // Power on COM immediately. The inrush current will may cause a brownout,
    // but since USB isn't started yet, it won't crash the terminal
    power_on_com();
    power_on_pay();
    k_msleep(1500); // Wait for voltage rails to fully recover

    // NOW boot usb
    if (init_usb_console() == 0)
    {
        init_usb_uart();
        printk("\n--- CDH BOOT SEQUENCE START ---\n");
        printk("Console & USB UART Live\n");
    }

    // Boot Hardware UART
    init_hardware_uarts();
    printk("Hardware UARTs Live\n");

    // Sync with COM
    printk("COM is powered. Starting Handshake...\n");
    k_msleep(50000); // yikes
    gpio_pin_toggle_dt(&led1);
    check_com_online(); // TODO: overcome COM delay for alive ACK.
    printk("Handshake Complete! COM is online.\n");

    static tx_cmd_buff_t local_demo_tx = {.size = CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_demo_tx);

    // rx_cmd_buff_t dummy_rx = {.route_id = CDH, .bus_msg_id = 0};
    // com_start_demo(&dummy_rx, &local_demo_tx);
    route_tx_packet(&local_demo_tx);

    // Blink forever
    while (1)
    {
        // printk("CDH Heartbeat - System Nominal.\n");
        gpio_pin_toggle_dt(&led1);
        k_msleep(1000);
    }

    return 0;
}