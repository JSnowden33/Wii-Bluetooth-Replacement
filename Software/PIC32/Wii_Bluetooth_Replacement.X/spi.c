#include <xc.h>
#include "spi.h"
#include "delay.h"

void spi_master_init(const uint32_t clk, uint8_t mode) {
	SDI1Rbits.SDI1R = 2;    // SDI on RB1
	RPB2Rbits.RPB2R = 3;    // SDO on RB2
	// SCK (RB14) is not remappable

	SPI1CONbits.ON = 0;
	SPI1CONbits.ENHBUF = 0;       // FIFO buffers disabled

	SPI1BRG = (PBCLK / (2 * clk)) - 1;

	SPI1CONbits.MSTEN = 1;      // Master mode
	SPI1CONbits.DISSDO = 0;     // SDO controlled by module
	SPI1CONbits.DISSDI = 0;     // SDI controlled by module
	SPI1CONbits.SMP = 0;        // SDI sampling time (0 = middle, 1 = end)
	SPI1CONbits.MCLKSEL = 0;    // BRG powered by peripheral clock (PBCLK)
	SPI1CONbits.FRMEN = 0;      // Framed SPI disabled
	SPI1CONbits.FRMPOL = 0;     // CS active low
	SPI1CONbits.MSSEN = 0;      // CS not controlled by module
	SPI1CONbits.SIDL = 1;       // Do no operate in idle mode
	SPI1CONbits.MODE16 = 0;     // 8-bit mode
	SPI1CONbits.MODE32 = 0;
	SPI1CON2bits.SPISGNEXT = 0;
	SPI1CON2bits.IGNROV = 1;
	SPI1CON2bits.IGNTUR = 1;
	SPI1CON2bits.AUDEN = 0;     // Audio mode disabled

	switch (mode) {
		case 0:
			SPI1CONbits.CKE = 1;    // SDO transmission edge (0 = idle-to-active CLK, 1 = active-to-idle CLK)
			SPI1CONbits.CKP = 0;    // CLK polarity (0 = idle low, 1 = idle high)
			break;
		case 1:
			SPI1CONbits.CKE = 0;
			SPI1CONbits.CKP = 0;
			break;
		case 2:
			SPI1CONbits.CKE = 1;
			SPI1CONbits.CKP = 1;
			break;
		case 3:
			SPI1CONbits.CKE = 0;
			SPI1CONbits.CKP = 1;
			break;
		default:
			SPI1CONbits.CKE = 1;
			SPI1CONbits.CKP = 0;
			break;
	}

	SPI1STATbits.SPIROV = 0;
	SPI1CONbits.ON = 1;
	SPI_CS = 1;
}

void spi_off() {
	SPI1CONbits.ON = 0;
}

uint8_t spi_busy() {
	return SPI1STATbits.SPIBUSY;
}

uint8_t spi_tx_full() {
	return SPI1STATbits.SPITBF;
}

uint8_t spi_rx_full() {
	return SPI1STATbits.SPIRBF;
}

uint8_t spi_transfer(uint8_t data) {
	SPI1BUF = data;

	uint16_t timeout = 5000;
	while(spi_tx_full() && timeout--);   // Wait until data is transmitted

	timeout = 5000;
	while(!spi_rx_full() && timeout--);  // Wait until data is received

	return SPI1BUF;
}

void spi_write(uint8_t data) {
	SPI1BUF = data;
}

uint8_t spi_read() {
	return SPI1BUF;
}