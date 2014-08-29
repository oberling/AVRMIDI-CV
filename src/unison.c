#include <string.h>
#include "midi_datatypes.h"
#include "midinote_stack.h"
#include "unison.h"

void __update_notes_unison(midinote_stack_t* note_stack, playingnote_t* playing_notes) {
	midinote_t* it;
	uint8_t num_notes = 0;
	uint8_t i = 0;
	midinote_stack_peek_n(note_stack, 1, &it, &num_notes);
	if(num_notes == 1) {
		if(playing_notes[i].midinote.note != it->note) {
			for(;i<NUM_PLAY_NOTES; i++) {
				playing_notes[i].midinote = *it;
				SET(playing_notes[i].flags, TRIGGER_FLAG);
			}
		} else if (playing_notes[i].midinote.velocity != it->velocity) {
			for(;i<NUM_PLAY_NOTES; i++) {
				playing_notes[i].midinote.velocity = it->velocity;
			}
		}
	} else {
		memset(playing_notes, 0, sizeof(playingnote_t)*NUM_PLAY_NOTES);
	}
}

update_notefunction_t update_notes_unison = &__update_notes_unison;
