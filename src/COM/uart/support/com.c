// com.c
// COM board support implementation file
//
// Written by Bradley Denby
// Other contributors: None
//
// See the top-level LICENSE file for the license.

// Standard library headers
#include <stdint.h>                 // uint8_t

// libopencm3 library
#include <libopencm3/nrf/clock.h> // used in init_clock
#include <libopencm3/nrf/gpio.h>  // used in init_gpio
#include <libopencm3/nrf/uart.h>  // used in init_uart

// Board-specific header
#include <com.h>                    // COM header

// TAB header
#include <tab.h>                    // TAB header

// Macros

//// GPIO Port 0
#define P0 (GPIO_BASE)

//// Flash memory
#define NVMC_CONFIG MMIO32(NVMC_BASE + 0x504)
#define NVMC_CONFIG_REN (0     )
#define NVMC_CONFIG_WEN (1 << 1)
#define NVMC_CONFIG_EEN (1 << 2)
#define NVMC_ERASEPAGE MMIO32(NVMC_BASE + 0x508)
#define NVMC_READY MMIO32(NVMC_BASE + 0x400)
#define NVMC_READY_BUSY (0     )

// Utility functions

void flash_erase_page(uint32_t page) {
  // Enable erase
  NVMC_CONFIG = NVMC_CONFIG_EEN;
  __asm__("isb 0xF");
  __asm__("dsb 0xF");
  // Erase the page
  NVMC_ERASEPAGE = page*(0x00001000);
  // Wait for erase to complete
  while(NVMC_READY==NVMC_READY_BUSY) {}
  // Enable read-only mode
  NVMC_CONFIG = NVMC_CONFIG_REN;
  __asm__("isb 0xF");
  __asm__("dsb 0xF");
}

// Functions required by TAB

// This example implementation of handle_common_data checks whether the bytes
// are strictly increasing, i.e. each subsequent byte is strictly greater than
// the previous byte
int handle_common_data(common_data_t common_data_buff_i) {
  int strictly_increasing = 1;
  uint8_t prev_byte = common_data_buff_i.data[0];
  for(size_t i=1; i<common_data_buff_i.end_index; i++) {
    if(prev_byte>=common_data_buff_i.data[i]) {
      strictly_increasing = 0;
      i = common_data_buff_i.end_index;
    } else {
      prev_byte = common_data_buff_i.data[i];
    }
  }
  return strictly_increasing;
}

// This example implementation of handle_bootloader_erase erases the portion of
// Flash accessible to bootloader_write_page
int handle_bootloader_erase(void){
  for(size_t subpage_id=0; subpage_id<255; subpage_id++) {
    // subpage_id==0x00 writes to APP_ADDR==0x00008000 i.e. start of page 8
    // So subpage_id==0x20 writes to addr 0x00009000 i.e. start of page 9 etc
    // Need to erase page once before writing inside of it
    if((subpage_id*BYTES_PER_BLR_PLD)%BYTES_PER_FLASH_PAGE==0) {
      flash_erase_page(8+(subpage_id*BYTES_PER_BLR_PLD)/BYTES_PER_FLASH_PAGE);
    }
  }
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
    uint32_t subpage_id = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX]);
    // subpage_id==0x00 writes to APP_ADDR==0x00008000 i.e. start of page 8
    // So subpage_id==0x20 writes to addr 0x00009000 i.e. start of page 9 etc
    // Need to erase page once before writing inside of it
    if((subpage_id*BYTES_PER_BLR_PLD)%BYTES_PER_FLASH_PAGE==0) {
      flash_erase_page(8+(subpage_id*BYTES_PER_BLR_PLD)/BYTES_PER_FLASH_PAGE);
    }
    // write data
    uint32_t start_addr = APP_ADDR+subpage_id*BYTES_PER_BLR_PLD;
    for(size_t i=0; i<BYTES_PER_BLR_PLD; i+=4) {
      uint32_t word = *(uint32_t*)((rx_cmd_buff->data)+PLD_START_INDEX+1+i);
      // Enable write
      NVMC_CONFIG = NVMC_CONFIG_WEN;
      __asm__("isb 0xF");
      __asm__("dsb 0xF");
      // Write word
      MMIO32(i+start_addr) = word;
      // Wait for write to complete
      while(NVMC_READY==NVMC_READY_BUSY) {}
      // Enable read-only mode
      NVMC_CONFIG = NVMC_CONFIG_REN;
      __asm__("isb 0xF");
      __asm__("dsb 0xF");
    }
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
    uint32_t addr_1 = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX]);
    uint32_t addr_2 = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX+1]);
    uint32_t addr_3 = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX+2]);
    uint32_t addr_4 = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX+3]);
    uint32_t start_addr = (addr_1<<24)|(addr_2<<16)|(addr_3<<8)|(addr_4<<0);
    // write data
    for(size_t i=0; i<BYTES_PER_BLR_PLD; i+=4) {
      // APP_ADDR==0x00008000 corresponds to the start of Flash page 8, and
      // 0x00008800 corresponds to the start of Flash page 9, etc.
      // Need to erase page once before writing inside of it
      // Check for every new word since write_addr32 need not be page-aligned
      if((i+start_addr)%BYTES_PER_FLASH_PAGE==0) {
        flash_erase_page((i+start_addr)/BYTES_PER_FLASH_PAGE);
      }
      uint32_t word = *(uint32_t*)((rx_cmd_buff->data)+PLD_START_INDEX+1+i);
      // Enable write
      NVMC_CONFIG = NVMC_CONFIG_WEN;
      __asm__("isb 0xF");
      __asm__("dsb 0xF");
      // Write word
      MMIO32(i+start_addr) = word;
      // Wait for write to complete
      while(NVMC_READY==NVMC_READY_BUSY) {}
      // Enable read-only mode
      NVMC_CONFIG = NVMC_CONFIG_REN;
      __asm__("isb 0xF");
      __asm__("dsb 0xF");
    }
    return 1;
  } else {
    return 0;
  }
}

