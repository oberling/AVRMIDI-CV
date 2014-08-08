#include "analog_in.h"
#include <avr/io.h>

void init_analogin(void) {
	ADMUX = (1<<REFS0); // take AVCC as reference
	ADCSRA = (1<<ADPS1) | (1<<ADPS2); // pre-scaler
	ADCSRA |= (1<<ADEN); // enable
	ADCSRA |= (1<<ADSC); // one dummy read
	while(ADCSRA & (1<<ADSC));
	(void) ADCW;
}

uint16_t analog_read(uint8_t channel) {
	ADMUX = (ADMUX & ~(0x1F))|(channel&0x1F);
	ADCSRA |= (1<<ADSC);
	while(ADCSRA & (1<<ADSC));
	return ADCW;
}

