// com.c
// com board support implementation file

#include <stddef.h>                 
#include <stdint.h>                 
#include <libopencm3/nrf/clock.h> 
#include <libopencm3/nrf/gpio.h>
#include <libopencm3/nrf/uart.h>
#include <libopencm3/nrf/radio.h>    
#include <libopencm3/nrf/ppi.h>      
#include <com.h>                    
#include <tab.h>                    

// flash memory macros
#define NVMC_CONFIG MMIO32    (NVMC_BASE + 0x504)
#define NVMC_CONFIG_REN       (0     )
#define NVMC_CONFIG_WEN       (1 << 1)
#define NVMC_CONFIG_EEN       (1 << 2)
#define NVMC_ERASEPAGE MMIO32 (NVMC_BASE + 0x508)
#define NVMC_READY MMIO32     (NVMC_BASE + 0x400)
#define NVMC_READY_BUSY       (0     )

// Additional radio event definitions not in libopencm3
#define RADIO_EVENT_RXREADY           MMIO32(RADIO_BASE + 0x158)
#define RADIO_EVENT_TXREADY           MMIO32(RADIO_BASE + 0x154)
#define RADIO_EVENT_FRAMESTART        MMIO32(RADIO_BASE + 0x138)
#define RADIO_EVENT_PHYEND            MMIO32(RADIO_BASE + 0x16C)

// dma buffers for non-blocking radio
uint8_t dma_rx_buff[128];
uint8_t dma_tx_buff[128];

int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff) {
  if(common_data_buff_i.end_index < 2) {
    return 0; 
  }

  uint8_t var_code = common_data_buff_i.data[0];
  uint8_t var_len  = common_data_buff_i.data[1];

  if(common_data_buff_i.end_index < (size_t)(2 + var_len)) {
    return 0; 
  }

  uint8_t* val_ptr = &common_data_buff_i.data[2];

  switch(var_code) {
    case VAR_CODE_ALIVE:
    switch(*val_ptr){ 
        case 0x01:
          send_alive(rx_cmd_buff, tx_cmd_buff);
          return 1;
        default:
          return 0;
      }
      break;

    case VAR_CODE_COM_EN:
      switch(*val_ptr){ 
        case VAR_ENABLE: 
          break;
        case VAR_DISABLE: 
          break;
        default:
          return 0;
      }
      break;

    case VAR_CODE_PAY_EN:
      switch(*val_ptr){ 
        case VAR_ENABLE: 
          cdh_enable_pay(rx_cmd_buff, tx_cmd_buff);
          return 1;
        case VAR_DISABLE: 
          cdh_disable_pay(rx_cmd_buff, tx_cmd_buff);
          return 1;
        default:
          return 0;              
      } 
      break;

    case VAR_CODE_RF_EN:
      switch(*val_ptr){ 
        case VAR_ENABLE:
          enable_rf();
          return 1;
        case VAR_DISABLE:
          disable_rf();
          return 1;
      }
      break;

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

    default:
      while(1) {
        for(int i=0; i<4000000; i++) {
          __asm__("nop");
        }
        gpio_toggle(P0, LED1);
        gpio_toggle(P0, LED2);
      }
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
  gpio_set(  P0, GPIO30); 
  gpio_clear(P0, GPIO29); 
}

void init_uart(void) {
  uart_disable(UART0);
  gpio_mode_setup(P0, GPIO_MODE_INPUT,  GPIO_PUPD_NONE,   GPIO5);
  gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO4);
  uart_configure( 
  UART0, GPIO4, GPIO5, GPIO_UNCONNECTED, GPIO_UNCONNECTED,
  UART_BAUD_115200, 0
  );
  uart_enable(UART0);
}

void init_gpio(void) {
    // pin configs moved to init_radio
}

//Initialize Radio Peripheral
void init_radio(void) {
	radio_enable();

	RADIO_MODE = 15;

	radio_enable_whitening();
	radio_set_datawhiteiv(0);

	radio_set_balen(4);
	radio_set_maxlen(64);
	radio_set_frequency(10);  // 2410 MHz
	radio_set_crclen(2);      // Enable CRC

	RADIO_CRCPOLY = 0X11021;
	RADIO_CRCINIT = 0;
	RADIO_CRCCNF |= (2 << 8); // Skip address and start crc after len byte as per IEEE 802.15.4

	radio_configure_packet(8,0,0);

	radio_set_addr(0, RADIO_DAB(0), RADIO_DAP(0));
	radio_set_tx_address(0);
	radio_set_rx_address(0);

	radio_set_txpower(RADIO_TXPOWER_POS_4DBM);
	RADIO_CCACTRL = 1;	// CCA Mode to Carrier mode -- needed for IEEE

	P1_PIN_CNF(9) = 1; // RF PLD to OUTPUT (Dir = 1)
	gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GPIO15); // RF CSN
	gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GPIO3); // TX
	gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GPIO2); // RX
	gpio_mode_setup(P0, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GPIO28); // MODE
	
	P1_OUTSET = GPIO9;     // enable front-end PND (active high)
	gpio_clear(P0, GPIO3); // TX disabled
	gpio_clear(P0, GPIO2); // RX disabled

