// com_main.c
// main application for the com board

#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <com.h>


// external function from com.c
// extern void init_radio_tx_test(void);

// pin definitions

int main(void) {
  // setup mcu
  init_leds();
  init_uart();
  init_clock();
  init_gpio();
  init_radio();

  //blast_carrier();

  // TAB initialization
  rx_cmd_buff_t rx_cmd_buff = {.size=CMD_MAX_LEN, .route_id=COM};
  clear_rx_cmd_buff(&rx_cmd_buff);
  tx_cmd_buff_t tx_cmd_buff = {.size=CMD_MAX_LEN};
  clear_tx_cmd_buff(&tx_cmd_buff);


  // TAB loop - wait for commands
  while(1) {
    rx_uart(&rx_cmd_buff);             // Collect command bytes
    reply(&rx_cmd_buff, &tx_cmd_buff); // Command reply logic
    tx_uart(&tx_cmd_buff);             // Send a response if any
  }
  // Should never reach this point
  return 0;
}

