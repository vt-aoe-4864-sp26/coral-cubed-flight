// cdh_main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include <cdh.h>

// Map our devicetree aliases
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

// Map our 4 UART lanes
const struct device *uart_gnd_dev = DEVICE_DT_GET(DT_ALIAS(uart_0)); // USB-C / Ground
const struct device *uart_com_dev = DEVICE_DT_GET(DT_ALIAS(uart_1)); // COM
const struct device *uart_ext_dev = DEVICE_DT_GET(DT_ALIAS(uart_2)); // External/Debug
const struct device *uart_pay_dev = DEVICE_DT_GET(DT_ALIAS(uart_3)); // Payload (Coral)

// Define Message Queue for ALL Completed RX Commands
K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 20, 4);

// Context struct to track the state of each lane independently
typedef struct {
    const struct device *dev;
    rx_cmd_buff_t rx_buff;
} uart_lane_ctx_t;

// Instantiate the four lanes with their appropriate route IDs
static uart_lane_ctx_t uart_lanes[4] = {
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = GND} },
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = CDH} }, // Listens for CDH
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = PLD} },
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = CDH} }  // Debug routes to CDH
};

// The Unified UART RX Interrupt Callback
static void generic_uart_callback(const struct device *dev, void *user_data) {
    // Cast the user_data back to our specific buffer for this lane
    uart_lane_ctx_t *ctx = (uart_lane_ctx_t *)user_data;

    if (!uart_irq_update(dev)) {
        return;
    }

    if (uart_irq_rx_ready(dev)) {
        uint8_t c;
        while (uart_fifo_read(dev, &c, 1) == 1) {
            push_rx_cmd_buff(&ctx->rx_buff, c);
            
            if (ctx->rx_buff.state == RX_CMD_BUFF_STATE_COMPLETE) {
                // Packet finished! Throw it in the central queue
                k_msgq_put(&rx_cmd_queue, &ctx->rx_buff, K_NO_WAIT);
                clear_rx_cmd_buff(&ctx->rx_buff); 
            }
        }
    }
}

// Hardware Init Helper
void init_all_uarts(void) {
    uart_lanes[0].dev = uart_gnd_dev;
    uart_lanes[1].dev = uart_com_dev;
    uart_lanes[2].dev = uart_pay_dev;
    uart_lanes[3].dev = uart_ext_dev;

    for (int i = 0; i < 4; i++) {
        uart_lane_ctx_t *ctx = &uart_lanes[i];
        clear_rx_cmd_buff(&ctx->rx_buff);

        if (ctx->dev != NULL && device_is_ready(ctx->dev)) {
            uart_irq_callback_user_data_set(ctx->dev, generic_uart_callback, ctx);
            uart_irq_rx_enable(ctx->dev);
        }
    }
}

// Command processing thread
void cmd_processor_entry(void) {
    rx_cmd_buff_t local_rx;
    tx_cmd_buff_t local_tx = {.size = CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_tx);

    printk("Command Processor Thread Started.\n");

    while (1) {
        if (k_msgq_get(&rx_cmd_queue, &local_rx, K_FOREVER) == 0) {
            
            // Intercept the ACK from COM to wake up the boot sequence
            if (local_rx.data[OPCODE_INDEX] == COMMON_ACK_OPCODE) {
                k_sem_give(&com_awake_sem);
            }

            // Execute logic
            reply(&local_rx, &local_tx); 
            
            // If the parser generated a response, route it out!
            if (!local_tx.empty) {
                route_tx_packet(&local_tx); 
            }
        }
    }
}

// Define the thread: Stack size 1024, Priority 5
K_THREAD_DEFINE(cmd_processor_tid, 1024, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void) {
    uint32_t dtr = 0;
    int usb_err = -1;

    // Initialize Status LED
    if (device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    }

    // Boot the USB Console
    if (device_is_ready(console_dev)) {
        usb_err = usb_enable(NULL);
        if (usb_err == 0) {
            int timeout = 25; // 2.5 sec delay
            while (!dtr && timeout > 0) {
                uart_line_ctrl_get(console_dev, UART_LINE_CTRL_DTR, &dtr);
                k_msleep(100);
                timeout--;
            }
        }
    }

    // Boot the unified UART pipelines
    init_all_uarts();

    // Pre-emptively turn on COM, and wait for it to respond
    power_on_com();
    check_com_online(); 

    // Main Flight Loop (Heartbeat / Watchdog)
    while (1) {
        printk("CDH Heartbeat - System Nominal.\n");
        gpio_pin_toggle_dt(&led);
        k_msleep(1000); 
    }
    
    return 0;
}