/* Debugging Code
	gpio_configure_task(0, 3, GPIO_TE_POLARITY_TOGGLE, 0); // TX toggle
	gpio_configure_task(1, 2, GPIO_TE_POLARITY_TOGGLE, 0); // RX_toggle
	gpio_configure_task(2, 4, GPIO_TE_POLARITY_TOGGLE, 0); // LED1 toggle (tx)
	gpio_configure_task(3, 5, GPIO_TE_POLARITY_TOGGLE, 0); // LED2 toggle (rx)
	
	ppi_configure_channel(0, (uint32_t)(&RADIO_EVENT_CCAIDLE), (uint32_t)(&GPIO_TASK_OUT(0)));  // TX_EN enable
	ppi_configure_channel(1, (uint32_t)(&RADIO_EVENT_DISABLED), (uint32_t)(&GPIO_TASK_OUT(0))); // TX_EN disable
	
	ppi_configure_channel(2, (uint32_t)(&RADIO_EVENT_RXREADY), (uint32_t)(&GPIO_TASK_OUT(1)));  // RX_EN disable
	ppi_configure_channel(3, (uint32_t)(&RADIO_EVENT_DISABLED), (uint32_t)(&GPIO_TASK_OUT(1))); // RX_EN disable
	
	//ppi_configure_channel(4, (uint32_t)(&RADIO_EVENT_RXREADY), (uint32_t)(&GPIO_TASK_OUT(2))); // debug
	ppi_configure_channel(5, (uint32_t)(&RADIO_EVENT_CCAIDLE), (uint32_t)(&GPIO_TASK_OUT(2))); // debug
	
	ppi_configure_channel(6, (uint32_t)(&RADIO_EVENT_READY), (uint32_t)(&GPIO_TASK_OUT(3)));    // debug
	ppi_configure_channel(7, (uint32_t)(&RADIO_EVENT_DISABLED), (uint32_t)(&GPIO_TASK_OUT(3))); // debug
	ppi_enable_channels(PPI_CH0 | PPI_CH1 | PPI_CH2 | PPI_CH3 | PPI_CH4 | PPI_CH5 | PPI_CH6 | PPI_CH7);
*/
}




// ========== UART Communication functions ========== //

void rx_uart(rx_cmd_buff_t* rx_cmd_buff_o) {
  uart_start_rx(UART0);                              
  while(                                             
  rx_cmd_buff_o->state!=RX_CMD_BUFF_STATE_COMPLETE  
  ) {                                                
    while(!UART_EVENT_RXDRDY(UART0)) {}              
    UART_EVENT_RXDRDY(UART0) = 0;                    
    uint8_t b = uart_recv(UART0);                    
    push_rx_cmd_buff(rx_cmd_buff_o, b);              
  }                                                  
  uart_stop_rx(UART0);                               
}

void reply(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o) {
  if(                                                  
  rx_cmd_buff_o->state==RX_CMD_BUFF_STATE_COMPLETE && 
  tx_cmd_buff_o->empty                                
  ) {                                                  
    write_reply(rx_cmd_buff_o, tx_cmd_buff_o);         
  }                                                    
}

void tx_uart(tx_cmd_buff_t* tx_cmd_buff_o) {
  uart_start_tx(UART0);                              
  while(                                             
  !(tx_cmd_buff_o->empty)                           
  ) {                                                
    uint8_t b = pop_tx_cmd_buff(tx_cmd_buff_o);      
    uart_send(UART0,b);                              
    while(!UART_EVENT_TXDRDY(UART0)) {}              
    UART_EVENT_TXDRDY(UART0) = 0;                    
    if(tx_cmd_buff_o->empty) {                       
      gpio_toggle(P0, GPIO30);                       
      gpio_toggle(P0, GPIO29);                       
    }                                                
  }                                                  
  uart_stop_tx(UART0);                               
}

