#ifndef _SR74HC165_H_
#define _SR74HC165_H_
#include <stdint.h>
#include "spi.h"

#ifndef SR_CE_PIN
#pragma message "please define SR_CE_PIN - defaulting to PD6"
#define SR_CE_PIN	PD6
#endif
#ifndef SR_PL_PIN
#pragma message "please define SR_PL_PIN - defaulting to PD7"
#define SR_PL_PIN	PD7
#endif
#ifndef SR_PL_CE_PORT
#pragma message "SR_PL_CE_PORT not defined - defaulting to PORTD"
#define SR_PL_CE_PORT	PORTD
#endif
#ifndef SR_PL_CE_DDR
#pragma message "SR_PL_CE_DDR not defined - defaulting to DDRD"
#define SR_PL_CE_DDR	DDRD
#endif

void sr74hc165_init(uint8_t num_modules);

void sr74hc165_read(unsigned char* output_buffer, uint8_t num_modules);

#endif
