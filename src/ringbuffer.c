#include "ringbuffer.h"

bool ringbuffer_init(ringbuffer_t* b) {
	b->pos_read = 0;
	b->pos_write = 0;
	return true;
}

bool ringbuffer_get(ringbuffer_t* b, unsigned char* out) {
	// false if buffer empty
	if(ringbuffer_empty(b))
		return false;
	*out = b->buffer[b->pos_read];
	b->pos_read = (b->pos_read+1) & RINGBUFFER_MASK;
	return true;
}

bool ringbuffer_put(ringbuffer_t* b, unsigned char a) {
	uint8_t next = (b->pos_write+1)&RINGBUFFER_MASK;
	// false if full
	if(next == b->pos_read) 
		return false;
	b->buffer[b->pos_write] = a;
	b->pos_write = next;
	return true;
}

