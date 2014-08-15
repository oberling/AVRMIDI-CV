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

#define DAC_WRITE_UPDATE_N			(3)

#define GATE_PORT	gate_port
#define GATE_DDR	DDRC
#define GATE1		PC0
#define GATE2		PC1
#define GATE3		PC2
#define GATE4		PC3
#define GATE_OFFSET	(0)

#define TRIGGER_PORT	trigger_port
#define TRIGGER_DDR		DDRD
#define TRIGGER1		PD2
#define TRIGGER2		PD3
#define TRIGGER3		PD4
#define TRIGGER4		PD5
#define TRIGGER_OFFSET	(2)

#define RETRIGGER_POTI_CHANNEL	(4)

#define NUM_PLAY_MODES 2
#define POLYPHONIC_MODE 0
#define UNISON_MODE 1

// User Input defines
#define ANALOG_READ_COUNTER (2)
#define SHIFTIN_TRIGGER		(6)
#define NUM_SHIFTIN_REG		(1)
// bits in the bytes to represent certain modes
#define POLY_UNI_MODE_BIT		(0x01)
// if set we will retrigger
#define RETRIGGER_INPUT_BIT		(0x02)
// have us 8 different clock trigger modes possible
#define TRIGGER_CLOCK_BIT0		(0x04)
#define TRIGGER_CLOCK_BIT1		(0x08)
#define TRIGGER_CLOCK_BIT2		(0x10)
#define TRIGGER_BIT_MASK		(0x0F)
// if this and the RETRIGGER_INPUT_BIT are set we trigger according to the midi-clock signal
#define TRIGGER_ON_CLOCK_BIT	(0x20)

// those voltages created for the values by the DAC
// will be ~doubled by a OpAmp
// (not exactly doubled because it's 127 at 5V but the
// 10th octave completes at 120 already - so we must
// land at something like
//		(10V/120semitones)*127semitones = 10.5833V
// if we output 5V from the dac for the 127th semitone
// - that makes a factor of amplification of 2.1166666)
uint32_t voltage[11] = {
	6192, // calculated: ((2^16)/127)*0*12
	12385,// calculated: ((2^16)/127)*1*12
	18577,// calculated: ((2^16)/127)*2*12
	24769,// ... u get it :-)
	30962,
	37154,
	43347,
	49539,
	55731,
	61924,
	68116
};

// 24 CLOCK_SIGNALs per Beat (Quarter note)
// 96 - full note; 48 - half note; ... 3 - 32th note
uint8_t clock_trigger_limit[8] = {	96,
									48,
									24,
									18,
									12,
									9,
									6,
									3  };

midibuffer_t midi_buffer;
midinote_stack_t note_stack;
playingnote_t playing_notes[NUM_PLAY_NOTES];
playmode_t mode[NUM_PLAY_MODES];
uint8_t playmode = POLYPHONIC_MODE;
volatile bool must_update_dac = false;
uint8_t shift_in_trigger_counter = SHIFTIN_TRIGGER;
volatile bool get_shiftin = false;
uint8_t analog_in_counter = ANALOG_READ_COUNTER;
volatile bool get_analogin = false;
retriggercounter_t retrig = 1000;
volatile bool update_clock = false;
uint8_t midiclock_trigger_mode = 0;
uint8_t midiclock_counter = 0;
uint8_t midiclock_trigger_limit = 0;

#define RETRIGGER			(0x01)
#define TRIGGER_CLOCK		(0x02)

uint8_t program_options = 0x00;

// additional variables to emulate hardware I/O
uint8_t gate_port = 0x00;
uint8_t trigger_port = 0x00;
uint8_t input_buffer[NUM_SHIFTIN_REG];
uint16_t analog_input_value = 0x00;
// ----------------------------------------------

// some additional variables needed for our tests
typedef struct {
	uint8_t byte[3];
} testnote_t;
testnote_t a;
testnote_t b;
testnote_t c;
testnote_t d;
testnote_t e;
// ----------------------------------------------

bool midi_handler_function(midimessage_t* m);
void get_voltage(uint8_t val, uint32_t* voltage_out);
void update_dac(void);
void process_user_input(void);
void process_analog_in(void);
void init_variables(void);
void init_io(void);

// some additional functions needed for our tests
void dac8568c_write(uint8_t command, uint8_t address, uint32_t data);
void sr74hc165_read(uint8_t* buffer, uint8_t numsr);
void init_notes(void);
void prepare_four_notes_on_stack(void);
void insert_midibuffer_test(testnote_t n);
void timer1_overflow_function(void);
void handle_trigger(void);
// ----------------------------------------------

