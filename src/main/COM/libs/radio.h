// radio.h
#ifndef RADIO_H
#define RADIO_H

void init_radio(void);

// Hardware Controls
void enable_rf(void);
void disable_rf(void);
void enable_rx(void);
void disable_rx(void); 
void enable_tx(void);
void disable_tx(void);

#endif