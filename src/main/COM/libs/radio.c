// radio.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "radio.h"

// ========== Hardware Aliases ========== //
#define FEM_NODE DT_NODELABEL(nrf_radio_fem)

static const struct gpio_dt_spec fem_tx_en = GPIO_DT_SPEC_GET(FEM_NODE, tx_en_gpios);
static const struct gpio_dt_spec fem_rx_en = GPIO_DT_SPEC_GET(FEM_NODE, rx_en_gpios);
static const struct gpio_dt_spec fem_pdn   = GPIO_DT_SPEC_GET(FEM_NODE, pdn_gpios);
static const struct gpio_dt_spec fem_mode  = GPIO_DT_SPEC_GET(FEM_NODE, mode_gpios);

// ========== Init ========== //
void init_radio(void) {
    if(device_is_ready(fem_pdn.port)) {
        gpio_pin_configure_dt(&fem_pdn, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_tx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_rx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_mode, GPIO_OUTPUT_INACTIVE); 
    }
}

// ========== Hardware Controls ========== //
void enable_rf(void) { 
    gpio_pin_set_dt(&fem_pdn, 1); 
}

void disable_rf(void) { 
    gpio_pin_set_dt(&fem_pdn, 0); 
    gpio_pin_set_dt(&fem_tx_en, 0); 
    gpio_pin_set_dt(&fem_rx_en, 0); 
}

void enable_tx(void) { 
    gpio_pin_set_dt(&fem_rx_en, 0); 
    gpio_pin_set_dt(&fem_tx_en, 1); 
}

void disable_tx(void) { 
    gpio_pin_set_dt(&fem_tx_en, 0); 
}

void enable_rx(void) { 
    gpio_pin_set_dt(&fem_tx_en, 0); 
    gpio_pin_set_dt(&fem_rx_en, 1); 
}

void disable_rx(void) { 
    gpio_pin_set_dt(&fem_rx_en, 0); 
}