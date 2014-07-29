#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "datatypes.h"
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
playingnote_t playing_notes[NUM_PLAY_NOTES];
playmode_t mode[NUM_PLAY_MODES];
uint8_t playmode = POLYPHONIC_MODE;

typedef struct {
	uint8_t byte[3];
} testnote_t;

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
	memset(playing_notes, 0, sizeof(playingnote_t)*NUM_PLAY_NOTES);
	memset(mode, 0, sizeof(playmode)*NUM_PLAY_MODES);
	mode[POLYPHONIC_MODE].update_notes = update_notes_polyphonic;
	mode[UNISON_MODE].update_notes = update_notes_unison;
}

void insert_midibuffer_test(testnote_t n) {
	assert(midibuffer_put(&midi_buffer, n.byte[0]) == true);
	assert(midibuffer_put(&midi_buffer, n.byte[1]) == true);
	assert(midibuffer_put(&midi_buffer, n.byte[2]) == true);
	uint8_t i=0;
	for(;i<3; i++) {
		if(midi_buffer.buffer.pos_write-(3-i)<0) {
			assert(midi_buffer.buffer.buffer[RINGBUFFER_SIZE+midi_buffer.buffer.pos_write-(3-i)] == n.byte[i]);
		} else {
			assert(midi_buffer.buffer.buffer[midi_buffer.buffer.pos_write-(3-i)] == n.byte[i]);
		}
	}
}

