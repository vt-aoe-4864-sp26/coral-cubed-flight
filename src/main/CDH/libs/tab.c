// tab.c
// TAB serial communication protocol header file
//
// Written by Bradley Denby
// Other contributors: Chad Taylor, Jack Rathert
//
// See the top-level LICENSE file for the license.

// Standard library
#include <stddef.h> // size_t
#include <stdint.h> // uint8_t, uint32_t

// TAB
#include <tab.h>    // TAB header

// External handler functions
extern int handle_common_data(common_data_t common_data_buff_i, rx_cmd_buff_t* rx_cmd_buff, tx_cmd_buff_t* tx_cmd_buff);
extern int handle_bootloader_erase(void);
extern int handle_bootloader_write_page(rx_cmd_buff_t* rx_cmd_buff);
extern int handle_bootloader_write_page_addr32(rx_cmd_buff_t* rx_cmd_buff);
extern int handle_bootloader_jump(void);
extern int bootloader_active(void);

// ========== Stateful Message Tracking ========== //

#define MAX_PENDING_IDS 16
static uint16_t pending_tx_ids[MAX_PENDING_IDS] = {0};
static int pending_tx_active[MAX_PENDING_IDS] = {0};

static void tab_track_outgoing_id(uint16_t msg_id) {
  for (int i = 0; i < MAX_PENDING_IDS; i++) {
    if (!pending_tx_active[i]) {
      pending_tx_ids[i] = msg_id;
      pending_tx_active[i] = 1;
      return;
    }
  }
}

static int check_and_clear_pending_id(uint16_t msg_id) {
  for (int i = 0; i < MAX_PENDING_IDS; i++) {
    if (pending_tx_active[i] && pending_tx_ids[i] == msg_id) {
      pending_tx_active[i] = 0; // Clear it
      return 1; // It was a reply to us
    }
  }
  return 0; // Unsolicited
}

// ========== Helper functions ========== //

// Clears rx_cmd_buff data and resets state and indices
void clear_rx_cmd_buff(rx_cmd_buff_t* rx_cmd_buff_o) {
  rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_START_BYTE_0;
  rx_cmd_buff_o->start_index = 0;
  rx_cmd_buff_o->end_index = 0;
  for(size_t i=0; i<rx_cmd_buff_o->size; i++) {
    rx_cmd_buff_o->data[i] = ((uint8_t)0x00);
  }
}

// Clears tx_cmd_buff data and resets state and indices
void clear_tx_cmd_buff(tx_cmd_buff_t* tx_cmd_buff_o) {
  tx_cmd_buff_o->empty = 1;
  tx_cmd_buff_o->start_index = 0;
  tx_cmd_buff_o->end_index = 0;
  for(size_t i=0; i<tx_cmd_buff_o->size; i++) {
    tx_cmd_buff_o->data[i] = ((uint8_t)0x00);
  }
}

// ========== Protocol functions ========== //

