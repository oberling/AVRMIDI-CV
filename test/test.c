#include "defines.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "midi_datatypes.h"
#include "midibuffer.h"
#include "midinote_stack.h"
#include "playmode.h"
#include "polyphonic.h"
#include "unison.h"

#define SET(x,y)	(x |= (y))
#define ISSET(x,y)	(x & y)
#define UNSET(x,y)	(x &= ~(y))

#define NUM_PLAY_MODES 2
#define POLYPHONIC_MODE 0
#define UNISON_MODE 1

// those voltages will be doubled by the OpAmp
uint32_t voltage[10] = { 6700,
						13250,
						19800,
						26350,
						32900,
						39450,
						46000,
						52550,
						59100,
						65650 };

midibuffer_t midi_buffer;
midinote_stack_t note_stack;
midinote_t playing_notes[NUM_PLAY_NOTES];
playmode_t mode[NUM_PLAY_MODES];
uint8_t playmode = POLYPHONIC_MODE;

bool midi_handler_function(midimessage_t* m) {
	midinote_t mnote;
	switch(m->byte[0]) {
		case NOTE_ON:
			mnote.note = m->byte[1];
			mnote.velocity = m->byte[2];
			SET(mnote.flags, TRIGGER_FLAG);
			midinote_stack_push(&note_stack, mnote);
			break;
		case NOTE_OFF:
			midinote_stack_remove(&note_stack, m->byte[1]);
			break;
		default:
			return false;
	}
	return true;
}

void get_voltage(uint8_t val, uint32_t* voltage_out) {
	// we ignore the first octave... why so ever...
	if(val<12) {
		*voltage_out = 0;
		return;
	}
	uint8_t i = (val/12); // which octave are we in?
	float step = (val-(i*12))/12.0; // relative position in octave
	if(i>0) {
		*voltage_out = (voltage[i]-voltage[i-1])*step+voltage[i-1];
	} else {
		*voltage_out = (voltage[i])*step;
	}
}

void init_variables(void) {
	midinote_stack_init(&note_stack);
	midibuffer_init(&midi_buffer, &midi_handler_function);
	memset(playing_notes, 0, sizeof(midinote_t)*NUM_PLAY_NOTES);
	memset(mode, 0, sizeof(playmode)*NUM_PLAY_MODES);
	mode[POLYPHONIC_MODE].update_notes = update_notes_polyphonic;
	mode[UNISON_MODE].update_notes = update_notes_unison;
}

int main(int argc, char** argv) {
	uint8_t i=0;
	printf("testing init_variables() ");
	{
		init_variables();
		for(;i<MIDINOTE_STACK_SIZE; i++) {
			assert(note_stack.data[i].note == 0);
			assert(note_stack.data[i].velocity == 0);
			assert(note_stack.data[i].flags == 0);
		}
		for(i=0; i<RINGBUFFER_SIZE; i++) {
			assert(midi_buffer.buffer.buffer[i] == 0);
		}
		assert(mode[POLYPHONIC_MODE].update_notes != NULL);
		assert(mode[UNISON_MODE].update_notes != NULL);
	}
	printf(" success\n");
	printf("testing empty buffer and stack");
	{
		assert(midibuffer_tick(&midi_buffer)==false);
		midinote_t mnote;
		midinote_t* it = &mnote;
		uint8_t num_notes;
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes)==false);
	}
	printf(" success\n");
	printf("inserting note into midibuffer");
	{
		assert(midibuffer_put(&midi_buffer, NOTE_ON)==true);
		assert(midibuffer_put(&midi_buffer, 0xff)==true);
		assert(midibuffer_put(&midi_buffer, 0xdd)==true);
		assert(midi_buffer.buffer.buffer[0] == NOTE_ON);
		assert(midi_buffer.buffer.buffer[1] == 0xff);
		assert(midi_buffer.buffer.buffer[2] == 0xdd);
		assert(midi_buffer.buffer.pos_read == 0);
		assert(midi_buffer.buffer.pos_write == 3);
	}
	printf(" success\n");
	printf("testing our note");
	{
		assert(midibuffer_tick(&midi_buffer)==true);
		assert(midi_buffer.buffer.pos_read == 3);
		assert(midi_buffer.buffer.pos_write == 3);
	}
	printf(" success\n");
	printf("checking note arrived on note_stack");
	{
		midinote_t* it;
		uint8_t num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes)==true);
		assert(num_notes == 1);
		assert(it->note == 0xff);
		assert(it->velocity == 0xdd);
	}
	printf(" success\n");
	printf("checking note handling");
	{
		assert(playing_notes[0].note == 0x00);
		mode[playmode].update_notes(&note_stack, &playing_notes);
		assert(playing_notes[0].note == 0xff);
	}
	printf(" success\n");
	printf("testing peek does not pop");
	{
		memset(playing_notes, 0, sizeof(midinote_t)*NUM_PLAY_NOTES);
		assert(playing_notes[0].note == 0x00 && playing_notes[0].velocity == 0x00);
		mode[playmode].update_notes(&note_stack, &playing_notes);
		assert(playing_notes[0].note == 0xff && playing_notes[0].velocity == 0xdd);
	}
	printf(" success\n");

	return 0;
}
