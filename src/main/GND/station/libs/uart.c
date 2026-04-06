// uart.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include "uart.h"

// ========== OS Hooks ========== //
extern struct k_msgq rx_cmd_queue;

// ========== Hardware Aliases ========== //
const struct device *uart_gnd_dev = DEVICE_DT_GET(DT_ALIAS(uart_0)); // USB-C / Ground
const struct device *uart_cdh_dev = DEVICE_DT_GET(DT_ALIAS(uart_1)); // Hardware to CDH

// ========== UART Pipeline ========== //
typedef struct {
    const struct device *dev;
    rx_cmd_buff_t rx_buff;
} uart_lane_ctx_t;

static uart_lane_ctx_t uart_lanes[2] = {
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = COMG, .bus_msg_id = 0} },
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = COMG, .bus_msg_id = 0} }
};

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

void init_hardware_uarts(void) {
    uart_lanes[1].dev = uart_cdh_dev;
    uart_lane_ctx_t *ctx = &uart_lanes[1];
    clear_rx_cmd_buff(&ctx->rx_buff);

    if (ctx->dev != NULL && device_is_ready(ctx->dev)) {
        uart_irq_callback_user_data_set(ctx->dev, generic_uart_callback, ctx);
        uart_irq_rx_enable(ctx->dev);
    }
}

void init_usb_uart(void) {
    uart_lanes[0].dev = uart_gnd_dev;
    uart_lane_ctx_t *ctx = &uart_lanes[0];
    clear_rx_cmd_buff(&ctx->rx_buff);

    if (ctx->dev != NULL && device_is_ready(ctx->dev)) {
        uart_irq_callback_user_data_set(ctx->dev, generic_uart_callback, ctx);
        uart_irq_rx_enable(ctx->dev);
    }
}