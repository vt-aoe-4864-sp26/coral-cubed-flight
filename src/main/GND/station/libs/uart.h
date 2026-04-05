// uart.h
#ifndef UART_H
#define UART_H

#include <zephyr/device.h>
#include "tab.h"

extern const struct device *uart_dev;
extern const struct device *uart_cdh_dev;

void init_hardware_uarts(void);
void init_usb_uart(void);

#endif