#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include <cdh.h>

/* Map our devicetree aliases */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
const struct device *uart1_dev = DEVICE_DT_GET(DT_ALIAS(uart_1)); // COM UART

/* 1. Define Message Queue for Completed RX Commands */
K_MSGQ_DEFINE(rx_cmd_queue, sizeof(rx_cmd_buff_t), 10, 4);

/* ISR specific buffer (only modified inside the ISR) */
static rx_cmd_buff_t isr_rx_buff = {.size = CMD_MAX_LEN, .route_id = CDH};

/* 2. UART RX Interrupt Callback */
static void uart1_cb(const struct device *dev, void *user_data) {
    if (!uart_irq_update(dev)) {
        return;
    }

    if (uart_irq_rx_ready(dev)) {
        uint8_t c;
        // Read until the FIFO is empty
        while (uart_fifo_read(dev, &c, 1) == 1) {
            push_rx_cmd_buff(&isr_rx_buff, c);
            
            // If the TAB machine just finished a packet, queue it up!
            if (isr_rx_buff.state == RX_CMD_BUFF_STATE_COMPLETE) {
                // Push to thread, drop packet if queue is full (K_NO_WAIT in ISR)
                k_msgq_put(&rx_cmd_queue, &isr_rx_buff, K_NO_WAIT);
                clear_rx_cmd_buff(&isr_rx_buff); // Reset for the next incoming command
            }
        }
    }
}

/* 3. Command Processing Thread */
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

            reply(&local_rx, &local_tx); 
            
            if (!local_tx.empty) {
                tx_usart1(&local_tx); 
            }
        }
    }
}

/* Define the thread: Stack size 1024, Priority 5 */
K_THREAD_DEFINE(cmd_processor_tid, 1024, cmd_processor_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void) {
    uint32_t dtr = 0;
    int usb_err = -1;

    /* 1. Initialize Status LED */
    if (device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    }

    /* 2. Boot the USB Console */
    if (device_is_ready(console_dev)) {
        usb_err = usb_enable(NULL);
        if (usb_err == 0) {
            int timeout = 25; /* Wait up to 2.5 seconds for PC to connect */
            while (!dtr && timeout > 0) {
                uart_line_ctrl_get(console_dev, UART_LINE_CTRL_DTR, &dtr);
                k_msleep(100);
                timeout--;
            }
        }
    }

    /* Initialize ISR state machine */
    clear_rx_cmd_buff(&isr_rx_buff);

    /* Configure UART Interrupts */
    if (device_is_ready(uart1_dev)) {
        uart_irq_callback_user_data_set(uart1_dev, uart1_cb, NULL);
        uart_irq_rx_enable(uart1_dev);
    }

    /* Turn on COM and wait for it */
    power_on_com();
    
    // Note: check_com_online needs a rewrite (see below)
    check_com_online(); 

    /* Main Flight Loop (Heartbeat / Watchdog) */
    while (1) {
        printk("CDH Heartbeat - System Nominal.\n");
        gpio_pin_toggle_dt(&led);
        k_msleep(1000); // Main thread just sleeps and kicks the dog now
    }
    
    return 0;
}


// // cdh_main.h
// // CDH main applications
// //
// // Written by Jack Rathert
// // Other contributors: Bradley Denby, Chad Taylor, Alok Anand
// //
// // See the top-level LICENSE file for the license.

// // Includes for standard libs, BSP, and common TAB command parsing
// #include <stddef.h> // size_t
// #include <stdint.h> // uint8_t, uint32_t
// #include <zephyr/kernel.h>
// #include <cdh.h>    // CDH header
// #include <tab.h>    // TAB header

// // ========== Super Loop ========== //
// int main(void) {

//   // MCU initialization //
//   init_clock(); // HSE 32 MHz
//   init_leds();
//   init_uart(); // shared UART to COM (SP26 CDH 0.1.0)
//   init_gpio();

//   // init UART for control of COM/PAY
//   rx_cmd_buff_t rx_cmd_buff = {.size=CMD_MAX_LEN, .route_id=CDH};
//   clear_rx_cmd_buff(&rx_cmd_buff);
//   tx_cmd_buff_t tx_cmd_buff = {.size=CMD_MAX_LEN};
//   clear_tx_cmd_buff(&tx_cmd_buff);

// // turn on radio front end by default
//   power_on_com(); // enable com pcb power off the rip
  
//   // wait for com to wake up and respond
//   check_com_online(&rx_cmd_buff, &tx_cmd_buff);

//   // clean up before entering main loop
//   clear_rx_cmd_buff(&rx_cmd_buff);

//   // safe to enable rf frontend now
//   // com_enable_rf(&rx_cmd_buff, &tx_cmd_buff);

//   com_start_demo(&rx_cmd_buff, &tx_cmd_buff);

//   // UART Control Loop //
//   while(1) {
//     rx_usart1(&rx_cmd_buff);           // Collect command bytes
//     reply(&rx_cmd_buff, &tx_cmd_buff); // Command reply logic
//     tx_usart1(&tx_cmd_buff);           // Send a response if any
//   }
//   return 0;
// }
