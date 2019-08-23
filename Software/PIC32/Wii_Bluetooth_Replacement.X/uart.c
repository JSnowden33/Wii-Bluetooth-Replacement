#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <xc.h>
#include "uart.h"
#include "delay.h"

void uart_configure(uint32_t baud){
	RPB15Rbits.RPB15R = 1; //SET RB15 to TX

	U1MODE = 0;
	U1STA = 0;
	U1BRG = ((PBCLK / (16 * baud)) - 1);
	U1MODEbits.PDSEL = 0;
	U1MODEbits.STSEL = 0;
	U1STAbits.UTXEN = 1;
	U1MODEbits.ON = 1;
}

void uart_transmit(const char *buffer, uint8_t newline_return) {
	unsigned int size = strlen(buffer);
	while(size) {
		while(U1STAbits.UTXBF); // wait while TX buffer full
		U1TXREG = *buffer; // send single character to transmit buffer

		buffer++; // transmit next character on following loop
		size--; // loop until all characters sent (when size = 0)
	}
	if (newline_return) {
		while(U1STAbits.UTXBF);
		U1TXREG = 10;   // Newline
		while(U1STAbits.UTXBF);
		U1TXREG = 13;   // Return
	}
	while(!U1STAbits.TRMT); // wait for last transmission to finish
}

void uart_transmit_val(uint64_t val, uint8_t num_chars, uint8_t newline_return) {
	int8_t i;
	for (i = num_chars - 1; i >= 0; i--) {
		while(U1STAbits.UTXBF); // wait while TX buffer full
		U1TXREG = "0123456789ABCDEF"[(val & (0xF << (i * 4))) >> (i * 4)];
	}
	if (newline_return) {
		while(U1STAbits.UTXBF);
		U1TXREG = 10;   // Newline
		while(U1STAbits.UTXBF);
		U1TXREG = 13;   // Return
	}
	while(!U1STAbits.TRMT); // wait for last transmission to finish
}

void uart_hexdump(uint8_t * buf, uint8_t len) {
	uint8_t i;
	for (i = 0; i < len; i++) {
		uart_transmit_val(buf[i], 2, 0);
		uart_transmit(" ", 0);
	}
	uart_transmit("", 1);
}