int main(int argc, char** argv) {
	uint8_t i=0;
	testnote_t a;
	a.byte[0] = NOTE_ON;
	a.byte[1] = 0xff;
	a.byte[2] = 0xdd;
	testnote_t b;
	b.byte[0] = NOTE_ON;
	b.byte[1] = 0xee;
	b.byte[2] = 0xcc;
	testnote_t c;
	c.byte[0] = NOTE_ON;
	c.byte[1] = 0xbb;
	c.byte[2] = 0xaa;
	testnote_t d;
	d.byte[0] = NOTE_ON;
	d.byte[1] = 0x99;
	d.byte[2] = 0x88;
	testnote_t e;
	e.byte[0] = NOTE_ON;
	e.byte[1] = 0x77;
	e.byte[2] = 0x66;
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
		midinote_t* it;
		uint8_t num_notes;
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes)==false);
	}
	printf(" success\n");
	printf("inserting note into midibuffer");
	{
		insert_midibuffer_test(a);
	}
	printf(" success\n");
	printf("testing midibuffer positions");
	{
		assert(midi_buffer.buffer.pos_read == 0);
		assert(midi_buffer.buffer.pos_write == 3);
	}
	printf(" success\n");
	printf("testing our midibuffer_tick");
	{
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midibuffer_tick(&midi_buffer) == false);
	}
	printf(" success\n");
	printf("testing some ringbuffer behaviour {");
	{
		printf("\n");
		printf("\tnot getting more than buffersize ");
		unsigned char mybuffer[RINGBUFFER_SIZE*2];
		assert(ringbuffer_getn_or_nothing(&(midi_buffer.buffer), mybuffer, RINGBUFFER_SIZE*2)==false);
		printf("success\n");
		printf("\tnot getting anything if buffer rings ");
		uint8_t old_read = midi_buffer.buffer.pos_read;
		midi_buffer.buffer.pos_read = midi_buffer.buffer.pos_write;
		assert(ringbuffer_getn_or_nothing(&(midi_buffer.buffer), mybuffer, 1)==false);
		midi_buffer.buffer.pos_read = old_read;
		printf("success\n");
	}
	printf("} success\n");
	printf("checking note arrived on note_stack");
	{
		midinote_t* it;
		uint8_t num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes) == true);
		assert(num_notes == 1);
		assert(it->note == 0xff);
		assert(it->velocity == 0xdd);
	}
	printf(" success\n");
	printf("checking note handling");
	{
		assert(playing_notes[0].midinote.note == 0x00);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == 0xff);
	}
	printf(" success\n");
	printf("testing peek does not pop");
	{
		memset(playing_notes, 0, sizeof(playingnote_t)*NUM_PLAY_NOTES);
		assert(playing_notes[0].midinote.note == 0x00 && playing_notes[0].midinote.velocity == 0x00);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == 0xff && playing_notes[0].midinote.velocity == 0xdd);
	}
	printf(" success\n");
	printf("testing NOTE_OFF-Handling");
	{
		a.byte[0] = NOTE_OFF;
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) == true);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == 0x00);
	}
	printf(" success\n");
	printf("testing multiple notes {");
	{
		printf("\n");
		a.byte[0] = NOTE_ON;
		printf("\tinserting some notes... ");
		uint8_t pos_read_test = midi_buffer.buffer.pos_read;
		insert_midibuffer_test(a);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(b);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(c);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(d);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(e);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(a);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(b);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(c);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		printf(" success\n");
		// making the buffer overflow
		printf("\tinserting buffer_overflow_note... ");
		insert_midibuffer_test(d);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		printf(" success\n");
		printf("\ttaking them over to the stack... ");
		uint8_t i=0;
		for(; i<9; i++) {
			assert(midibuffer_tick(&midi_buffer) == true);
		}
		printf(" success\n");
		printf("\ttrying empty buffer again");
		assert(midibuffer_tick(&midi_buffer) == false);
		printf(" success\n");
		printf("\ttesting peek for 4 notes but with 5 on stack");
		insert_midibuffer_test(a);
		insert_midibuffer_test(b);
		insert_midibuffer_test(c);
		insert_midibuffer_test(d);
		insert_midibuffer_test(e);
		midinote_stack_init(&note_stack);
		for(i=0;i<5;i++) {
			midibuffer_tick(&midi_buffer);
		}
		midinote_t* it;
		uint8_t num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 4, &it, &num_notes) == true);
		assert(num_notes==4);
		assert(it->note == b.byte[1]);
		assert(it->velocity == b.byte[2]);
		printf(" success\n");
	}
	printf("} success\n");
	printf("testing polyphonic mode {");
	{
		printf("\n");
		midinote_t* it;
		uint8_t num_notes = 0;
		printf("\ttesting playnotes in polyphonic mode");
		{
			playmode = POLYPHONIC_MODE;
			mode[playmode].update_notes(&note_stack, playing_notes);
			assert(playing_notes[0].midinote.note == b.byte[1]);
			assert(playing_notes[1].midinote.note == c.byte[1]);
			assert(playing_notes[2].midinote.note == d.byte[1]);
			assert(playing_notes[3].midinote.note == e.byte[1]);
		}
		printf(" success\n");
		printf("\tremoving one note from playing notes");
		b.byte[0] = NOTE_OFF;
		insert_midibuffer_test(b);
		midibuffer_tick(&midi_buffer);
		num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 4, &it, &num_notes) == true);
		assert(num_notes == 4);
		assert(it->note == a.byte[1]);
		assert(it->velocity == a.byte[2]);
		assert((it+1)->note == c.byte[1]);
		assert((it+1)->velocity == c.byte[2]);
		printf(" success\n");
		printf("\ttesting playnotes in polyphonic mode now again");
		{
			playmode = POLYPHONIC_MODE;
			mode[playmode].update_notes(&note_stack, playing_notes);
			assert(playing_notes[0].midinote.note == a.byte[1]);
			assert(playing_notes[1].midinote.note == c.byte[1]);
			assert(playing_notes[2].midinote.note == d.byte[1]);
			assert(playing_notes[3].midinote.note == e.byte[1]);
		}
		printf(" success\n");
		printf("\tremoving another note and test again");
		d.byte[0] = NOTE_OFF;
		insert_midibuffer_test(d);
		midibuffer_tick(&midi_buffer);
		num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 4, &it, &num_notes) == true);
		assert(num_notes == 3);
		printf(" success\n");
		printf("\ttesting playnotes in polyphonic mode now again");
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == a.byte[1]);
		assert(playing_notes[1].midinote.note == c.byte[1]);
		assert(playing_notes[2].midinote.note == 0x00);
		assert(playing_notes[3].midinote.note == e.byte[1]);
		printf(" success\n");
		printf("\tinserting a new note");
		b.byte[0] = NOTE_ON;
		insert_midibuffer_test(b);
		midibuffer_tick(&midi_buffer);
		num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 4, &it, &num_notes) == true);
		assert(num_notes == 4);
		printf(" success\n");
		printf("\ttesting playnotes in polyphonic mode now again");
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == a.byte[1]);
		assert(playing_notes[1].midinote.note == c.byte[1]);
		assert(playing_notes[2].midinote.note == b.byte[1]);
		assert(playing_notes[3].midinote.note == e.byte[1]);
		printf(" success\n");
	}
	printf("} success\n");
	printf("testing unison mode {");
	{
		printf("\n");
		printf("\tinitializing everything");
		init_variables();
		playmode = UNISON_MODE;
		a.byte[0] = NOTE_ON;
		b.byte[0] = NOTE_ON;
		c.byte[0] = NOTE_ON;
		d.byte[0] = NOTE_ON;
		e.byte[0] = NOTE_ON;
		uint8_t pos_read_test = midi_buffer.buffer.pos_read;
		insert_midibuffer_test(a);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(b);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(c);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(d);
		assert(midi_buffer.buffer.pos_read == pos_read_test);
		insert_midibuffer_test(e);
		uint8_t i=0;
		for(; i<5; i++) {
			assert(midibuffer_tick(&midi_buffer) == true);
		}
		printf(" success\n");
		printf("\tnow checking one note");
		midinote_t* it;
		uint8_t num_notes;
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes) == true);
		assert(num_notes == 1);
		assert(it->note == e.byte[1]);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == e.byte[1]);
		assert(playing_notes[1].midinote.note == e.byte[1]);
		assert(playing_notes[2].midinote.note == e.byte[1]);
		assert(playing_notes[3].midinote.note == e.byte[1]);
		printf(" success\n");
		printf("\ttrying to remove one note");
		d.byte[0] = NOTE_OFF;
		insert_midibuffer_test(d);
		assert(midibuffer_tick(&midi_buffer) == true);
		num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 2, &it, &num_notes) == true);
		assert(it->note == c.byte[1]);
		printf(" success\n");
		printf("\tremoving playing note");
		e.byte[0] = NOTE_OFF;
		insert_midibuffer_test(e);
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes) == true);
		assert(it->note == c.byte[1]);
		printf(" success\n");
		printf("\tchecking for correct remaining playing note");
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == c.byte[1]);
		assert(playing_notes[1].midinote.note == c.byte[1]);
		assert(playing_notes[2].midinote.note == c.byte[1]);
		assert(playing_notes[3].midinote.note == c.byte[1]);
		printf(" success\n");
	}
	printf("} success\n");
	printf("testing empty buffer and stack again after some notes");
	{
		a.byte[0] = NOTE_OFF;
		b.byte[0] = NOTE_OFF;
		c.byte[0] = NOTE_OFF;
		insert_midibuffer_test(a);
		insert_midibuffer_test(b);
		insert_midibuffer_test(c);
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midibuffer_tick(&midi_buffer) == false);
		assert(midibuffer_tick(&midi_buffer) == false);
		mode[playmode].update_notes(&note_stack, playing_notes);
		midinote_t* it;
		uint8_t num_notes;
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes) == false);
		assert(playing_notes[0].midinote.note == 0x00);
		assert(playing_notes[1].midinote.note == 0x00);
		assert(playing_notes[2].midinote.note == 0x00);
		assert(playing_notes[3].midinote.note == 0x00);
	}
	printf(" success\n");
	printf("testing some NOTE_OFF-Events for non-playing notes");
	{
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) == true);
		midinote_t* it;
		uint8_t num_notes;
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes)==false);
	}
	printf(" success\n");
	printf("testing some nasty SYSEX_Messages");
	{
		assert(midibuffer_put(&midi_buffer, SYSEX_BEGIN) == true);
		assert(midibuffer_put(&midi_buffer, 0x11) == true);
		assert(midibuffer_put(&midi_buffer, 0x22) == true);
		assert(midibuffer_put(&midi_buffer, 0x33) == true);
		insert_midibuffer_test(b);
		assert(midibuffer_put(&midi_buffer, SYSEX_END) == true);
		assert(midibuffer_tick(&midi_buffer) == false);
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) == true);
	}
	printf(" success\n");
	printf("testing gate-setting process");
	{
		a.byte[0] = NOTE_ON;
		b.byte[0] = NOTE_ON;
		c.byte[0] = NOTE_ON;
		d.byte[0] = NOTE_ON;
		insert_midibuffer_test(a);
		insert_midibuffer_test(b);
		insert_midibuffer_test(c);
		insert_midibuffer_test(d);
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midibuffer_tick(&midi_buffer) == true);
		playmode = POLYPHONIC_MODE;
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == a.byte[1]);
		assert(playing_notes[1].midinote.note == b.byte[1]);
		assert(playing_notes[2].midinote.note == c.byte[1]);
		assert(playing_notes[3].midinote.note == d.byte[1]);
		uint8_t i=0;
		uint8_t HARDWARE_GATEPORT = 0;
		for(;i<NUM_PLAY_NOTES; i++) {
			if(playing_notes[i].midinote.note != 0) {
				HARDWARE_GATEPORT |= (1<<i);
			} else {
				HARDWARE_GATEPORT &= ~(1<<i);
			}
		}
		assert(HARDWARE_GATEPORT == 15);
	}
	printf(" success\n");

	return 0;
}
