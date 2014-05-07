#ifndef __SPI_H_
#define __SPI_H_
#include <avr/io.h>
#include <stdint.h>

#define SPI_PORT	PORTB
#define SPI_SS		PB2
#define SPI_MOSI	PB3
#define SPI_SCK		PB5
#define SPI_MISO	PB4

void init_spi(void);
void spi_transfer(uint8_t data);
#endif
