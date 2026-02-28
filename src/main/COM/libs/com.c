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

// radio mmio definitions
#define RADIO_BASE 0x40001000
#define RADIO_REG(offset) MMIO32(RADIO_BASE + (offset))
// radio offsets
#define TASKS_TXEN 0x000    // Enable RADIO in TX mode
#define TASKS_RXEN 0x004    // Enable RADIO in RX mode
#define TASKS_START 0x008  // Start RADIO
#define TASKS_STOP 0x00C    // Stop RADIO
#define TASKS_DISABLE 0x010 // Disable RADIO
#define TASKS_RSSISTART 0x014 // Start the RSSI and take one single sample of the receive signal strength
#define TASKS_RSSISTOP 0x018 // Stop the RSSI measurement
#define TASKS_BCSTART 0x01C // Start the bit counter
#define TASKS_BCSTOP 0x020 // Stop the bit counter
#define TASKS_EDSTART 0x024 // Start the energy detect measurement used in IEEE 802.15.4 mode
#define TASKS_EDSTOP 0x028 // Stop the energy detect measurement
#define TASKS_CCASTART 0x02C // Start the clear channel assessment used in IEEE 802.15.4 mode
#define TASKS_CCASTOP 0x030 // Stop the clear channel assessment
#define EVENTS_READY 0x100 // RADIO has ramped up and is ready to be started
#define EVENTS_ADDRESS 0x104 // Address sent or received
#define EVENTS_PAYLOAD 0x108 // Packet payload sent or received
#define EVENTS_END 0x10C // Packet sent or received
#define EVENTS_DISABLED 0x110 // RADIO has been disabled
#define EVENTS_DEVMATCH 0x114 // A device address match occurred on the last received packet
#define EVENTS_DEVMISS 0x118 // No device address match occurred on the last received packet
#define EVENTS_RSSIEND 0x11C // Sampling of receive signal strength complete
#define EVENTS_BCMATCH 0x128 // Bit counter reached bit count value
#define EVENTS_CRCOK 0x130 // Packet received with CRC ok
#define EVENTS_CRCERROR 0x134 // Packet received with CRC error
#define EVENTS_FRAMESTART 0x138 // IEEE 802.15.4 length field received
#define EVENTS_EDEND 0x13C // Sampling of energy detection complete.
#define EVENTS_EDSTOPPED 0x140 //The sampling of energy detection has stopped
#define EVENTS_CCAIDLE 0x144 // Wireless medium in idle - clear to send
#define EVENTS_CCABUSY 0x148 // Wireless medium busy - do not send
#define EVENTS_CCASTOPPED 0x14C //The CCA has stopped
#define EVENTS_RATEBOOST 0x150 //Ble_LR CI field received, receive mode is changed from Ble_LR125Kbit to Ble_LR500Kbit.
#define EVENTS_TXREADY 0x154 //RADIO has ramped up and is ready to be started TX path
#define EVENTS_RXREADY 0x158 //RADIO has ramped up and is ready to be started RX path
#define EVENTS_MHRMATCH 0x15C //MAC header match found
#define EVENTS_SYNC 0x168 //Preamble indicator
#define EVENTS_PHYEND 0x16C //Generated when last bit is sent on air, or received from air
#define EVENTS_CTEPRESENT 0x170 //CTE is present (early warning right after receiving CTEInfo byte)
#define SHORTS 0x200 //Shortcuts between local events and tasks
#define INTENSET 0x304 //Enable interrupt
#define INTENCLR 0x308 //Disable interrupt
#define CRCSTATUS 0x400 //CRC status
#define RXMATCH 0x408 //Received address
#define RXCRC 0x40C //CRC field of previously received packet
#define DAI 0x410 //Device address match index
#define PDUSTAT 0x414 //Payload status
#define CTESTATUS 0x44C //CTEInfo parsed from received packet
#define DFESTATUS 0x458 //DFE status information
#define PACKETPTR 0x504 //Packet pointer
#define FREQUENCY 0x508 //Frequency
#define TXPOWER 0x50C //Output power
#define MODE 0x510// Data rate and modulation
#define PCNF0 0x514 //Packet configuration register 0
#define PCNF1 0x518 //Packet configuration register 1
#define BASE0 0x51C //Base address 0
#define BASE1 0x520 //Base address 1
#define PREFIX0 0x524 //Prefixes bytes for logical addresses 0-3
#define PREFIX1 0x528 //Prefixes bytes for logical addresses 4-7
#define TXADDRESS 0x52C //Transmit address select
#define RXADDRESSES 0x530 //Receive address select
#define CRCCNF 0x534 //CRC configuration
#define CRCPOLY 0x538 //CRC polynomial
#define CRCINIT 0x53C //CRC initial value
#define TIFS 0x544 //Interframe spacing in μs
#define RSSISAMPLE 0x548 //RSSI sample
#define STATE 0x550 //Current radio state
#define DATAWHITEIV 0x554 //Data whitening initial value
#define BCC 0x560 //Bit counter compare
#define DAB[n] 0x600 //Device address base segment n
#define DAP[n] 0x620 //Device address prefix n
#define DACNF 0x640 //Device address match configuration
#define MHRMATCHCONF 0x644 //Search pattern configuration
#define MHRMATCHMAS 0x648 //Pattern mask
#define MODECNF0 0x650 //Radio mode configuration register 0
#define SFD 0x660 //IEEE 802.15.4 start of frame delimiter
#define EDCNT 0x664 //IEEE 802.15.4 energy detect loop count
#define EDSAMPLE 0x668 //IEEE 802.15.4 energy detect level
#define CCACTRL 0x66C //IEEE 802.15.4 clear channel assessment control
#define DFEMODE 0x900 //Whether to use Angle-of-Arrival (AOA) or Angle-of-Departure (AOD)
#define CTEINLINECONF 0x904 //Configuration for CTE inline mod
#define POWER 0xFFC // Periphal Power


