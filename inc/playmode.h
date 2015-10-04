#ifndef __PLAYMODE_H_
#define __PLAYMODE_H_

#include <stdint.h>
#include "datatypes.h"
#include "midinote_stack.h"

typedef void (*init_playmodefunction_t)(void);
typedef void (*update_notefunction_t)(midinote_stack_t* note_stack, playingnote_t* playing_notes);

typedef struct {
	update_notefunction_t update_notes;
	init_playmodefunction_t init;
} playmode_t;

#endif
