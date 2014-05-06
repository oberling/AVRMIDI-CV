#include "midibuffer.h"

bool midibuffer_issysex=false;

bool midibuffer_init(midibuffer_t* b, midimessage_handler h) {
	b->f = h;
	return ringbuffer_init(&(b->buffer));
}

bool midibuffer_get(midibuffer_t* b, midimessage_t* m) {
	if(ringbuffer_empty(&(b->buffer)))
		return false;
	unsigned char byte;
	// discard all sysex-message-bytes
	do {
		// check if we have a sysex-message here
		ringbuffer_peek(&(b->buffer), &byte);
		if(byte == SYSEX_BEGIN) {
			midibuffer_issysex = true;
		} else if (byte == SYSEX_END) {
			midibuffer_issysex = false;
		} else {
			// if not - get out of the do-while-loop
			if(!midibuffer_issysex)
				break;
		}
		ringbuffer_get(&(b->buffer), &byte);
	} while(!ringbuffer_empty(&(b->buffer)));
	// get and return next midimessage
	return ringbuffer_getn_or_nothing(&(b->buffer), m->byte, sizeof(midimessage_t));
}

bool midibuffer_empty(midibuffer_t* b) {
	return ringbuffer_empty(&(b->buffer));
}

bool midibuffer_tick(midibuffer_t* b) {
	midimessage_t m;
	// if there is a message to dispatch
	if(midibuffer_get(b, &m)) {
		// dispatch it
		return b->f(&m);
	}
	return false;
}

bool midibuffer_put(midibuffer_t* b, unsigned char a) {
	return ringbuffer_put(&(b->buffer), a);
}

