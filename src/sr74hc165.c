#include "sr74hc165.h"
#include <util/delay.h>

void sr74hc165_init(uint8_t num_modules) {
	init_spi();
	SR_PL_CE_DDR |= (1<<SR_CE_PIN)|(1<<SR_PL_PIN); // set as output
	SR_PL_CE_PORT |= (1<<SR_PL_PIN);
	SR_PL_CE_PORT |= (1<<SR_CE_PIN);
}

void sr74hc165_read(unsigned char* output_buffer, uint8_t num_modules) {
	// load inputs to shift register
	SPI_PORT |= (1<<SPI_SCK);
	SR_PL_CE_PORT &= ~(1<<SR_PL_PIN);
	_delay_us(1);
	SR_PL_CE_PORT |= (1<<SR_PL_PIN);
	// chip select
	SR_PL_CE_PORT &= ~(1<<SR_CE_PIN);
	// now shift our data into the output_buffer
	uint8_t i=0;
	for(; i<num_modules; i++) {
		*(output_buffer+i) = spi_transfer(0x00);
	}
	SR_PL_CE_PORT |= (1<<SR_CE_PIN);
}

