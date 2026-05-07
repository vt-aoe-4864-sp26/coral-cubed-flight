// uart_task.cc
#include "uart.h"
#include "libs/base/tasks.h"
#include "libs/base/led.h"
#include "libs/base/console_m7.h" // Replace with actual Coral LPUART driver if not using USB CDC
#include <cstdio>
// com.h removed
#include "../configs.h"

// Static buffers for the TAB state machine
static rx_cmd_buff_t rx_buff = {.state = RX_CMD_BUFF_STATE_START_BYTE_0, .start_index = 0, .end_index = 0, .size = CMD_MAX_LEN, .route_id = 0, .bus_msg_id = 0, .data = {0}};
static tx_cmd_buff_t tx_buff = {.empty = 1, .start_index = 0, .end_index = 0, .size = CMD_MAX_LEN, .data = {0}};
volatile uint8_t g_run_inference = 0;

// --- Required TAB Protocol Implementations ---

extern "C"
{

    // Handles application-specific payloads (e.g., trigger an inference)
    int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t *rx_cmd_buff, tx_cmd_buff_t *tx_cmd_buff)
    {
        // Check if this message was routed to the Payload
        uint8_t target_route = (rx_cmd_buff->data[ROUTE_INDEX] >> 4) & 0x0F;
        if (target_route != PLD)
        {
            return 0; // Not for us
        }

        if (common_data_buff_i.end_index < 2)
            return 0;
        uint8_t var_code = common_data_buff_i.data[0];
        uint8_t var_len = common_data_buff_i.data[1];
        if (common_data_buff_i.end_index < (size_t)(2 + var_len))
            return 0;

        uint8_t *val_ptr = &common_data_buff_i.data[2];
        (void)val_ptr;

        switch (var_code)
        {
        case VAR_CODE_CORAL_WAKE:
            // printf("TAB Command: Coral Wake (%d)\r\n", *val_ptr);
            //  TODO: implement wake
            return 1;
        case VAR_CODE_CORAL_CAM_ON:
            // printf("TAB Command: Coral Cam On (%d)\r\n", *val_ptr);
            //  TODO: implement cam on
            return 1;
        case VAR_CODE_CORAL_INFER_DENBY:
        case VAR_CODE_RUN_DEMO:
            // printf("TAB Command Received: Triggering Demo Inference...\r\n");
            g_run_inference = 1;
            return 1;
        case VAR_CODE_CORAL_INFER_BLK:
            g_run_inference = 2;
            return 1;
        default:
            return 0; // Unknown command, tab.c will NACK
        }
    }

    // Empty bootloader stubs for generic tab.c library
    int handle_bootloader_erase(void) { return 0; }
    int handle_bootloader_write_page(rx_cmd_buff_t *rx_cmd_buff) { return 0; }
    int handle_bootloader_write_page_addr32(rx_cmd_buff_t *rx_cmd_buff) { return 0; }
    int handle_bootloader_jump(void) { return 0; }
    int bootloader_active(void) { return 0; }

} // extern "C"

// --- UART Task Logic ---

static void FlushTxBuffer()
{
    if (!tx_buff.empty)
    {
        int len = tx_buff.end_index - tx_buff.start_index;
        if (len > 0)
        {
            // TODO: Replace ConsoleM7 with LPUART hardware write
            coralmicro::ConsoleM7::GetSingleton()->Write((char *)&tx_buff.data[tx_buff.start_index], len);
        }
        clear_tx_cmd_buff(&tx_buff);
    }
}

// Exposed function for TPU task to call when inference is complete
void SendInferenceResult(uint8_t *result_data, size_t len)
{
    // Use the common data opcode (0x16) array payload
    uint8_t payload[256];
    payload[0] = VAR_CODE_INFERENCE_RESULT;
    payload[1] = len;

    // Copy result_data
    for (size_t i = 0; i < len; ++i)
    {
        payload[2 + i] = result_data[i];
    }

    // Package it up and route it to COM (which will route to GND)
    // 0x16 is COMMON_DATA_OPCODE
    msg_to_gnd(&rx_buff, &tx_buff, 0x16, payload, len + 2);
    FlushTxBuffer();
}

[[noreturn]] static void UartTaskMain(void *param)
{
    (void)param;

    // Initialize TAB buffers
    clear_rx_cmd_buff(&rx_buff);
    clear_tx_cmd_buff(&tx_buff);

    rx_buff.route_id = PLD; // We are the payload

    uint8_t ch;
    while (true)
    {
        // Read 1 byte from UART (blocking or polling depending driver setup)
        int bytes = coralmicro::ConsoleM7::GetSingleton()->Read((char *)&ch, 1);

        if (bytes == 1)
        {
            // Push byte into the TAB state machine
            push_rx_cmd_buff(&rx_buff, ch);

            // Check if a full packet was assembled
            if (rx_buff.state == RX_CMD_BUFF_STATE_COMPLETE)
            {
                // Process the packet. This calls handle_common_data under the hood
                // and populates tx_buff if an ACK/NACK or reply is needed.
                write_reply(&rx_buff, &tx_buff);

                // Blast the reply out over UART
                FlushTxBuffer();
            }
        }
        taskYIELD();
    }
}

void StartUartTask()
{
    xTaskCreate(UartTaskMain, "UartTask", configMINIMAL_STACK_SIZE * 4, nullptr, 3, nullptr);
}