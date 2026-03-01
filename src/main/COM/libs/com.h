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
#define P0              (GPIO_BASE)
#define P1              (GPIO_BASE + 0x300)

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



// ========== UART common_data handles ========== //
#define VAR_CODE_COM_EN         ((uint8_t)0x01)
#define VAR_CODE_PAY_EN         ((uint8_t)0x02)

// ========== COM Commands
#define VAR_CODE_RF_EN          ((uint8_t)0x03)
#define VAR_CODE_RF_TX          ((uint8_t)0x04)
#define VAR_CODE_RF_RX          ((uint8_t)0x05)

// ========== Payload Commands
#define VAR_CODE_CORAL_WAKE     ((uint8_t)0x06)
#define VAR_CODE_CORAL_CAM_ON   ((uint8_t)0x07)
#define VAR_CODE_CORAL_INFER    ((uint8_t)0x08)

// ========== Common_Data_Vars
#define VAR_ENABLE              ((uint8_t)0x01)
#define VAR_DISABLE             ((uint8_t)0x02)

// ========== Functions Required by TAB/OpenLST Protocols ========== // 

int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);


// ========== Board Initialization ========== //

void init_clock(void);
void init_leds(void);
void init_uart(void);

// ========== COM Functions ========== //

void init_radio_tx_test(void);
void blast_noise(void);

// ========== General UART ========== //

void rx_uart(rx_cmd_buff_t* rx_cmd_buff_o);
void reply(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o);
void tx_uart(tx_cmd_buff_t* tx_cmd_buff_o);

// ========== Utility functions  ========== //

void flash_erase_page(uint32_t page);
int handle_bootloader_erase(void);
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_jump(void);
int bootloader_active(void);

#endif