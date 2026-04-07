// radio.c
// radio support package for Satellite COM
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
#include <errno.h>
#include "radio.h"
#include "tab.h"
#include "com.h"

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

// ========== Configuration ========== //
#define MAX_RETRIES 15
#define RETRY_TIMEOUT_MS 750
#define RADIO_QUEUE_SIZE 10

// nRF52833 Supported TX Power levels (escalation path)
static const int8_t tx_power_escalation[] = {-12, -4, 0, 4, 8};
#define NUM_POWER_LEVELS (sizeof(tx_power_escalation) / sizeof(int8_t))

// ========== Network Addressing ========== //
#define CORAL_PAN_ID 0xCC01
#define ADDR_GROUND_STATION 0x0001
#define ADDR_SATELLITE 0x0002

#if CURRENT_BOARD_ROLE == ROLE_GROUND_STATION
#define MY_SHORT_ADDR ADDR_GROUND_STATION
#define TARGET_SHORT_ADDR ADDR_SATELLITE
#else
#define MY_SHORT_ADDR ADDR_SATELLITE
#define TARGET_SHORT_ADDR ADDR_GROUND_STATION
#endif

#define RADIO_UDP_PORT 4242
#define RADIO_MULTICAST_IPV6 "ff02::1"

static int radio_rx_sock = -1;
static int radio_tx_sock = -1;
static struct sockaddr_in6 target_addr = {0};

// ========== Data Structures ========== //
K_MSGQ_DEFINE(radio_tx_queue, sizeof(tx_cmd_buff_t), RADIO_QUEUE_SIZE, 4);
extern struct k_msgq rx_cmd_queue; // Receive command queue from com_main

typedef struct
{
    int active;
    uint8_t retries;
    int8_t current_power_idx;
    uint32_t last_tx_time;
    tx_cmd_buff_t payload;
} pending_tx_t;

static pending_tx_t pending_msg = {.payload = {.size = CMD_MAX_LEN}};

// Mutex to prevent race conditions between RX and TX threads on pending_msg
K_MUTEX_DEFINE(pending_msg_mutex);

// ========== Hardware Aliases ========== //
#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)
static const struct gpio_dt_spec fem_tx_en = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, fem_tx_en_gpios);
static const struct gpio_dt_spec fem_rx_en = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, fem_rx_en_gpios);
static const struct gpio_dt_spec fem_pdn = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, fem_pdn_gpios);
static const struct gpio_dt_spec fem_mode = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, fem_mode_gpios);

// ========== Hardware Controls ========== //
void enable_rf(void) { gpio_pin_set_dt(&fem_pdn, 1); }

void disable_rf(void)
{
    gpio_pin_set_dt(&fem_pdn, 0);
    gpio_pin_set_dt(&fem_tx_en, 0);
    gpio_pin_set_dt(&fem_rx_en, 0);
}

void enable_tx(void)
{
    gpio_pin_set_dt(&fem_rx_en, 0);
    k_busy_wait(20); // 20us guard band to prevent shoot-through
    gpio_pin_set_dt(&fem_tx_en, 1);
}

void disable_tx(void) { gpio_pin_set_dt(&fem_tx_en, 0); }

void enable_rx(void)
{
    gpio_pin_set_dt(&fem_tx_en, 0);
    k_busy_wait(20); // 20us guard band to prevent shoot-through
    gpio_pin_set_dt(&fem_rx_en, 1);
}

void disable_rx(void) { gpio_pin_set_dt(&fem_rx_en, 0); }

// ========== TX Execution Helper ========== //
static void execute_radio_tx(void)
{
    enable_tx();
    gpio_pin_set_dt(&led1, 1);
    k_busy_wait(30);

    int sent_bytes = zsock_sendto(radio_tx_sock,
                                pending_msg.payload.data,
                                pending_msg.payload.end_index,
                                ZSOCK_MSG_DONTWAIT,
                                (const struct sockaddr *)&target_addr,
                                sizeof(target_addr));

    if (sent_bytes < 0)
    {
        int specific_error = errno;
        disable_tx();
        while (1)
        {
            for (int i = 0; i < specific_error; i++)
            {
                gpio_pin_set_dt(&led1, 1);
                k_busy_wait(200000);
                gpio_pin_set_dt(&led1, 0);
                k_busy_wait(200000);
            }
            k_msleep(2000);
        }
    }

    k_msleep(15);
    gpio_pin_set_dt(&led1, 0);
    enable_rx();
}

