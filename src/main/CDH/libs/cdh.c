// cdh.c
// CDH board support implementation file

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include <cdh.h> 
#include <tab.h> 

// ========== OS Hooks ========== //
extern struct k_msgq rx_cmd_queue; 

// ========== Hardware Aliasing ========== //
const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static const struct gpio_dt_spec com_en_pin = GPIO_DT_SPEC_GET(DT_ALIAS(com_en), gpios);
static const struct gpio_dt_spec pay_en_pin = GPIO_DT_SPEC_GET(DT_ALIAS(pay_en), gpios);

const struct device *uart_gnd_dev = DEVICE_DT_GET(DT_ALIAS(uart_0)); 
const struct device *uart_com_dev = DEVICE_DT_GET(DT_ALIAS(uart_1)); 
const struct device *uart_ext_dev = DEVICE_DT_GET(DT_ALIAS(uart_2)); 
const struct device *uart_pay_dev = DEVICE_DT_GET(DT_ALIAS(uart_3)); 

// ========== Concurrency Primitives ========== //
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

// ========== UART Pipeline Configuration ========== //
typedef struct {
    const struct device *dev;
    rx_cmd_buff_t rx_buff;
} uart_lane_ctx_t;

static uart_lane_ctx_t uart_lanes[4] = {
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = CDH, .bus_msg_id = 0} },
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = CDH, .bus_msg_id = 0} }, 
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = CDH, .bus_msg_id = 0} },
    { .dev = NULL, .rx_buff = {.size = CMD_MAX_LEN, .route_id = CDH, .bus_msg_id = 0} }  
};

static void generic_uart_callback(const struct device *dev, void *user_data) {
    uart_lane_ctx_t *ctx = (uart_lane_ctx_t *)user_data;
    if (!uart_irq_update(dev)) return;

    if (uart_irq_rx_ready(dev)) {
        uint8_t c;
        while (uart_fifo_read(dev, &c, 1) == 1) {
            push_rx_cmd_buff(&ctx->rx_buff, c);
            if (ctx->rx_buff.state == RX_CMD_BUFF_STATE_COMPLETE) {
                k_msgq_put(&rx_cmd_queue, &ctx->rx_buff, K_NO_WAIT);
                clear_rx_cmd_buff(&ctx->rx_buff); 
            }
        }
    }
}

// ========== Hardware Init ========== //
void init_clock(void) {} 

void init_leds(void) {
    if (device_is_ready(led1.port)) {
        gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
        gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    }
    k_work_init(&blink_demo_work, blink_demo_handler);
}

void init_gpio(void){
  gpio_pin_configure_dt(&pay_en_pin, GPIO_OUTPUT_INACTIVE);
  gpio_pin_configure_dt(&com_en_pin, GPIO_OUTPUT_INACTIVE);
}

int init_usb_console(void) {
    if (device_is_ready(console_dev)) {
        return usb_enable(NULL); 
    }
    return -1; // Failure
}

void init_hardware_uarts(void) {
    uart_lanes[1].dev = uart_com_dev;
    uart_lanes[2].dev = uart_pay_dev;
    uart_lanes[3].dev = uart_ext_dev;

    for (int i = 1; i < 4; i++) {
        uart_lane_ctx_t *ctx = &uart_lanes[i];
        clear_rx_cmd_buff(&ctx->rx_buff);

        if (ctx->dev != NULL && device_is_ready(ctx->dev)) {
            uart_irq_callback_user_data_set(ctx->dev, generic_uart_callback, ctx);
            uart_irq_rx_enable(ctx->dev);
            printk("Hardware UART %d Bound and Ready.\n", i);
        } else {
            printk("WARNING: Hardware UART %d Failed to Bind!\n", i);
        }
    }
}

void init_usb_uart(void) {
    uart_lanes[0].dev = uart_gnd_dev;
    uart_lane_ctx_t *ctx = &uart_lanes[0];
    clear_rx_cmd_buff(&ctx->rx_buff);

    if (ctx->dev != NULL && device_is_ready(ctx->dev)) {
        uart_irq_callback_user_data_set(ctx->dev, generic_uart_callback, ctx);
        uart_irq_rx_enable(ctx->dev);
    }
}

