#include "defines.h"
#include "dac8568c.h"
#include "spi.h"
#include <stdbool.h>
#include <util/delay.h>

#define DACPORT		PORTB
#define LDAC_PIN	PB0
#define CLR_PIN		PB1

#define DAC_TOGGLE_LDAC true

void __dac8568c_output_bytes(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, bool ldacswitch);

void dac8568c_init(void) {
	init_spi();
	dac8568c_write(DAC_SETUP_INTERNAL_REGISTER, 0, 1);
}

void dac8568c_write(uint8_t command, uint8_t address, uint32_t data) {
	switch (command) {
		case DAC_WRITE_UPDATE_N:
			{
				uint8_t b1 = 0b00000000|command; //padding at beginning of uint8_t 1
				uint8_t b2 = address << 4 | data >> 12; //4 address bits and 4 MSBs of data
				uint8_t b3 = (data << 4) >> 8; // middle 8 bits of data
				uint8_t b4 = (data << 12) >> 8 |0b00001111;
				__dac8568c_output_bytes(b1, b2, b3, b4, DAC_TOGGLE_LDAC);
				break;
			}
		case DAC_SETUP_INTERNAL_REGISTER:
			{
				uint8_t b1 = 0b00001000; //padding at beginning of byte
				uint8_t b2 = 0b00000000;
				uint8_t b3 = 0b00000000;
				uint8_t b4 = 0b00000000|data;
				__dac8568c_output_bytes(b1, b2, b3, b4, false);
				break;
			}
		case DAC_RESET:
			{
				uint8_t b1 = 0b00000111; //padding at beginning of byte
				uint8_t b2 = 0b00000000;
				uint8_t b3 = 0b00000000;
				uint8_t b4 = 0b00000000|data;
				__dac8568c_output_bytes(b1, b2, b3, b4, false);
				break;
			}
		case DAC_POWER:
			{
				uint8_t b1 = 0b00000100; //padding at beginning of byte
				uint8_t b2 = 0b00000000;
				uint8_t b3 = 0b00000000;
				uint8_t b4 = 0b11111111;
				__dac8568c_output_bytes(b1, b2, b3, b4, false);
				break;
			}
	}
}

void dac8568c_enable_internal_ref(void) {
	// 08 00 00 01:
	uint8_t b1 = 0b00001000;
	uint8_t b2 = 0b00000000;
	uint8_t b3 = 0b00000000;
	uint8_t b4 = 0b00000001;
	__dac8568c_output_bytes(b1, b2, b3, b4, false);
}

void dac8568c_disable_internal_ref(void) {
	// 08 00 00 00:
	uint8_t b1 = 0b00001000;
	uint8_t b2 = 0b00000000;
	uint8_t b3 = 0b00000000;
	uint8_t b4 = 0b00000000;
	__dac8568c_output_bytes(b1, b2, b3, b4, false);
}

void __dac8568c_output_bytes(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, bool ldacswitch){
	SPI_PORT &= (1<<SPI_SS);
	// wait till that pin is really set
	__asm("nop\n\t");
	spi_transfer(b1);
	spi_transfer(b2);
	spi_transfer(b3);
	spi_transfer(b4);
	if(ldacswitch) {
		SPI_PORT |= ~(1<<LDAC_PIN);
		_delay_us(1);
//		__asm("nop\n\t");
		SPI_PORT |= (1<<LDAC_PIN);
		_delay_us(1);
//		__asm("nop\n\t");
	}
	_delay_us(1);
//	__asm("nop\n\t");
	SPI_PORT |= (1<<SPI_SS);
}


