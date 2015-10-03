#include "dac8568c.h"
#include "spi.h"
#include <stdbool.h>

typedef union {
	uint8_t b[4];
	uint32_t command;
} dac_command_t;

void __dac8568c_output_bytes(dac_command_t command, bool ldacswitch);

void dac8568c_init(void) {
	init_spi();
	DAC_DDR |= (1<<DAC_CS_PIN);
	SPI_PORT &= ~(1<<SPI_MOSI);
	DAC_PORT &= ~(1<<DAC_CS_PIN);
	DAC_PORT |= (1<<DAC_LDAC_PIN);
	DAC_PORT &= ~(1<<DAC_CLR_PIN);
	__asm("nop\n\t");
	__asm("nop\n\t");
	DAC_PORT |= (1<<DAC_CLR_PIN);
	dac8568c_write(DAC_SETUP_INTERNAL_REFERENCE, 0, DAC_INTERNAL_REFERENCE_ON);
}

void dac8568c_write(uint8_t command, uint8_t address, uint16_t data) {
	bool ldac_trigger = false;
	dac_command_t transfer;
	switch (command) {
		case DAC_WRITE_UPDATE_N:
			// shifting in our message bit per bit
			transfer.command = command;
			// make space for the address and put it in
			transfer.command = transfer.command << 4;
			transfer.command |= (address & 0x0f);
			// make some space for our data and put it in
			transfer.command = transfer.command << 16;
			transfer.command |= (data);
			// make some space for the function bits and put them in
			transfer.command = transfer.command << 4;
			transfer.command |= 0x0f;
			// now every bit is right in place
			ldac_trigger = true;
			break;
		case DAC_SETUP_INTERNAL_REFERENCE:
			// we could limit data with &0x01 but lets save some valuable flash-bytes here
			transfer.command = 0x08000000 | (data);
			break;
		case DAC_RESET:
			// we could limit data with &0x03 but lets save some valuable flash-bytes here
			transfer.command = 0x07000000 | (data);
			break;
		case DAC_POWER:
			transfer.command = 0x040000ff;
			break;
		default:
			return;
	}
	__dac8568c_output_bytes(transfer, ldac_trigger);
}

void dac8568c_enable_internal_ref(void) {
	dac_command_t command;
	command.command = 0x08000001;
	__dac8568c_output_bytes(command, false);
}

void dac8568c_disable_internal_ref(void) {
	dac_command_t command;
	command.command = 0x08000000;
	__dac8568c_output_bytes(command, false);
}

void __dac8568c_output_bytes(dac_command_t command, bool ldacswitch){
	// change to right clock phase for this chip
	// ATTENTION: this is only save if this method cannot be interrupted by another method that might overwrite SPCR again!
	SPCR |= (1<<CPHA);
	DAC_PORT &= ~(1<<DAC_CS_PIN);
	// wait till that pin is really set
	__asm("nop\n\t");
	spi_transfer(command.b[3]);
	spi_transfer(command.b[2]);
	spi_transfer(command.b[1]);
	spi_transfer(command.b[0]);
	if(ldacswitch) {
		DAC_PORT &= ~(1<<DAC_LDAC_PIN);
		__asm("nop\n\t");
		__asm("nop\n\t");
		DAC_PORT |= (1<<DAC_LDAC_PIN);
	}
	__asm("nop\n\t");
	DAC_PORT |= (1<<DAC_CS_PIN);
}