// Attempts to push byte to end of rx_cmd_buff
void push_rx_cmd_buff(rx_cmd_buff_t* rx_cmd_buff_o, uint8_t b) {
  switch(rx_cmd_buff_o->state) {
    case RX_CMD_BUFF_STATE_START_BYTE_0:
      if(b==START_BYTE_0) {
        rx_cmd_buff_o->data[START_BYTE_0_INDEX] = b;
        rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_START_BYTE_1;
      }
      break;
    case RX_CMD_BUFF_STATE_START_BYTE_1:
      if(b==START_BYTE_1) {
        rx_cmd_buff_o->data[START_BYTE_1_INDEX] = b;
        rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_MSG_LEN;
      } else {
        clear_rx_cmd_buff(rx_cmd_buff_o);
      }
      break;
    case RX_CMD_BUFF_STATE_MSG_LEN:
      if(((uint8_t)0x06)<=b) {
        rx_cmd_buff_o->data[MSG_LEN_INDEX] = b;
        rx_cmd_buff_o->start_index = ((uint8_t)0x09);
        rx_cmd_buff_o->end_index = (b+((uint8_t)0x03));
        rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_HWID_LSB;
      } else {
        clear_rx_cmd_buff(rx_cmd_buff_o);
      }
      break;
    case RX_CMD_BUFF_STATE_HWID_LSB:
      rx_cmd_buff_o->data[HWID_LSB_INDEX] = b;
      rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_HWID_MSB;
      break;
    case RX_CMD_BUFF_STATE_HWID_MSB:
      rx_cmd_buff_o->data[HWID_MSB_INDEX] = b;
      rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_MSG_ID_LSB;
      break;
    case RX_CMD_BUFF_STATE_MSG_ID_LSB:
      rx_cmd_buff_o->data[MSG_ID_LSB_INDEX] = b;
      rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_MSG_ID_MSB;
      break;
    case RX_CMD_BUFF_STATE_MSG_ID_MSB:
      rx_cmd_buff_o->data[MSG_ID_MSB_INDEX] = b;
      
      // piece together the 16-bit id we just heard on the line
      rx_cmd_buff_o->bus_msg_id = (rx_cmd_buff_o->data[MSG_ID_MSB_INDEX] << 8) | 
                                  rx_cmd_buff_o->data[MSG_ID_LSB_INDEX];

      rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_ROUTE;
      break;
    case RX_CMD_BUFF_STATE_ROUTE:
      rx_cmd_buff_o->data[ROUTE_INDEX] = b;
      rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_OPCODE;
      break;
    case RX_CMD_BUFF_STATE_OPCODE:
      rx_cmd_buff_o->data[OPCODE_INDEX] = b;
      if(rx_cmd_buff_o->start_index<rx_cmd_buff_o->end_index) {
        rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_PLD;
      } else {
        rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_COMPLETE;
      }
      break;
    case RX_CMD_BUFF_STATE_PLD:
      if(rx_cmd_buff_o->start_index<rx_cmd_buff_o->end_index) {
        rx_cmd_buff_o->data[rx_cmd_buff_o->start_index] = b;
        rx_cmd_buff_o->start_index += 1;
      }
      if(rx_cmd_buff_o->start_index==rx_cmd_buff_o->end_index) {
        rx_cmd_buff_o->state = RX_CMD_BUFF_STATE_COMPLETE;
      }
      break;
    case RX_CMD_BUFF_STATE_COMPLETE:
      break;
    default:
      break;
  }
}

