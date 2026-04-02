// cdh.h
// CDH board support header file

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

// ========== UART common_data handles ========== //
#define VAR_CODE_ALIVE          ((uint8_t)0x00)
#define VAR_CODE_COM_EN         ((uint8_t)0x01)
#define VAR_CODE_PAY_EN         ((uint8_t)0x02)

// ========== COM Commands
#define VAR_CODE_RF_EN          ((uint8_t)0x03)
#define VAR_CODE_RF_TX          ((uint8_t)0x04)
#define VAR_CODE_RF_RX          ((uint8_t)0x05)
#define VAR_CODE_BLINK_CDH      ((uint8_t)0x06)
#define VAR_CODE_BLINK_COM      ((uint8_t)0x07)

// ========== Payload Commands
#define VAR_CODE_CORAL_WAKE     ((uint8_t)0x08)
#define VAR_CODE_CORAL_CAM_ON   ((uint8_t)0x09)
#define VAR_CODE_CORAL_INFER    ((uint8_t)0x0a)

// ========== Testing & Debug
#define VAR_CODE_RUN_DEMO       ((uint8_t)0x0b)

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
void init_gpio(void);
void init_hardware_uarts(void);
void init_usb_uart(void);

int  init_usb_console(void);

extern const struct gpio_dt_spec led1;
extern const struct gpio_dt_spec led2;

// ========== Concurrency Variables ========== //
extern struct k_sem com_awake_semaphore;

// ========== CDH Functions ========== //

void power_on_com(void);
void power_off_com(void);
void power_on_pay(void);
void power_off_pay(void);
void cdh_blink_demo(void);
void com_start_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);

// ========== UART Commands to COM ========== //
void check_com_online(void);
void com_enable_rf(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void com_disable_rf(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void com_enable_rx(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void com_disable_rx(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void com_enable_tx(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
void com_disable_tx(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);

void process_rx_packet(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o);
void route_tx_packet(tx_cmd_buff_t* tx_cmd_buff_o);

#endif