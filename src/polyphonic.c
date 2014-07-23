#include <string.h>
#include "polyphonic.h"
#include "defines.h"
#include "midi_datatypes.h"
#include "midinote_stack.h"

void __update_notes_polyphonic(midinote_stack_t* note_stack, midinote_t* playing_notes) {
	// worst: O(n) = 3n² // with n = NUM_PLAY_NOTES -> 3*4²*4 CMDs = 192 CMDs
	// at 16 MHz -> 12µs (at 5 CMDs per iteration (240CMDs): 15µs)
	// +3µs per command on deepes loop layer
	midinote_t mnotes[NUM_PLAY_NOTES];
	midinote_t* it = mnotes;
	uint8_t actual_played_notes;
	uint8_t i=0;
	uint8_t j=0;
	bool found = false;
	// get the actually to be played notes
	midinote_stack_peek_n(note_stack, NUM_PLAY_NOTES, &it, &actual_played_notes);
	// remove nonplaying notes - leave alone the still playing notes
	for(i=0; i<NUM_PLAY_NOTES; i++) {
		found = false;
		for(;j<actual_played_notes; j++) {
			if(mnotes[j].note==playing_notes[i].note) {
				found = true;
				break;
			}
		}
		// reset that note as it isn't played anymore
		if(!found)
			memset(playing_notes+i, 0, sizeof(midinote_t));
	}
	// add new playing notes - leave alone the already playing notes
	for(i=0;i<actual_played_notes; i++) {
		found = false;
		for(j=0;j<NUM_PLAY_NOTES; j++) {
			if(mnotes[j].note==playing_notes[i].note) {
				found = true;
				break;
			}
		}
		if(!found) {
			for(j=0; j<NUM_PLAY_NOTES; j++) {
				if(playing_notes[i].note == 0) {
					playing_notes[i] = mnotes[j];
					break;
				}
			}
		}
	}
}

update_notefunction_t update_notes_polyphonic = &__update_notes_polyphonic;
