// radio.h
#ifndef RADIO_H
#define RADIO_H

#include <zephyr/kernel.h>
#include "tab.h"

// Initialize the radio subsystem and thread
void init_radio(void);

// Thread-safe function for other threads to send a packet
int radio_send_packet(tx_cmd_buff_t *tx_buff);

// Hardware Controls (FEM)
void enable_rf(void);
void disable_rf(void);
void enable_rx(void);
void disable_rx(void); 
void enable_tx(void);
void disable_tx(void);

// Diagnostics/Testing Functions
void radio_blast_cw_max_power(void);

#endif