#ifndef _SPI_H_
#define	_SPI_H_

#define SPI_EN GPIO_NUM_5

void spi_slave_post_trans_cb();
void spi_slave_init(uint64_t mosi_pin, uint64_t miso_pin, uint64_t sclk_pin, uint64_t cs_pin);
uint8_t spi_slave_queue_data(uint8_t * data_buf, uint8_t data_len);
uint8_t * spi_slave_get_data(uint8_t * data_len);

#endif