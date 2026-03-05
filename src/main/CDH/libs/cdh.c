// cdh.c
// CDH board support implementation file
//
// Written by Bradley Denby
// Other contributors: Chad Taylor, Alok Anand, Jack Rathert
//
// See the top-level LICENSE file for the license.

// Standard library headers
#include <stdint.h>                 // uint8_t
#include <libopencm3/stm32/flash.h> // used in init_clock
#include <libopencm3/stm32/gpio.h>  // used in init_gpio
#include <libopencm3/stm32/rcc.h>   // used in init_clock, init_rtc
#include <libopencm3/stm32/usart.h> // used in init_uart
#include <cdh.h>                    // CDH header
#include <tab.h>                    // TAB header

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
  rcc_osc_on(RCC_HSI16);                    // 16 MHz internal RC oscillator
  rcc_wait_for_osc_ready(RCC_HSI16);        // Wait until oscillator is ready
  rcc_set_sysclk_source(RCC_CFGR_SW_HSI16); // Sets sysclk source for RTOS
  rcc_set_hpre(RCC_CFGR_HPRE_NODIV);        // AHB at 80 MHz (80 MHz max.)
  rcc_set_ppre1(RCC_CFGR_PPRE_DIV2);        // APB1 at 40 MHz (80 MHz max.)
  rcc_set_ppre2(RCC_CFGR_PPRE_NODIV);       // APB2 at 80 MHz (80 MHz max.)
  //flash_prefetch_enable();                  // Enable instr prefetch buffer
  flash_set_ws(FLASH_ACR_LATENCY_4WS);      // RM0351: 4 WS for 80 MHz
  //flash_dcache_enable();                    // Enable data cache
  //flash_icache_enable();                    // Enable instruction cache
  rcc_set_main_pll(                         // Setup 80 MHz clock
   RCC_PLLCFGR_PLLSRC_HSI16,                // PLL clock source
   4,                                       // PLL VCO division factor
   40,                                      // PLL VCO multiplication factor
   0,                                       // PLL P clk output division factor
   0,                                       // PLL Q clk output division factor
   RCC_PLLCFGR_PLLR_DIV2                    // PLL sysclk output division factor
  ); // 16MHz/4 = 4MHz; 4MHz*40 = 160MHz VCO; 160MHz/2 = 80MHz PLL
  rcc_osc_on(RCC_PLL);                      // 80 MHz PLL
  rcc_wait_for_osc_ready(RCC_PLL);          // Wait until PLL is ready
  rcc_set_sysclk_source(RCC_CFGR_SW_PLL);   // Sets sysclk source for RTOS
  rcc_wait_for_sysclk_status(RCC_PLL);
  rcc_ahb_frequency = 80000000;
  rcc_apb1_frequency = 40000000;
  rcc_apb2_frequency = 80000000;
}

void init_leds(void) {
  // inits both LEDs, defaults LED1 on
  rcc_periph_clock_enable(RCC_GPIOB);
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED1);
  gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED2);
  gpio_set(GPIOB, LED1);
  gpio_clear(GPIOB, LED2);
}

void init_uart(void) {
  // Enable peripheral clocks
  rcc_periph_reset_pulse(RST_USART1);
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_USART1);

  // Setup UART modes
  gpio_mode_setup(GPIOA,GPIO_MODE_AF,GPIO_PUPD_NONE,GPIO9|GPIO10);
  gpio_set_af(GPIOA,GPIO_AF7,GPIO9);  // USART1_TX is alternate function 7
  gpio_set_af(GPIOA,GPIO_AF7,GPIO10); // USART1_RX is alternate function 7

  // === DEFAULT BAUD TO 115200 === //
  usart_set_baudrate(USART1,115200);
  usart_set_databits(USART1,8);
  usart_set_stopbits(USART1,USART_STOPBITS_1);
  usart_set_mode(USART1,USART_MODE_TX_RX);
  usart_set_parity(USART1,USART_PARITY_NONE);
  usart_set_flow_control(USART1,USART_FLOWCONTROL_NONE);
  usart_enable(USART1);

}

void init_gpio(void){
  // Open Port C // 
  rcc_periph_clock_enable(RCC_GPIOC);

    // GPIO Initialization //
  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PAY_EN_PIN);
  gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, COM_EN_PIN);

  // Default Low //
  gpio_clear(GPIOC, PAY_EN_PIN);
  gpio_clear(GPIOC, COM_EN_PIN);
}

