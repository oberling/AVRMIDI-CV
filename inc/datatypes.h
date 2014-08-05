#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#define SET(x,y)	(x |= (y))
#define ISSET(x,y)	(x & y)
#define UNSET(x,y)	(x &= ~(y))

#include "midi_datatypes.h"

#ifndef TRIGGER_COUNTER_INIT
#pragma message "TRIGGER_COUNTER_INIT not defined - defining as 6"
#define TRIGGER_COUNTER_INIT	(6)
#endif

#define TRIGGER_FLAG		(0x01)

typedef uint8_t triggercounter_t;
typedef uint16_t retriggercounter_t;
typedef uint8_t flag_t;

typedef struct {
	midinote_t midinote;
	flag_t flags;
	triggercounter_t trigger_counter;
	retriggercounter_t retrigger_counter;
} playingnote_t;

#endif