bool midi_handler_function(midimessage_t* m) {
	midinote_t mnote;
	switch(m->byte[0]) {
		case NOTE_ON:
			mnote.note = m->byte[1];
			mnote.velocity = m->byte[2];
			midinote_stack_push(&note_stack, mnote);
			break;
		case NOTE_OFF:
			midinote_stack_remove(&note_stack, m->byte[1]);
			break;
		case CLOCK_SIGNAL:
			midiclock_counter++;
			update_clock = true;
			break;
		case CLOCK_START:
			midiclock_counter = 0;
			break;
		case CLOCK_STOP:
			midiclock_counter = 0;
			break;
		case CLOCK_CONTINUE:
			break;
		default:
			return false;
	}
	return true;
}

void get_voltage(uint8_t val, uint32_t* voltage_out) {
	uint8_t i = (val/12); // which octave are we in?
	float step = (val-(i*12))/12.0; // relative position in octave
	if(i>0) {
		*voltage_out = (voltage[i]-voltage[i-1])*step+voltage[i-1];
	} else {
		*voltage_out = (voltage[i])*step;
	}
	if(*voltage_out > 65536)
		*voltage_out = 65536;
}

void update_dac(void) {
	uint8_t i = 0;
	for(; i<NUM_PLAY_NOTES; i++) {
		unsigned int note = playing_notes[i].midinote.note;
		unsigned int velocity = playing_notes[i].midinote.velocity;
		uint32_t voltage = 0;
		get_voltage(note, &voltage);
		dac8568c_write(DAC_WRITE_UPDATE_N, i, voltage);
		// Send velocity
		get_voltage(velocity, &voltage);
		dac8568c_write(DAC_WRITE_UPDATE_N, i+NUM_PLAY_NOTES, voltage);

		// not putting this if-clause at start because we would have to reset all 
		// other pins/dac-outputs anyway... but as of memset to 0 in update_notes 
		// they are already 0 here if this note is not playing and will get reset 
		// implicitly here
		if(playing_notes[i].midinote.note != 0) {
			GATE_PORT |= (1<<(i+(GATE_OFFSET)));
		} else {
			GATE_PORT &= ~(1<<(i+(GATE_OFFSET)));
		}
		if(playing_notes[i].trigger_counter > 0) {
			TRIGGER_PORT |= (1<<(i+(TRIGGER_OFFSET)));
		} else {
			TRIGGER_PORT &= ~(1<<(i+(TRIGGER_OFFSET)));
		}
	}
}

void process_user_input(void) {
	uint8_t input[NUM_SHIFTIN_REG];
	sr74hc165_read(input, NUM_SHIFTIN_REG);
	if(ISSET(input[0], POLY_UNI_MODE_BIT)) {
		playmode = POLYPHONIC_MODE;
	} else {
		playmode = UNISON_MODE;
	}
	if(ISSET(input[0], RETRIGGER_INPUT_BIT)) {
		SET(program_options, RETRIGGER);
	} else {
		UNSET(program_options, RETRIGGER);
	}
	if(ISSET(input[0], TRIGGER_ON_CLOCK_BIT)) {
		SET(program_options, TRIGGER_CLOCK);
	} else {
		UNSET(program_options, TRIGGER_CLOCK);
	}
	midiclock_trigger_mode = ((input[0]>>TRIGGER_CLOCK_BIT0) & TRIGGER_BIT_MASK);
}

void process_analog_in(void) {
	assert(analog_input_value<1024);
	retrig = analog_input_value;
}