void enable_rf(void) {
    P1_OUTSET = RF_PLD;
}

void disable_rf(void) {
    P1_OUTCLR = RF_PLD;
}

void enable_rx(void) {
  gpio_clear(P0, TX_EN_PIN);
  gpio_set(P0, RX_EN_PIN);
}

void enable_tx(void) {
  gpio_clear(P0, RX_EN_PIN);
  gpio_set(P0, TX_EN_PIN);
}

void send_alive(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_ACK_OPCODE, NULL, 3);
}

void cdh_enable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_PAY_EN, 0x01, VAR_ENABLE};
  msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

void cdh_disable_pay(rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff){
  uint8_t my_payload[] = {VAR_CODE_PAY_EN, 0x01, VAR_DISABLE};
  msg_to_cdh(rx_cmd_buff, tx_cmd_buff, COMMON_DATA_OPCODE, my_payload, 3);
}

uint8_t dummy_packet[4] = {0xde, 0xad, 0xbe, 0xef};

void blast_carrier(void) {
  enable_rf();
  enable_tx();
  RADIO_TASK_TXEN = 1;
  while (RADIO_EVENT_READY == 0) {}
  RADIO_EVENT_READY = 0;
  while (1) {
    for(int i=0; i<4000000; i++) {
      __asm__("nop");
    }
    gpio_toggle(P0, LED2);
  }
}

void flash_erase_page(uint32_t page) {
  NVMC_CONFIG = NVMC_CONFIG_EEN;
  __asm__("isb 0xF");
  __asm__("dsb 0xF");
  NVMC_ERASEPAGE = page*(0x00001000);
  while(NVMC_READY==NVMC_READY_BUSY) {}
  NVMC_CONFIG = NVMC_CONFIG_REN;
  __asm__("isb 0xF");
  __asm__("dsb 0xF");
}

void radio_set_rx_address(uint8_t addr_index) {
    RADIO_RXADDRESSES |= (1 << addr_index);
}

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
    for(size_t i=0; i<BYTES_PER_BLR_PLD; i+=4) {
      if((i+start_addr)%BYTES_PER_FLASH_PAGE==0) {
        flash_erase_page((i+start_addr)/BYTES_PER_FLASH_PAGE);
      }
      uint32_t word = *(uint32_t*)((rx_cmd_buff->data)+PLD_START_INDEX+1+i);
      NVMC_CONFIG = NVMC_CONFIG_WEN;
      __asm__("isb 0xF");
      __asm__("dsb 0xF");
      MMIO32(i+start_addr) = word;
      while(NVMC_READY==NVMC_READY_BUSY) {}
      NVMC_CONFIG = NVMC_CONFIG_REN;
      __asm__("isb 0xF");
      __asm__("dsb 0xF");
    }
    return 1;
  } else {
    return 0;
  }
}

int handle_bootloader_jump(void){
  return 0;
}

int bootloader_active(void) {
  return 1;
}

int handle_bootloader_erase(void){
  for(size_t subpage_id=0; subpage_id<255; subpage_id++) {
    if((subpage_id*BYTES_PER_BLR_PLD)%BYTES_PER_FLASH_PAGE==0) {
      flash_erase_page(8+(subpage_id*BYTES_PER_BLR_PLD)/BYTES_PER_FLASH_PAGE);
    }
  }
  return 1;
}

int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff){
  if(
  rx_cmd_buff->state==RX_CMD_BUFF_STATE_COMPLETE &&
  rx_cmd_buff->data[OPCODE_INDEX]==BOOTLOADER_WRITE_PAGE_OPCODE
  ) {
    uint32_t subpage_id = (uint32_t)(rx_cmd_buff->data[PLD_START_INDEX]);
    if((subpage_id*BYTES_PER_BLR_PLD)%BYTES_PER_FLASH_PAGE==0) {
      flash_erase_page(8+(subpage_id*BYTES_PER_BLR_PLD)/BYTES_PER_FLASH_PAGE);
    }
    uint32_t start_addr = APP_ADDR+subpage_id*BYTES_PER_BLR_PLD;
    for(size_t i=0; i<BYTES_PER_BLR_PLD; i+=4) {
      uint32_t word = *(uint32_t*)((rx_cmd_buff->data)+PLD_START_INDEX+1+i);
      NVMC_CONFIG = NVMC_CONFIG_WEN;
      __asm__("isb 0xF");
      __asm__("dsb 0xF");
      MMIO32(i+start_addr) = word;
      while(NVMC_READY==NVMC_READY_BUSY) {}
      NVMC_CONFIG = NVMC_CONFIG_REN;
      __asm__("isb 0xF");
      __asm__("dsb 0xF");
    }
    return 1;
  } else {
    return 0;
  }
}

