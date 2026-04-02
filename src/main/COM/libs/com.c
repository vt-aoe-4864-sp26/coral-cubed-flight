// com.c
#include <stdint.h>                 
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <com.h>                    
#include <tab.h>

// ========== Macros ========== //
#define NVMC_CONFIG MMIO32    (NVMC_BASE + 0x504)
#define NVMC_CONFIG_REN       (0     )
#define NVMC_CONFIG_WEN       (1 << 1)
#define NVMC_CONFIG_EEN       (1 << 2)
#define NVMC_ERASEPAGE MMIO32 (NVMC_BASE + 0x508)
#define NVMC_READY MMIO32     (NVMC_BASE + 0x400)
#define NVMC_READY_BUSY       (0     )

#define FEM_NODE DT_NODELABEL(nrf_radio_fem)

// ========== Hardware Aliases ========== //
const struct device *uart_gnd_dev = DEVICE_DT_GET(DT_ALIAS(uart_0)); // USB-C / Ground
const struct device *uart_cdh_dev = DEVICE_DT_GET(DT_ALIAS(uart_1)); // Hardware to CDH

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static const struct gpio_dt_spec fem_tx_en = GPIO_DT_SPEC_GET(FEM_NODE, tx_en_gpios);
static const struct gpio_dt_spec fem_rx_en = GPIO_DT_SPEC_GET(FEM_NODE, rx_en_gpios);
static const struct gpio_dt_spec fem_pdn   = GPIO_DT_SPEC_GET(FEM_NODE, pdn_gpios);
static const struct gpio_dt_spec fem_mode  = GPIO_DT_SPEC_GET(FEM_NODE, mode_gpios);

// ========== Concurrency Primitives ========== //

// 1. Comm Router Queue
K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 20, 4);

// 2. Application Events
#define EVENT_BLINK_DEMO BIT(0)
#define EVENT_RUN_DEMO   BIT(1)
K_EVENT_DEFINE(app_events);

typedef struct {
    const struct device *dev;
    rx_cmd_buff_t rx_buff;
} uart_lane_ctx_t;

static uart_lane_ctx_t uart_lanes[2] = {
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = COM} },
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = COM} }
};


// ========== Hardware Init ========== //
void init_leds(void) {
    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
}

void init_gpio(void) {
    if(device_is_ready(fem_pdn.port)) {
        gpio_pin_configure_dt(&fem_pdn, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_tx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_rx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_mode, GPIO_OUTPUT_INACTIVE); 
    }
}


// ========== UART ISR Pipeline ========== //
static void generic_uart_callback(const struct device *dev, void *user_data) {
    uart_lane_ctx_t *ctx = (uart_lane_ctx_t *)user_data;

    if (!uart_irq_update(dev)) return;

    if (uart_irq_rx_ready(dev)) {
        uint8_t c;
        while (uart_fifo_read(dev, &c, 1) == 1) {
            push_rx_cmd_buff(&ctx->rx_buff, c);
            
            if (ctx->rx_buff.state == RX_CMD_BUFF_STATE_COMPLETE) {
                k_msgq_put(&rx_cmd_queue, &ctx->rx_buff, K_NO_WAIT);
                clear_rx_cmd_buff(&ctx->rx_buff); 
            }
        }
    }
}

void init_all_uarts(void) {
    uart_lanes[0].dev = uart_gnd_dev;
    uart_lanes[1].dev = uart_cdh_dev;

    for (int i = 0; i < 2; i++) {
        uart_lane_ctx_t *ctx = &uart_lanes[i];
        clear_rx_cmd_buff(&ctx->rx_buff);

        if (ctx->dev != NULL && device_is_ready(ctx->dev)) {
            uart_irq_callback_user_data_set(ctx->dev, generic_uart_callback, ctx);
            uart_irq_rx_enable(ctx->dev);
        }
    }
}


// ========== Thread 1: Comm Router (Priority 5) ========== //
void cmd_processor_entry(void) {
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};
    
    while (1) {
        if (k_msgq_get(&rx_cmd_queue, &local_rx, K_FOREVER) == 0) {
            clear_tx_cmd_buff(&local_tx); // Ensure clean state before routing

            route_rx_packet(&local_rx, &local_tx); 
            
            if (!local_tx.empty) {
                route_tx_packet(&local_tx); 
            }
        }
    }
}
K_THREAD_DEFINE(cmd_processor_tid, 1024, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);


