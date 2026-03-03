// blink.c
// Blinks LEDs on the CDH board
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
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO29);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO30);
  gpio_set(  P0, GPIO30); // LED1 is P0.30
  gpio_clear(P0, GPIO29); // LED2 is P0.29
  while(1) {
    // blink for 15 seconds
  for(int k=0; k<480000000; k++){
      for(int i=0; i<4000000; i++) { 
      __asm__("nop");
    }
    gpio_toggle(P0, GPIO30);
    gpio_toggle(P0, GPIO29);
  }
  // Added a faster blink for 15 seconds 
  for(int k=0; k<480000000; k++){
      for(int i=0; i<400000; i++) { 
      __asm__("nop");
    }
    gpio_toggle(P0, GPIO30);
    gpio_toggle(P0, GPIO29);
  }  

  }

}
