#include "uart.h"

void uart_init(void) {
	UBRRH = UBRR_VAL >> 8;
	UBRRL = UBRR_VAL & 0xFF;
	
	UCSRB |= (1<<TXEN);							// enable UART TX
#if defined(__AVR_ATmega8__)
	UCSRC  = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0);  // asynchronous 8N1
#elif defined(__AVR_ATtiny2313__)
	UCSRC  = (1<<UCSZ1)|(1<<UCSZ0);				// asynchronous 8N1
#endif
	UCSRB |= (1<<RXEN);							// enable UART RX
	UCSRB |= (1<<RXCIE);
}

bool uart_putc(unsigned char c) {
	while(!(UCSRA & (1<<UDRE)));
	UDR = c;
	return true;
}

bool uart_puts(char* s) {
	while(*s) {
		uart_putc(*s);
		s++;
	}
	return true;
}

bool uart_getc(char* out) {
	while(!(UCSRA & (1<<RXC)));
	*out = UDR;
	return true;
}