// ========== UART Communication functions ========== //

void rx_usart1(rx_cmd_buff_t* rx_cmd_buff_o) {
  while(                                             // while
   usart_get_flag(USART1,USART_ISR_RXNE) &&          //  USART1 RX not empty AND
   rx_cmd_buff_o->state!=RX_CMD_BUFF_STATE_COMPLETE  //  Command not complete
  ) {                                                //
    uint8_t b = usart_recv(USART1);                  // Receive byte from RX pin
    push_rx_cmd_buff(rx_cmd_buff_o, b);              // Push byte to buffer
  }                                                  //
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
  while(                                             // while
   usart_get_flag(USART1,USART_ISR_TXE) &&           //  USART1 TX empty AND
   !(tx_cmd_buff_o->empty)                           //  TX buffer not empty
  ) {                                                //
    uint8_t b = pop_tx_cmd_buff(tx_cmd_buff_o);      // Pop byte from TX buffer
    usart_send(USART1,b);                            // Send byte to TX pin
    if(tx_cmd_buff_o->empty) {                       // if TX buffer empty
      gpio_toggle(GPIOB, GPIO2);                     //  Toggle LED1
      gpio_toggle(GPIOB, GPIO1);                     //  Toggle LED2
    }                                                //
  }                                                  //
}

// ========== GPIO functions ========== //
void power_on_com(){
  gpio_set(GPIOC, COM_EN_PIN); // power on COM
}

void power_off_com(){
  gpio_clear(GPIOC, COM_EN_PIN); // power off COM
}

void power_on_pay(){
  gpio_set(GPIOC, PAY_EN_PIN); // power on PAY
}

void power_off_pay(){
  gpio_clear(GPIOC, PAY_EN_PIN); // power off PAY
}

void cdh_blink_demo(void){
    // blink for 15 seconds (slow)
  for(int k = 0; k < 20; k++) {
    for(int i = 0; i < 4000000; i++) {
      __asm__("nop");
    }
    gpio_toggle(GPIOB, LED1);
    gpio_toggle(GPIOB, LED2);
  }

  // faster blink for 15 seconds (fast)
  for(int k = 0; k < 20; k++) {
    for(int i = 0; i < 2000000; i++) {
      __asm__("nop");
    }
    gpio_toggle(GPIOB, LED1);
    gpio_toggle(GPIOB, LED2);
  }  
}


// ========== UART Commands to COM ========== //