// INFO: assure that this function is called on each increment of midiclock_counter
//       otherwise we may lose trigger-points
void update_clock_trigger(void) {
	midiclock_counter %= clock_trigger_limit[midiclock_trigger_mode];
	if( midiclock_counter == 0 &&
		(ISSET(program_options, TRIGGER_CLOCK))) {
		uint8_t i=0;
		for(; i<NUM_PLAY_NOTES; i++) {
			SET(playing_notes[i].flags, TRIGGER_FLAG);
		}
		must_update_dac = true;
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

void init_io(void) {
	// don't have no hardware - nothing to do here...
}

void init_notes(void) {
	a.byte[0] = NOTE_ON;
	a.byte[1] = 0xff;
	a.byte[2] = 0xdd;
	b.byte[0] = NOTE_ON;
	b.byte[1] = 0xee;
	b.byte[2] = 0xcc;
	c.byte[0] = NOTE_ON;
	c.byte[1] = 0xbb;
	c.byte[2] = 0xaa;
	d.byte[0] = NOTE_ON;
	d.byte[1] = 0x99;
	d.byte[2] = 0x88;
	e.byte[0] = NOTE_ON;
	e.byte[1] = 0x77;
	e.byte[2] = 0x66;
}

void prepare_four_notes_on_stack(void) {
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
void sr74hc165_read(uint8_t* buffer, uint8_t numsr) {
	uint8_t i=0;
	for(;i<numsr;i++) {
		*(buffer+i) = *(input_buffer+i);
	}
}

void dac8568c_write(uint8_t command, uint8_t address, uint32_t data) {
	// don't have no hardware here - hard to test
}

void timer1_overflow_function(void) {
	uint8_t i=0;
	for(;i<NUM_PLAY_NOTES; i++) {
		if(playing_notes[i].trigger_counter > 0) {
			playing_notes[i].trigger_counter--;
			if(playing_notes[i].trigger_counter == 0) {
				must_update_dac = true;
			}
		}
		if(	ISSET(program_options, RETRIGGER) &&
			(!ISSET(program_options, TRIGGER_CLOCK))) {
			if(playing_notes[i].retrigger_counter++ >= retrig) {
				playing_notes[i].retrigger_counter = 0;
				SET(playing_notes[i].flags, TRIGGER_FLAG);
			}
		}
	}
	if(shift_in_trigger_counter-- == 0) {
		shift_in_trigger_counter = SHIFTIN_TRIGGER;
		get_shiftin = true;
	}
}

void handle_trigger(void) {
	uint8_t i=0;
	// handle newly triggered notes
	for(;i<NUM_PLAY_NOTES; ++i) {
		if(ISSET(playing_notes[i].flags, TRIGGER_FLAG)) {
			UNSET(playing_notes[i].flags, TRIGGER_FLAG);
			playing_notes[i].trigger_counter = TRIGGER_COUNTER_INIT;
			must_update_dac = true;
		}
	}
}


int main(int argc, char** argv) {
	uint8_t i=0;
	init_notes();
	printf("testing init_variables() ");
	{
		init_variables();
		for(;i<MIDINOTE_STACK_SIZE; i++) {
			assert(note_stack.data[i].note == 0);
			assert(note_stack.data[i].velocity == 0);
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
		init_notes();
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
		assert(num_notes == 4);
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
		init_notes();
		prepare_four_notes_on_stack();
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
				HARDWARE_GATEPORT |= (1<<(i+(GATE_OFFSET)));
			} else {
				HARDWARE_GATEPORT &= ~(1<<(i+(GATE_OFFSET)));
			}
		}
		assert(HARDWARE_GATEPORT == 15);
	}
	printf(" success\n");
	printf("testing trigger-setting and unsetting process {");
	{
		printf("\n");
		printf("\ttesting simultanous notes");
		init_variables();
		init_notes();
		prepare_four_notes_on_stack();
		playmode = POLYPHONIC_MODE;
		must_update_dac = false;
		handle_trigger();
		assert(must_update_dac == false);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == a.byte[1]);
		assert(playing_notes[1].midinote.note == b.byte[1]);
		assert(playing_notes[2].midinote.note == c.byte[1]);
		assert(playing_notes[3].midinote.note == d.byte[1]);
		uint8_t i=0;
		for(;i<NUM_PLAY_NOTES; i++) {
			assert(ISSET(playing_notes[i].flags, TRIGGER_FLAG)==true);
		}
		handle_trigger();
		assert(must_update_dac == true);
		// here we could already update our dac - but let's test that in a different test
		must_update_dac = false;
		for(i=TRIGGER_COUNTER_INIT; i>0; i--) {
			timer1_overflow_function();
			uint8_t j=0;
			for(; j<NUM_PLAY_NOTES; j++) {
				assert(playing_notes[j].trigger_counter == i-1);
				if(i-1 != 0) {
					assert(must_update_dac == false);
				}
			}
		}
		assert(must_update_dac == true);
		printf(" success\n");
		printf("\ttesting some notes on and off");
		init_variables();
		init_notes();
		playmode = POLYPHONIC_MODE;
		insert_midibuffer_test(a);
		midibuffer_tick(&midi_buffer);
		mode[playmode].update_notes(&note_stack, playing_notes);
		handle_trigger();
		timer1_overflow_function(); //Note a TRIGGER_COUNTER_INIT-1
		timer1_overflow_function(); //Note a TRIGGER_COUNTER_INIT-2
		insert_midibuffer_test(b);
		midibuffer_tick(&midi_buffer);
		mode[playmode].update_notes(&note_stack, playing_notes);
		timer1_overflow_function(); //...
		handle_trigger();
		insert_midibuffer_test(c);
		midibuffer_tick(&midi_buffer);
		mode[playmode].update_notes(&note_stack, playing_notes);
		handle_trigger();
		timer1_overflow_function(); //...
		insert_midibuffer_test(d);
		midibuffer_tick(&midi_buffer);
		mode[playmode].update_notes(&note_stack, playing_notes);
		handle_trigger();
		timer1_overflow_function();
		assert(playing_notes[0].trigger_counter == TRIGGER_COUNTER_INIT-5);
		assert(playing_notes[1].trigger_counter == TRIGGER_COUNTER_INIT-2);
		assert(playing_notes[2].trigger_counter == TRIGGER_COUNTER_INIT-2);
		assert(playing_notes[3].trigger_counter == TRIGGER_COUNTER_INIT-1);
		timer1_overflow_function();
		if(TRIGGER_COUNTER_INIT == 6) // that's a little odd... but how else shall we do it?
			assert(playing_notes[0].trigger_counter == 0);
		assert(ISSET(playing_notes[0].flags, TRIGGER_FLAG) == false);
		timer1_overflow_function();
		timer1_overflow_function();
		c.byte[0] = NOTE_OFF;
		insert_midibuffer_test(c);
		midibuffer_tick(&midi_buffer);
		assert(playing_notes[2].trigger_counter == TRIGGER_COUNTER_INIT-5);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[2].trigger_counter == 0);
		assert(playing_notes[2].midinote.note == 0);
		timer1_overflow_function();
		assert(playing_notes[1].trigger_counter == 0);
		assert(ISSET(playing_notes[1].flags, TRIGGER_FLAG) == false);
		assert(playing_notes[3].trigger_counter == TRIGGER_COUNTER_INIT-5);
		printf(" success\n");
	}
	printf("} success\n");
	printf("testing user-input");
	{
		input_buffer[0] = 0x00;
		SET(input_buffer[0], POLY_UNI_MODE_BIT);
		SET(input_buffer[0], RETRIGGER_INPUT_BIT);
		SET(input_buffer[0], TRIGGER_ON_CLOCK_BIT);
		SET(input_buffer[0], TRIGGER_CLOCK_BIT1);
		process_user_input();
		assert(playmode == POLYPHONIC_MODE);
		// cannot check againstr ture as 0x02 != 0x01 but 0x01 is defined as true
		// but ISSET will evaluate against something other than 0x01
		assert(ISSET(program_options, RETRIGGER) != false);
		assert(ISSET(program_options, TRIGGER_CLOCK) != false);
		assert(midiclock_trigger_mode == 2);
	}
	printf(" success\n");
	printf("testing midi_clock trigger");
	{
		midiclock_counter = 0;
		uint8_t j=0;
		// testing it two times... just to make shure...
		for(j=0; j<2; j++) {
			uint8_t i=0;
			for(i=0; i<NUM_PLAY_NOTES; i++) {
				assert(ISSET(playing_notes[0].flags, TRIGGER_FLAG) == false);
			}
			for(i=0; i<clock_trigger_limit[midiclock_trigger_mode];i++) {
				assert(midibuffer_put(&midi_buffer, CLOCK_SIGNAL) == true);
				assert(midibuffer_tick(&midi_buffer) == true);
				assert(midiclock_counter == i+1);
				must_update_dac = false;
				update_clock_trigger();
				if(i+1 == clock_trigger_limit[midiclock_trigger_mode])
					assert(must_update_dac == true);
				else
					assert(must_update_dac == false);
			}
			for(i=0; i<NUM_PLAY_NOTES; i++) {
				assert(ISSET(playing_notes[0].flags, TRIGGER_FLAG) != false);
			}
			handle_trigger();
		}
		assert(midibuffer_put(&midi_buffer, CLOCK_SIGNAL) == true);
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midiclock_counter == 1);
		must_update_dac = false;
		update_clock_trigger();
		assert(must_update_dac == false);
		assert(midibuffer_put(&midi_buffer, CLOCK_STOP) == true);
		assert(midibuffer_tick(&midi_buffer) == true);
		assert(midiclock_counter == 0);
		must_update_dac = false;
	}
	printf(" success\n");
	printf("testing get_voltage ");
	{
		uint8_t val = 0;
		uint32_t out = 1;
		uint8_t i=0;
		for(; i<10; i++) {
			uint8_t j=0;
			for(;j<12; j++) {
				out = 1;
				get_voltage(val, &out);
				val++;
				assert(out<voltage[i]);
			}
		}
	}
	printf(" success\n");

	return 0;
}
