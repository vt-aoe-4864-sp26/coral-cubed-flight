// com.h
// COM board support header file
//
// Written by Bradley Denby
// Other contributors: None
//
// See the top-level LICENSE file for the license.

#ifndef COM_H
#define COM_H

// TAB header
#include <tab.h> // common_data_t, rx_cmd_buff_t, tx_cmd_buff_t

// ========== Macros ========== //

// ========== Ports
#define P0                  (GPIO_BASE)
#define P1_BASE             (0x50000300)
#define P1_PIN_CNF(N)       (*(volatile uint32_t *)(P1_BASE + 0x700 + 0x4 * (N)))
#define P1_OUTSET           (*(volatile uint32_t *)(P1_BASE + 0x508))
#define P1_OUTCLR           (*(volatile uint32_t *)(P1_BASE + 0x50C))


// ========== Pin Definitions
#define RF_FRONTEND_PIN GPIO9
#define TX_EN_PIN       GPIO3
#define RX_EN_PIN       GPIO2
#define LED1            GPIO30
#define LED2            GPIO29

// ========== Byte counts
#define BYTES_PER_BLR_PLD    ((uint32_t)128)
#define BYTES_PER_FLASH_PAGE ((uint32_t)4096)

// ========== Addresses
// Application Start Page
#define APP_ADDR   ((uint32_t)0x00008000U)
// SRAM1 start address
#define SRAM1_BASE ((uint32_t)0x20000000U)
// SRAM1 size
#define SRAM1_SIZE ((uint32_t)0x00020000U)

// ========== RADIO Commands Not Def'd by LibOpen

#define RADIO_SHORTS_READY_START             (1 << 0)
#define RADIO_SHORTS_END_DISABLE             (1 << 1)
#define RADIO_SHORTS_DISABLED_TXEN           (1 << 2)
#define RADIO_SHORTS_DISABLED_RXEN           (1 << 3)
#define RADIO_SHORTS_ADDRESS_RSSISTART       (1 << 4)
#define RADIO_SHORTS_END_START               (1 << 5)
#define RADIO_SHORTS_ADDRESS_BCSTART         (1 << 6)
#define RADIO_SHORTS_DISABLED_RSSISTOP       (1 << 8)
#define RADIO_SHORTS_RXREADY_CCASTART        (1 << 11)
#define RADIO_SHORTS_CCAIDLE_TXEN            (1 << 12)
#define RADIO_SHORTS_CCABUSY_DISABLE         (1 << 13)
#define RADIO_SHORTS_FRAMESTART_BCSTART      (1 << 14)
#define RADIO_SHORTS_CCAIDLE_STOP            (1 << 17)
#define RADIO_SHORTS_TXREADY_START           (1 << 18)
#define RADIO_SHORTS_RXREADY_START           (1 << 19)
#define RADIO_SHORTS_PHYEND_DISABLE          (1 << 20)

#define RADIO_CCACTRL                MMIO32(RADIO_BASE + 0x66C)
#define RADIO_RXADDRESS              MMIO32(RADIO_BASE + 0x530)
#define RADIO_CRCSTATUS              MMIO32(RADIO_BASE + 0x400)
#define RADIO_EDSAMPLE               MMIO32(RADIO_BASE + 0x668)
#define RADIO_EDCNT                  MMIO32(RADIO_BASE + 0x664)

// Radio EVENTS and TASKS 
#define RADIO_EVENT_READY            MMIO32(RADIO_BASE + 0x100)
#define RADIO_EVENT_ADDRESS          MMIO32(RADIO_BASE + 0x104)
#define RADIO_EVENT_PAYLOAD          MMIO32(RADIO_BASE + 0x108)
#define RADIO_EVENT_END              MMIO32(RADIO_BASE + 0x10C)
#define RADIO_EVENT_DISABLED         MMIO32(RADIO_BASE + 0x110)
#define RADIO_EVENT_DEVMATCH         MMIO32(RADIO_BASE + 0x114)
#define RADIO_EVENT_DEVMISS          MMIO32(RADIO_BASE + 0x118)
#define RADIO_EVENT_RSSIEND          MMIO32(RADIO_BASE + 0x11C)
#define RADIO_EVENT_FRAMESTART       MMIO32(RADIO_BASE + 0x138)
#define RADIO_EVENT_EDEND            MMIO32(RADIO_BASE + 0x13C)
#define RADIO_EVENT_CCAIDLE          MMIO32(RADIO_BASE + 0x144)
#define RADIO_EVENT_CCABUSY          MMIO32(RADIO_BASE + 0x148)
#define RADIO_EVENT_TXREADY          MMIO32(RADIO_BASE + 0x154)
#define RADIO_EVENT_RXREADY          MMIO32(RADIO_BASE + 0x158)
#define RADIO_EVENT_PHYEND           MMIO32(RADIO_BASE + 0x16C)
#define RADIO_EVENT_BCMATCH          MMIO32(RADIO_BASE + 0x128)
#define RADIO_EVENT_CRCOK            MMIO32(RADIO_BASE + 0x130)
#define RADIO_EVENT_CRCERROR         MMIO32(RADIO_BASE + 0x134)

