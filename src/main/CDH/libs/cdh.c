// cdh.c
// CDH board support implementation file
//
// Written by Bradley Denby
// Other contributors: Chad Taylor, Alok Anand, Jack Rathert
//
// See the top-level LICENSE file for the license.

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include <cdh.h> 
#include <tab.h> 

// ========== Aliasing ========== //
const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static const struct gpio_dt_spec com_en_pin = GPIO_DT_SPEC_GET(DT_ALIAS(com_en), gpios);
static const struct gpio_dt_spec pay_en_pin = GPIO_DT_SPEC_GET(DT_ALIAS(pay_en), gpios);

extern const struct device *uart_gnd_dev;
extern const struct device *uart_com_dev;
extern const struct device *uart_pay_dev;
extern const struct device *uart_ext_dev;

// ========== Concurrency ========== //
static struct k_work blink_demo_work;

K_SEM_DEFINE(com_awake_semaphore, 0, 1)

// ========== Workqueue Handling ========== //

static void blink_demo_handler(struct k_work *work) {
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
    gpio_pin_set_dt(&led1, 1);
    gpio_pin_set_dt(&led2, 0);
}

// ========== Tab Handling ========== //

int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff) {
  if(common_data_buff_i.end_index < 2) return 0; 

  uint8_t var_code = common_data_buff_i.data[0];
  uint8_t var_len  = common_data_buff_i.data[1];

  if(common_data_buff_i.end_index < (size_t)(2 + var_len)) return 0; 

  uint8_t* val_ptr = &common_data_buff_i.data[2];

  switch(var_code) {
    case VAR_CODE_ALIVE:
      switch(*val_ptr){ 
        case 0x01: return 1;
        default: return 0;
      }
      break;

    case VAR_CODE_COM_EN:
      switch(*val_ptr){ 
        case VAR_ENABLE: power_on_com(); return 1;
        case VAR_DISABLE: power_off_com(); return 1;
        default: return 0;
      }
      break;

    case VAR_CODE_PAY_EN:
      switch(*val_ptr){ 
        case VAR_ENABLE: power_on_pay(); return 1;
        case VAR_DISABLE: power_off_pay(); return 1;
        default: return 0;              
      } 
      break;

    case VAR_CODE_RF_EN:
      switch(*val_ptr){ 
        case VAR_ENABLE: com_enable_rf(rx_cmd_buff, tx_cmd_buff); return 1;
        case VAR_DISABLE: com_disable_rf(rx_cmd_buff, tx_cmd_buff); return 1;
      }
      break;

    case VAR_CODE_RF_TX: 
      switch(*val_ptr){ 
        case VAR_ENABLE: com_enable_tx(rx_cmd_buff, tx_cmd_buff); return 1;
        case VAR_DISABLE: com_disable_tx(rx_cmd_buff, tx_cmd_buff); return 1;
        default: return 0;
      }

    case VAR_CODE_RF_RX:
      switch(*val_ptr){ 
        case VAR_ENABLE: com_enable_rx(rx_cmd_buff, tx_cmd_buff); return 1;
        case VAR_DISABLE: com_disable_rx(rx_cmd_buff, tx_cmd_buff); return 1;
        default: return 0;
      }

    case VAR_CODE_BLINK_CDH:
      switch(*val_ptr){
        case VAR_ENABLE: cdh_blink_demo(); return 1;
        case VAR_DISABLE: return 1;
      }

    case VAR_CODE_CORAL_WAKE: break;
    case VAR_CODE_CORAL_CAM_ON: break;
    case VAR_CODE_CORAL_INFER: break;

    default: return 0;
  }
  return 0;
}

// ========== Board initialization functions ========== //

void init_clock(void) {} 

void init_leds(void) {
    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    k_work_init(&blink_demo_work, blink_demo_handler);
}

void init_uart(void) {} 

void init_gpio(void){
  gpio_pin_configure_dt(&pay_en_pin, GPIO_OUTPUT_INACTIVE);
  gpio_pin_configure_dt(&com_en_pin, GPIO_OUTPUT_INACTIVE);
}

