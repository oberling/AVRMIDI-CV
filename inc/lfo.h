#ifndef _LFO_H_
#define _LFO_H_

#include <stdint.h>
#include <stdbool.h>

#define TRIANGLE	(0)
#define PULSE		(1)
#define SAWTOOTH	(2)

#define LFO_TABLE_LENGTH		(0xffff)
#define LFO_HALF_TABLE_LENGTH	(0x7fff)

extern uint16_t clock_limit[];

typedef struct lfo_t lfo_t;

typedef uint16_t (*get_lfo_value_t)(lfo_t* lfo);

struct lfo_t {
	bool clock_sync;
	bool retrigger_on_new_note;
	uint16_t stepwidth;
	uint32_t position;
	uint32_t last_cycle_completed_tick;
	uint16_t clock_counter;
	uint8_t clock_mode;
	get_lfo_value_t get_value;
};


extern get_lfo_value_t lfo_get_sawtooth;
extern get_lfo_value_t lfo_get_pulse;
extern get_lfo_value_t lfo_get_triangle;

#endif