// ========== Routing Logic ========== //
void process_rx_packet(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o) {
  if(rx_cmd_buff_o->state == RX_CMD_BUFF_STATE_COMPLETE && tx_cmd_buff_o->empty) {
    uint8_t dest_id = (rx_cmd_buff_o->data[ROUTE_INDEX] & 0xF0) >> 4;

    if (dest_id == CDH) {
      write_reply(rx_cmd_buff_o, tx_cmd_buff_o);
    } else {
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
      case GND: target_dev = uart_com_dev; break; 
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

// ========== Tab Handling ========== //
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

    case VAR_CODE_COM_EN:
      if (*val_ptr == VAR_ENABLE) power_on_com();
      else if (*val_ptr == VAR_DISABLE) power_off_com();
      return 1;

    case VAR_CODE_PAY_EN:
      if (*val_ptr == VAR_ENABLE) power_on_pay();
      else if (*val_ptr == VAR_DISABLE) power_off_pay();
      return 1;

    case VAR_CODE_CORAL_WAKE:
      cdh_coral_wake(*val_ptr);
      return 1;
    case VAR_CODE_CORAL_CAM_ON:
      cdh_coral_cam_on(*val_ptr);
      return 1;
    case VAR_CODE_CORAL_INFER:
      cdh_coral_infer(*val_ptr);
      return 1;

    case VAR_CODE_RF_EN:
      if (*val_ptr == VAR_ENABLE) com_enable_rf(rx_cmd_buff, tx_cmd_buff);
      else if (*val_ptr == VAR_DISABLE) com_disable_rf(rx_cmd_buff, tx_cmd_buff);
      return 1;

    case VAR_CODE_RF_TX: 
      if (*val_ptr == VAR_ENABLE) com_enable_tx(rx_cmd_buff, tx_cmd_buff);
      else if (*val_ptr == VAR_DISABLE) com_disable_tx(rx_cmd_buff, tx_cmd_buff);
      return 1;

    case VAR_CODE_RF_RX:
      if (*val_ptr == VAR_ENABLE) com_enable_rx(rx_cmd_buff, tx_cmd_buff);
      else if (*val_ptr == VAR_DISABLE) com_disable_rx(rx_cmd_buff, tx_cmd_buff);
      return 1;

    case VAR_CODE_BLINK_CDH:
      if (*val_ptr == VAR_ENABLE) cdh_blink_demo();
      return 1;
    
    

    default: return 0;
  }
  return 0;
}

// ========== GPIO functions ========== //
void power_on_com(){ gpio_pin_set_dt(&com_en_pin, 1); }
void power_off_com(){ gpio_pin_set_dt(&com_en_pin, 0); }
void power_on_pay(){ gpio_pin_set_dt(&pay_en_pin, 1); }
void power_off_pay(){ gpio_pin_set_dt(&pay_en_pin, 0); }

void cdh_blink_demo(void){ k_work_submit(&blink_demo_work); }

// ========== UART Commands to Payload (Coral) ========== //
void cdh_coral_wake(uint8_t val){
    tx_cmd_buff_t local_tx = {.size=CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_tx);
    rx_cmd_buff_t dummy_rx = {.route_id = CDH, .bus_msg_id = 0};

    uint8_t my_payload[] = {VAR_CODE_CORAL_WAKE, 0x01, val};
    msg_to_pay(&dummy_rx, &local_tx, COMMON_DATA_OPCODE, my_payload, 3);
    route_tx_packet(&local_tx);
}

void cdh_coral_cam_on(uint8_t val){
    tx_cmd_buff_t local_tx = {.size=CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_tx);
    rx_cmd_buff_t dummy_rx = {.route_id = CDH, .bus_msg_id = 0};

    uint8_t my_payload[] = {VAR_CODE_CORAL_CAM_ON, 0x01, val};
    msg_to_pay(&dummy_rx, &local_tx, COMMON_DATA_OPCODE, my_payload, 3);
    route_tx_packet(&local_tx);
}

void cdh_coral_infer(uint8_t val){
    tx_cmd_buff_t local_tx = {.size=CMD_MAX_LEN};
    clear_tx_cmd_buff(&local_tx);
    rx_cmd_buff_t dummy_rx = {.route_id = CDH, .bus_msg_id = 0};

    uint8_t my_payload[] = {VAR_CODE_CORAL_INFER, 0x01, val};
    msg_to_pay(&dummy_rx, &local_tx, COMMON_DATA_OPCODE, my_payload, 3);
    route_tx_packet(&local_tx);
}

// ========== UART Commands to COM ========== //
void check_com_online(void) {
    static tx_cmd_buff_t local_tx = {.size=CMD_MAX_LEN};
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

// ========== Bootloader Stubs ========== //
int handle_bootloader_erase(void){ return 1; }
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff){ return 1; }
int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff){ return 1; }
int handle_bootloader_jump(void){ return 0; }
int bootloader_active(void) { return 1; }