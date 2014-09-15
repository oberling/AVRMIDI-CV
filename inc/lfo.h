#ifndef _LFO_H_
#define _LFO_H_

#include <stdint.h>
#include <stdbool.h>

#define TRIANGLE	(0)
#define PULSE		(1)
#define SAWTOOTH	(2)

#define LFO_TABLE_LENGTH		(65535)
#define LFO_HALF_TABLE_LENGTH	(32767)

typedef struct lfo_t lfo_t;

typedef uint16_t (*get_lfo_value_t)(lfo_t* lfo);

struct lfo_t {
	bool clock_sync;
	uint16_t stepwidth;
	uint16_t position;
	get_lfo_value_t get_value;
};


extern get_lfo_value_t lfo_get_sawtooth;
extern get_lfo_value_t lfo_get_pulse;
extern get_lfo_value_t lfo_get_triangle;

#endif