#define RADIO_TASK_TXEN              MMIO32(RADIO_BASE + 0x000)
#define RADIO_TASK_RXEN              MMIO32(RADIO_BASE + 0x004)
#define RADIO_TASK_START             MMIO32(RADIO_BASE + 0x008)
#define RADIO_TASK_STOP              MMIO32(RADIO_BASE + 0x00C)
#define RADIO_TASK_DISABLE           MMIO32(RADIO_BASE + 0x010)
#define RADIO_TASK_RSSISTART         MMIO32(RADIO_BASE + 0x014)
#define RADIO_TASK_RSSISTOP          MMIO32(RADIO_BASE + 0x018)
#define RADIO_TASK_BCSTART           MMIO32(RADIO_BASE + 0x01C)
#define RADIO_TASK_BCSTOP            MMIO32(RADIO_BASE + 0x020)
#define RADIO_TASK_EDSTART           MMIO32(RADIO_BASE + 0x024)
#define RADIO_TASK_EDSTOP            MMIO32(RADIO_BASE + 0x028)
#define RADIO_TASK_CCASTART          MMIO32(RADIO_BASE + 0x02C)



// ========== UART common_data handles ========== //
#define VAR_CODE_ALIVE               ((uint8_t)0x00)
#define VAR_CODE_COM_EN             ((uint8_t)0x01)
#define VAR_CODE_PAY_EN             ((uint8_t)0x02)

// ========== COM Commands
#define VAR_CODE_RF_EN              ((uint8_t)0x03)
#define VAR_CODE_RF_TX              ((uint8_t)0x04)
#define VAR_CODE_RF_RX              ((uint8_t)0x05)
#define VAR_CODE_BLINK_CDH         ((uint8_t)0x06)
#define VAR_CODE_BLINK_COM         ((uint8_t)0x07)

// ========== Payload Commands
#define VAR_CODE_CORAL_WAKE      ((uint8_t)0x08)
#define VAR_CODE_CORAL_CAM_ON    ((uint8_t)0x09)
#define VAR_CODE_CORAL_INFER      ((uint8_t)0x0a)

// ========== Testing & Debug
#define VAR_CODE_RUN_DEMO       ((uint8_t)0x0b)

// ========== Common_Data_Vars
#define VAR_ENABLE                 ((uint8_t)0x01)
#define VAR_DISABLE                ((uint8_t)0x02)

// ========== Functions Required by TAB/OpenLST Protocols ========== // 

int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);


// ========== Board Initialization ========== //

void init_clock(void);
void init_leds(void);
void init_uart(void);
void init_gpio(void);
void init_radio(void);

// ========== COM Functions ========== //


void init_radio_tx_test(void);
void blast_noise(void);
void blast_carrier(void);
void enable_rf(void);
void disable_rf(void);
void enable_rx(void);
void enable_tx(void);
void cdh_enable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void cdh_disable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void com_blink_demo(void);
void cdh_blink_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);

// ========== RADIO ========== //

uint8_t sample_ed(void);
void tx_cmd_buff_config(tx_cmd_buff_t* buff, uint8_t msg_id);
void tx_radio(uint8_t* pckt, size_t length);
void rx_radio(uint8_t* pckt, size_t length);
void test_tx(void);


// ========== General UART ========== //

void rx_uart(rx_cmd_buff_t* rx_cmd_buff_o);
void reply(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o);
void tx_uart(tx_cmd_buff_t* tx_cmd_buff_o);

// ========== Utility functions  ========== //

void run_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void flash_erase_page(uint32_t page);
int handle_bootloader_erase(void);
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_jump(void);
int bootloader_active(void);
void radio_set_rx_address(uint8_t addr_index);

#endif