void check_com_online(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff) {
  int com_awake = 0;
  
  while(!com_awake) {
    // send request for acknowledgement
    uint8_t my_payload[] = {VAR_CODE_ALIVE, 0x01, 0x01};
    msg_to_com(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
    tx_usart1(tx_cmd_buff);

    for(int i = 0; i < 500000; i++) {
      rx_usart1(rx_cmd_buff);
      
      if(rx_cmd_buff->state == RX_CMD_BUFF_STATE_COMPLETE) {
        // listen for the ack that com auto-generates on success
        if(rx_cmd_buff->data[OPCODE_INDEX] == COMMON_ACK_OPCODE) {
          com_awake = 1;
          break; // com is alive, exit delay loop
        }
        clear_rx_cmd_buff(rx_cmd_buff); // clear noise from bad response
      }
    }

    gpio_toggle(GPIOB, LED2);
  }
  
  // clean buffer
  clear_rx_cmd_buff(rx_cmd_buff);
  gpio_clear(GPIOB, LED2);
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




// Bootloader opcode functions: I don't see us using these but left in for clarity - Jack

// This example implementation of handle_bootloader_erase erases the portion of
// Flash accessible to bootloader_write_page
int handle_bootloader_erase(void){
  flash_unlock();
  for(size_t subpage_id=0; subpage_id<255; subpage_id++) {
    // subpage_id==0x00 writes to APP_ADDR==0x08008000 i.e. start of page 16
    // So subpage_id==0x10 writes to addr 0x08008800 i.e. start of page 17 etc
    // Need to erase page once before writing inside of it
    if((subpage_id*BYTES_PER_BLR_PLD)%BYTES_PER_FLASH_PAGE==0) {
      flash_erase_page(16+(subpage_id*BYTES_PER_BLR_PLD)/BYTES_PER_FLASH_PAGE);
      flash_clear_status_flags();
    }
  }
  flash_lock();
  return 1;
}

// This example implementation of handle_bootloader_write_page writes 128 bytes 
// of data to a region of memory indexed by the "page number" parameter (the
// "sub-page ID").
int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff){
  if(
   rx_cmd_buff->state==RX_CMD_BUFF_STATE_COMPLETE &&
   rx_cmd_buff->data[OPCODE_INDEX]==BOOTLOADER_WRITE_PAGE_OPCODE
  ) {
    flash_unlock();
    uint32_t subpage_id = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX]);
    // subpage_id==0x00 writes to APP_ADDR==0x08008000 i.e. start of page 16
    // So subpage_id==0x10 writes to addr 0x08008800 i.e. start of page 17 etc
    // Need to erase page once before writing inside of it
    if((subpage_id*BYTES_PER_BLR_PLD)%BYTES_PER_FLASH_PAGE==0) {
      flash_erase_page(16+(subpage_id*BYTES_PER_BLR_PLD)/BYTES_PER_FLASH_PAGE);
      flash_clear_status_flags();
    }
    // write data
    uint32_t start_addr = APP_ADDR+subpage_id*BYTES_PER_BLR_PLD;
    for(size_t i=0; i<BYTES_PER_BLR_PLD; i+=8) {
      uint64_t dword = *(uint64_t*)((rx_cmd_buff->data)+PLD_START_INDEX+1+i);
      flash_wait_for_last_operation();
      FLASH_CR |= FLASH_CR_PG;
      MMIO32(i+start_addr)   = (uint32_t)(dword);
      MMIO32(i+start_addr+4) = (uint32_t)(dword >> 32);
      flash_wait_for_last_operation();
      FLASH_CR &= ~FLASH_CR_PG;
      flash_clear_status_flags();
    }
    flash_lock();
    return 1;
  } else {
    return 0;
  }
}

// This example implementation of bootloader_write_page_addr32 writes 128 bytes
// of data to a region of memory beginning at the start address
int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff){
  if (
   rx_cmd_buff->state==RX_CMD_BUFF_STATE_COMPLETE &&
   rx_cmd_buff->data[OPCODE_INDEX]==BOOTLOADER_WRITE_PAGE_ADDR32_OPCODE
  ) {
    flash_unlock();
    uint32_t addr_1 = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX]);
    uint32_t addr_2 = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX+1]);
    uint32_t addr_3 = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX+2]);
    uint32_t addr_4 = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX+3]);
    uint32_t start_addr = (addr_1<<24)|(addr_2<<16)|(addr_3<<8)|(addr_4<<0);
    // write data
    for(size_t i=0; i<BYTES_PER_BLR_PLD; i+=8) {
      // APP_ADDR==0x08008000 corresponds to the start of Flash page 16, and
      // 0x08008800 corresponds to the start of Flash page 17, etc.
      // Need to erase page once before writing inside of it
      // Check for every new dword since write_addr32 need not be page-aligned
      if((i+start_addr)%BYTES_PER_FLASH_PAGE==0) {
        flash_erase_page((i+start_addr)/BYTES_PER_FLASH_PAGE);
        flash_clear_status_flags();
      }
      uint64_t dword = *(uint64_t*)((rx_cmd_buff->data)+PLD_START_INDEX+4+i);
      flash_wait_for_last_operation();
      FLASH_CR |= FLASH_CR_PG;
      MMIO32(i+start_addr)   = (uint32_t)(dword);
      MMIO32(i+start_addr+4) = (uint32_t)(dword >> 32);
      flash_wait_for_last_operation();
      FLASH_CR &= ~FLASH_CR_PG;
      flash_clear_status_flags();
    }
    flash_lock();
    return 1;
  } else {
    return 0;
  }
}

// This example implementation of handle_bootloader_jump returns 0 because the
// cdh_monolithic example program does not allow execution of user applications
int handle_bootloader_jump(void){
  return 0;
}

// This example implementation of bootloader_active always returns 1 because the
// cdh_monolithic example program does not allow execution of user applications
int bootloader_active(void) {
  return 1;
}
