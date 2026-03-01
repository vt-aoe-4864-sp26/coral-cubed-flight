// cdh_main.h
// CDH main applications
//
// Written by Jack Rathert
// Other contributors: Bradley Denby, Chad Taylor, Alok Anand
//
// See the top-level LICENSE file for the license.

// Includes for standard libs, BSP, and common TAB command parsing
#include <stddef.h> // size_t
#include <stdint.h> // uint8_t, uint32_t

#include <cdh.h>    // CDH header
#include <tab.h>    // TAB header

// ========== Super Loop ========== //
int main(void) {

  // MCU initialization //
  init_clock(); // HSE 32 MHz
  init_leds();
  init_uart(); // shared UART to COM (SP26 CDH 0.1.0)
  init_gpio();

  // init_com(); // Enable COM PCB Power off the rip
  // init_rf(); // Start in RX mode

  rx_cmd_buff_t rx_cmd_buff = {.size=CMD_MAX_LEN, .route_id=CDH};
  clear_rx_cmd_buff(&rx_cmd_buff);
  tx_cmd_buff_t tx_cmd_buff = {.size=CMD_MAX_LEN};
  clear_tx_cmd_buff(&tx_cmd_buff);

  // UART Control Loop //
  while(1) {
    rx_usart1(&rx_cmd_buff);           // Collect command bytes
    reply(&rx_cmd_buff, &tx_cmd_buff); // Command reply logic
    tx_usart1(&tx_cmd_buff);           // Send a response if any
  }
  return 0;
}
