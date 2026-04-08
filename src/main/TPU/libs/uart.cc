// uart_task.cc
#include "uart_task.h"
#include "libs/base/tasks.h"
#include "libs/base/led.h"
#include "libs/base/console_m7.h" // Replace with actual Coral LPUART driver if not using USB CDC
#include <cstdio>

// Static buffers for the TAB state machine
static rx_cmd_buff_t rx_buff;
static tx_cmd_buff_t tx_buff;

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

        // Example: First byte of payload is a command ID
        uint8_t cmd_id = common_data_buff_i.data[0];

        if (cmd_id == 0x01)
        { // Let's say 0x01 means "Run Inference"
            printf("Received Command: Run Inference!\r\n");
            coralmicro::LedSet(coralmicro::Led::kUser, true);

            // TODO: Signal your TPU inference task here using a FreeRTOS Queue or Semaphore

            return 1; // Success, tab.c will automatically ACK this
        }

        return 0; // Unknown command, tab.c will NACK
    }

} // extern "C"

// --- UART Task Logic ---

static void FlushTxBuffer()
{
    // While there are bytes in the TX buffer, pop and write them to UART
    while (!tx_buff.empty)
    {
        uint8_t b = pop_tx_cmd_buff(&tx_buff);
        // TODO: Replace ConsoleM7 with your specific LPUART hardware write
        coralmicro::ConsoleM7::GetSingleton()->Write((char *)&b, 1);
    }
}

// Exposed function for your TPU task to call when inference is complete
void SendInferenceResult(uint8_t *result_data, size_t len)
{
    // Package it up and route it to CDH (which will route to COM/GND)
    // Using a custom opcode, e.g., 0x20 for "Inference Data"
    msg_to_cdh(&rx_buff, &tx_buff, 0x20, result_data, len);
    FlushTxBuffer();
}

[[noreturn]] static void UartTaskMain(void *param)
{
    (void)param;

    // Initialize TAB buffers
    clear_rx_cmd_buff(&rx_buff);
    clear_tx_cmd_buff(&tx_buff);

    rx_buff.size = CMD_MAX_LEN;
    tx_buff.size = CMD_MAX_LEN;
    rx_buff.route_id = PLD; // We are the payload

    uint8_t ch;
    while (true)
    {
        // Read 1 byte from UART (blocking or polling depending on your driver setup)
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