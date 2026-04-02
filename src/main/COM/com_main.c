// com_main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include <com.h>

// Bring in the USB UART device defined in com.c for our heartbeat & DTR checks
extern const struct device *uart_gnd_dev;

// Local reference to LED1 just for the boot sequence
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void) {
    uint32_t dtr = 0;
    
    // 1. Hardware Init
    init_leds();
    init_gpio();

    // 2. USB Bringup
    if (!device_is_ready(uart_gnd_dev)) {
        while(1) {
            k_msleep(50); 
        }
    }

    int err = usb_enable(NULL);
    if (err != 0) {
        while(1) {
            k_msleep(250); 
        }
    }

    // 3. Wait for the host PC to open the COM port (DTR Signal)
    int timeout = 100; // 10 seconds to open terminal
    while (!dtr && timeout > 0) {
        uart_line_ctrl_get(uart_gnd_dev, UART_LINE_CTRL_DTR, &dtr);
        k_msleep(100);
        timeout--;
        gpio_pin_toggle_dt(&led1); 
    }
    
    gpio_pin_set_dt(&led1, 1); // Solid LED1 when connected

    // 4. Boot ISR pipelines (Feeds the threads in com.c)
    init_all_uarts();
    /*

    printk("System Boot Complete. Starting Heartbeat.\n");

    5. Main Status Loop (USB Heartbeat)
    const char *heartbeat_msg = "[SYS] COM Board Heartbeat Active\r\n";
    
    while (1) {
        // Only send heartbeat if DTR is still active (terminal is open)
        uart_line_ctrl_get(uart_gnd_dev, UART_LINE_CTRL_DTR, &dtr);
        if (dtr) {
            for (int i = 0; heartbeat_msg[i] != '\0'; i++) {
                uart_poll_out(uart_gnd_dev, heartbeat_msg[i]);
            }
        }
        
        k_msleep(3000); // 3 second heartbeat
    }
    */
    
    return 0;
}