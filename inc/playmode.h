#ifndef __PLAYMODE_H_
#define __PLAYMODE_H_

#include <stdint.h>
#include "midinote_stack.h"

typedef void (*update_notefunction_t)(midinote_stack_t* note_stack, midinote_t* playing_notes);
typedef uint8_t parameter_t;

typedef struct {
	update_notefunction_t update_notes;
	parameter_t* parameter;
	uint8_t num_param;
} playmode_t;

#endif