// ========== Thread 2: Application Tasks (Priority 7) ========== //
void app_thread_entry(void *p1, void *p2, void *p3) {
    while (1) {
        uint32_t events = k_event_wait(&app_events, (EVENT_BLINK_DEMO | EVENT_RUN_DEMO), false, K_FOREVER);

        if (events & EVENT_BLINK_DEMO) {
            k_event_clear(&app_events, EVENT_BLINK_DEMO);
            for(int k = 0; k < 20; k++) {
                k_msleep(250);
                gpio_pin_toggle_dt(&led1);
                gpio_pin_toggle_dt(&led2);
            }
            gpio_pin_set_dt(&led1, 0);
            gpio_pin_set_dt(&led2, 0);
        }

        if (events & EVENT_RUN_DEMO) {
            k_event_clear(&app_events, EVENT_RUN_DEMO);
            
            tx_cmd_buff_t demo_tx = {.size=CMD_MAX_LEN};
            clear_tx_cmd_buff(&demo_tx);
            rx_cmd_buff_t dummy_rx = {.route_id = COM, .bus_msg_id = 0};

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
K_THREAD_DEFINE(app_tid, 1024, app_thread_entry, NULL, NULL, NULL, 7, 0, 0);


// ========== Routing Logic ========== //
void route_rx_packet(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o) {
    if(rx_cmd_buff_o->state == RX_CMD_BUFF_STATE_COMPLETE && tx_cmd_buff_o->empty) {
        uint8_t dest_id = (rx_cmd_buff_o->data[ROUTE_INDEX] & 0xF0) >> 4;

        if (dest_id == COM) {
            write_reply(rx_cmd_buff_o, tx_cmd_buff_o);
        } else {
            for(size_t i = 0; i < rx_cmd_buff_o->end_index; i++) {
                tx_cmd_buff_o->data[i] = rx_cmd_buff_o->data[i];
            }
            tx_cmd_buff_o->start_index = 0;
            tx_cmd_buff_o->end_index = rx_cmd_buff_o->end_index;
            tx_cmd_buff_o->empty = 0;
            clear_rx_cmd_buff(rx_cmd_buff_o);
        }
    }
}

void route_tx_packet(tx_cmd_buff_t* tx_cmd_buff_o) {
    if (tx_cmd_buff_o->empty) return;

    uint8_t dest_id = (tx_cmd_buff_o->data[ROUTE_INDEX] & 0xF0) >> 4;
    const struct device *target_dev = NULL;

    switch (dest_id) {
        case GND: target_dev = uart_gnd_dev; break; 
        case CDH: 
        case PLD: target_dev = uart_cdh_dev; break; 
        default:  return; 
    }

    if (target_dev != NULL && device_is_ready(target_dev)) {
        while(!(tx_cmd_buff_o->empty)) {
            uint8_t b = pop_tx_cmd_buff(tx_cmd_buff_o);
            uart_poll_out(target_dev, b);
        }
        gpio_pin_toggle_dt(&led2); // Flash LED2 on successful route
    } else {
        clear_tx_cmd_buff(tx_cmd_buff_o); 
    }
}


// ========== Command Handlers ========== //
int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff) {
    if(common_data_buff_i.end_index < 2) return 0; 
    uint8_t var_code = common_data_buff_i.data[0];
    uint8_t var_len  = common_data_buff_i.data[1];
    if(common_data_buff_i.end_index < (size_t)(2 + var_len)) return 0; 

    uint8_t* val_ptr = &common_data_buff_i.data[2];

    switch(var_code) {
        case VAR_CODE_ALIVE:
            if (*val_ptr == 0x01){
                gpio_pin_set_dt(&led2, 1); 
                return 1;
            } 
            return 0;
        case VAR_CODE_PAY_EN:
            if (*val_ptr == VAR_ENABLE) cdh_enable_pay(rx_cmd_buff, tx_cmd_buff);
            else if (*val_ptr == VAR_DISABLE) cdh_disable_pay(rx_cmd_buff, tx_cmd_buff);
            return 1;
        case VAR_CODE_RUN_DEMO:
            k_event_post(&app_events, EVENT_RUN_DEMO);
            return 1;
        case VAR_CODE_BLINK_COM:
            k_event_post(&app_events, EVENT_BLINK_DEMO);
            return 1;
        case VAR_CODE_RF_EN:
            if (*val_ptr == VAR_ENABLE) enable_rf();
            else if (*val_ptr == VAR_DISABLE) disable_rf();
            return 1;
        case VAR_CODE_RF_TX:
            if (*val_ptr == VAR_ENABLE) enable_tx();
            else if (*val_ptr == VAR_DISABLE) disable_tx();
            return 1;
        case VAR_CODE_RF_RX:
            if (*val_ptr == VAR_ENABLE) enable_rx();
            else if (*val_ptr == VAR_DISABLE) disable_rx();
            return 1;
        default: return 0;
    }
}

// ========== Outbound Messaging Targets ========== //
void cdh_enable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
    uint8_t my_payload[] = {VAR_CODE_PAY_EN, 0x01, VAR_ENABLE};
    msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void cdh_disable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
    uint8_t my_payload[] = {VAR_CODE_PAY_EN, 0x01, VAR_DISABLE};
    msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void cdh_blink_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
    uint8_t my_payload[] = {VAR_CODE_BLINK_CDH, 0x01, VAR_ENABLE};
    msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void run_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
    k_event_post(&app_events, EVENT_RUN_DEMO);
}

// ========== Hardware Controls ========== //
void enable_rf(void) { gpio_pin_set_dt(&fem_pdn, 1); }
void disable_rf(void) { 
    gpio_pin_set_dt(&fem_pdn, 0); 
    gpio_pin_set_dt(&fem_tx_en, 0); 
    gpio_pin_set_dt(&fem_rx_en, 0); 
}

void enable_tx(void) { 
    gpio_pin_set_dt(&fem_rx_en, 0); 
    gpio_pin_set_dt(&fem_tx_en, 1); 
}
void disable_tx(void) { gpio_pin_set_dt(&fem_tx_en, 0); }

void enable_rx(void) { 
    gpio_pin_set_dt(&fem_tx_en, 0); 
    gpio_pin_set_dt(&fem_rx_en, 1); 
}
void disable_rx(void) { gpio_pin_set_dt(&fem_rx_en, 0); }

void flash_erase_page(uint32_t page) {
    NRF_NVMC->CONFIG = NVMC_CONFIG_EEN;
    __asm__("isb 0xF");
    __asm__("dsb 0xF");
    NRF_NVMC->ERASEPAGE = page*(0x00001000);
    while(NRF_NVMC->READY==NVMC_READY_BUSY) {}
    NRF_NVMC->CONFIG = NVMC_CONFIG_REN;
    __asm__("isb 0xF");
    __asm__("dsb 0xF");
}

int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff) { return 1; }
int handle_bootloader_jump(void) { return 0; }
int bootloader_active(void) { return 1; }
int handle_bootloader_erase(void) { return 1; }
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff) { return 1; }