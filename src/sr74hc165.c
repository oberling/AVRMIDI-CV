#include "sr74hc165.h"

void sr74hc165_init(void) {
	init_spi();
	SR_PL_CE_DDR |= (1<<SR_CE_PIN)|(1<<SR_PL_PIN); // set as output
	SR_PL_CE_PORT |= (1<<SR_PL_PIN);
	SR_PL_CE_PORT |= (1<<SR_CE_PIN);
}

void sr74hc165_read(unsigned char* output_buffer, uint8_t num_modules) {
	// change to right clock phase for this chip
	// ATTENTION: this is only save if this method cannot be interrupted by another method that might overwrite SPCR again!
	SPCR &= ~(1<<CPHA);
	// load inputs to shift register
	SR_PL_CE_PORT &= ~(1<<SR_PL_PIN);
	__asm("nop\n\t");
	__asm("nop\n\t"); // ... just to make shure ...
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

