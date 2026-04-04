// radio.c
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/ieee802154_radio.h>
#include <zephyr/net/ieee802154.h>
#include <zephyr/net/ieee802154_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/ethernet.h> 
#include <zephyr/net/net_if.h>
#include <zephyr/sys/byteorder.h>       
#include <hal/nrf_radio.h>
#include <string.h> 
#include "radio.h"
#include "tab.h"

// ========== Configuration ========== //
#define MAX_RETRIES          5
#define RETRY_TIMEOUT_MS     250
#define RADIO_QUEUE_SIZE     10

// nRF52833 Supported TX Power levels (escalation path)
static const int8_t tx_power_escalation[] = {-12, -4, 0, 4, 8}; 
#define NUM_POWER_LEVELS (sizeof(tx_power_escalation) / sizeof(int8_t))

// ========== Network Addressing ========== //
#define CORAL_PAN_ID           0xCC01
#define ADDR_GROUND_STATION    0x0001
#define ADDR_SATELLITE         0x0002

// TODO: configure based on which board this is!!!
#define MY_SHORT_ADDR          ADDR_GROUND_STATION 
#define TARGET_SHORT_ADDR      ADDR_SATELLITE
//

static int radio_sock = -1;
static struct sockaddr_ll target_sll = {0};

// ========== Data Structures ========== //
// Queue for incoming transmission requests
K_MSGQ_DEFINE(radio_tx_queue, sizeof(tx_cmd_buff_t), RADIO_QUEUE_SIZE, 4);

// recieve command queue from com_main
extern struct k_msgq rx_cmd_queue;

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
    

    while (1) {
        // Check if we have an active message awaiting ACK
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

        // Poll for new messages 
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
                
                // Send 
                zsock_sendto(radio_sock, 
                            pending_msg.payload.data, 
                            pending_msg.payload.end_index, 
                            0, 
                            (const struct sockaddr *)&target_sll, 
                            sizeof(target_sll));
                pending_msg.last_tx_time = k_uptime_get_32();
            }
        }
    }
}
K_THREAD_DEFINE(radio_tid, 2048, radio_thread_entry, NULL, NULL, NULL, 6, 0, 0);

void radio_rx_thread_entry(void *p1, void *p2, void *p3) {
    uint8_t rx_buffer[128]; 
    rx_cmd_buff_t radio_rx_tab = {.size = CMD_MAX_LEN, .route_id = COM, .bus_msg_id = 0};

    while (1) {
        if (radio_sock < 0) {
            k_msleep(100);
            continue;
        }
        // Blocking read on the radio socket
        int len = zsock_recv(radio_sock, rx_buffer, sizeof(rx_buffer), 0);
        if (len > 0) {
            // Feed the incoming bytes into the TAB parser
            for (int i = 0; i < len; i++) {
                push_rx_cmd_buff(&radio_rx_tab, rx_buffer[i]);
                // When a complete TAB message is formed
                if (radio_rx_tab.state == RX_CMD_BUFF_STATE_COMPLETE) {
                    
                    // IS THIS AN ACK TO OUR PENDING MESSAGE?
                    if (pending_msg.active && 
                        radio_rx_tab.data[OPCODE_INDEX] == COMMON_ACK_OPCODE) {
                        uint16_t incoming_id = (radio_rx_tab.data[MSG_ID_MSB_INDEX] << 8) | 
                                                radio_rx_tab.data[MSG_ID_LSB_INDEX];
                        uint16_t pending_id = (pending_msg.payload.data[MSG_ID_MSB_INDEX] << 8) | 
                                            pending_msg.payload.data[MSG_ID_LSB_INDEX];
                        if (incoming_id == pending_id) {
                            // Queue cleared! The ground station heard us.
                            pending_msg.active = 0;
                        }
                    }
                    // Pass the complete message back to the main app thread for routing
                    k_msgq_put(&rx_cmd_queue, &radio_rx_tab, K_NO_WAIT);
                    clear_rx_cmd_buff(&radio_rx_tab);
                }
            }
        }
    }
}
K_THREAD_DEFINE(radio_rx_tid, 2048, radio_rx_thread_entry, NULL, NULL, NULL, 6, 0, 0);

// ========== Public API ========== //

void init_radio(void) {
    // 1. Existing FEM GPIO Setup
    if(device_is_ready(fem_pdn.port)) {
        gpio_pin_configure_dt(&fem_pdn, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_tx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_rx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_mode, GPIO_OUTPUT_INACTIVE); 
    }

    struct net_if *iface = net_if_get_ieee802154();
    if (!iface) {
        printk("Error: No IEEE 802.15.4 interface found.\n");
        return;
    }

    // 2. Configure Hardware Filtering (PAN ID & Short Address)
    uint16_t pan_id = sys_cpu_to_le16(CORAL_PAN_ID);
    uint16_t short_addr = sys_cpu_to_le16(MY_SHORT_ADDR);

    net_mgmt(NET_REQUEST_IEEE802154_SET_PAN_ID, iface, &pan_id, sizeof(pan_id));
    net_mgmt(NET_REQUEST_IEEE802154_SET_SHORT_ADDR, iface, &short_addr, sizeof(short_addr));
    
    // Optional: Set a specific channel (11-26)
    uint16_t channel = 26; 
    net_mgmt(NET_REQUEST_IEEE802154_SET_CHANNEL, iface, &channel, sizeof(channel));

    // 3. Open DGRAM Socket (Zephyr handles the MAC header)
    radio_sock = zsock_socket(AF_PACKET, SOCK_DGRAM, ETH_P_IEEE802154);
    if (radio_sock < 0) {
        printk("Failed to create radio socket\n");
        return;
    }

    // 4. Pre-compute the target destination address for the TX thread
    target_sll.sll_family = AF_PACKET;
    target_sll.sll_ifindex = net_if_get_by_iface(iface);
    target_sll.sll_halen = 2; // 2 bytes for Short Address
    target_sll.sll_addr[0] = (TARGET_SHORT_ADDR >> 8) & 0xFF; // MSB
    target_sll.sll_addr[1] = TARGET_SHORT_ADDR & 0xFF;        // LSB
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
void enable_rf(void) { 
    gpio_pin_set_dt(&fem_pdn, 1); }
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
    gpio_pin_set_dt(&fem_tx_en, 0); }
void enable_rx(void) { 
    gpio_pin_set_dt(&fem_tx_en, 0); 
    gpio_pin_set_dt(&fem_rx_en, 1); 
}
void disable_rx(void) { 
    gpio_pin_set_dt(&fem_rx_en, 0); }