// ========== Threads ========== //
void radio_thread_entry(void *p1, void *p2, void *p3)
{
    tx_cmd_buff_t new_tx;
    struct net_if *iface = net_if_get_ieee802154();

    while (1)
    {
        k_mutex_lock(&pending_msg_mutex, K_FOREVER);
        int is_active = pending_msg.active;
        k_mutex_unlock(&pending_msg_mutex);

        if (is_active)
        {
            uint32_t now = k_uptime_get_32();

            if ((now - pending_msg.last_tx_time) > RETRY_TIMEOUT_MS)
            {
                k_mutex_lock(&pending_msg_mutex, K_FOREVER);

                if (pending_msg.active)
                {
                    pending_msg.retries++;

                    if (pending_msg.retries >= MAX_RETRIES)
                    {
                        pending_msg.active = 0;
                    }
                    else
                    {
                        if (pending_msg.current_power_idx < (NUM_POWER_LEVELS - 1))
                        {
                            pending_msg.current_power_idx++;
                            int16_t new_pwr = tx_power_escalation[pending_msg.current_power_idx];
                            net_mgmt(NET_REQUEST_IEEE802154_SET_TX_POWER, iface, &new_pwr, sizeof(new_pwr));
                        }

                        execute_radio_tx();
                        pending_msg.last_tx_time = k_uptime_get_32();
                    }
                }
                k_mutex_unlock(&pending_msg_mutex);
            }
            k_msleep(10);
            continue;
        }

        if (k_msgq_get(&radio_tx_queue, &new_tx, K_FOREVER) == 0)
        {
            k_mutex_lock(&pending_msg_mutex, K_FOREVER);

            if (!pending_msg.active)
            {
                pending_msg.active = 1;
                pending_msg.retries = 0;
                pending_msg.current_power_idx = 0;

                int16_t base_pwr = tx_power_escalation[0];
                net_mgmt(NET_REQUEST_IEEE802154_SET_TX_POWER, iface, &base_pwr, sizeof(base_pwr));

                pending_msg.payload.empty = new_tx.empty;
                pending_msg.payload.start_index = new_tx.start_index;
                pending_msg.payload.end_index = new_tx.end_index;
                memcpy(pending_msg.payload.data, new_tx.data, new_tx.end_index);

                execute_radio_tx();
                pending_msg.last_tx_time = k_uptime_get_32();
            }
            k_mutex_unlock(&pending_msg_mutex);
        }
    }
}
K_THREAD_DEFINE(radio_tid, 2048, radio_thread_entry, NULL, NULL, NULL, 6, 0, 0);

void radio_rx_thread_entry(void *p1, void *p2, void *p3)
{
    uint8_t rx_buffer[128];
    rx_cmd_buff_t radio_rx_tab = {.size = CMD_MAX_LEN, .route_id = COM, .bus_msg_id = 0};

    while (1)
    {
        if (radio_rx_sock < 0)
        {
            k_msleep(100);
            continue;
        }

        // Non-blocking socket read
        int len = zsock_recv(radio_rx_sock, rx_buffer, sizeof(rx_buffer), ZSOCK_MSG_DONTWAIT);

        // no data available right now (returns negative error code, usually -EAGAIN).
        // Yield the thread to let the TX thread acquire the socket lock if it's waiting.
        if (len < 0)
        {
            k_msleep(5);
            continue;
        }

        // Data received! Process the buffer.
        if (len > 0)
        {
            gpio_pin_toggle_dt(&led1);
            for (int i = 0; i < len; i++)
            {
                push_rx_cmd_buff(&radio_rx_tab, rx_buffer[i]);

                if (radio_rx_tab.state == RX_CMD_BUFF_STATE_COMPLETE)
                {

                    // --- Loopback Prevention ---
                    uint8_t src_id = radio_rx_tab.data[ROUTE_INDEX] & 0x0F;
                    int is_loopback = 0;
#if CURRENT_BOARD_ROLE == ROLE_GROUND_STATION
                    if (src_id == GND || src_id == COMG)
                        is_loopback = 1;
#else
                    if (src_id == COM || src_id == CDH || src_id == PLD)
                        is_loopback = 1;
#endif
                    if (is_loopback)
                    {
                        clear_rx_cmd_buff(&radio_rx_tab);
                        continue;
                    }
                    // ---------------------------

                    k_mutex_lock(&pending_msg_mutex, K_FOREVER);
                    if (pending_msg.active &&
                        radio_rx_tab.data[OPCODE_INDEX] == COMMON_ACK_OPCODE)
                    {
                        uint16_t incoming_id = (radio_rx_tab.data[MSG_ID_MSB_INDEX] << 8) |
                                               radio_rx_tab.data[MSG_ID_LSB_INDEX];
                        uint16_t pending_id = (pending_msg.payload.data[MSG_ID_MSB_INDEX] << 8) |
                                              pending_msg.payload.data[MSG_ID_LSB_INDEX];
                        if (incoming_id == pending_id)
                        {
                            pending_msg.active = 0;
                        }
                    }
                    k_mutex_unlock(&pending_msg_mutex);

                    k_msgq_put(&rx_cmd_queue, &radio_rx_tab, K_NO_WAIT);
                    clear_rx_cmd_buff(&radio_rx_tab);
                }
            }
        }
    }
}
K_THREAD_DEFINE(radio_rx_tid, 2048, radio_rx_thread_entry, NULL, NULL, NULL, 6, 0, 0);

