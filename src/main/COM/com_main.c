// com_main.c
// main application for the com board

#include <stddef.h>
#include <stdint.h>
#include <libopencm3/nrf/clock.h>
#include <libopencm3/nrf/gpio.h>
#include <com.h>
#include <tab.h>
#include <com.h>

// external function from com.c
// extern void init_radio_tx_test(void);

// pin definitions
#define P0              (GPIO_BASE)
#define P1              (GPIO_BASE + 0x300)
#define RF_FRONTEND_PIN GPIO9
#define TX_EN_PIN       GPIO3
#define RX_EN_PIN       GPIO2

int main(void) {
  // setup mcu
  init_leds();
  init_uart();
  init_clock();

  // setup radio for burst test
  init_radio_tx_test();

  // setup rf frontend pins
  gpio_mode_setup(P1, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RF_FRONTEND_PIN);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TX_EN_PIN);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RX_EN_PIN);
  
  // default: frontend on, rx enabled, tx disabled
  gpio_set(P1, RF_FRONTEND_PIN);
  gpio_clear(P0, TX_EN_PIN);
  gpio_set(P0, RX_EN_PIN);

  // setup tab buffers
  rx_cmd_buff_t rx_cmd_buff = {.size=CMD_MAX_LEN};
  clear_rx_cmd_buff(&rx_cmd_buff);
  
  tx_cmd_buff_t tx_cmd_buff = {.size=CMD_MAX_LEN};
  clear_tx_cmd_buff(&tx_cmd_buff);

  // listen for commands
  while(1) {
    // get packet
    rx_uart(&rx_cmd_buff);
    
    // parse and execute (0x16 opcode -> 0x03 action triggers burst)
    reply(&rx_cmd_buff, &tx_cmd_buff);
    
    // send response
    tx_uart(&tx_cmd_buff);
  }

  return 0;
}