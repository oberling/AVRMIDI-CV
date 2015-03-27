#ifndef _CLOCK_TRIGGER_H_
#define _CLOCK_TRIGGER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct clock_trigger_t clock_trigger_t;

struct clock_trigger_t {
	uint8_t mode;
	uint8_t active_countdown;
};

#endif
