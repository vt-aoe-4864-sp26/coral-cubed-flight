// gnd.c
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include "gnd.h"
#include "tab.h"
#include "radio.h"
#include "uart.h"

// ========== Macros ========== //
#define NVMC_CONFIG MMIO32(NVMC_BASE + 0x504)
#define NVMC_CONFIG_REN (0)
#define NVMC_CONFIG_WEN (1 << 1)
#define NVMC_CONFIG_EEN (1 << 2)
#define NVMC_ERASEPAGE MMIO32(NVMC_BASE + 0x508)
#define NVMC_READY MMIO32(NVMC_BASE + 0x400)
#define NVMC_READY_BUSY (0)

// ========== Hardware Aliases ========== //
const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// ========== Application Events ========== //
#define EVENT_BLINK_DEMO BIT(0)
#define EVENT_RUN_DEMO BIT(1)
K_EVENT_DEFINE(app_events);

void init_leds(void)
{
    if (device_is_ready(led1.port))
    {
        gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    }
}

// ========== Thread 2: Application Tasks (Priority 7) ========== //
// ========== Thread 2: Application Tasks (Priority 7) ========== //
void app_thread_entry(void *p1, void *p2, void *p3)
{
    while (1)
    {
        // Build a manual payload for COM blink
        k_msleep(2000);
        tx_cmd_buff_t ping_tx = {.size = CMD_MAX_LEN};
        clear_tx_cmd_buff(&ping_tx);
        rx_cmd_buff_t dummy_rx = {.route_id = COMG, .bus_msg_id = 0};

        uint8_t my_payload[] = {VAR_CODE_BLINK_COM, 0x01, VAR_ENABLE};

        msg_to_com(&dummy_rx, &ping_tx, COMMON_DATA_OPCODE, my_payload, 3);
        ping_tx.data[ROUTE_INDEX] = (COM << 4) | COMG;

        // Blast the ping
        route_tx_packet(&ping_tx);

        // Act as our 2-second delay timer
        uint32_t events = k_event_wait(&app_events, (EVENT_BLINK_DEMO | EVENT_RUN_DEMO), false, K_MSEC(2000));

        if (events & EVENT_BLINK_DEMO)
        {
            k_event_clear(&app_events, EVENT_BLINK_DEMO);
            for (int k = 0; k < 20; k++)
            {
                k_msleep(250);
                gpio_pin_toggle_dt(&led1);
                gpio_pin_toggle_dt(&led2);
            }
            gpio_pin_set_dt(&led1, 0);
            gpio_pin_set_dt(&led2, 0);
        }

        if (events & EVENT_RUN_DEMO)
        {
            k_event_clear(&app_events, EVENT_RUN_DEMO);

            tx_cmd_buff_t demo_tx = {.size = CMD_MAX_LEN};
            clear_tx_cmd_buff(&demo_tx);

            k_event_post(&app_events, EVENT_BLINK_DEMO);
            k_msleep(1000);

            cdh_blink_demo(&dummy_rx, &demo_tx);
            route_tx_packet(&demo_tx);
            k_msleep(1000);

            enable_rf();
            k_msleep(1000);
            enable_rx();
            k_msleep(1000);
            enable_tx();
            k_msleep(1000);
            disable_tx();
            disable_rf();

            clear_tx_cmd_buff(&demo_tx);
            cdh_enable_pay(&dummy_rx, &demo_tx);
            route_tx_packet(&demo_tx);
        }
    }
}
K_THREAD_DEFINE(app_tid, 2048, app_thread_entry, NULL, NULL, NULL, 7, 0, 0);

// ========== Routing Logic ========== //
void process_rx_packet(rx_cmd_buff_t *rx_cmd_buff_o, tx_cmd_buff_t *tx_cmd_buff_o)
{
    if (rx_cmd_buff_o->state == RX_CMD_BUFF_STATE_COMPLETE && tx_cmd_buff_o->empty)
    {
        uint8_t dest_id = (rx_cmd_buff_o->data[ROUTE_INDEX] & 0xF0) >> 4;

        if (dest_id == COMG)
        {
            write_reply(rx_cmd_buff_o, tx_cmd_buff_o);
        }
        else
        {
            for (size_t i = 0; i < rx_cmd_buff_o->end_index; i++)
            {
                tx_cmd_buff_o->data[i] = rx_cmd_buff_o->data[i];
            }
            tx_cmd_buff_o->start_index = 0;
            tx_cmd_buff_o->end_index = rx_cmd_buff_o->end_index;
            tx_cmd_buff_o->empty = 0;
            clear_rx_cmd_buff(rx_cmd_buff_o);
        }
    }
}

