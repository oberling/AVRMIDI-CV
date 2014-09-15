#include "lfo.h"

uint16_t __get_sawtooth(lfo_t* lfo) {
	return lfo->position;
}

uint16_t __get_pulse(lfo_t* lfo) {
	return (lfo->position > LFO_HALF_TABLE_LENGTH) ? 1 : 0;
}

uint16_t __get_triangle(lfo_t* lfo) {
	return (lfo->position > LFO_HALF_TABLE_LENGTH) ? (LFO_TABLE_LENGTH - lfo->position)*2 : lfo->position*2;
}

get_lfo_value_t lfo_get_sawtooth = &__get_sawtooth;
get_lfo_value_t lfo_get_pulse = &__get_pulse;
get_lfo_value_t lfo_get_triangle = &__get_triangle;
