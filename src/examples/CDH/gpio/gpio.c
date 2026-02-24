// gpio.c
// Toggles GPIOs on the CDH board
//
// Written by Bradley Denby
// Other contributors: None
//
// See the top-level LICENSE file for the license.

// libopencm3
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

int main(void) {
  rcc_periph_clock_enable(RCC_GPIOC);
  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO6);
  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO7);
  gpio_set(  GPIOC, GPIO6); // COMEN (radio enable) set to high (logical 1)
  gpio_clear(GPIOC, GPIO7); // PLDEN (payload enable) set to low (logical 0)
  while(1) {
    // nop means no operation; do nothing for 4000000 instructions
    for(int i=0; i<8000000; i++) {
      __asm__("nop");
    }
    // toggle to the opposite GPIO setting (high to low or low to high)
    gpio_toggle(GPIOC, GPIO6);
    gpio_toggle(GPIOC, GPIO7);
  }
}
