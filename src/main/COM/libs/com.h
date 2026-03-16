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

// Raw MMIO and pin definitions removed for Zephyr compatibility



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