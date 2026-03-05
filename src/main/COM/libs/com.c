// com.c
// COM board support implementation file
//
// Written by Bradley Denby
// Other contributors: None
//
// See the top-level LICENSE file for the license.

// Standard library headers
#include <stdint.h>                 // uint8_t
#include <libopencm3/nrf/clock.h> 
#include <libopencm3/nrf/gpio.h>
#include <libopencm3/nrf/uart.h>
#include <libopencm3/nrf/radio.h>    // used in init_radio
#include <libopencm3/nrf/ppi.h>      // used in init_radio
#include <com.h>                    // COM header
#include <tab.h>                    // TAB header

// ========== Macros ========== //

// Flash memory
#define NVMC_CONFIG MMIO32    (NVMC_BASE + 0x504)
#define NVMC_CONFIG_REN       (0     )
#define NVMC_CONFIG_WEN       (1 << 1)
#define NVMC_CONFIG_EEN       (1 << 2)
#define NVMC_ERASEPAGE MMIO32 (NVMC_BASE + 0x508)
#define NVMC_READY MMIO32     (NVMC_BASE + 0x400)
#define NVMC_READY_BUSY       (0     )

// Energy Detection
#define ED_RSSISCALE 4

// ========== TAB Handling ========== //

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
      // COM should never hear this case - we probably don't want to give CDH the ability to turn off COM
      // with no plan to turn it back on? TODO: Add power off reset to CDH for COM, which com could pass through.
      switch(*val_ptr){ 
        
        case VAR_ENABLE: // told to power on com

          break;

        case VAR_DISABLE: // told to power off com

          break;

        default:
          return 0;
      }
      break;

    case VAR_CODE_PAY_EN:
      // 
      switch(*val_ptr){ 
        case VAR_ENABLE: // told to power on pay
          cdh_enable_pay(rx_cmd_buff, tx_cmd_buff);
          return 1;

        case VAR_DISABLE: // told to power off pay
          cdh_disable_pay(rx_cmd_buff, tx_cmd_buff);
          return 1;

        default:
          return 0;              
      } 
      break;

    case VAR_CODE_RF_EN:
      // 
      switch(*val_ptr){ 
          
        case VAR_ENABLE:
          enable_rf();
          return 1;

        case VAR_DISABLE:
          disable_rf();
          return 1;
      }
      break;

      case VAR_CODE_RUN_DEMO:
        run_demo(rx_cmd_buff, tx_cmd_buff);
        return 1;

    case VAR_CODE_RF_TX: 
      enable_tx();
      return 1;

    case VAR_CODE_RF_RX:
      enable_rx();
      return 1;

    case VAR_CODE_CORAL_WAKE:
      break;

    case VAR_CODE_CORAL_CAM_ON:
      break;

    case VAR_CODE_CORAL_INFER:
      break;

    case VAR_CODE_BLINK_COM:
      com_blink_demo();
      return 1;
    case VAR_CODE_BLINK_CDH:
      cdh_blink_demo(rx_cmd_buff, tx_cmd_buff);
      return 1;
    
    default:
      return 0;
    }
      return 0;
}

// ========== Board initialization functions ========== //

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
  UART_BAUD_115200, 0
  );
  uart_enable(UART0);
}

void init_gpio(void) {
    P1_PIN_CNF(9) = (1UL << 0) | (1UL << 1);
    
    uint32_t verify = P1_PIN_CNF(9);

    // // LED1 = write succeeded, LED2 = write failed
    // if (verify != 0) {
    //     gpio_set(P0, LED1);
    // } else {
    //     gpio_set(P0, LED2);
    // }
    
    // while(1); // stop here so we can see result

    gpio_mode_setup(GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TX_EN_PIN);
    gpio_mode_setup(GPIO, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RX_EN_PIN);
    gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GPIO15); // RF CSN
}


void init_radio(void) {
  radio_enable();
  RADIO_MODE = 15; // ieee standard

  radio_set_datawhiteiv(0);
  radio_enable_whitening();

  radio_set_balen(4);
  radio_set_maxlen(64);
  radio_set_frequency(40);  // 2440 mhz
  radio_set_crclen(2);      // enable crc

  RADIO_CRCPOLY = 0X11021;
  RADIO_CRCINIT = 0;
  RADIO_CRCCNF |= (2 << 8); // skip address and start crc after len byte

  radio_configure_packet(8,0,0);

  radio_set_addr(0, RADIO_DAB(0), RADIO_DAP(0));
  radio_set_tx_address(0);
  radio_set_rx_address(0);

  radio_set_txpower(RADIO_TXPOWER_POS_4DBM);
  RADIO_CCACTRL = 1;  // cca mode to carrier
}

