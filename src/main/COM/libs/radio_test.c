#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "tab.h"
#include "radio.h"
#include "com.h"

#define RF_TEST_PERIOD_MS 1000

static uint16_t rf_test_msg_id = 0;

static void build_rf_test_packet(tx_cmd_buff_t *tx)
{
    clear_tx_cmd_buff(tx);

    /*
     * Known byte pattern.
     * This is intentionally easy to recognize on the receiver.
     */
    uint8_t payload[] = {
        0x55, 0xAA, 0x00, 0xFF,
        0x12, 0x34, 0x56, 0x78,
        'R', 'F', '_', 'T', 'E', 'S', 'T'
    };

    size_t payload_len = sizeof(payload);

    tx->data[START_BYTE_0_INDEX] = START_BYTE_0;   // 0x22
    tx->data[START_BYTE_1_INDEX] = START_BYTE_1;   // 0x69

    /*
     * TAB length convention in your code:
     * MSG_LEN = 6 + payload length
     */
    tx->data[MSG_LEN_INDEX] = 6 + payload_len;

    tx->data[HWID_LSB_INDEX] = SAT_HWID_LSB;
    tx->data[HWID_MSB_INDEX] = SAT_HWID_MSB;

    tx->data[MSG_ID_LSB_INDEX] = rf_test_msg_id & 0xFF;
    tx->data[MSG_ID_MSB_INDEX] = (rf_test_msg_id >> 8) & 0xFF;

    /*
     * Route byte:
     * upper nibble = destination
     * lower nibble = source
     *
     * Destination: COM on the satellite side
     * Source: GND
     */
    tx->data[ROUTE_INDEX] = (COM << 4) | GND;

    /*
     * Use existing debug opcode.
     * No new opcode needed for this first RF test.
     */
    tx->data[OPCODE_INDEX] = COMMON_DEBUG_OPCODE;

    for (size_t i = 0; i < payload_len; i++) {
        tx->data[PLD_START_INDEX + i] = payload[i];
    }

    tx->end_index = tx->data[MSG_LEN_INDEX] + 3;
    tx->empty = 0;

    rf_test_msg_id++;
}

void rf_test_thread_entry(void *p1, void *p2, void *p3)
{
#if CURRENT_BOARD_ROLE == ROLE_GROUND_STATION

    tx_cmd_buff_t test_tx = {.size = CMD_MAX_LEN};

    k_msleep(3000);

    printk("\n[RF TEST] Starting known-pattern RF transmit test\n");

    while (1) {
        build_rf_test_packet(&test_tx);

        printk("[RF TEST] Sending test packet ID %u, %u bytes\n",
               rf_test_msg_id - 1,
               test_tx.end_index);

        int ret = radio_send_packet(&test_tx);

        if (ret != 0) {
            printk("[RF TEST] radio_send_packet failed: %d\n", ret);
        }

        k_msleep(RF_TEST_PERIOD_MS);
    }

#else

    while (1) {
        k_msleep(1000);
    }

#endif
}

K_THREAD_DEFINE(rf_test_tid, 2048, rf_test_thread_entry,
                NULL, NULL, NULL, 8, 0, 0);