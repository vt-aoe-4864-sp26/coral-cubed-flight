// com.c
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

// ========== Hardware Aliases ========== //
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

#define FEM_NODE DT_NODELABEL(nrf_radio_fem)
static const struct gpio_dt_spec fem_tx_en = GPIO_DT_SPEC_GET(FEM_NODE, tx_en_gpios);
static const struct gpio_dt_spec fem_rx_en = GPIO_DT_SPEC_GET(FEM_NODE, rx_en_gpios);
static const struct gpio_dt_spec fem_pdn   = GPIO_DT_SPEC_GET(FEM_NODE, pdn_gpios);
static const struct gpio_dt_spec fem_mode  = GPIO_DT_SPEC_GET(FEM_NODE, mode_gpios);

extern const struct device *uart_gnd_dev;
extern const struct device *uart_cdh_dev;

// ========== Concurrency Objects ========== //
static struct k_work blink_demo_work;
static struct k_work run_demo_work;

int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff) {
  if(common_data_buff_i.end_index < 2) return 0; 
  uint8_t var_code = common_data_buff_i.data[0];
  uint8_t var_len  = common_data_buff_i.data[1];
  if(common_data_buff_i.end_index < (size_t)(2 + var_len)) return 0; 

  uint8_t* val_ptr = &common_data_buff_i.data[2];

  switch(var_code) {
    case VAR_CODE_ALIVE:
      if (*val_ptr == 0x01){
        gpio_pin_set_dt(&led2, 1); 
        return 1;
      } 
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
      k_work_submit(&run_demo_work);
      return 1;

    case VAR_CODE_BLINK_COM:
      k_work_submit(&blink_demo_work);
      return 1;

    case VAR_CODE_RF_EN:
      if (*val_ptr == VAR_ENABLE) enable_rf();
      else if (*val_ptr == VAR_DISABLE) disable_rf();
      return 1;

    case VAR_CODE_RF_TX:
      if (*val_ptr == VAR_ENABLE) enable_tx();
      else if (*val_ptr == VAR_DISABLE) disable_tx();
      return 1;

    case VAR_CODE_RF_RX:
      if (*val_ptr == VAR_ENABLE) enable_rx();
      else if (*val_ptr == VAR_DISABLE) disable_rx();
      return 1;
    
    default: return 0;
  }
}

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
  gpio_pin_set_dt(&led1, 0);
  gpio_pin_set_dt(&led2, 0);
}

static void run_demo_handler(struct k_work *work) {
  tx_cmd_buff_t demo_tx = {.size=CMD_MAX_LEN};
  clear_tx_cmd_buff(&demo_tx);
  rx_cmd_buff_t dummy_rx = {.route_id = COM, .bus_msg_id = 0};

  k_work_submit(&blink_demo_work); 
  k_msleep(1000); 

  cdh_blink_demo(&dummy_rx, &demo_tx);
  route_tx_packet(&demo_tx);

  k_msleep(1000);

  enable_rf();
  k_msleep(1000);
  enable_rx();
  k_msleep(1000);
  enable_tx();
  k_msleep(1000);
  disable_tx();
  disable_rf();

  cdh_enable_pay(&dummy_rx, &demo_tx);
  route_tx_packet(&demo_tx);
}

void init_leds(void) {
  gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
  gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
  
  k_work_init(&blink_demo_work, blink_demo_handler);
  k_work_init(&run_demo_work, run_demo_handler);
}

void init_gpio(void) {
  if(device_is_ready(fem_pdn.port)) {
    gpio_pin_configure_dt(&fem_pdn, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&fem_tx_en, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&fem_rx_en, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&fem_mode, GPIO_OUTPUT_INACTIVE); 
  }
}

void enable_rf(void) { gpio_pin_set_dt(&fem_pdn, 1); }
void disable_rf(void) { 
  gpio_pin_set_dt(&fem_pdn, 0); 
  gpio_pin_set_dt(&fem_tx_en, 0); 
  gpio_pin_set_dt(&fem_rx_en, 0); 
}

void enable_tx(void) { 
  gpio_pin_set_dt(&fem_rx_en, 0); 
  gpio_pin_set_dt(&fem_tx_en, 1); 
}
void disable_tx(void) { gpio_pin_set_dt(&fem_tx_en, 0); }

void enable_rx(void) { 
  gpio_pin_set_dt(&fem_tx_en, 0); 
  gpio_pin_set_dt(&fem_rx_en, 1); 
}
void disable_rx(void) { gpio_pin_set_dt(&fem_rx_en, 0); }

// ========== UART Routing ========== //

// NEW Router Function - Replaces reply()
void process_rx_packet(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o) {
  if(rx_cmd_buff_o->state == RX_CMD_BUFF_STATE_COMPLETE && tx_cmd_buff_o->empty) {
    
    // Extract destination ID from the high nibble
    uint8_t dest_id = (rx_cmd_buff_o->data[ROUTE_INDEX] & 0xF0) >> 4;

    // If it's for us, handle it natively.
    if (dest_id == COM) {
      write_reply(rx_cmd_buff_o, tx_cmd_buff_o);
    } 
    // Otherwise, we are a transparent router. Copy bytes exactly as they arrived.
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
      case GND: target_dev = uart_gnd_dev; break; 
      case CDH: target_dev = uart_cdh_dev; break; 
      case PLD: target_dev = uart_cdh_dev; break; // PLD is routed through CDH from here
      default:  return; 
  }

  if (target_dev != NULL && device_is_ready(target_dev)) {
      while(!(tx_cmd_buff_o->empty)) {
          uint8_t b = pop_tx_cmd_buff(tx_cmd_buff_o);
          uart_poll_out(target_dev, b);
      }
      gpio_pin_toggle_dt(&led1);
  } else {
      clear_tx_cmd_buff(tx_cmd_buff_o); 
  }
}

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

void run_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  k_work_submit(&run_demo_work); 
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