// ========== UART Communication functions ========== //

// NEW Router Function - Replaces reply()
void process_rx_packet(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o) {
  if(rx_cmd_buff_o->state == RX_CMD_BUFF_STATE_COMPLETE && tx_cmd_buff_o->empty) {
    
    // Extract destination ID from the high nibble
    uint8_t dest_id = (rx_cmd_buff_o->data[ROUTE_INDEX] & 0xF0) >> 4;

    // If it's for us, handle it natively.
    if (dest_id == CDH) {
      write_reply(rx_cmd_buff_o, tx_cmd_buff_o);
    } 
    // Otherwise, we act as a transparent router (e.g., GND -> CDH -> PLD)
    else {
      for(size_t i = 0; i < rx_cmd_buff_o->end_index; i++) {
        tx_cmd_buff_o->data[i] = rx_cmd_buff_o->data[i];
      }
      tx_cmd_buff_o->start_index = 0;
      tx_cmd_buff_o->end_index = rx_cmd_buff_o->end_index;
      tx_cmd_buff_o->empty = 0;
      
      clear_rx_cmd_buff(rx_cmd_buff_o);
    }
  }
}

void route_tx_packet(tx_cmd_buff_t* tx_cmd_buff_o) {
  if (tx_cmd_buff_o->empty) return;

  uint8_t dest_id = (tx_cmd_buff_o->data[ROUTE_INDEX] & 0xF0) >> 4;
  const struct device *target_dev = NULL;

  switch (dest_id) {
      case GND: target_dev = uart_gnd_dev; break; // (Assuming alias points to COM via devicetree)
      case COM: target_dev = uart_com_dev; break; 
      case PLD: target_dev = uart_pay_dev; break; 
      case CDH: target_dev = uart_ext_dev; break; 
      default:  return; 
  }

  if (target_dev != NULL && device_is_ready(target_dev)) {
      while(!(tx_cmd_buff_o->empty)) {
          uint8_t b = pop_tx_cmd_buff(tx_cmd_buff_o);
          uart_poll_out(target_dev, b);
      }
  } else {
      clear_tx_cmd_buff(tx_cmd_buff_o); 
  }
}

// ========== GPIO functions ========== //

void power_on_com(){ gpio_pin_set_dt(&com_en_pin, 1); }
void power_off_com(){ gpio_pin_set_dt(&com_en_pin, 0); }
void power_on_pay(){ gpio_pin_set_dt(&pay_en_pin, 1); }
void power_off_pay(){ gpio_pin_set_dt(&pay_en_pin, 0); }

void cdh_blink_demo(void){ k_work_submit(&blink_demo_work); }

// ========== UART Commands to COM ========== //

void check_com_online(void) {
    tx_cmd_buff_t local_tx = {.size=CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_tx);
    rx_cmd_buff_t dummy_rx = {.route_id = CDH, .bus_msg_id = 0};
    
    while(1) {
        gpio_pin_set_dt(&led2, 1); 

        uint8_t my_payload[] = {VAR_CODE_ALIVE, 0x01, 0x01};
        msg_to_com(&dummy_rx, &local_tx, COMMON_DATA_OPCODE, my_payload, 3);
        route_tx_packet(&local_tx); 

        if (k_sem_take(&com_awake_semaphore, K_MSEC(500)) == 0) {
            for(int i = 0; i < 6; i++) {
                gpio_pin_toggle_dt(&led1);
                gpio_pin_toggle_dt(&led2);
                k_msleep(50);
            }
            break; 
        }
        
        gpio_pin_set_dt(&led2, 0); 
        k_msleep(500); 
    }
    
    gpio_pin_set_dt(&led1, 0);
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

int handle_bootloader_erase(void){ return 1; }
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff){ return 1; }
int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff){ return 1; }
int handle_bootloader_jump(void){ return 0; }
int bootloader_active(void) { return 1; }