#include "midi_datatypes.h"
#include "midinote_stack.h"
#include "unison.h"

void __update_notes_unison(midinote_stack_t* note_stack, midinote_t* playing_notes) {
	midinote_t mnote;
	midinote_t* it = &mnote;
	uint8_t num_notes = 0;
	uint8_t i = 0;
	midinote_stack_peek_n(note_stack, 1, &it, &num_notes);
	if(num_notes == 1) {
		for(;i<NUM_PLAY_NOTES; i++) {
			playing_notes[i] = mnote;
		}
	}
}

update_notefunction_t update_notes_unison = &__update_notes_unison;
