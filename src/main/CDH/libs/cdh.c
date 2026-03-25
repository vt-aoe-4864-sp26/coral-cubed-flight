// cdh.c
// CDH board support implementation file
//
// Written by Bradley Denby
// Other contributors: Chad Taylor, Alok Anand, Jack Rathert
//
// See the top-level LICENSE file for the license.

// Standard library headers
#include <stdint.h>                 // uint8_t
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include <cdh.h>                    // CDH header
#include <tab.h>                    // TAB header


// ========== Aliasing ========== //

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec com_en_pin = GPIO_DT_SPEC_GET(DT_ALIAS(com_en), gpios);
static const struct gpio_dt_spec pay_en_pin = GPIO_DT_SPEC_GET(DT_ALIAS(pay_en), gpios);
const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(usart1));

// ========== Concurrency ========== //

K_SEM_DEFINE(com_awake_sem, 0, 1)

// ========== Tab Handling ========== //

// This function utilizes the OpenLST HANDLE_COMMON_DATA Opcode
// It expects to recieve a payload of the form [var_code var_len val_ptr]
// These can be utilized to save things from the UART buffer and execute different onboard
// behevaiors. Define new VAR_CODEs in cdh.h
int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff) {
  // need at least type and length bytes
  if(common_data_buff_i.end_index < 2) {
    return 0; 
  }

  uint8_t var_code = common_data_buff_i.data[0];
  uint8_t var_len  = common_data_buff_i.data[1];

  // make sure we actually received all the bytes the length byte claims
  if(common_data_buff_i.end_index < (size_t)(2 + var_len)) {
    return 0; 
  }

  // pointer to the start of the value payload
  uint8_t* val_ptr = &common_data_buff_i.data[2];

  switch(var_code) {
      case VAR_CODE_ALIVE:
      switch(*val_ptr){ 
        case 0x01:
          return 1;

        default:
          return 0;
      }
      break;

    case VAR_CODE_COM_EN:
      // val_ptr=1 drives ic high on eps to enable power to com pcb
      switch(*val_ptr){ 
        
        case VAR_ENABLE: // told to power on com
          power_on_com();
          return 1;

        case VAR_DISABLE: // told to power off com
          power_off_com();
          return 1;

        default:
          return 0;
      }
      break;

    case VAR_CODE_PAY_EN:
      switch(*val_ptr){ 
        case VAR_ENABLE: 
          power_on_pay();
          return 1;

        case VAR_DISABLE: 
          power_off_pay();
          return 1;

        default:
          return 0;              
      } 
      break;

    case VAR_CODE_RF_EN:
      // val_ptr=1 to passthrough cdh to com to enable rf frontend
      switch(*val_ptr){ 
          
        case VAR_ENABLE: {
          
          com_enable_rf(rx_cmd_buff, tx_cmd_buff);
          return 1;
        }

        case VAR_DISABLE: {
          com_disable_rf(rx_cmd_buff, tx_cmd_buff);
          return 1;
        }
      }
      break;

    case VAR_CODE_RF_TX: 
      switch(*val_ptr){ 
          
        case VAR_ENABLE: {
          com_enable_tx(rx_cmd_buff, tx_cmd_buff);
          return 1;
        }

        case VAR_DISABLE: {
          com_disable_tx(rx_cmd_buff, tx_cmd_buff);
          return 1;
        }
        default:
          return 0;
      }


    case VAR_CODE_RF_RX:
    switch(*val_ptr){ 
          
        case VAR_ENABLE: {
          com_enable_rx(rx_cmd_buff, tx_cmd_buff);
          return 1;
        }

        case VAR_DISABLE: {
          com_disable_rx(rx_cmd_buff, tx_cmd_buff);
          return 1;
        }
      
      default:
        return 0;
    }

    case VAR_CODE_BLINK_CDH:
    switch(*val_ptr){

      case VAR_ENABLE: {
          cdh_blink_demo();
          return 1;
        }

        case VAR_DISABLE: {
          
          return 1;
        }
    }

    case VAR_CODE_CORAL_WAKE:
      break;

    case VAR_CODE_CORAL_CAM_ON:
      break;

    case VAR_CODE_CORAL_INFER:
      break;

    default:
      return 0;
    }
      return 0;
}


// ========== Board initialization functions ========== //

