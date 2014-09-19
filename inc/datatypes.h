#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#define SET(x,y)	(x |= (y))
#define ISSET(x,y)	(x & y)
#define UNSET(x,y)	(x &= ~(y))

#include "midi_datatypes.h"

typedef uint8_t flag_t;

typedef struct {
	midinote_t midinote;
	flag_t flags;
} playingnote_t;

#endif
