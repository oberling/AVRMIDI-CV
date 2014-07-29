#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#include "midi_datatypes.h"

#ifndef TRIGGER_COUNTER_INIT
#pragma message "TRIGGER_COUNTER_INIT not defined - defining as 6"
#define TRIGGER_COUNTER_INIT	(6)
#endif

typedef uint8_t triggercounter_t;

typedef struct {
	midinote_t midinote;
	triggercounter_t trigger_counter;
} playingnote_t;

#endif