// This example implementation of handle_bootloader_jump returns 0 because this
// example program does not implement execution of user applications
int handle_bootloader_jump(void){
  return 0;
}

// This example implementation of bootloader_active always returns 1 because
// this example program does not implement execution of user applications
int bootloader_active(void) {
  return 1;
}

// Board initialization functions

void init_clock(void) {
  clock_start_hfclk(0);
}

void init_leds(void) {
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO29);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO30);
  gpio_set(  P0, GPIO30); // LED1 is P0.30
  gpio_clear(P0, GPIO29); // LED2 is P0.29
}

void init_uart(void) {
  uart_disable(UART0);
  gpio_mode_setup(P0, GPIO_MODE_INPUT,  GPIO_PUPD_NONE,   GPIO5);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO4);
  uart_configure( // TX1 is P0.04 and RX1 is P0.05
   UART0, GPIO4, GPIO5, GPIO_UNCONNECTED, GPIO_UNCONNECTED,
   UART_BAUD_9600, 0
  );
  uart_enable(UART0);
}

// Feature functions

void rx_uart(rx_cmd_buff_t* rx_cmd_buff_o) {
  uart_start_rx(UART0);                              // Generate STARTRX task
  while(                                             // while
   rx_cmd_buff_o->state!=RX_CMD_BUFF_STATE_COMPLETE  //  Command not complete
  ) {                                                //
    while(!UART_EVENT_RXDRDY(UART0)) {}              // Wait until RXD is ready
    UART_EVENT_RXDRDY(UART0) = 0;                    // Clear RXDRDY event
    uint8_t b = uart_recv(UART0);                    // Receive byte from RX pin
    push_rx_cmd_buff(rx_cmd_buff_o, b);              // Push byte to buffer
  }                                                  //
  uart_stop_rx(UART0);                               // Generate STOPRX task
}

void reply(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o) {
  if(                                                  // if
   rx_cmd_buff_o->state==RX_CMD_BUFF_STATE_COMPLETE && // rx_cmd is valid AND
   tx_cmd_buff_o->empty                                // tx_cmd is empty
  ) {                                                  //
    write_reply(rx_cmd_buff_o, tx_cmd_buff_o);         // execute cmd and reply
  }                                                    //
}

void tx_uart(tx_cmd_buff_t* tx_cmd_buff_o) {
  uart_start_tx(UART0);                              // Generate STARTTX task
  while(                                             // while
   !(tx_cmd_buff_o->empty)                           //  TX buffer not empty
  ) {                                                //
    uint8_t b = pop_tx_cmd_buff(tx_cmd_buff_o);      // Pop byte from TX buffer
    uart_send(UART0,b);                              // Send byte to TX pin
    while(!UART_EVENT_TXDRDY(UART0)) {}              // Wait until TXD is ready
    UART_EVENT_TXDRDY(UART0) = 0;                    // Clear TXDRDY event
    if(tx_cmd_buff_o->empty) {                       // if TX buffer empty
      gpio_toggle(P0, GPIO30);                       //  Toggle LED1
      gpio_toggle(P0, GPIO29);                       //  Toggle LED2
    }                                                //
  }                                                  //
  uart_stop_tx(UART0);                               // Generate STOPTX task
}
