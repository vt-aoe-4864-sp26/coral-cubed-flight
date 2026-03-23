#include <stdint.h>                 
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <com.h>                    
#include <tab.h>                    

// ========== Macros ========== //
// Flash memory
#define NVMC_CONFIG MMIO32    (NVMC_BASE + 0x504)
#define NVMC_CONFIG_REN       (0     )
#define NVMC_CONFIG_WEN       (1 << 1)
#define NVMC_CONFIG_EEN       (1 << 2)
#define NVMC_ERASEPAGE MMIO32 (NVMC_BASE + 0x508)
#define NVMC_READY MMIO32     (NVMC_BASE + 0x400)
#define NVMC_READY_BUSY       (0     )

// ========== TAB Handling ========== //
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));

int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff) {
  if(common_data_buff_i.end_index < 2) return 0; 

  uint8_t var_code = common_data_buff_i.data[0];
  uint8_t var_len  = common_data_buff_i.data[1];

  if(common_data_buff_i.end_index < (size_t)(2 + var_len)) return 0; 

  uint8_t* val_ptr = &common_data_buff_i.data[2];

  switch(var_code) {
    case VAR_CODE_ALIVE:
      if (*val_ptr == 0x01) return 1;
      return 0;

    case VAR_CODE_PAY_EN:
      if (*val_ptr == VAR_ENABLE) {
        cdh_enable_pay(rx_cmd_buff, tx_cmd_buff);
        return 1;
      } else if (*val_ptr == VAR_DISABLE) {
        cdh_disable_pay(rx_cmd_buff, tx_cmd_buff);
        return 1;
      }
      return 0;

    case VAR_CODE_RUN_DEMO:
      run_demo(rx_cmd_buff, tx_cmd_buff);
      return 1;



    case VAR_CODE_BLINK_COM:
      com_blink_demo();
      return 1;

    case VAR_CODE_BLINK_CDH:
      cdh_blink_demo(rx_cmd_buff, tx_cmd_buff);
      return 1;
    
    default:
      return 0;
  }
}

// ========== Board initialization functions ========== //
void init_leds(void) {
  gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
  gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
}

void init_uart(void) {
  if (!device_is_ready(uart_dev)) {
    while(1) {
      gpio_pin_toggle_dt(&led1);
      k_msleep(100);
    }
  }
}

void init_gpio(void) {

}

// ========== UART Communication functions ========== //
void rx_uart(rx_cmd_buff_t* rx_cmd_buff_o) {
  while(rx_cmd_buff_o->state != RX_CMD_BUFF_STATE_COMPLETE) {
    uint8_t c;
    if (uart_poll_in(uart_dev, &c) == 0) {
      push_rx_cmd_buff(rx_cmd_buff_o, c);
    } else {
      break;
    }
  }
}

void reply(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o) {
  if(rx_cmd_buff_o->state==RX_CMD_BUFF_STATE_COMPLETE && tx_cmd_buff_o->empty) {                                                  
    write_reply(rx_cmd_buff_o, tx_cmd_buff_o);         
  }                                                    
}

void tx_uart(tx_cmd_buff_t* tx_cmd_buff_o) {
  while(!(tx_cmd_buff_o->empty)) {
    uint8_t b = pop_tx_cmd_buff(tx_cmd_buff_o);
    uart_poll_out(uart_dev, b);
    if(tx_cmd_buff_o->empty) {
      gpio_pin_toggle_dt(&led1);
      gpio_pin_toggle_dt(&led2);
    }
  }
}

// ========== GPIO Functions ========== //

void com_blink_demo(void) {
  for(int k = 0; k < 20; k++) {
    k_msleep(250);
    gpio_pin_toggle_dt(&led1);
    gpio_pin_toggle_dt(&led2);
  }
  for(int k = 0; k < 20; k++) {
    k_msleep(100);
    gpio_pin_toggle_dt(&led1);
    gpio_pin_toggle_dt(&led2);
  }
}

// ========== UART Messages to CDH ========== //
void cdh_enable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_PAY_EN, 0x01, VAR_ENABLE};
  msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void cdh_disable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_PAY_EN, 0x01, VAR_DISABLE};
  msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void cdh_blink_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_BLINK_CDH, 0x01, VAR_ENABLE};
  msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

// ========== Utility functions ========== //
void run_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  k_msleep(1000);
  com_blink_demo();
  k_msleep(1000);
  cdh_blink_demo(rx_cmd_buff, tx_cmd_buff);
  tx_uart(tx_cmd_buff);

  /*
  enable_rf();
  k_msleep(1000);
  enable_rx();
  k_msleep(1000);
  enable_tx();
  k_msleep(1000);
  */

  cdh_enable_pay(rx_cmd_buff, tx_cmd_buff);
  tx_uart(tx_cmd_buff);
}

void flash_erase_page(uint32_t page) {
  NRF_NVMC->CONFIG = NVMC_CONFIG_EEN;
  __asm__("isb 0xF");
  __asm__("dsb 0xF");
  NRF_NVMC->ERASEPAGE = page*(0x00001000);
  while(NRF_NVMC->READY==NVMC_READY_BUSY) {}
  NRF_NVMC->CONFIG = NVMC_CONFIG_REN;
  __asm__("isb 0xF");
  __asm__("dsb 0xF");
}

int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff) { return 1; }
int handle_bootloader_jump(void) { return 0; }
int bootloader_active(void) { return 1; }
int handle_bootloader_erase(void) { return 1; }
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff) { return 1; }