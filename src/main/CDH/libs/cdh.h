// cdh.h
// CDH board support header file
//
// Written by Jack Rathert
// Other contributors: Bradley Denby, Chad Taylor, Alok Anand, Jack Rathert
//
// See the top-level LICENSE file for the license.

#ifndef CDH_H
#define CDH_H

// TAB header //
#include <tab.h>

// Macros //

// ========== Pin Definitions ========== //

#define LED1                GPIO2   // PB
#define LED2                GPIO1   // PB
#define COM_EN_PIN          GPIO6   // PC
#define PAY_EN_PIN          GPIO7   // PC
#define EXT_GONE            GPIO15  // PA

// ========== Byte counts
#define BYTES_PER_BLR_PLD    ((uint32_t)128)
#define BYTES_PER_FLASH_PAGE ((uint32_t)2048)

// ========== Start of application address space
#define APP_ADDR   ((uint32_t)0x08008000U)

// ========== SRAM1 start address
#define SRAM1_BASE ((uint32_t)0x20000000U)

// ========== SRAM1 size
#define SRAM1_SIZE ((uint32_t)0x00040000U)


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
int handle_bootloader_erase(void);
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_jump(void);
int bootloader_active(void);

// ========== Board Initialization ========== //

void init_clock(void);
void init_leds(void);
void init_uart(void);
void init_gpio(void);

// ========== CDH Functions ========== //

void init_com(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
// void init_rf(void);
// void init_pay(void);


void rx_usart1(rx_cmd_buff_t* rx_cmd_buff_o);
void reply(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o);
void tx_usart1(tx_cmd_buff_t* tx_cmd_buff_o);

#endif
