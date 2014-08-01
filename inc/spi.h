#ifndef __SPI_H_
#define __SPI_H_
#include <avr/io.h>
#include <stdint.h>

#ifndef SPI_DDR
#pragma message "SPI_DDR not defined - defaulting to DDRB"
#define SPI_DDR		DDRB
#endif
#ifndef SPI_PORT
#pragma message "SPI_PORT not defined - defaulting to PORTB"
#define SPI_PORT	PORTB
#endif
#ifndef SPI_MOSI
#pragma message "SPI_MOSI not defined - defaulting to PB3"
#define SPI_MOSI	PB3
#endif
#ifndef SPI_MISO
#pragma message "SPI_MISO not defined - defaulting to PB4"
#define SPI_MISO	PB4
#endif
#ifndef SPI_SCK
#pragma message "SPI_SCK not defined - defaulting to PB5"
#define SPI_SCK		PB5
#endif
// CHIP_SELECT line must be defined by the according init-function of the chip
// and also set before spi_transfer and unset afterwards...

/**
 * Function to initialize the SPI-Interface except CS-Pin. Please initialize the
 * CS-Pin of your Chip in its own environmental function.
 * Initializes the SPI-Interface using 1<<SPE 1<<MSTR 1<<CPHA and 1<<SPI2X
 */
void init_spi(void);

/**
 * Function to transmit and receive one byte of data via SPI
 * @param data the byte to transmit
 * @return the received byte after transmission
 */
uint8_t spi_transfer(uint8_t data);

#endif // __SPI_H_