// Attempts to clear rx_cmd_buff and populate tx_cmd_buff with reply
void write_reply(rx_cmd_buff_t* rx_cmd_buff_o, tx_cmd_buff_t* tx_cmd_buff_o) {
  if(
    rx_cmd_buff_o->state==RX_CMD_BUFF_STATE_COMPLETE &&
    tx_cmd_buff_o->empty
  ) {

    tx_cmd_buff_o->data[START_BYTE_0_INDEX] = START_BYTE_0;
    tx_cmd_buff_o->data[START_BYTE_1_INDEX] = START_BYTE_1;
    tx_cmd_buff_o->data[HWID_LSB_INDEX] = rx_cmd_buff_o->data[HWID_LSB_INDEX];
    tx_cmd_buff_o->data[HWID_MSB_INDEX] = rx_cmd_buff_o->data[HWID_MSB_INDEX];
    tx_cmd_buff_o->data[MSG_ID_LSB_INDEX] = rx_cmd_buff_o->data[MSG_ID_LSB_INDEX];
    tx_cmd_buff_o->data[MSG_ID_MSB_INDEX] = rx_cmd_buff_o->data[MSG_ID_MSB_INDEX];
    tx_cmd_buff_o->data[ROUTE_INDEX] =
      (0x0f & rx_cmd_buff_o->data[ROUTE_INDEX]) << 4 |
      (0xf0 & rx_cmd_buff_o->data[ROUTE_INDEX]) >> 4;

    size_t i = 0;
    common_data_t common_data_buff = {.size=PLD_MAX_LEN};
    int success = 0;
    int send_reply = 1; // Flag to dictate transmission

    switch(rx_cmd_buff_o->data[OPCODE_INDEX]) {
      case COMMON_ACK_OPCODE: {
        uint16_t incoming_id = (rx_cmd_buff_o->data[MSG_ID_MSB_INDEX] << 8) | 
                                rx_cmd_buff_o->data[MSG_ID_LSB_INDEX];
        
        if (check_and_clear_pending_id(incoming_id)) {
            // It's a reply to our command. Close the loop silently.
            send_reply = 0; 
        } else {
            // Unsolicited ACK! Treat it as a ping and bounce it back.
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
            tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_ACK_OPCODE;
            send_reply = 1;
        }
        break;
      }
      case COMMON_NACK_OPCODE:
        send_reply = 0; // Always consume NACKs quietly to prevent loops
        break;
      case COMMON_DEBUG_OPCODE:
        tx_cmd_buff_o->data[MSG_LEN_INDEX] = rx_cmd_buff_o->data[MSG_LEN_INDEX];
        tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_DEBUG_OPCODE;
        for(i=PLD_START_INDEX; i<rx_cmd_buff_o->end_index; i++) {
          tx_cmd_buff_o->data[i] = rx_cmd_buff_o->data[i];
        }
        break;
      case COMMON_DATA_OPCODE:
        for(i=PLD_START_INDEX; i<rx_cmd_buff_o->end_index; i++) {
          common_data_buff.data[i-PLD_START_INDEX] = rx_cmd_buff_o->data[i];
        }
        common_data_buff.end_index = rx_cmd_buff_o->end_index-PLD_START_INDEX;
        success = handle_common_data(common_data_buff,rx_cmd_buff_o, tx_cmd_buff_o);
        
        if(tx_cmd_buff_o->empty) {
          if(success) {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
            tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_ACK_OPCODE;
          } else {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
            tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_NACK_OPCODE;
          }
        } else {
            send_reply = 1; // Custom payload was built, guarantee transmission
        }
        break;
      case BOOTLOADER_ACK_OPCODE:
        tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
        tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_NACK_OPCODE;
        break;
      case BOOTLOADER_NACK_OPCODE:
        tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
        tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_NACK_OPCODE;
        break;
      case BOOTLOADER_PING_OPCODE:
        if(bootloader_active()) {
          tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x07);
          tx_cmd_buff_o->data[OPCODE_INDEX] = BOOTLOADER_ACK_OPCODE;
          tx_cmd_buff_o->data[PLD_START_INDEX] = BOOTLOADER_ACK_REASON_PONG;
        } else {
          tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
          tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_NACK_OPCODE;
        }
        break;
      case BOOTLOADER_ERASE_OPCODE:
        if(bootloader_active()) {
          success = handle_bootloader_erase();
          if(success) {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x07);
            tx_cmd_buff_o->data[OPCODE_INDEX] = BOOTLOADER_ACK_OPCODE;
            tx_cmd_buff_o->data[PLD_START_INDEX] = BOOTLOADER_ACK_REASON_ERASED;
          } else {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
            tx_cmd_buff_o->data[OPCODE_INDEX] = BOOTLOADER_NACK_OPCODE;
          }
        } else {
          tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
          tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_NACK_OPCODE;
        }
        break;
      case BOOTLOADER_WRITE_PAGE_OPCODE:
        if(bootloader_active()) {
          success = handle_bootloader_write_page(rx_cmd_buff_o);
          if(success) {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x07);
            tx_cmd_buff_o->data[OPCODE_INDEX] = BOOTLOADER_ACK_OPCODE;
            tx_cmd_buff_o->data[PLD_START_INDEX] =
              rx_cmd_buff_o->data[PLD_START_INDEX];
          } else {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
            tx_cmd_buff_o->data[OPCODE_INDEX] = BOOTLOADER_NACK_OPCODE;
          }
        } else{
          tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
          tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_NACK_OPCODE;
        }
        break;
      case BOOTLOADER_WRITE_PAGE_ADDR32_OPCODE:
        if(bootloader_active()) {
          success = handle_bootloader_write_page_addr32(rx_cmd_buff_o);
          if(success) {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x0a);
            tx_cmd_buff_o->data[OPCODE_INDEX] = BOOTLOADER_ACK_OPCODE;
            tx_cmd_buff_o->data[PLD_START_INDEX] = rx_cmd_buff_o->data[PLD_START_INDEX];
            tx_cmd_buff_o->data[PLD_START_INDEX+1] = rx_cmd_buff_o->data[PLD_START_INDEX+1];
            tx_cmd_buff_o->data[PLD_START_INDEX+2] = rx_cmd_buff_o->data[PLD_START_INDEX+2];
            tx_cmd_buff_o->data[PLD_START_INDEX+3] = rx_cmd_buff_o->data[PLD_START_INDEX+3];
          } else {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
            tx_cmd_buff_o->data[OPCODE_INDEX] = BOOTLOADER_NACK_OPCODE;
          }
        } else{
          tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
          tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_NACK_OPCODE;
        }
        break;
      case BOOTLOADER_JUMP_OPCODE:
        if(bootloader_active()) {
          success = handle_bootloader_jump();
          if(success) {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x07);
            tx_cmd_buff_o->data[OPCODE_INDEX] = BOOTLOADER_ACK_OPCODE;
            tx_cmd_buff_o->data[PLD_START_INDEX] = BOOTLOADER_ACK_REASON_JUMPED;
          } else {
            tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
            tx_cmd_buff_o->data[OPCODE_INDEX] = BOOTLOADER_NACK_OPCODE;
          }
        } else {
          tx_cmd_buff_o->data[MSG_LEN_INDEX] = ((uint8_t)0x06);
          tx_cmd_buff_o->data[OPCODE_INDEX] = COMMON_NACK_OPCODE;
        }
        break;
      default:
        break;
    }
    
    // Process final transmission behavior based on our state tracking
    if (send_reply) {
      tx_cmd_buff_o->end_index = (tx_cmd_buff_o->data[MSG_LEN_INDEX]+((uint8_t)0x03));
      tx_cmd_buff_o->empty = 0;
    } else {
      tx_cmd_buff_o->empty = 1;
    }
    
    clear_rx_cmd_buff(rx_cmd_buff_o);
  }
}