// ========== UART Communication functions ========== //

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

// ========== GPIO Functions ========== //

void enable_rf(void) {
    P1_OUTSET = (1UL << 9);
}

void disable_rf(void) {
    P1_OUTCLR = (1 << 9);
}

void enable_rx(){
  gpio_clear(P0, TX_EN_PIN);
      for(int i=0; i<2000000; i++) {
      __asm__("nop"); // Settle time to ensure RX_ENABLE is LOW
    }
  gpio_set(P0, RX_EN_PIN);
}

void enable_tx(){
  gpio_clear(P0, RX_EN_PIN);
      for(int i=0; i<2000000; i++) {
      __asm__("nop"); // Settle time to ensure TX_ENABLE is LOW
    }
  gpio_set(P0, TX_EN_PIN);
}

void com_blink_demo(void) {
    // blink for 15 seconds (slow)
    for(int k = 0; k < 20; k++) {
      for(int i = 0; i < 4000000; i++) {
        __asm__("nop");
      }
      gpio_toggle(P0, LED1);
      gpio_toggle(P0, LED2);
    }

    // faster blink for 15 seconds (fast)
    for(int k = 0; k < 20; k++) {
      for(int i = 0; i < 2000000; i++) {
        __asm__("nop");
      }
      gpio_toggle(P0, LED1);
      gpio_toggle(P0, LED2);
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
  // send msg to cdh for it to blink
  uint8_t my_payload[] = {VAR_CODE_BLINK_CDH, 0x01, VAR_ENABLE};
  msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

// ========== Radio Functions ========== //
uint8_t dummy_packet[4] = {0xde, 0xad, 0xbe, 0xef};

void init_radio_tx_test(void) {
  // 1. HARD RESET THE RADIO PERIPHERAL
  // This wipes all IEEE mode artifacts that are hanging the state machine!
  MMIO32(RADIO_BASE + 0x500) = 0; // POWER OFF
  for(int i=0; i<1000; i++) { __asm__("nop"); } // Let silicon settle
  MMIO32(RADIO_BASE + 0x500) = 1; // POWER ON

  // 2. Configure for basic unmodulated carrier wave
  RADIO_MODE = 0;           // basic 1Mbps mode
  RADIO_FREQUENCY = 10;     // 2410 MHz
  RADIO_TXPOWER = 4;        // +4 dBm
  
  // Basic packet config so the synth doesn't get confused
  RADIO_PCNF0 = (0 << 16) | (1 << 8) | (8 << 0);
  RADIO_PCNF1 = (255 << 16) | (255 << 8) | (4 << 0);
  RADIO_BASE0 = 0x01234567;
  RADIO_PREFIX0 = 0x89;
  RADIO_TXADDRESS = 0; 
  RADIO_CRCCNF = 0; 
  RADIO_SHORTS = 0;
}
void blast_carrier(void) {
  // ensure the external rf frontend amplifiers are turned on
  enable_rf();
  enable_tx();

  // clear any lingering events
  RADIO_EVENT_READY = 0;

  // tell the nrf radio to ramp up for transmit
  RADIO_TASK_TXEN = 1;

  // Wait for the radio to be ready
  while (RADIO_EVENT_READY == 0) {
    __asm__("nop");
  }

  // trap the cpu here so it just keeps broadcasting and flashing the LED
  while (1) {
    for(int i=0; i<4000000; i++) {
      __asm__("nop");
    }
    gpio_toggle(P0, LED2);
  }
}

uint8_t sample_ed(void){
    int val;
    RADIO_EDCNT = 100; // set count
    RADIO_TASK_EDSTART = 1; // Start
    
    while (RADIO_EVENT_EDEND != 1) {
        __asm__("nop");
    }
    
    RADIO_TASK_EDSTART = 0; // set back to 0
    RADIO_EVENT_EDEND = 0; // set back to 0
    
    val = RADIO_EDSAMPLE * ED_RSSISCALE; // Read level
    return (uint8_t)(val>255 ? 255 : val); // Convert to IEEE 802.15.4 scale
}

void radio_set_rx_address(uint8_t addr_index)
{
    RADIO_RXADDRESSES |= (1 << addr_index);
}

void tx_cmd_buff_config(tx_cmd_buff_t* buff, uint8_t msg_id) {
    clear_tx_cmd_buff(buff);

    buff->data[START_BYTE_0_INDEX] = (uint8_t)0x22;
    buff->data[START_BYTE_1_INDEX] = (uint8_t)0x69;
    buff->data[MSG_LEN_INDEX] = (uint8_t)0x16;
    buff->data[HWID_LSB_INDEX] = (uint8_t)0xaf;
    buff->data[HWID_MSB_INDEX] = (uint8_t)0xbe;
    buff->data[MSG_ID_LSB_INDEX] = msg_id;
    buff->data[MSG_ID_MSB_INDEX] = (uint8_t)0x00;
    buff->data[ROUTE_INDEX] = (uint8_t)0x10;
    buff->data[OPCODE_INDEX] = (uint8_t)0x16;
    buff->end_index = buff->data[MSG_LEN_INDEX] + (uint8_t)0x03;
    buff->empty = 0;
}
void tx_radio(uint8_t* pckt, size_t length) {
	
	// Radio state variable
	uint8_t temp_radio_state;

	// Tab initialization e
	tx_cmd_buff_t tx_cmd_buff = {.size=CMD_MAX_LEN};
	clear_tx_cmd_buff(&tx_cmd_buff);

	gpio_set(P0, GPIO2); // enable front rx
	
	radio_set_packet_ptr(pckt); // point to packet that we made

	if (length > 125) {
	// IEEE 802.15.4 packets have a maximum payload size of 127 bytes (125 b/c crc uses 2 playload bytes)
	return;
	}

	radio_enable_rx(); // TASK_RXEN = 1
	
	// RXEN -> RXRU -> RXIDLE
	while(RADIO_STATE != 1){ // Pass when not in RXRU
		__asm__("nop");
	}
	while (RADIO_EVENT_RXREADY == 0){
		__asm__("nop");
	}

	RADIO_EVENT_RXREADY = 0; // Reset

	while(RADIO_STATE != 2){ // Pass when not in RXIDLE
		__asm__("nop");
	}

	// RXIDLE -> RX
	RADIO_TASK_CCASTART = 1;
	temp_radio_state = RADIO_STATE;

	int i = 0;
	while(temp_radio_state != 3){ // Pass when not in RX
		temp_radio_state = RADIO_STATE;
		__asm__("nop");
		i++;
		if ( i>= 1000){
			break;
		}
	}
	
	// while (RADIO_EVENT_CCAIDLE == 0){ // checks if noise floor is too high
	// 	__asm__("nop");
	// }
	
	//Transmit
	// TXEN -> TXRU

	RADIO_EVENT_CCAIDLE = 0;
	RADIO_TASK_TXEN = 1;

	while(RADIO_STATE != 9){ // Pass when not in TXRU
		__asm__("nop");
	}

	while(RADIO_STATE != 10){ // Pass when not TXIDLE
		__asm__("nop");
	}

	//TX_READY -> START
	while (RADIO_EVENT_TXREADY == 0) {
		__asm__("nop");
	}
	
	RADIO_TASK_START = 1;
	RADIO_EVENT_TXREADY = 0;

	// START
	RADIO_TASK_START = 1; // start radio
	RADIO_EVENT_TXREADY = 0; // set back to zero.

	//TXIDLE -> TX
	while(RADIO_STATE!=11){ // Pass when not in TX
		__asm__("nop");
	}
	
	while (RADIO_EVENT_FRAMESTART == 0) {
		__asm__("nop");
	}
	
	// wait until radio is done TX
	// last bit sent
	while (RADIO_EVENT_PHYEND == 0) {
		__asm__("nop");
	}
	
	while (RADIO_EVENT_END == 0){
		__asm__("nop");
	}

	RADIO_EVENT_FRAMESTART = 0; // set back to 0
	
	//TX -> TXIDLE
	while(RADIO_STATE!=10){ // Pass when not in TXIDLE
		__asm__("nop");
	}
	
	RADIO_EVENT_PHYEND = 0; // set back to 0
	RADIO_EVENT_END = 0; // set back to 0
	RADIO_TASK_DISABLE = 1; // disable radio
	
	// TXIDLE -> DISABLE
	while (RADIO_EVENT_DISABLED == 0) {
		__asm__("nop");
	}
	RADIO_EVENT_DISABLED = 0; // set back to 0
	
	gpio_clear(P0, GPIO3); // disable front tx
}

//RADIO RX
//Receive - pg. 508
void rx_radio(uint8_t* pckt, size_t length) {
	
	// Figure 112 pg 508 IEEE RX sequence
	// Init temp radio state variable
	uint8_t temp_radio_state;
	
	// TAB Initialization
	tx_cmd_buff_t tx_cmd_buff = {.size=CMD_MAX_LEN};
	clear_tx_cmd_buff(&tx_cmd_buff);
	
	gpio_set(P0, GPIO2); // enable front rx
	
	radio_set_packet_ptr(pckt); // point to packet that we made
	
	if (length > 125) {
	// IEEE 802.15.4 packets have a maximum payload size of 127 bytes (125 b/c crc uses 2 playload bytes)
	return;
	}

	radio_enable_rx(); // TASK_RXEN = 1
	
	// RXEN -> RXRU -> RXIDLE
	while(RADIO_STATE != 1){ // Pass when not in RXRU
		__asm__("nop");
	}
	
	while (RADIO_EVENT_RXREADY == 0){
		__asm__("nop");
	}

	RADIO_EVENT_RXREADY = 0; // Reset

	while(RADIO_STATE != 2){ // Pass when not in RXIDLE
		__asm__("nop");
	}

	RADIO_TASK_START = 1; // start START
	temp_radio_state = RADIO_STATE; // RX
	
	int i = 0;
	while(temp_radio_state != 3){ // Pass when not in RX
		temp_radio_state = RADIO_STATE;
		__asm__("nop");
	}

	// FRAMESTART
	while(RADIO_EVENT_FRAMESTART == 9){
		__asm__("nop");
	}

	// Sample the energy detect level
	uint8_t energy_level = sample_ed();
	pckt[11] = energy_level;
	
	// packet recieved
	while (RADIO_EVENT_END == 0) {
		__asm__("nop");
	}

	
	
	while(RADIO_STATE!=2){ // while RADIO_STATE not RXIDLE
		__asm__("nop");
	}
	
	// RXIDLE -> DISABLE
	RADIO_EVENT_FRAMESTART = 0; // set back to 0
	RADIO_EVENT_PHYEND = 0; // set back to 0
	RADIO_EVENT_END = 0; // set back to 0
	RADIO_TASK_DISABLE = 1; // disable radio
	
	while(RADIO_STATE!=0){ // while RADIO_STATE not DISABLED
		__asm__("nop");
	}
	
	while (RADIO_EVENT_DISABLED == 0) {
		__asm__("nop");
	}
	
	RADIO_EVENT_DISABLED = 0; // set back to 0
	
	gpio_clear(P0, GPIO2); // disable front rx
	
	pckt[15] = (uint8_t)RADIO_CRCSTATUS; 
}

void test_tx(void) {
  // dummy packet. the first byte usually needs to be the length for the radio 
  uint8_t test_packet[16] = {15, 0xde, 0xad, 0xbe, 0xef};

  // trap the processor here and blast packets
  while(1) {
    tx_radio(test_packet, 16);
    
    // tiny delay so the radio state machine can breathe
    for(int i=0; i<100000; i++) { 
      __asm__("nop");
    }
    
    // toggle an led so you can visually see the packets flying
    gpio_toggle(P0, LED2);
  }
}

// ========== Utility functions ========== //



void run_demo(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){

  for(int i=0; i<48000000; i++) { __asm__("nop"); }

  com_blink_demo();

  for(int i=0; i<48000000; i++) { __asm__("nop"); }
  
  // Blink CDH (No rx_uart catch needed!)
  cdh_blink_demo(rx_cmd_buff, tx_cmd_buff);
  tx_uart(tx_cmd_buff);

  enable_rf();
  for(int i=0; i<48000000; i++) { __asm__("nop"); }

  enable_rx();
  for(int i=0; i<48000000; i++) { __asm__("nop"); }

  enable_tx();
  for(int i=0; i<48000000; i++) { __asm__("nop"); }

  // Enable Payload (No rx_uart catch needed!)
  cdh_enable_pay(rx_cmd_buff, tx_cmd_buff);
  tx_uart(tx_cmd_buff);
  
  for(int i=0; i<48000000; i++) { __asm__("nop"); }

  // Flush the RX pin just in case garbage is sitting there
  uart_stop_rx(UART0);

  // Re-initialize the radio exactly how it was at boot
  init_radio();
  
  // Blast it!
  blast_carrier();
}



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



// ========== Example Bootloader Functions ========== //

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
