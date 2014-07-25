#include "ringbuffer.h"

bool ringbuffer_init(ringbuffer_t* b) {
	b->pos_read = 0;
	b->pos_write = 0;
	return true;
}

bool ringbuffer_get(ringbuffer_t* b, unsigned char* out) {
	// false if buffer empty
	if(b->pos_read == b->pos_write)
		return false;
	*out = b->buffer[b->pos_read];
	b->pos_read = (b->pos_read+1) & RINGBUFFER_MASK;
	return true;
}

bool ringbuffer_peek(ringbuffer_t* b, unsigned char* out) {
	// false if buffer empty
	if(b->pos_read == b->pos_write)
		return false;
	*out = b->buffer[b->pos_read];
	return true;
}

bool ringbuffer_getn_or_nothing(ringbuffer_t* b, unsigned char* out, uint8_t num_bytes) {
	uint8_t i=0;
	if(
		(num_bytes > RINGBUFFER_SIZE) || // can't return more bytes than size of buffer
		(b->pos_read == b->pos_write) || // won't return a byte if our buffer is empty
		(
			(b->pos_read < b->pos_write) &&  // otherwise we already had a buffer_overflow
			(((b->pos_read+num_bytes) & RINGBUFFER_MASK) > b->pos_write)
		) // not enough bytes in buffer
	)
		return false;
	// we have enough bytes - return them
	do {
		*out = b->buffer[(b->pos_read+i)&RINGBUFFER_MASK];
		out++;
		i++;
	} while (i<num_bytes);
	b->pos_read = (b->pos_read+i) & RINGBUFFER_MASK;
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

