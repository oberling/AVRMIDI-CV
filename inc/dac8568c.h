#ifndef __DAC8568C_H_
#define __DAC8568C_H_
#include <stdint.h>
//DAC8568C & AD5668 Command definitions
#define DAC_WRITE					(0)
#define DAC_UPDATE					(1)
#define DAC_WRITE_UPDATE_ALL		(2)
#define DAC_WRITE_UPDATE_N			(3)
#define DAC_POWER					(4)
#define DAC_LOAD_CC_REG				(5)
#define DAC_LOAD_LDAC_REG			(6)
#define DAC_RESET					(7)
#define DAC_SETUP_INTERNAL_REGISTER	(8)

#ifndef DAC_PORT
#pragma message "DAC_PORT not defined - defaulting to PORTB"
#define DAC_PORT	PORTB
#endif
#ifndef DAC_DDR
#pragma message "DAC_DDR not defined - defaulting to DDRB"
#define DAC_DDR		DDRB
#endif
#ifndef DAC_LDAC_PIN
#pragma message "DAC_LDAC_PIN not defined - defaulting to PB0"
#define DAC_LDAC_PIN	PB0
#endif
#ifndef DAC_CLR_PIN
#pragma message "DAC_CLR_PIN not defined - defaulting to PB1"
#define DAC_CLR_PIN		PB1
#endif
#ifndef DAC_CS_PIN
#pragma message "DAC_CS_PIN not defined - defaulting to PB2"
#define DAC_CS_PIN	PB2
#endif

/**
 * \brief Function to initialize the DAC8568C.
 * \description This function initializes the SPI Interface for usage
 * with the DAC8568C.
 */
void dac8568c_init(void);

/**
 * \brief Function to write the command to the DAC8568C
 * \description This function writes the given command to the DAC8568C.
 * The Adress might be any value - it is only used when sending a
 * DAC_WRITE_UPDATE_N command. Data has to be according to the datasheet
 * and can be left blank for any DAC_POWER operations.
 */
void dac8568c_write(uint8_t command, uint8_t address, uint32_t data);

/**
 * Function to enable the internal reference
 */
void dac8568c_enable_internal_ref(void);

/**
 * Function to disable the internal reference
 */
void dac8568c_disable_internal_ref(void);

#endif