// our test payload
uint8_t dummy_packet[4] = {0xde, 0xad, 0xbe, 0xef};

void init_radio_tx_test(void) {
  // 0 = 1mbit nordic proprietary mode
  RADIO_MODE = 0; 
  // 2400 + 40 = 2440 mhz channel
  RADIO_FREQUENCY = 40; 
  // 0 dbm tx power (nrf52833 can go up to +8)
  RADIO_TXPOWER = 0; 

  // basic packet config: 8 bit length field
  RADIO_PCNF0 = (0 << 16) | (1 << 8) | (8 << 0);
  // max payload 255, base address length 4 bytes
  RADIO_PCNF1 = (255 << 16) | (255 << 8) | (4 << 0);

  // set dummy sync word / address
  RADIO_BASE0 = 0x01234567;
  RADIO_PREFIX0 = 0x89;
  // use logical address 0
  RADIO_TXADDRESS = 0; 

  // disable crc to just push raw bytes
  RADIO_CRCCNF = 0; 
}

void blast_noise(void) {
  // give the radio the memory address of our array
  RADIO_PACKETPTR = (uint32_t)dummy_packet;

  // spin up the radio in tx mode
  RADIO_TASKS_TXEN = 1;

  // wait for the pll to lock and radio to be ready
  while (RADIO_EVENTS_READY == 0) {}
  RADIO_EVENTS_READY = 0;

  // start transmission
  RADIO_TASKS_START = 1;

  // wait for the packet to finish sending
  while (RADIO_EVENTS_END == 0) {}
  RADIO_EVENTS_END = 0;
}

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

int handle_common_data(common_data_t common_data_buff_i) {
  // fail if payload is empty
  if(common_data_buff_i.end_index == 0) {
    return 0; 
  }

  // first check if payload bytes are strictly increasing
  int strictly_increasing = 1;
  uint8_t prev_byte = common_data_buff_i.data[0];
  
  for(size_t i=1; i<common_data_buff_i.end_index; i++) {
    if(prev_byte >= common_data_buff_i.data[i]) {
      strictly_increasing = 0;
      break; 
    } else {
      prev_byte = common_data_buff_i.data[i];
    }
  }

  // reject if it failed the check
  if(!strictly_increasing) {
    return 0;
  }

  // now handle the actual command action
  uint8_t action = common_data_buff_i.data[0];

  if(action == 0x01) {
    // enable tx, disable rx
    gpio_clear(P0, RX_EN_PIN);
    gpio_set(P0, TX_EN_PIN);
    return 1;
  } else if(action == 0x02) {
    // enable rx, disable tx
    gpio_clear(P0, TX_EN_PIN);
    gpio_set(P0, RX_EN_PIN);
    return 1;
  } else if(action == 0x03) {
    // enable tx pin for nrf21540
    gpio_clear(P0, RX_EN_PIN);
    gpio_set(P0, TX_EN_PIN);
    
    // wait a tiny bit for the pa to power up
    for(volatile int i=0; i<100; i++); 

    // send the dummy packet
    blast_noise();
    return 1;
  }

  // unknown command
  return 0; 
}

// This example implementation of handle_common_data checks whether the bytes
// are strictly increasing, i.e. each subsequent byte is strictly greater than
// the previous byte

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
