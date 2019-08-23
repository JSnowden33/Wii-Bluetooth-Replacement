#ifndef UART_H__
#define UART_H__

void uart_configure(uint32_t baud);
void uart_transmit(const char *buffer, uint8_t newline_return);
void uart_transmit_val(uint64_t val, uint8_t num_chars, uint8_t newline_return);
void uart_hexdump(uint8_t * buf, uint8_t len);

#endif