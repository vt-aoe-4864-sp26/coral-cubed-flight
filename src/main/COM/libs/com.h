#ifndef COM_H
#define COM_H

#include <tab.h> 

#define VAR_CODE_ALIVE             ((uint8_t)0x00)
#define VAR_CODE_COM_EN            ((uint8_t)0x01)
#define VAR_CODE_PAY_EN            ((uint8_t)0x02)
#define VAR_CODE_RF_EN             ((uint8_t)0x03)
#define VAR_CODE_RF_TX             ((uint8_t)0x04)
#define VAR_CODE_RF_RX             ((uint8_t)0x05)
#define VAR_CODE_BLINK_CDH         ((uint8_t)0x06)
#define VAR_CODE_BLINK_COM         ((uint8_t)0x07)
#define VAR_CODE_CORAL_WAKE        ((uint8_t)0x08)
#define VAR_CODE_CORAL_CAM_ON      ((uint8_t)0x09)
#define VAR_CODE_CORAL_INFER       ((uint8_t)0x0a)
#define VAR_CODE_RUN_DEMO          ((uint8_t)0x0b)
#define VAR_ENABLE                 ((uint8_t)0x01)
#define VAR_DISABLE                ((uint8_t)0x02)

int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);

void init_leds(void);
void init_uart(void);
void init_gpio(void);

void enable_rf(void);
void disable_rf(void);
void enable_rx(void);
void enable_tx(void);
void cdh_enable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void cdh_disable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void com_blink_demo(void);
void cdh_blink_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);

void rx_uart(rx_cmd_buff_t* rx_cmd_buff_o);
void reply(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o);
void tx_uart(tx_cmd_buff_t* tx_cmd_buff_o);

void run_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void flash_erase_page(uint32_t page);
int handle_bootloader_erase(void);
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff);
int handle_bootloader_jump(void);
int bootloader_active(void);

#endif