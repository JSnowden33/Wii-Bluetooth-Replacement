#ifndef SPI_H_
#define SPI_H_

#define SPI_EN PORTBbits.RB4
#define SPI_CS LATBbits.LATB3

void spi_master_init(const uint32_t clk, uint8_t mode);
void spi_off();
uint8_t spi_busy();
uint8_t spi_tx_full();
uint8_t spi_rx_full();
uint8_t spi_transfer(uint8_t data);
void spi_write(uint8_t data);
uint8_t spi_read();

#endif