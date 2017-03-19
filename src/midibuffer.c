#include "midibuffer.h"

bool midibuffer_issysex=false;
midimessage_t current_message = {{0}};
uint8_t current_message_next_fillbyte = 0;

bool midibuffer_init(midibuffer_t* b, midimessage_handler h) {
	b->f = h;
	return ringbuffer_init(&(b->buffer));
}

bool midibuffer_get(midibuffer_t* b, midimessage_t* m) {
	unsigned char byte = 0x00;

	//
	// MIDI Message specifications:
	// https://www.midi.org/specifications/item/table-1-summary-of-midi-message
	//

	while(!ringbuffer_empty(&(b->buffer))) {
		// handle message byte by byte - peek byte and dispatch it
		ringbuffer_get(&(b->buffer), &byte);

		// sysex handling first - discard all sysex
		if(byte == SYSEX_BEGIN) {
			midibuffer_issysex = true;
			continue; // discard byte
		} else if (byte == SYSEX_END) {
			midibuffer_issysex = false;
			continue; // discard byte
		}

		if(midibuffer_issysex) {
			continue; // discard byte
		} 

		// no more sysex handling from here on
		if (byte >= CLOCK_SIGNAL) { // realtime message
			// dispatch the following...
			if(
				byte == CLOCK_SIGNAL || 
				byte == CLOCK_START || 
				byte == CLOCK_CONTINUE || 
				byte == CLOCK_STOP ||
				byte == 0xFE ||		// active sensing
				byte == 0xFF		// reset
			) {
				m->byte[0] = byte;
				return true;
			} else if( // ... and discard these
				byte == 0xF9 ||		// undefined - reserved
				byte == 0xFD		// undefined - reserved
			) {
				continue; // discard;
			}
		} else if (byte >= 0xF4) { // other one-byte messages
			// including 0xF4 (undefined), 0xF5 (undefined), 0xF6 (tune request)
			// and 0xF7 (SYSEX_END) which is already handled above
			continue; // discard
		} else if (byte >= 0x80) { // status byte
			current_message.byte[0] = byte;
			current_message_next_fillbyte = 1;
			continue;
		} else { // data byte
			// get messagetype without possible channel to be able to compare it
			uint8_t current_status = current_message.byte[0];
			if(current_status == 0) {
				continue;
			}
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
			} else if ( // 2-byte message
				status_nibble == 0xC ||   // program change
				status_nibble == 0xD ||   // channel pressure (simple aftertouch)
				current_status == 0xF1 || // MIDI Timecode Quarter Frame
				current_status == 0xF3    // Song Select
			) {
				num_expected_message_bytes = 2;
			}
			current_message.byte[current_message_next_fillbyte++] = byte;
			if(current_message_next_fillbyte == num_expected_message_bytes) {
				while(current_message_next_fillbyte--) {
					// copy all bytes to outgoing midi message
					m->byte[current_message_next_fillbyte] = current_message.byte[current_message_next_fillbyte];
					if(current_message_next_fillbyte>0 || current_status >= 0xF0) { // running status
						current_message.byte[current_message_next_fillbyte] = 0x00;
					}
				}
				// preserve last status for running status
				if(current_status >=0xF0) {
					current_message_next_fillbyte = 0;
				} else { // ... but only for 0x80-0xF0
					current_message_next_fillbyte = 1;
				}
				return true;
			}
			continue;
		}
	}
	return false;
}

bool midibuffer_tick(midibuffer_t* b) {
	midimessage_t m = {0};
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