// ========== Public API ========== //

void init_radio(void)
{
    if (device_is_ready(fem_pdn.port))
    {
        gpio_pin_configure_dt(&fem_tx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_rx_en, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&fem_pdn, GPIO_OUTPUT_ACTIVE);
        gpio_pin_configure_dt(&fem_mode, GPIO_OUTPUT_ACTIVE);

        enable_rx(); // Explicitly force RX mode on boot
    }

    // --- SETUP RX SOCKET (UDP IPv6) ---
    radio_rx_sock = zsock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (radio_rx_sock < 0)
    {
        while (1)
        {
            gpio_pin_toggle_dt(&led1);
            k_msleep(100);
        }
    }

    struct sockaddr_in6 rx_addr = {0};
    rx_addr.sin6_family = AF_INET6;
    rx_addr.sin6_port = htons(RADIO_UDP_PORT);
    rx_addr.sin6_addr = in6addr_any;

    if (zsock_bind(radio_rx_sock, (struct sockaddr *)&rx_addr, sizeof(rx_addr)) < 0)
    {
        while (1)
        {
            gpio_pin_toggle_dt(&led1);
            k_msleep(500);
        }
    }

    // Join the Multicast Group so we actually listen to the ether
    struct ipv6_mreq mreq = {0};
    zsock_inet_pton(AF_INET6, RADIO_MULTICAST_IPV6, &mreq.ipv6mr_multiaddr);
    mreq.ipv6mr_ifindex = 0;
    zsock_setsockopt(radio_rx_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq));

    // --- SETUP TX SOCKET (UDP IPv6) ---
    radio_tx_sock = zsock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (radio_tx_sock < 0)
    {
        while (1)
        {
            gpio_pin_toggle_dt(&led2);
            k_msleep(100);
        }
    }

    struct zsock_timeval tv = {.tv_sec = 0, .tv_usec = 50000};
    (void)zsock_setsockopt(radio_tx_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // Prepare the TX Struct target
    target_addr.sin6_family = AF_INET6;
    target_addr.sin6_port = htons(RADIO_UDP_PORT);
    zsock_inet_pton(AF_INET6, RADIO_MULTICAST_IPV6, &target_addr.sin6_addr);
}

int radio_send_packet(tx_cmd_buff_t *tx_buff)
{
    gpio_pin_toggle_dt(&led2);
    return k_msgq_put(&radio_tx_queue, tx_buff, K_NO_WAIT);
}

// ========== Diagnostics ========== //

void radio_blast_cw_max_power(void)
{
    disable_rf();

    NRF_RADIO->TASKS_DISABLE = 1;
    while (NRF_RADIO->EVENTS_DISABLED == 0)
    {
    }

    NRF_RADIO->SHORTS = 0;
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Pos8dBm << RADIO_TXPOWER_TXPOWER_Pos);
    NRF_RADIO->FREQUENCY = 40;
    NRF_RADIO->MODE = (RADIO_MODE_MODE_Ieee802154_250Kbit << RADIO_MODE_MODE_Pos);

    enable_rf();
    enable_tx();

    NRF_RADIO->TASKS_TXEN = 1;
    while (NRF_RADIO->EVENTS_READY == 0)
    {
    }

    k_sleep(K_SECONDS(60));

    NRF_RADIO->TASKS_DISABLE = 1;
    disable_tx();
    disable_rf();
}