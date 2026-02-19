// gpio.c
// Toggles GPIOs on the CDH board
//
// Written by Bradley Denby
// Other contributors: None
//
// See the top-level LICENSE file for the license.

// libopencm3
#include <libopencm3/nrf/clock.h>
#include <libopencm3/nrf/gpio.h>

// macros
#define P0 (GPIO_BASE)
#define P1 (GPIO_BASE + 0x300)

int main(void) {
  clock_start_hfclk(0);
  gpio_mode_setup(P1, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO9);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO3);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO2);
  gpio_set(  P1, GPIO9); // PDN (RF front end enable) set to high (logical 1)
  gpio_clear(P0, GPIO3); // TX_EN (TX enable) set to low (logical 0)
  gpio_clear(P0, GPIO2); // RX_EN (RX enable) set to low (logical 0)
  while(1) {
    // nop means no operation; do nothing for 8000000 instructions
    for(int i=0; i<8000000; i++) {
      __asm__("nop");
    }
    // enter TX mode
    gpio_toggle(P0, GPIO3);
    // nop means no operation; do nothing for 8000000 instructions
    for(int i=0; i<8000000; i++) {
      __asm__("nop");
    }
    // enter "program" mode
    gpio_toggle(P0, GPIO3);
    // nop means no operation; do nothing for 8000000 instructions
    for(int i=0; i<8000000; i++) {
      __asm__("nop");
    }
    // enter RX mode
    gpio_toggle(P0, GPIO2);
    // nop means no operation; do nothing for 8000000 instructions
    for(int i=0; i<8000000; i++) {
      __asm__("nop");
    }
    // enter "program" mode
    gpio_toggle(P0, GPIO2);
  }
}