// non-blocking radio functions for router loop

// "working" radio

void blink_led(int pin, int times) {
  for(int j = 0; j < times; j++) {
    for(int i = 0; i < 4000000; i++) {
      __asm__("nop");
    }
    gpio_toggle(P0, pin);
  }
  gpio_clear(P0, pin);
}


// === FULL function body goes here ===
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
	
	while (RADIO_EVENT_CCAIDLE == 0){
		__asm__("nop");
	}
	
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

	// Packet fully received --> flash LED
	blink_led(GPIO30, 3);
	
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


// Not working radio
int radio_packet_ready(void);

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

int radio_packet_ready(void) {
  return (RADIO_EVENT_END == 1);
}

void fetch_radio_packet(rx_cmd_buff_t* rx_buff) {
  if (RADIO_EVENT_END == 1) {
    uint8_t energy_level = sample_ed();
    dma_rx_buff[11] = energy_level;

    RADIO_EVENT_END = 0;
    
    // total size is encoded in the first byte mapped by radio hardware
    uint8_t total_len = dma_rx_buff[0]; 
    
    // The DMA packet consists of [len_byte] [real_data_bytes...]
    // real_data starts at index 1
    for (int i = 0; i < total_len; i++) {
      push_rx_cmd_buff(rx_buff, dma_rx_buff[i+1]);
    }
    
    RADIO_EVENT_FRAMESTART = 0;
    RADIO_EVENT_PHYEND = 0;
    RADIO_EVENT_END = 0;
    RADIO_TASK_DISABLE = 1;
    while(RADIO_STATE!=0){ __asm__("nop"); }
    while (RADIO_EVENT_DISABLED == 0) { __asm__("nop"); }
    RADIO_EVENT_DISABLED = 0;

    radio_listen();
  }
}

void radio_listen(void) {
  RADIO_TASK_DISABLE = 1;
  while(RADIO_STATE!=0){}
  RADIO_EVENT_DISABLED = 0;

  radio_set_packet_ptr(dma_rx_buff);
  
  // Tie RX_READY dynamically to START, automating RX. Let hardware halt receiver after PHYEND
  MMIO32(RADIO_BASE + 0x200) = RADIO_SHORTS_RXREADY_START | RADIO_SHORTS_PHYEND_DISABLE;

  RADIO_TASK_RXEN = 1;
}

void radio_send(tx_cmd_buff_t* tx_buff) {
  // length byte at idx 0 for MAC engine framing, containing TAB packet + IEEE 802 CRC sizing
  dma_tx_buff[0] = tx_buff->end_index + 2;

  // Insert TAB buffer exactly behind the MAC length frame
  for (size_t i = 0; i < tx_buff->end_index; i++) {
    dma_tx_buff[i+1] = tx_buff->data[i];
  }

  RADIO_TASK_DISABLE = 1;
  while(RADIO_STATE != 0) {}
  RADIO_EVENT_DISABLED = 0;

  radio_set_packet_ptr(dma_tx_buff);

  // Configure shortcuts to seamlessly tie RX_READY -> CCA, CCA_IDLE -> TX_EN, TX_RDY -> TX_START
  MMIO32(RADIO_BASE + 0x200) = RADIO_SHORTS_RXREADY_CCASTART | \
                               RADIO_SHORTS_CCAIDLE_TXEN | \
                               RADIO_SHORTS_CCABUSY_DISABLE | \
                               RADIO_SHORTS_TXREADY_START | \
                               RADIO_SHORTS_PHYEND_DISABLE;
                               
  RADIO_TASK_RXEN = 1;
  
  // Wait for the entire chain to collapse back to DISABLED
  while(RADIO_EVENT_DISABLED == 0) {
      if(RADIO_EVENT_CCABUSY == 1) {
          // Path busy; halt chain and retry manually
          RADIO_EVENT_CCABUSY = 0;
          RADIO_TASK_CCASTART = 1;
      }
      __asm__("nop");
  }
  RADIO_EVENT_DISABLED = 0;

  // Strip shortcuts to zero
  MMIO32(RADIO_BASE + 0x200) = 0;

  clear_tx_cmd_buff(tx_buff);
  radio_listen();
}