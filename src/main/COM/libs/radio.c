// radio.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/ieee802154_radio.h>
#include <zephyr/net/net_if.h>
#include <hal/nrf_radio.h>
#include <string.h> // Required for memcpy
#include "radio.h"
#include "tab.h"

// ========== Configuration ========== //
#define MAX_RETRIES          5
#define RETRY_TIMEOUT_MS     250
#define RADIO_QUEUE_SIZE     10

// nRF52833 Supported TX Power levels (escalation path)
static const int8_t tx_power_escalation[] = {-12, -4, 0, 4, 8}; 
#define NUM_POWER_LEVELS (sizeof(tx_power_escalation) / sizeof(int8_t))

// ========== Data Structures ========== //
// Queue for incoming transmission requests
K_MSGQ_DEFINE(radio_tx_queue, sizeof(tx_cmd_buff_t), RADIO_QUEUE_SIZE, 4);

// Tracker for the currently transmitting un-ACK'd message
typedef struct {
    int active;
    uint8_t retries;
    int8_t current_power_idx;
    uint32_t last_tx_time;
    tx_cmd_buff_t payload;
} pending_tx_t;

// Initialize the const size member at declaration to avoid compiler warnings
static pending_tx_t pending_msg = { .payload = { .size = CMD_MAX_LEN } };

// ========== Hardware Aliases ========== //
#define FEM_NODE DT_NODELABEL(nrf_radio_fem)
static const struct gpio_dt_spec fem_tx_en = GPIO_DT_SPEC_GET(FEM_NODE, tx_en_gpios);
static const struct gpio_dt_spec fem_rx_en = GPIO_DT_SPEC_GET(FEM_NODE, rx_en_gpios);
static const struct gpio_dt_spec fem_pdn   = GPIO_DT_SPEC_GET(FEM_NODE, pdn_gpios);
static const struct gpio_dt_spec fem_mode  = GPIO_DT_SPEC_GET(FEM_NODE, mode_gpios);

// ========== Thread Definition ========== //
void radio_thread_entry(void *p1, void *p2, void *p3) {
    tx_cmd_buff_t new_tx;
    
    // Updated Zephyr API call
    struct net_if *iface = net_if_get_ieee802154();

    while (1) {
        // 1. Check if we have an active message awaiting ACK
        if (pending_msg.active) {
            uint32_t now = k_uptime_get_32();
            if ((now - pending_msg.last_tx_time) > RETRY_TIMEOUT_MS) {
                
                pending_msg.retries++;
                
                if (pending_msg.retries >= MAX_RETRIES) {
                    // Drop packet, reset state
                    pending_msg.active = 0;
                } else {
                    // Escalate power if possible
                    if (pending_msg.current_power_idx < (NUM_POWER_LEVELS - 1)) {
                        pending_msg.current_power_idx++;
                    }
                    
                    // Resend logic goes here (push to net_if or raw radio API)
                    pending_msg.last_tx_time = now;
                }
            }
        }

        // 2. Poll for new messages 
        k_timeout_t wait_time = pending_msg.active ? K_MSEC(10) : K_FOREVER;
        
        if (k_msgq_get(&radio_tx_queue, &new_tx, wait_time) == 0) {
            // New message arrived!
            if (!pending_msg.active) {
                pending_msg.active = 1;
                pending_msg.retries = 0;
                pending_msg.current_power_idx = 0; 
                pending_msg.last_tx_time = k_uptime_get_32();
                
                // Copy mutable struct members individually to avoid const violation
                pending_msg.payload.empty = new_tx.empty;
                pending_msg.payload.start_index = new_tx.start_index;
                pending_msg.payload.end_index = new_tx.end_index;
                memcpy(pending_msg.payload.data, new_tx.data, new_tx.end_index);
                
                // Send logic goes here
            }
        }
    }
}
K_THREAD_DEFINE(radio_tid, 2048, radio_thread_entry, NULL, NULL, NULL, 6, 0, 0);

// ========== Public API ========== //

void init_radio(void) {
    if(device_is_ready(fem_pdn.port)) {
        gpio_pin_configure_dt(&fem_pdn, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_tx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_rx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_mode, GPIO_OUTPUT_INACTIVE); 
    }
}

int radio_send_packet(tx_cmd_buff_t *tx_buff) {
    return k_msgq_put(&radio_tx_queue, tx_buff, K_NO_WAIT);
}

// ========== Diagnostics ========== //

void radio_blast_cw_max_power(void) {
    // Force Zephyr's radio driver to stop
    disable_rf();
    
    // Bare-metal unmodulated carrier
    NRF_RADIO->TASKS_DISABLE = 1;
    while(NRF_RADIO->EVENTS_DISABLED == 0) {}
    
    NRF_RADIO->SHORTS = 0; // Disable shorts
    NRF_RADIO->EVENTS_READY = 0;
    
    // Set to Max Power (+8 dBm)
    NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Pos8dBm 
                            << RADIO_TXPOWER_TXPOWER_Pos);
    
    // Set frequency (e.g., 2440 MHz -> channel 40)
    NRF_RADIO->FREQUENCY = 40; 
    
    // Set mode to IEEE 802.15.4
    NRF_RADIO->MODE = (RADIO_MODE_MODE_Ieee802154_250Kbit 
                        << RADIO_MODE_MODE_Pos);
    // Power up the Front End
    enable_rf();
    enable_tx();
    
    // Fire the carrier
    NRF_RADIO->TASKS_TXEN = 1;
    while(NRF_RADIO->EVENTS_READY == 0) {}
    
    // Block the thread for 60 seconds
    printk("Starting 60s max power CW blast...\n");
    k_sleep(K_SECONDS(60));
    printk("CW blast complete.\n");
    
    // Clean up and disable
    NRF_RADIO->TASKS_DISABLE = 1;
    disable_tx();
    disable_rf();
}

// ========== Standard Hardware Controls ========== //
void enable_rf(void) { gpio_pin_set_dt(&fem_pdn, 1); }
void disable_rf(void) { 
    gpio_pin_set_dt(&fem_pdn, 0); 
    gpio_pin_set_dt(&fem_tx_en, 0); 
    gpio_pin_set_dt(&fem_rx_en, 0); 
}
void enable_tx(void) { 
    gpio_pin_set_dt(&fem_rx_en, 0); 
    gpio_pin_set_dt(&fem_tx_en, 1); 
}
void disable_tx(void) { gpio_pin_set_dt(&fem_tx_en, 0); }
void enable_rx(void) { 
    gpio_pin_set_dt(&fem_tx_en, 0); 
    gpio_pin_set_dt(&fem_rx_en, 1); 
}
void disable_rx(void) { gpio_pin_set_dt(&fem_rx_en, 0); }