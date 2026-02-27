// Standard libs
#include <stddef.h> // size_t
#include <stdint.h> // uint8_t, uint32_t
// BSP
#include <libopencm3/nrf/clock.h>
#include <libopencm3/nrf/gpio.h>
// Board-specific header
#include <com.h>    // COM header
// TAB/OpenLST
#include <tab.h>    // TAB header

// macros
#define PORT0 (GPIO_BASE)
#define PORT1 (GPIO_BASE + 0x300)

int RX_ON = 1;

// in main.c
int main(void) {
  // mcu init
  init_leds();
  init_uart();
  init_clock();

  // rf frontend init
  gpio_mode_setup(P1, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RF_FRONTEND_PIN);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TX_EN_PIN);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RX_EN_PIN);
  
  // set default state (rx on, tx off)
  gpio_set(P1, RF_FRONTEND_PIN); 
  gpio_clear(P0, TX_EN_PIN);     
  gpio_set(P0, RX_EN_PIN);       

  // tab init
  rx_cmd_buff_t rx_cmd_buff = {.size=CMD_MAX_LEN};
  clear_rx_cmd_buff(&rx_cmd_buff);
  tx_cmd_buff_t tx_cmd_buff = {.size=CMD_MAX_LEN};
  clear_tx_cmd_buff(&tx_cmd_buff);

  while(1) {
    // block and wait for a full command
    rx_uart(&rx_cmd_buff);             
    
    // parses opcode. if 0x16, it triggers handle_common_data to flip the pins
    reply(&rx_cmd_buff, &tx_cmd_buff); 
    
    // send ack/nack back to cdh/pc
    tx_uart(&tx_cmd_buff);             
  }
}