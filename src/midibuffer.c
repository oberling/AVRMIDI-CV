#include "midibuffer.h"

bool midibuffer_issysex=false;

bool midibuffer_init(midibuffer_t* b, midimessage_handler h) {
	b->f = h;
	return ringbuffer_init(&(b->buffer));
}

bool midibuffer_get(midibuffer_t* b, midimessage_t* m) {
	if(ringbuffer_empty(&(b->buffer)))
		return false;
	unsigned char byte = 0x00;
	// discard all sysex-message-bytes
	do {
		// check if we have a sysex- or clock-message here
		// ignore sysex; dispatch clock;
		if(!ringbuffer_peek(&(b->buffer), &byte)) {
			return false;
		}
		switch(byte) {
			case SYSEX_BEGIN:
				midibuffer_issysex = true;
				break;
			case SYSEX_END:
				midibuffer_issysex = false;
				ringbuffer_get(&(b->buffer), &byte);
				break;
			case CLOCK_SIGNAL:
			case CLOCK_START:
			case CLOCK_CONTINUE:
			case CLOCK_STOP:
				return ringbuffer_get(&(b->buffer), m->byte);
				break;
		}
		if(!midibuffer_issysex)
			break;
		ringbuffer_get(&(b->buffer), &byte);
	} while(!ringbuffer_empty(&(b->buffer)));

	//
	// midi-definitions from http://www.midi.org/techspecs/midimessages.php
	//
	
	uint8_t msg = byte >> 4;
	// prefer NOTE_MESSAGES - this may boost performance
	if(msg == 0x8 || msg == 0x9 || msg == 0xD)
		return ringbuffer_getn_or_nothing(&(b->buffer), m->byte, 3);
	// ignore everything below note_on-events
	if(msg < 0x8) return false;
	// return program change and channel pressure
	if(msg == 0xC || msg == 0xD)
		return ringbuffer_getn_or_nothing(&(b->buffer), m->byte, 2);
	// MIDI Time Code Quarter Frame or Song Select
	if(byte == 0xF1 || byte == 0xF3)
		return ringbuffer_getn_or_nothing(&(b->buffer), m->byte, 2);
	// just ignore all other (one byte) SYSTEM_MESSAGES
	if(msg == 0xF) return false;
	// get and return next midimessage
	return ringbuffer_getn_or_nothing(&(b->buffer), m->byte, 3);
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

