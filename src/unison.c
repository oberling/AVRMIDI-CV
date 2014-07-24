#include <string.h>
#include "midi_datatypes.h"
#include "midinote_stack.h"
#include "unison.h"

void __update_notes_unison(midinote_stack_t* note_stack, midinote_t* playing_notes) {
	midinote_t* it;
	uint8_t num_notes = 0;
	uint8_t i = 0;
	midinote_stack_peek_n(note_stack, 1, &it, &num_notes);
	if(num_notes == 1) {
		for(;i<NUM_PLAY_NOTES; i++) {
			playing_notes[i] = *it;
		}
	} else {
		memset(playing_notes, 0, sizeof(midinote_t)*NUM_PLAY_NOTES);
	}
}

update_notefunction_t update_notes_unison = &__update_notes_unison;
