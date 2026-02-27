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

// Pin definitions

#define RF_FRONTEND_PIN  GPIO9 
#define TX_EN_PIN        GPIO3
#define RX_EN_PIN        GPIO2

int RX_ON = 1;

int main(void) {
  // MCU initialization
  init_leds();
  init_uart();
  init_clock();

  // RF Frontend Init

  gpio_mode_setup(P1, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RF_FRONTEND_PIN);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TX_EN_PIN);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RX_EN_PIN);
  gpio_set(  P1, RF_FRONTEND_PIN); // PDN (RF front end enable) set to high (logical 1)
  gpio_clear(P0, TX_EN_PORT); // TX_EN (TX enable) set to low (logical 0)
  gpio_clear(P0, RX_EN_PIN); // RX_EN (RX enable) set to low (logical 0)

  // TAB initialization
  rx_cmd_buff_t rx_cmd_buff = {.size=CMD_MAX_LEN};
  clear_rx_cmd_buff(&rx_cmd_buff);
  tx_cmd_buff_t tx_cmd_buff = {.size=CMD_MAX_LEN};
  clear_tx_cmd_buff(&tx_cmd_buff);

  while(RX_ON) {
    // rx_uart(&rx_cmd_buff);             // Collect command bytes
    // reply(&rx_cmd_buff, &tx_cmd_buff); // Command reply logic
    // tx_uart(&tx_cmd_buff);             // Send a response if any



    // listen rx
    // cdh or PC to send RX disable/tx enable

    RX_ON = 0;

  }
  
gpio_toggle(P0, RX_EN_PIN);
gpio_toggle(P0, TX_EN_PIN);



  
}