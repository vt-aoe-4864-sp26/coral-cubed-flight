// com_main.c
// Unified COM driver mapping UART logic natively down to radio broadcasts 
// Functioning seamlessly for both CDH integration or demo.py Ground interactions.

#include <stddef.h>
#include <stdint.h>
#include <libopencm3/nrf/clock.h>
#include <libopencm3/nrf/gpio.h>
#include <libopencm3/nrf/uart.h>
#include <com.h>

#define P1 (GPIO_BASE + 0x300)
#define RX_BUFFER_ADDR 0x20000000
#define TRX_BUFFER_SIZE 125

void send_reliable_payload(uint8_t* payload, size_t length, uint8_t* reply_buffer) {
	int attempts = 0;
	int success = 0;
	while (attempts < 5 && !success) {
		tx_radio(payload, length);
		
		// Set a 200,000 NOP loop timeout (~3-4ms timeout for ACK return)
		if (rx_radio(reply_buffer, TRX_BUFFER_SIZE, 200000)) {
			// Check if packet passed CRC
			if (reply_buffer[15] == 1) { 
				success = 1;
			}
		} else {
			// Timeout elapsed
			attempts++;
		}
	}
}

int main(void) {
	// MCU initialization
	init_clock();
	init_leds();
	init_uart();
	init_radio();
	
	rx_cmd_buff_t uart_rx = {.size=CMD_MAX_LEN, .route_id=COM};
	tx_cmd_buff_t uart_tx = {.size=CMD_MAX_LEN};
	clear_rx_cmd_buff(&uart_rx);
	clear_tx_cmd_buff(&uart_tx);

	uint8_t* radio_pckt_in = (uint8_t*)(volatile uint8_t *)(RX_BUFFER_ADDR);
	uint8_t radio_pckt_out[TRX_BUFFER_SIZE];
	
	uart_start_rx(UART0);

	while(1) {
		// 1. Service UART incoming characters (from flatsat OR demo.py script)
		if (UART_EVENT_RXDRDY(UART0)) {
			UART_EVENT_RXDRDY(UART0) = 0;
			push_rx_cmd_buff(&uart_rx, uart_recv(UART0));
		}

		// 2. Transmit assembled UART packet transparently over radio
		if(uart_rx.state == RX_CMD_BUFF_STATE_COMPLETE) {
			
			uint8_t dest_id = (uart_rx.data[ROUTE_INDEX] & 0xf0) >> 4;
			if (dest_id == COM) {
				// Destined for the COM board directly, reply locally
				reply(&uart_rx, &uart_tx);
			} else {
				// Package up the raw TAB bytes for radio transmission (pre-pending length)
				radio_pckt_out[0] = uart_rx.end_index + 2; // packet total len mapping
				for(size_t i = 0; i < uart_rx.end_index; i++) {
					radio_pckt_out[i+1] = uart_rx.data[i];
				}
				
				// Ship it securely across the sky
				send_reliable_payload(radio_pckt_out, uart_rx.end_index + 1, radio_pckt_in);
				
				// Route the received radio payload dynamically back up the UART pipeline 
				// to whoever originally sent it!
				for(size_t i = 0; i < radio_pckt_in[0]; i++) { 
					uart_tx.data[i] = radio_pckt_in[i+1];
				}
				uart_tx.end_index = radio_pckt_in[0];
				uart_tx.empty = 0;
			}
			
			clear_rx_cmd_buff(&uart_rx);
		}

    	// 3. Clear pending UART transmissions automatically
		if(!uart_tx.empty) {
			tx_uart(&uart_tx);
		}

		// 4. Finally, block on radio passively using a long timeout
		if (rx_radio(radio_pckt_in, TRX_BUFFER_SIZE, 5000000)) {
			// Random Ground Station traffic appeared on our Antenna
			// Forward it aggressively into our UART hardware instantly.
			if (radio_pckt_in[15] == 1) { // valid crc byte check
				for(size_t i = 0; i < radio_pckt_in[0]; i++) { 
					uart_tx.data[i] = radio_pckt_in[i+1];
				}
				uart_tx.end_index = radio_pckt_in[0];
				uart_tx.empty = 0;
				
				tx_uart(&uart_tx);
			}
		}

	}

	return 0;
}