void init_clock(void) {
  // Clock is initialized by Zephyr devicetree
}

void init_leds(void) {
  gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
  gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
}

void init_uart(void) {
  if (!device_is_ready(uart_dev)) {
    return;
  }
}

void init_gpio(void){
  gpio_pin_configure_dt(&pay_en_pin, GPIO_OUTPUT_INACTIVE);
  gpio_pin_configure_dt(&com_en_pin, GPIO_OUTPUT_INACTIVE);
}

// ========== UART Communication functions ========== //

void rx_usart1(rx_cmd_buff_t* rx_cmd_buff_o) {
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
  if(                                                  // if
   rx_cmd_buff_o->state==RX_CMD_BUFF_STATE_COMPLETE && // rx_cmd is valid AND
   tx_cmd_buff_o->empty                                // tx_cmd is empty
  ) {                                                  //
    write_reply(rx_cmd_buff_o, tx_cmd_buff_o);         // execute cmd and reply
  }                                                    //
}

void tx_usart1(tx_cmd_buff_t* tx_cmd_buff_o) {
  while(!(tx_cmd_buff_o->empty)) {
    uint8_t b = pop_tx_cmd_buff(tx_cmd_buff_o);
    uart_poll_out(uart_dev, b);
    if(tx_cmd_buff_o->empty) {
      gpio_pin_toggle_dt(&led1);
      gpio_pin_toggle_dt(&led2);
    }
  }
}

// ========== GPIO functions ========== //
void power_on_com(){
  gpio_pin_set_dt(&com_en_pin, 1); // power on COM
}

void power_off_com(){
  gpio_pin_set_dt(&com_en_pin, 0); // power off COM
}

void power_on_pay(){
  gpio_pin_set_dt(&pay_en_pin, 1); // power on PAY
}

void power_off_pay(){
  gpio_pin_set_dt(&pay_en_pin, 0); // power off PAY
}

void cdh_blink_demo(void){
  // blink for 15 seconds (slow)
  for(int k = 0; k < 20; k++) {
    k_msleep(250);
    gpio_pin_toggle_dt(&led1);
    gpio_pin_toggle_dt(&led2);
  }

  // faster blink for 15 seconds (fast)
  for(int k = 0; k < 20; k++) {
    k_msleep(100);
    gpio_pin_toggle_dt(&led1);
    gpio_pin_toggle_dt(&led2);
  }  
}


// ========== UART Commands to COM ========== //

void check_com_online(void) {
    tx_cmd_buff_t local_tx = {.size=CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_tx);
    
    // Create a dummy RX buffer to satisfy the TAB protocol's ID tracking
    rx_cmd_buff_t dummy_rx = {.route_id = CDH, .bus_msg_id = 0};
    
    while(1) {
        uint8_t my_payload[] = {VAR_CODE_ALIVE, 0x01, 0x01};
        msg_to_com(&dummy_rx, &local_tx, COMMON_DATA_OPCODE, my_payload, 3);
        tx_usart1(&local_tx);

        if (k_sem_take(&com_awake_sem, K_MSEC(500)) == 0) {
            break; // COM is alive!
        }
        
        gpio_pin_toggle_dt(&led2);
    }
    gpio_pin_set_dt(&led2, 0);
}

void com_enable_rf(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_RF_EN, 0x01, VAR_ENABLE};
  msg_to_com(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void com_disable_rf(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_RF_EN, 0x01, VAR_DISABLE};
  msg_to_com(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void com_enable_rx(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_RF_RX, 0x01, VAR_ENABLE};
  msg_to_com(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void com_disable_rx(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_RF_RX, 0x01, VAR_DISABLE};
  msg_to_com(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void com_enable_tx(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_RF_TX, 0x01, VAR_ENABLE};
  msg_to_com(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void com_disable_tx(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_RF_TX, 0x01, VAR_DISABLE};
  msg_to_com(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void com_start_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_RUN_DEMO, 0x01, VAR_ENABLE};
  msg_to_com(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}


// Bootloader opcode functions: I don't see us using these but left in for clarity - Jack

// This example implementation of handle_bootloader_erase erases the portion of
// Flash accessible to bootloader_write_page
int handle_bootloader_erase(void){ return 1; }
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff){ return 1; }
int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff){ return 1; }
int handle_bootloader_jump(void){ return 0; }
int bootloader_active(void) { return 1; }
