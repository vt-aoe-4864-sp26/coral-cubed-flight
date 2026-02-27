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

// Macros
//// Pin Definitions
#define RF_FRONTEND_PIN GPIO9
#define TX_EN_PIN       GPIO3
#define RX_EN_PIN       GPIO2


//// Byte counts
#define BYTES_PER_BLR_PLD    ((uint32_t)128)
#define BYTES_PER_FLASH_PAGE ((uint32_t)4096)

//// Start of application address space
#define APP_ADDR   ((uint32_t)0x00008000U)

//// SRAM1 start address
#define SRAM1_BASE ((uint32_t)0x20000000U)

//// SRAM1 size
#define SRAM1_SIZE ((uint32_t)0x00020000U)

// Utility functions

void flash_erase_page(uint32_t page);

// Functions required by TAB

int handle_common_data(common_data_t common_data_buff_i);
int handle_bootloader_erase(void);
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_jump(void);
int bootloader_active(void);

// Board initialization functions

void init_clock(void);
void init_leds(void);
void init_uart(void);

// Feature functions

void rx_uart(rx_cmd_buff_t* rx_cmd_buff_o);
void reply(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o);
void tx_uart(tx_cmd_buff_t* tx_cmd_buff_o);

#endif