// Attempts to pop byte from beginning of tx_cmd_buff
uint8_t pop_tx_cmd_buff(tx_cmd_buff_t* tx_cmd_buff_o) {
  uint8_t b = 0;
  if(
    !tx_cmd_buff_o->empty &&
    tx_cmd_buff_o->start_index<tx_cmd_buff_o->end_index
  ) {
    b = tx_cmd_buff_o->data[tx_cmd_buff_o->start_index];
    tx_cmd_buff_o->start_index += 1;
  }
  if(tx_cmd_buff_o->start_index==tx_cmd_buff_o->end_index) {
    clear_tx_cmd_buff(tx_cmd_buff_o);
  }
  return b;
}

static void build_new_msg(uint8_t dest, rx_cmd_buff_t* rx, tx_cmd_buff_t* tx, uint8_t opcode, uint8_t* pld, size_t pld_len) {
  if(tx->empty) {
    uint8_t local_route_id = COM; // Default to COM's ID
    uint8_t current_msg_id = 0;

    // Safely pull from RX if it exists (prevents unprompted demo crash)
    if (rx != NULL) {
        rx->bus_msg_id += 1;
        current_msg_id = rx->bus_msg_id;
        local_route_id = rx->route_id;
    }

    // Automatically log this outgoing ID so the board expects an ACK
    tab_track_outgoing_id((uint16_t)current_msg_id);

    tx->data[START_BYTE_0_INDEX] = START_BYTE_0;
    tx->data[START_BYTE_1_INDEX] = START_BYTE_1;
    tx->data[MSG_LEN_INDEX] = ((uint8_t)6) + pld_len; 
    
    tx->data[HWID_MSB_INDEX] = SAT_HWID_MSB;
    tx->data[HWID_LSB_INDEX] = SAT_HWID_LSB;

    tx->data[MSG_ID_LSB_INDEX] = (uint8_t)(current_msg_id & 0xff);
    tx->data[MSG_ID_MSB_INDEX] = (uint8_t)((current_msg_id >> 8) & 0xff);

    tx->data[ROUTE_INDEX] = (dest << 4) | (local_route_id & 0x0f);
    tx->data[OPCODE_INDEX] = opcode;

    for(size_t i = 0; i < pld_len; i++) {
      tx->data[PLD_START_INDEX + i] = pld[i];
    }

    tx->end_index = tx->data[MSG_LEN_INDEX] + 3;
    tx->empty = 0;
  }
}

// sends a brand new message to ground
void msg_to_gnd(rx_cmd_buff_t* rx, tx_cmd_buff_t* tx, uint8_t opcode, uint8_t* pld, size_t pld_len) {
  build_new_msg(GND, rx, tx, opcode, pld, pld_len);
}

// sends a brand new message to cdh
void msg_to_cdh(rx_cmd_buff_t* rx, tx_cmd_buff_t* tx, uint8_t opcode, uint8_t* pld, size_t pld_len) {
  build_new_msg(CDH, rx, tx, opcode, pld, pld_len);
}

// sends a brand new message to com
void msg_to_com(rx_cmd_buff_t* rx, tx_cmd_buff_t* tx, uint8_t opcode, uint8_t* pld, size_t pld_len) {
  build_new_msg(COM, rx, tx, opcode, pld, pld_len);
}

// sends a brand new message to payload / coral
void msg_to_pay(rx_cmd_buff_t* rx, tx_cmd_buff_t* tx, uint8_t opcode, uint8_t* pld, size_t pld_len) {
  build_new_msg(PLD, rx, tx, opcode, pld, pld_len);
}