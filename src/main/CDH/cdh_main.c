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

bool cdh_system_ready = false;

void cmd_processor_entry(void)
{
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};
    
    bool waiting_for_ack = false;
    uint16_t expected_ack_id = 0;

    while (1)
    {
        bool has_pending_nvs = (cdh_system_ready && nvs_queue_get_count() > 0 && !waiting_for_ack);

        // Execute stored commands first
        if (has_pending_nvs)
        {
            if (nvs_queue_peek(&local_rx) == 0)
            {
                uint8_t dest_id = (local_rx.data[ROUTE_INDEX] & 0xF0) >> 4;
                uint16_t msg_id = local_rx.bus_msg_id;
                
                clear_tx_cmd_buff(&local_tx);
                process_rx_packet(&local_rx, &local_tx);
                if (!local_tx.empty)
                {
                    route_tx_packet(&local_tx);
                }
                
                if (dest_id == CDH) {
                    // CDH handled it locally. We can pop it.
                    rx_cmd_buff_t dummy;
                    nvs_queue_pop(&dummy);
                } else if (dest_id == PLD) {
                    // We routed it to PLD. Wait for its ACK.
                    waiting_for_ack = true;
                    expected_ack_id = msg_id;
                }
            }
        }

        // Check for new incoming commands (non-blocking if queue has items and we are not waiting, otherwise wait)
        k_timeout_t wait_time = has_pending_nvs ? K_NO_WAIT : (cdh_system_ready ? K_FOREVER : K_MSEC(100));
        
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
            bool enqueued = false;
            
            if (dest_id == CDH || dest_id == PLD)
            {
                if (nvs_queue_push(&local_rx) == 0) {
                    enqueued = true;
                }
            }
            
            if (!enqueued)
            {
                // Unhandled route or NVS queue failure, just process immediately
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

static void load_demo_commands(void) {
    if (nvs_queue_get_count() > 0) return;

    printk("[DEMO] NVS Queue Empty. Pre-loading Lab Demo Commands...\n");

    rx_cmd_buff_t infer_cmd = {.size = CMD_MAX_LEN};
    clear_rx_cmd_buff(&infer_cmd);
    infer_cmd.state = RX_CMD_BUFF_STATE_COMPLETE;
    infer_cmd.data[START_BYTE_0_INDEX] = START_BYTE_0;
    infer_cmd.data[START_BYTE_1_INDEX] = START_BYTE_1;
    infer_cmd.data[MSG_LEN_INDEX] = 9;
    infer_cmd.data[HWID_LSB_INDEX] = SAT_HWID_LSB;
    infer_cmd.data[HWID_MSB_INDEX] = SAT_HWID_MSB;
    infer_cmd.data[MSG_ID_LSB_INDEX] = 0x01;
    infer_cmd.data[MSG_ID_MSB_INDEX] = 0x00;
    infer_cmd.bus_msg_id = 1;
    infer_cmd.data[ROUTE_INDEX] = (PLD << 4) | GND; 
    infer_cmd.data[OPCODE_INDEX] = COMMON_DATA_OPCODE;
    infer_cmd.data[PLD_START_INDEX] = VAR_CODE_CORAL_INFER_DENBY;
    infer_cmd.data[PLD_START_INDEX+1] = 1;
    infer_cmd.data[PLD_START_INDEX+2] = VAR_ENABLE;
    infer_cmd.end_index = 12;
    nvs_queue_push(&infer_cmd);

    rx_cmd_buff_t downlink_cmd = {.size = CMD_MAX_LEN};
    clear_rx_cmd_buff(&downlink_cmd);
    downlink_cmd.state = RX_CMD_BUFF_STATE_COMPLETE;
    downlink_cmd.data[START_BYTE_0_INDEX] = START_BYTE_0;
    downlink_cmd.data[START_BYTE_1_INDEX] = START_BYTE_1;
    downlink_cmd.data[MSG_LEN_INDEX] = 9;
    downlink_cmd.data[HWID_LSB_INDEX] = SAT_HWID_LSB;
    downlink_cmd.data[HWID_MSB_INDEX] = SAT_HWID_MSB;
    downlink_cmd.data[MSG_ID_LSB_INDEX] = 0x02;
    downlink_cmd.data[MSG_ID_MSB_INDEX] = 0x00;
    downlink_cmd.bus_msg_id = 2;
    downlink_cmd.data[ROUTE_INDEX] = (GND << 4) | CDH; // From CDH to GND
    downlink_cmd.data[OPCODE_INDEX] = COMMON_DATA_OPCODE;
    downlink_cmd.data[PLD_START_INDEX] = VAR_CODE_INFERENCE_RESULT;
    downlink_cmd.data[PLD_START_INDEX+1] = 1;
    downlink_cmd.data[PLD_START_INDEX+2] = 0x42; // Example result: 0x42
    downlink_cmd.end_index = 12;
    nvs_queue_push(&downlink_cmd);

    printk("[DEMO] Lab Demo Commands Loaded: %d items in queue.\n", nvs_queue_get_count());
}

K_THREAD_DEFINE(cmd_processor_tid, 2048, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{

    // NOW boot usb
    if (init_usb_console() == 0)
    {
        init_usb_uart();
        printk("\n--- CDH BOOT SEQUENCE START ---\n");
        printk("Console & USB UART Live\n");
    }

    printk("Initializing Hardware Systems\n");
    init_leds();
    init_gpio();
    tab_nvs_init();

    switch(BOARD_VERSION) {
        case 1:
            nvs_queue_clear();
            printk("Board Version 1: NVS Queue Cleared.\n");
            break;
        case 2:
            printk("Board Version 2: NVS Queue Retained. Pending commands: %d\n", nvs_queue_get_count());
            break;
        default:
            nvs_queue_clear();
            printk("Unknown Board Version: NVS Queue Cleared.\n");
            break;
    }

    printk("Hardware Systems Online\n");
    

    // Power on COM immediately. The inrush current will may cause a brownout,
    // but since USB isn't started yet, it won't crash the terminal
    power_on_com();
    power_on_pay();
    k_msleep(1500); // Wait for voltage rails to fully recover

    printk("Com and Payload Online\n");

    // Boot Hardware UART
    init_hardware_uarts();
    printk("Hardware UARTs Live\n");

    // Sync with COM
    printk("COM is powered. Starting Handshake...\n");
    k_msleep(50000); // yikes
    gpio_pin_toggle_dt(&led1);
    check_com_online(); // TODO: overcome COM delay for alive ACK.
    printk("Handshake Complete! COM is online.\n");
    cdh_system_ready = true;

    printk("Loading Demo Commands\n");
    load_demo_commands();

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