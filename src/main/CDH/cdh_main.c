#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>

/* Map our devicetree aliases */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
const struct device *uart1_dev = DEVICE_DT_GET(DT_ALIAS(uart_1));
const struct device *uart2_dev = DEVICE_DT_GET(DT_ALIAS(uart_2));
const struct device *uart3_dev = DEVICE_DT_GET(DT_ALIAS(uart_3));

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

    /* 3. Boot Sequence Logs (Routes to USB) */
    printk("\n====================================\n");
    printk(" Coral CDH Boot Sequence Initiated\n");
    printk("====================================\n");

    /* 4. Verify Board-to-Board Comm Lanes */
    printk("Checking UART Lanes...\n");
    printk(" - UART1 (PA9/10): %s\n", device_is_ready(uart1_dev) ? "OK" : "FAILED");
    printk(" - UART2 (PD5/6):  %s\n", device_is_ready(uart2_dev) ? "OK" : "FAILED");
    printk(" - UART3 (PC4/5):  %s\n", device_is_ready(uart3_dev) ? "OK" : "FAILED");
    printk("====================================\n");

    /* 5. Main Flight Loop */
    while (1) {
        printk("CDH Heartbeat - System Nominal.\n");
        gpio_pin_toggle_dt(&led);
        k_msleep(1000);
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
