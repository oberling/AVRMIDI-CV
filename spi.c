#include "spi.h"

void init_spi(void) {
	// Setting Up SPI Mode 
	// PB2 - SS // PB3 - MOSI // PB5 - SCK
	DDRB = (1<<PB2) | (1<<PB3) | (1<<PB5);
	// PB4 - MISO
	// DDRB &= ~(1<<PB4);
	// SPE - SPI Enable // MSTR - is SPI Master // CPHA - Clock Phase
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<CPHA);
	// CPOL - Clock Phase // DORD - MSB/LSB (setting to MSB) // SPR1 SPR2 - Speed 0 0 - /4 CPU (or /2 CPU when SPI2X is set)
	// SPCR &= ~((1<<CPOL)|(1<<DORD)|(1<<SPR1)|(1<<SPR2));
	SPSR = (1<<SPI2X);
}

void spi_transfer(uint8_t data) {
	SPDR = data;
	while(!(SPSR & (1<<SPIF)));
}


