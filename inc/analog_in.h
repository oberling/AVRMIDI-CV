#ifndef _ANALOG_IN_H_
#define _ANALOG_IN_H_
#include <stdint.h>

void init_analogin(void);
uint16_t analog_read(uint8_t channel);

#endif
