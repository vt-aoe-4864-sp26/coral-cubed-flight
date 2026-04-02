// com_main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include <com.h>

// Map our 2 UART lanes
const struct device *uart_gnd_dev = DEVICE_DT_GET(DT_ALIAS(uart_0)); // USB-C / Ground
const struct device *uart_cdh_dev = DEVICE_DT_GET(DT_ALIAS(uart_1)); // Hardware to CDH

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// Central Queue for TAB Parsing
K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 20, 4);

typedef struct {
    const struct device *dev;
    rx_cmd_buff_t rx_buff;
} uart_lane_ctx_t;

// Set route_id to COM so this board listens for messages targeted at it
static uart_lane_ctx_t uart_lanes[2] = {
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = COM} },
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = COM} }
};

static void generic_uart_callback(const struct device *dev, void *user_data) {
    uart_lane_ctx_t *ctx = (uart_lane_ctx_t *)user_data;

    if (!uart_irq_update(dev)) {
        return;
    }

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

void cmd_processor_entry(void) {
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_tx);

    printk("COM Processor Thread Started.\n");

    while (1) {
        if (k_msgq_get(&rx_cmd_queue, &local_rx, K_FOREVER) == 0) {
        
            route_rx_packet(&local_rx, &local_tx); 
            
            if (!local_tx.empty) {
                route_tx_packet(&local_tx); 
            }
        }
    }
}

K_THREAD_DEFINE(cmd_processor_tid, 1024, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void) {
    uint32_t dtr = 0;
    
    // Hardware Init
    init_leds();
    init_gpio();

    if (!device_is_ready(uart_gnd_dev)) {
        while(1) {
            gpio_pin_toggle_dt(&led2);
            k_msleep(50); 
        }
    }

    int err = usb_enable(NULL);
    if (err != 0) {
        while(1) {
            gpio_pin_toggle_dt(&led2);
            k_msleep(250); 
        }
    }

    // Wait for the host PC to open the COM port
    int timeout = 100; // 10 seconds to open terminal
    while (!dtr && timeout > 0) {
        uart_line_ctrl_get(uart_gnd_dev, UART_LINE_CTRL_DTR, &dtr);
        k_msleep(100);
        timeout--;
        gpio_pin_toggle_dt(&led1); 
    }
    
    gpio_pin_set_dt(&led1, 1); // Solid LED1 when connected

    // Boot ISR pipelines
    init_all_uarts();

    // Main Status Loop
    while (1) {
        // Toggle LED2 for a silent heartbeat since printk is gone
        gpio_pin_toggle_dt(&led2);
        k_msleep(2000); 
    }
    
    return 0;
}