void route_tx_packet(tx_cmd_buff_t *tx_cmd_buff_o)
{
    if (tx_cmd_buff_o->empty)
        return;

    uint8_t dest_id = (tx_cmd_buff_o->data[ROUTE_INDEX] & 0xF0) >> 4;
    const struct device *target_uart = NULL;
    int send_to_radio = 0;

#if CURRENT_BOARD_ROLE == ROLE_GROUND_STATION
    // Ground Station Topology
    if (dest_id == GND)
    {
        target_uart = uart_gnd_dev; // Local USB
    }
    else if (dest_id == CDH || dest_id == PLD || dest_id == COM)
    {                      // CHANGED: Send COM to Radio
        send_to_radio = 1; // Over the air to Satellite
    }
#else
    // Satellite Topology
    if (dest_id == GND)
    {
        send_to_radio = 1; // Over the air to Ground Station
    }
    else if (dest_id == CDH || dest_id == PLD)
    {
        target_uart = uart_cdh_dev; // Local physical UART
    }
#endif

    // Execute the routing
    if (send_to_radio)
    {
        radio_send_packet(tx_cmd_buff_o);
        clear_tx_cmd_buff(tx_cmd_buff_o);
    
    }
    else if (target_uart != NULL && device_is_ready(target_uart))
    {
        while (!(tx_cmd_buff_o->empty))
        {
            uint8_t b = pop_tx_cmd_buff(tx_cmd_buff_o);
            uart_poll_out(target_uart, b);
        }
        gpio_pin_toggle_dt(&led2);
    }
    else
    {
        // Nowhere valid to route, drop packet
        clear_tx_cmd_buff(tx_cmd_buff_o);
    }
}

// ========== Command Handlers ========== //
int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t *rx_cmd_buff, tx_cmd_buff_t *tx_cmd_buff)
{
    if (common_data_buff_i.end_index < 2)
        return 0;
    uint8_t var_code = common_data_buff_i.data[0];
    uint8_t var_len = common_data_buff_i.data[1];
    if (common_data_buff_i.end_index < (size_t)(2 + var_len))
        return 0;

    uint8_t *val_ptr = &common_data_buff_i.data[2];

    switch (var_code)
    {
    case VAR_CODE_ALIVE:
        if (*val_ptr == 0x01)
        {
            gpio_pin_set_dt(&led2, 1);
            return 1;
        }
        return 0;
    case VAR_CODE_PAY_EN:
        if (*val_ptr == VAR_ENABLE)
            cdh_enable_pay(rx_cmd_buff, tx_cmd_buff);
        else if (*val_ptr == VAR_DISABLE)
            cdh_disable_pay(rx_cmd_buff, tx_cmd_buff);
        return 1;
    case VAR_CODE_RUN_DEMO:
        k_event_post(&app_events, EVENT_RUN_DEMO);
        return 1;
    case VAR_CODE_BLINK_COM:
        k_event_post(&app_events, EVENT_BLINK_DEMO);
        return 1;
    case VAR_CODE_RF_EN:
        if (*val_ptr == VAR_ENABLE)
            enable_rf();
        else if (*val_ptr == VAR_DISABLE)
            disable_rf();
        return 1;
    case VAR_CODE_RF_TX:
        if (*val_ptr == VAR_ENABLE)
            enable_tx();
        else if (*val_ptr == VAR_DISABLE)
            disable_tx();
        return 1;
    case VAR_CODE_RF_RX:
        if (*val_ptr == VAR_ENABLE)
            enable_rx();
        else if (*val_ptr == VAR_DISABLE)
            disable_rx();
        return 1;
    default:
        return 0;
    }
}

// ========== Outbound Messaging Targets ========== //
void cdh_enable_pay(rx_cmd_buff_t *rx_cmd_buff, tx_cmd_buff_t *tx_cmd_buff)
{
    uint8_t my_payload[] = {VAR_CODE_PAY_EN, 0x01, VAR_ENABLE};
    msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void cdh_disable_pay(rx_cmd_buff_t *rx_cmd_buff, tx_cmd_buff_t *tx_cmd_buff)
{
    uint8_t my_payload[] = {VAR_CODE_PAY_EN, 0x01, VAR_DISABLE};
    msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void cdh_blink_demo(rx_cmd_buff_t *rx_cmd_buff, tx_cmd_buff_t *tx_cmd_buff)
{
    uint8_t my_payload[] = {VAR_CODE_BLINK_CDH, 0x01, VAR_ENABLE};
    msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void run_demo(rx_cmd_buff_t *rx_cmd_buff, tx_cmd_buff_t *tx_cmd_buff)
{
    k_event_post(&app_events, EVENT_RUN_DEMO);
}

// ========== Flash Operations ========== //
void flash_erase_page(uint32_t page)
{
    NRF_NVMC->CONFIG = NVMC_CONFIG_EEN;
    __asm__("isb 0xF");
    __asm__("dsb 0xF");
    NRF_NVMC->ERASEPAGE = page * (0x00001000);
    while (NRF_NVMC->READY == NVMC_READY_BUSY)
    {
    }
    NRF_NVMC->CONFIG = NVMC_CONFIG_REN;
    __asm__("isb 0xF");
    __asm__("dsb 0xF");
}

int handle_bootloader_write_page_addr32(rx_cmd_buff_t *rx_cmd_buff) { return 1; }
int handle_bootloader_jump(void) { return 0; }
int bootloader_active(void) { return 1; }
int handle_bootloader_erase(void) { return 1; }
int handle_bootloader_write_page(rx_cmd_buff_t *rx_cmd_buff) { return 1; }