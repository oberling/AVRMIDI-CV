#include "midibuffer.h"

bool midibuffer_issysex=false;
midimessage_t current_message = {0};
uint8_t current_message_next_fillbyte = 0;

bool midibuffer_init(midibuffer_t* b, midimessage_handler h) {
	b->f = h;
	return ringbuffer_init(&(b->buffer));
}

bool __is_realtime_byte_message(uint8_t byte) {
	return byte >= CLOCK_SIGNAL;
}

bool __handle_realtime_byte_message(midibuffer_t* b, midimessage_t* m, uint8_t byte) {
	// ... by dispatching the following messages
	if(
		byte == CLOCK_SIGNAL || 
		byte == CLOCK_START || 
		byte == CLOCK_CONTINUE || 
		byte == CLOCK_STOP ||
		byte == 0xFE ||		// active sensing
		byte == 0xFF		// reset
	) {
		return ringbuffer_get(&(b->buffer), m->byte);
	}
	if(
		byte == 0xF9 ||		// undefined - reserved
		byte == 0xFD		// undefined - reserved
	) {
		ringbuffer_get(&(b->buffer), &byte); // discard
		return false;
	}
	// never reached
	return false;
}

bool midibuffer_get(midibuffer_t* b, midimessage_t* m) {
	if(ringbuffer_empty(&(b->buffer)))
		return false;
	unsigned char byte = 0x00;

	//
	// midi-definitions from https://www.midi.org/specifications/item/table-1-summary-of-midi-message
	//

	// handle sysex-messages
	do {
		// peek byte and check if we have a sysex-message here
		if(!ringbuffer_peek(&(b->buffer), &byte)) {
			return false;
		}
		// the following saves us 4 Byte Flash Space compared to a switch-case statement
		if(byte == SYSEX_BEGIN) {
			midibuffer_issysex = true;
		} else if (byte == SYSEX_END) {
			midibuffer_issysex = false;
			ringbuffer_get(&(b->buffer), &byte); // take away last sysex byte
			// read next byte if possible for further message handling
			if(!ringbuffer_peek(&(b->buffer), &byte)) {
				return false;
			} else if (byte == SYSEX_BEGIN) { // yet another sysex message following 
				midibuffer_issysex = true;
			}
		}
		// handle real-time messages
		if(__is_realtime_byte_message(byte)) {
			return __handle_realtime_byte_message(b, m, byte);
		}
		// step out if we are not or no longer in a sysex message
		if(!midibuffer_issysex)
			break;
		// discard sysex bytes
		ringbuffer_get(&(b->buffer), &byte); // discard byte
	} while(!ringbuffer_empty(&(b->buffer)));

	// handle one-byte messages
	if(byte >= 0xF4) { // 0xF4 = first one-byte message
		// handle any one-byte message...
		if(__is_realtime_byte_message(byte)) {
			return __handle_realtime_byte_message(b, m, byte);
		}
		// ... and discarding the following messages
		if(	
			byte == 0xF4 ||		// undefined - reserved
			byte == 0xF5 ||		// undefined - reserved
			byte == 0xF6 ||		// tune request
			byte == SYSEX_END	// only for the sake of completeness (handled above)
		  ) {
			ringbuffer_get(&(b->buffer), &byte); // discard
			return false;
		}
		// never reached
	}

	// if status byte
	if(byte >= 0x80) {
		// save status type (needed afterwards and for running status)
		ringbuffer_get(&(b->buffer), current_message.byte);
		current_message_next_fillbyte = 1;
		// and try to get the first data byte
		// (we already handled all one-byte messages above)
		if(!ringbuffer_peek(&(b->buffer), &byte)) {
			return false;
		}
	}
	// byte must be the first data byte from here on

	// get messagetype without possible channel to be able to compare it
	uint8_t current_status = current_message.byte[0];
	uint8_t status_nibble = current_status >> 4;

	uint8_t num_expected_message_bytes = 0;
	if( // 3-byte message
		status_nibble == NOTE_OFF_NIBBLE ||
		status_nibble == NOTE_ON_NIBBLE ||
		status_nibble == 0xA || // polyphonic key pressure (polyphonic aftertouch)
		status_nibble == 0xB || // CC
		status_nibble == 0xE || // PitchBend
		current_status == 0xF2  // Song Position Pointer
	) {
		num_expected_message_bytes = 3;
//		return ringbuffer_getn_or_nothing(&(b->buffer), &(m->byte[1]), 2);
	} else if ( // 2-byte message
		status_nibble == 0xC ||   // program change
		status_nibble == 0xD ||   // channel pressure (simple aftertouch)
		current_status == 0xF1 || // MIDI Timecode Quarter Frame
		current_status == 0xF3    // Song Select
	) {
		num_expected_message_bytes = 2;
//		return ringbuffer_getn_or_nothing(&(b->buffer), &(m->byte[1]), 1);
	} else {
		ringbuffer_get(&(b->buffer), &byte); // discard
		return false;
	}

	do {
		if(!ringbuffer_peek(&(b->buffer), &byte)) {
			return false;
		}
		if(__is_realtime_byte_message(byte)) {
			return __handle_realtime_byte_message(b, m, byte);
		} else {
			ringbuffer_get(&(b->buffer), &(current_message.byte[current_message_next_fillbyte++]));
			if(current_message_next_fillbyte == num_expected_message_bytes) {
				while(current_message_next_fillbyte--) {
					m->byte[current_message_next_fillbyte] = current_message.byte[current_message_next_fillbyte];
					if(current_message_next_fillbyte>0) { // running status
						current_message.byte[current_message_next_fillbyte] = 0x00;
					}
				}
				return true;
			}
		}
	} while (!ringbuffer_empty(&(b->buffer)));
	// only maybe reached on boot/connect (current_status==0), when searching for first message
	return false;
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

