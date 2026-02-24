// blink.c
// Blinks LEDs on the CDH board
//
// Written by Bradley Denby
// Other contributors: None
//
// See the top-level LICENSE file for the license.

// libopencm3
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

int main(void) {
  rcc_periph_clock_enable(RCC_GPIOB);
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO1);
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO2);
  gpio_set(  GPIOB, GPIO2); // LED1 is B2
  gpio_clear(GPIOB, GPIO1); // LED2 is B1
  while(1) {
    for(int i=0; i<4000000; i++) {
      __asm__("nop");
    }
    gpio_toggle(GPIOB, GPIO2);
    gpio_toggle(GPIOB, GPIO1);
  }
}
