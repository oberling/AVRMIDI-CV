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

bool ringbuffer_peek(ringbuffer_t* b, unsigned char* out) {
	// false if buffer empty
	if(ringbuffer_empty(b))
		return false;
	*out = b->buffer[b->pos_read];
	return true;
}

bool ringbuffer_getn_or_nothing(ringbuffer_t* b, unsigned char* out, uint8_t num_bytes) {
	uint8_t i=0;
	if(num_bytes > RINGBUFFER_SIZE) {
		return false;
	}
	// empty buffer check
	if(ringbuffer_empty(b)) {
		return false;
	}
	// enough bytes without pos_write overflown buffer
	if(b->pos_read < b->pos_write) {
		if(b->pos_read + num_bytes > b->pos_write) {
			return false;
		}
	}
	// b->pos_read > b->pos_write (pos_write has already once crossed the buffer border)
	else {
		uint8_t last_pos = b->pos_read+num_bytes;
		// check whether b->pos_read also crosses the buffer border
		if(last_pos != (last_pos & RINGBUFFER_MASK)) {
			// then check if it now also would cross pos_write -> not enough in buffer
			if((last_pos & RINGBUFFER_MASK) > b->pos_write) {
				return false;
			}
		}
	}

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

