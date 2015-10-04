#include <string.h>
#include "polyphonic.h"
#include "datatypes.h"
#include "midi_datatypes.h"
#include "midinote_stack.h"

#ifndef NUM_PLAY_NOTES
#error "please include some -DNUM_PLAY_NOTES in your Makefile describing how many notes can be played simultanously!"
#endif

void __update_notes_polyphonic(midinote_stack_t* note_stack, playingnote_t* playing_notes) {
	// worst: O(n) = 3n² // with n = NUM_PLAY_NOTES -> 3*4²*4 CMDs = 192 CMDs
	// at 16 MHz -> 12µs (at 5 CMDs per iteration (240CMDs): 15µs)
	// +3µs per command on deepes loop layer
	midinote_t* it;
	uint8_t actual_played_notes = 0;
	uint8_t i = 0;
	uint8_t j = 0;
	bool found = false;
	// get the actually to be played notes
	midinote_stack_peek_n(note_stack, NUM_PLAY_NOTES, &it, &actual_played_notes);
	// remove nonplaying notes - leave alone the still playing notes
	for(i=0; i<NUM_PLAY_NOTES; i++) {
		found = false;
		for(j=0;j<actual_played_notes; j++) {
			if((it+j)->note==(playing_notes+i)->midinote.note) {
				found = true;
				break;
			}
		}
		// reset that note as it isn't played anymore
		if(!found)
			memset(playing_notes+i, 0, sizeof(playingnote_t));
	}
	// add new playing notes - update velocity of already playing notes
	for(i=0;i<actual_played_notes; i++) {
		found = false;
		for(j=0;j<NUM_PLAY_NOTES; j++) {
			if((it+i)->note==(playing_notes+j)->midinote.note) {
				(playing_notes+j)->midinote.velocity = (it+i)->velocity;
				found = true;
				break;
			}
		}
		if(!found) {
			for(j=0; j<NUM_PLAY_NOTES; j++) {
				if((playing_notes+lru[j])->midinote.note == 0) {
					(playing_notes+lru[j])->midinote = *(it+i);
					lru_cache_use(lru, j, NUM_PLAY_NOTES);
					break;
				}
			}
		}
	}
}

update_notefunction_t update_notes_polyphonic = &__update_notes_polyphonic;
init_playmodefunction_t init_polyphonic = &__init_polyphonic;
