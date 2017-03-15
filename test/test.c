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
#include "lfo.h"
#include "clock_trigger.h"

#define DAC_WRITE_UPDATE_N			(3)

#define GATE_PORT	gate_port
#define GATE_DDR	DDRC
#define GATE1		PC0
#define GATE2		PC1
#define GATE3		PC2
#define GATE4		PC3
#define GATE_OFFSET	(0)

#define BUTTON_LED_PORT	button_led_port
#define BUTTON_LED_DDR	DDRC
#define BUTTON			(0)
#define LED				(1)
#define BUTTON_PIN		button_pin

#define LFO_RATE_POTI0	(4)

#define CLOCK_RATE_POTI0	(2)

#define NUM_PLAY_MODES	(2)
#define POLYPHONIC_MODE	(0)
#define UNISON_MODE		(1)

// User Input defines
#define ANALOG_READ_COUNTER (2)
#define SHIFTIN_TRIGGER		(6)
#define NUM_SHIFTIN_REG		(2)

/*
 00000000
 \\\\\\\\_MIDI_CHANNEL_BIT0 \
  \\\\\\\_MIDI_CHANNEL_BIT1  \_MIDI_CHANNEL
   \\\\\\_MIDI_CHANNEL_BIT2  /
    \\\\\_MIDI_CHANNEL_BIT3 /
     \\\\_reserved
      \\\_MODE_BIT0 - polyphonic unison mode
       \\_MODE_BIT1 - yet only reserved
        \_LFO_CLOCK_ENABLE_BIT - lfo enable/velocity disable
*/

// bits in the bytes to represent certain modes
#define MODE_BIT0				(0x20)
#define MODE_BIT1				(0x40)
#define LFO_CLOCK_ENABLE_BIT	(0x80)

#define MIDI_CHANNEL_MASK		(0x0f)

/*
 00000000
 \\\\\\\\_LFO0_WAVE_BIT0 \_LFO0_WAVE_SETTINGS
  \\\\\\\_LFO0_WAVE_BIT1 /
   \\\\\\_LFO0_CLOCKSYNC - clocksync enable/disable for LFO0
    \\\\\_LFO0_RETRIGGER_ON_NEW_NOTE - enable/disable retrigger of LFO0 on new note
     \\\\_LFO1_WAVE_BIT0 \_LFO1_WAVE_SETTINGS
      \\\_LFO1_WAVE_BIT1 /
       \\_LFO1_CLOCKSYNC - clocksync enable/disable for LFO1
        \_LFO1_RETRIGGER_ON_NEW_NOTE - enable/disable retrigger of LFO1 on new note
*/

#define LFO0_WAVE_BIT0		(0x01)
#define LFO0_WAVE_BIT1		(0x02)
#define LFO0_CLOCKSYNC		(0x04)
#define LFO0_RETRIGGER_ON_NEW_NOTE	(0x08)
#define LFO1_WAVE_BIT0		(0x10)
#define LFO1_WAVE_BIT1		(0x20)
#define LFO1_CLOCKSYNC		(0x40)
#define LFO1_RETRIGGER_ON_NEW_NOTE	(0x80)

#define LFO_MASK	(0x03)
#define LFO0_OFFSET	(0)
#define LFO1_OFFSET	(3)

const uint8_t lfo_offset[2] = {
	0,
	4
};

#define SINGLE_BAR_COMPLETED	(96)
#define EIGHT_BARS_COMPLETED	(768)

/**
 * The whole trick about playing 4 notes at a time is the usage of a
 * peek_n-method on the note-stack: if a new (5th) note "overwrites" the oldest
 * playing note it is not specifically "overwritten" - it just doesn't
 * peek anymore from the stack - but now the new note does. As it is not peeked
 * anymore it gets erased from the playing notes array and the new note gets
 * inserted on that empty spot instead.
 * If any playing note stops playing (before a new one comes in), the newest
 * non-playing note will come back into the peek_n-return and continues
 * playing (but maybe on another channel...)
 */

// those voltages created for the values by the DAC
// will be ~doubled by a OpAmp
// (not exactly doubled because it's 127 at 5V but the
// 10th octave completes at 120 already - so we must
// land at something like
//		(10V/120semitones)*127semitones = 10.5833V
// if we output 5V from the dac for the 127th semitone
// - that makes a factor of amplification of 2.1166666)
uint32_t voltage[11] = {
	6192, // calculated: ((2^16)/127)*1*12
	12385,// calculated: ((2^16)/127)*2*12
	18577,// calculated: ((2^16)/127)*3*12
	24769,// ... u get it :-)
		  // one semitone is ((2^16)/127) round about 516
	30962,
	37154,
	43347,
	49539,
	55731,
	61924,
	68116
};

uint16_t pitchbend = 0x2000; // middle_position

// 24 CLOCK_SIGNALs per Beat (Quarter note)
// 768 - 8 bars; 96 - 1 bar or 1 full note; 48 - half note; ... 3 - 32th note
uint16_t clock_limit[12] = {
	1536,
	768,
	384,
	192,
	96,
	48,
	24,
	18,
	12,
	9,
	6,
	3
};

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

volatile bool update_clock = false;

uint32_t midiclock_counter = 0;
// avoid division by zero in clock synced lfo mode
uint32_t current_midiclock_tick = 2;
uint32_t last_midiclock_tick = 1;

uint32_t last_single_bar_completed_tick = 0;
uint32_t last_eight_bars_completed_tick = 0;

uint8_t old_midi_channel = 7;
uint8_t midi_channel = 7;

uint32_t ticks = 0;

#define NUM_LFO		(2)
lfo_t lfo[NUM_LFO];
volatile bool must_update_lfo = false;

#define LFO_AND_CLOCK_OUT_ENABLE			(0x04)

uint8_t program_options = 0x00;

#define NUM_CLOCK_OUTPUTS	(2)
#define CLOCK_TRIGGER_COUNTDOWN_INIT	(3)
clock_trigger_t clock_output[NUM_CLOCK_OUTPUTS];
volatile bool must_update_clock_output = false;

// additional variables to emulate hardware I/O
uint8_t button_led_port = 0x00;
uint8_t gate_port = 0x00;
uint8_t trigger_port = 0x00;
uint8_t button_pin = 0x00;
uint8_t input_buffer[NUM_SHIFTIN_REG];
uint16_t analog_input_value[4] = {0x00};
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
void update_lfo(void);
void update_clock_output(void);
void process_user_input(void);
void process_analog_in(void);
void update_clock_trigger(void);
void init_variables(void);
void init_lfo(void);
void init_io(void);

// some additional functions needed for our tests
void dac8568c_write(uint8_t command, uint8_t address, uint32_t data);
void cli() {}
void sei() {}
void sr74hc165_read(uint8_t* buffer, uint8_t numsr);
void init_input_buffer(void);
void init_notes(void);
void prepare_four_notes_on_stack(void);
void insert_midibuffer_test(testnote_t n);
void timer1_overflow_function(void);
// ----------------------------------------------

bool midi_handler_function(midimessage_t* m) {
	midinote_t mnote;
	uint8_t i=0;
	if(m->byte[0] == NOTE_ON(midi_channel)) {
		mnote.note = m->byte[1];
		mnote.velocity = m->byte[2];
		if(mnote.velocity != 0x00) {
			midinote_stack_push(&note_stack, mnote);
			for(i=0;i<NUM_LFO;i++) {
				if(lfo[i].retrigger_on_new_note)
					lfo[i].position = 0;
			}
		} else {
			midinote_stack_remove(&note_stack, m->byte[1]);
		}
		return true;
	} else if (m->byte[0] == NOTE_OFF(midi_channel)) {
		midinote_stack_remove(&note_stack, m->byte[1]);
		return true;
	} else if (m->byte[0] == PITCH_BEND(midi_channel)) {
		// TODO: implement some logic to really bend the pitch of the stack notes
		pitchbend = (m->byte[2]<<7) | m->byte[1];
		return true;
	} else if (m->byte[0] == CONTROL_CHANGE(midi_channel)) {
		if(ALL_NOTES_OFF(m->byte[2])) {
			midinote_stack_init(&note_stack);
		} else if (m->byte[2] == MOD_WHEEL) {
			//TODO: do something special here(?)
			return true;
		}
		return true;
	}
	switch(m->byte[0]) {
		case CLOCK_SIGNAL:
			midiclock_counter++;
			cli();
			last_midiclock_tick = current_midiclock_tick;
			current_midiclock_tick = ticks;
			sei();
			update_clock = true;
			break;
		case CLOCK_START:
		case CLOCK_STOP:
			midiclock_counter = 0;
			cli();
			// avoid division by zero in clock synced lfo mode
			last_midiclock_tick = 1;
			current_midiclock_tick = 2;
			sei();
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
		note_t note = playing_notes[i].midinote.note;
		vel_t velocity = playing_notes[i].midinote.velocity;
		uint32_t voltage;
		get_voltage(note, &voltage);
		if(voltage != 0x00) { // do not reset the oscillators pitch
			dac8568c_write(DAC_WRITE_UPDATE_N, i, voltage);
		}
		if(!ISSET(program_options, LFO_AND_CLOCK_OUT_ENABLE)) {
			// Send velocity
			get_voltage(velocity, &voltage);
			dac8568c_write(DAC_WRITE_UPDATE_N, i+NUM_PLAY_NOTES, voltage);
		}

		// not putting this if-clause at start because we would have to reset all
		// other pins/dac-outputs anyway... but as of memset to EMPTY_NOTE in update_notes
		// they are already EMPTY_NOTE here if this note is not playing and will get reset
		// implicitly here
		if(playing_notes[i].midinote.note != EMPTY_NOTE) {
			GATE_PORT |= (1<<(i+(GATE_OFFSET)));
		} else {
			GATE_PORT &= ~(1<<(i+(GATE_OFFSET)));
		}
	}
}

void update_lfo(void) {
	if(ISSET(program_options, LFO_AND_CLOCK_OUT_ENABLE)) {
		uint8_t i=0;
		for(;i<NUM_LFO;i++) {
			uint32_t voltage = lfo[i].get_value(lfo+i);
			dac8568c_write(DAC_WRITE_UPDATE_N, i+NUM_PLAY_NOTES, voltage);
		}
	}
}

void update_clock_output(void) {
	if(ISSET(program_options, LFO_AND_CLOCK_OUT_ENABLE)) {
		uint8_t i=0;
		for(i=0;i<NUM_CLOCK_OUTPUTS;i++) {
			uint32_t voltage = 0x0000;
			if(clock_output[i].active_countdown != 0) {
				voltage = 0xffff; //TODO: maybe adjustable clock trigger level instead?
			}
			dac8568c_write(DAC_WRITE_UPDATE_N, i+NUM_PLAY_NOTES+NUM_LFO, voltage);
		}
	}
}

void process_user_input(void) {
	uint8_t input[NUM_SHIFTIN_REG];
	sr74hc165_read(input, NUM_SHIFTIN_REG);
	uint8_t old_playmode = playmode;
	if(ISSET(input[0], MODE_BIT0)) {
		playmode = POLYPHONIC_MODE;
	} else {
		playmode = UNISON_MODE;
	}
	if(playmode != old_playmode) {
		mode[playmode].init();
	}
	if(ISSET(input[0], LFO_CLOCK_ENABLE_BIT)) {
		SET(program_options, LFO_AND_CLOCK_OUT_ENABLE);
	} else {
		UNSET(program_options, LFO_AND_CLOCK_OUT_ENABLE);
		must_update_dac = true;
		// TODO: unset the voltages
	}
	midi_channel = (input[0] & MIDI_CHANNEL_MASK);
	if(midi_channel != old_midi_channel) {
		cli();
		init_variables();
		must_update_dac = true;
		sei();
		old_midi_channel = midi_channel;
	}
	lfo[0].clock_sync = ISSET(input[1], LFO0_CLOCKSYNC);
	lfo[1].clock_sync = ISSET(input[1], LFO1_CLOCKSYNC);
	lfo[0].retrigger_on_new_note = ISSET(input[1], LFO0_RETRIGGER_ON_NEW_NOTE);
	lfo[1].retrigger_on_new_note = ISSET(input[1], LFO1_RETRIGGER_ON_NEW_NOTE);
	uint8_t wave_settings;
	uint8_t i= 0;
	for(;i<NUM_LFO;i++) {
		wave_settings = (input[1]>>lfo_offset[i])& LFO_MASK;
		switch(wave_settings) {
			case 0:
				lfo[i].get_value = lfo_get_rev_sawtooth;
				break;
			case 1:
				lfo[i].get_value = lfo_get_triangle;
				break;
			case 2:
				lfo[i].get_value = lfo_get_pulse;
				break;
			case 3:
				lfo[i].get_value = lfo_get_sawtooth;
				break;
			default:
				break;
		}
	}

	// if button not pressed - light up LED
	if( (BUTTON_PIN & (1<<BUTTON)) ) {
		BUTTON_LED_PORT |= (1<<LED);
	} else { // otherwise turn it off
		BUTTON_LED_PORT &= ~(1<<LED);
	}
}

uint16_t analog_read(uint8_t channel) {
	assert(channel<sizeof(analog_input_value));
	return analog_input_value[channel];
}

void process_analog_in(void) {
	int helper_i = 0;
	for(; helper_i<sizeof(analog_input_value); helper_i++) {
		assert(analog_input_value[helper_i]<1024);
	}
	if(ISSET(program_options, LFO_AND_CLOCK_OUT_ENABLE)) {
		uint8_t i=0;
		for(;i<NUM_LFO; i++) {
			uint16_t analog_value = analog_read(LFO_RATE_POTI0+i);
			if(lfo[i].clock_sync) {
				// analog_value/64 gives us 16 possible clock_modes
				lfo[i].clock_mode = (analog_value/64 > sizeof(clock_limit)-1) ? sizeof(clock_limit)-1 : analog_value/64;
			} else {
				lfo[i].stepwidth = ((analog_value+1)*4);
			}
		}
		for(i=0;i<NUM_CLOCK_OUTPUTS;i++) {
			uint16_t analog_value = analog_read(CLOCK_RATE_POTI0+i);
			clock_output[i].mode = analog_value/64; // make it 16 possible modes
		}
	}
}

// INFO: assure that this function is called on each increment of midiclock_counter
//       otherwise we may lose trigger-points
void update_clock_trigger(void) {
	uint8_t i;
	if(ISSET(program_options, LFO_AND_CLOCK_OUT_ENABLE)) {
		for(i=0;i<NUM_LFO;i++) {
			if((midiclock_counter % clock_limit[lfo[i].clock_mode]) == 0) {
				lfo[i].last_cycle_completed_tick = ticks;
				lfo[i].position = 0; // reset lfo position to always stay in sync with the clock
			}
		}
		if(midiclock_counter % SINGLE_BAR_COMPLETED == 0) {
			last_single_bar_completed_tick = ticks;
		}
		if(midiclock_counter % EIGHT_BARS_COMPLETED == 0) {
			last_eight_bars_completed_tick = ticks;
		}
		for(i=0;i<NUM_CLOCK_OUTPUTS;i++) {
			if((midiclock_counter % clock_limit[clock_output[i].mode]) == 0) {
				clock_output[i].active_countdown = CLOCK_TRIGGER_COUNTDOWN_INIT;
			}
		}
	}
}

void init_variables(void) {
	midinote_stack_init(&note_stack);
	midibuffer_init(&midi_buffer, &midi_handler_function);
	memset(playing_notes, EMPTY_NOTE, sizeof(playingnote_t)*NUM_PLAY_NOTES);
	memset(mode, 0, sizeof(playmode_t)*NUM_PLAY_MODES);
	mode[POLYPHONIC_MODE].update_notes = update_notes_polyphonic;
	mode[POLYPHONIC_MODE].init = init_polyphonic;
	mode[UNISON_MODE].update_notes = update_notes_unison;
	mode[UNISON_MODE].init = init_unison;
}

void init_lfo(void) {
	uint8_t i=0;
	for(;i<NUM_LFO; i++) {
		lfo[i].clock_sync = false;
		lfo[i].stepwidth = 1;
		lfo[i].get_value = lfo_get_triangle;
		lfo[i].position = 0;
	}
}

void init_io(void) {
	// don't have no hardware - nothing to do here...
}

void init_notes(void) {
	a.byte[0] = NOTE_ON(midi_channel);
	a.byte[1] = 0x6f;
	a.byte[2] = 0x5d;
	b.byte[0] = NOTE_ON(midi_channel);
	b.byte[1] = 0x6e;
	b.byte[2] = 0x4c;
	c.byte[0] = NOTE_ON(midi_channel);
	c.byte[1] = 0x3b;
	c.byte[2] = 0x2a;
	d.byte[0] = NOTE_ON(midi_channel);
	d.byte[1] = 0x19;
	d.byte[2] = 0x08;
	e.byte[0] = NOTE_ON(midi_channel);
	e.byte[1] = 0x77;
	e.byte[2] = 0x66;
}

void prepare_four_notes_on_stack(void) {
	a.byte[0] = NOTE_ON(midi_channel);
	b.byte[0] = NOTE_ON(midi_channel);
	c.byte[0] = NOTE_ON(midi_channel);
	d.byte[0] = NOTE_ON(midi_channel);
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

void init_input_buffer(void) {
	memset(input_buffer, 0, NUM_SHIFTIN_REG);
	input_buffer[1] = midi_channel;
}

void dac8568c_write(uint8_t command, uint8_t address, uint32_t data) {
	// don't have no hardware here - hard to test
}

void timer1_overflow_function(void) {
	if(shift_in_trigger_counter-- == 0) {
		shift_in_trigger_counter = SHIFTIN_TRIGGER;
		get_shiftin = true;
	}
	if(analog_in_counter-- == 0) {
		analog_in_counter = ANALOG_READ_COUNTER;
		get_analogin = true;
	}
}

void timer2_overflow_function(void) {
	ticks++;
	uint8_t i=0;
	for(;i<NUM_LFO;i++) {
		if(lfo[i].clock_sync) {
			// recalculate stepwidth everytime to even out division errors
			lfo[i].stepwidth = LFO_TABLE_LENGTH / ((current_midiclock_tick - last_midiclock_tick)*clock_limit[lfo[i].clock_mode]);
			lfo[i].position += lfo[i].stepwidth;
		} else {
			lfo[i].position += lfo[i].stepwidth;
		}
		lfo[i].position %= LFO_TABLE_LENGTH;
	}
	must_update_lfo = true;

	for(i=0;i<NUM_CLOCK_OUTPUTS;i++) {
		if(clock_output[i].active_countdown > 0) {
			clock_output[i].active_countdown--;
			must_update_clock_output = true;
		}
	}
}

int main(int argc, char** argv) {
	uint8_t i=0;
	init_notes();
	init_input_buffer();
	printf("testing init_variables() ");
	{
		init_variables();
		for(;i<MIDINOTE_STACK_SIZE; i++) {
			assert(note_stack.data[i].note == EMPTY_NOTE);
			assert(note_stack.data[i].velocity == EMPTY_NOTE);
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
		printf("\tbuffer overflow test ");
		uint8_t i=0;
		uint8_t old_write = midi_buffer.buffer.pos_write;
		for(; i<RINGBUFFER_SIZE-1; i++) {
			assert(ringbuffer_put(&(midi_buffer.buffer), i)==true);
		}
		assert(ringbuffer_put(&(midi_buffer.buffer), 'a') == false);
		midi_buffer.buffer.pos_write = old_write;
		printf("success\n");
	}
	printf("} success\n");
	printf("checking note arrived on note_stack");
	{
		midinote_t* it;
		uint8_t num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes) == true);
		assert(num_notes == 1);
		assert(it->note == 0x6f);
		assert(it->velocity == 0x5d);
	}
	printf(" success\n");
	printf("checking note handling");
	{
		assert(playing_notes[0].midinote.note == EMPTY_NOTE);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == 0x6f);
	}
	printf(" success\n");
	printf("testing peek does not pop");
	{
		memset(playing_notes, EMPTY_NOTE, sizeof(playingnote_t)*NUM_PLAY_NOTES);
		assert(playing_notes[0].midinote.note == EMPTY_NOTE && playing_notes[0].midinote.velocity == EMPTY_NOTE);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == 0x6f && playing_notes[0].midinote.velocity == 0x5d);
	}
	printf(" success\n");
	printf("testing NOTE_OFF-Handling");
	{
		a.byte[0] = NOTE_OFF(midi_channel);
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) == true);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == EMPTY_NOTE);
	}
	printf(" success\n");
	printf("testing multiple notes {");
	{
		printf("\n");
		a.byte[0] = NOTE_ON(midi_channel);
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
		printf("\tdeep note-on-off-checking");
		init_notes();
		init_variables();
		i=0;
		for(;i<14;i++) {
			a.byte[2] = 0x77;
			assert(midibuffer_put(&midi_buffer, a.byte[0]) == true);
			assert(midibuffer_tick(&midi_buffer) == false);
			assert(midibuffer_put(&midi_buffer, a.byte[1]) == true);
			assert(midibuffer_tick(&midi_buffer) == false);
			assert(midibuffer_put(&midi_buffer, a.byte[2]) == true);
			assert(midibuffer_tick(&midi_buffer) == true);
			a.byte[2] = 0x00;
			assert(midibuffer_put(&midi_buffer, a.byte[0]) == true);
			assert(midibuffer_tick(&midi_buffer) == false);
			assert(midibuffer_put(&midi_buffer, a.byte[1]) == true);
			assert(midibuffer_tick(&midi_buffer) == false);
			assert(midibuffer_put(&midi_buffer, a.byte[2]) == true);
			assert(midibuffer_tick(&midi_buffer) == true);
		}
		printf(" success\n");
	}
	printf("} success\n");
	printf("testing note_stack {\n");
	{
		init_variables();
		init_notes();
		printf("\ttesting overflow ");
		uint8_t i=0;
		midinote_t mnote;
		for(;i<MIDINOTE_STACK_SIZE; i++) {
			mnote.note = i;
			mnote.velocity = 0x55;
			assert(note_stack.position==i);
			midinote_stack_push(&note_stack, mnote);
		}
		mnote.note = i;
		assert(note_stack.data[0].note == 0);
		midinote_stack_push(&note_stack, mnote);
		assert(note_stack.position == (MIDINOTE_STACK_SIZE));
		assert(note_stack.data[0].note == 1);
		assert(note_stack.data[MIDINOTE_STACK_SIZE-1].note == i);
		i++;
		mnote.note = i;
		midinote_stack_push(&note_stack, mnote);
		assert(note_stack.position == (MIDINOTE_STACK_SIZE));
		assert(note_stack.data[0].note == 2);
		assert(note_stack.data[MIDINOTE_STACK_SIZE-1].note == i);
		i++;
		mnote.note = i;
		midinote_stack_push(&note_stack, mnote);
		assert(note_stack.position == (MIDINOTE_STACK_SIZE));
		assert(note_stack.data[0].note == 3);
		assert(note_stack.data[MIDINOTE_STACK_SIZE-1].note == i);
		printf("success\n");
	}
	printf("} success\n");
	printf("testing polyphonic mode {");
	{
		init_variables();
		init_notes();
		insert_midibuffer_test(a);
		insert_midibuffer_test(b);
		insert_midibuffer_test(c);
		insert_midibuffer_test(d);
		insert_midibuffer_test(e);
		midibuffer_tick(&midi_buffer);
		midibuffer_tick(&midi_buffer);
		midibuffer_tick(&midi_buffer);
		midibuffer_tick(&midi_buffer);
		midibuffer_tick(&midi_buffer);
		printf("\n");
		midinote_t* it;
		uint8_t num_notes = 0;
		printf("\ttesting playnotes in polyphonic mode");
		{
			playmode = POLYPHONIC_MODE;
			mode[playmode].init();
			mode[playmode].update_notes(&note_stack, playing_notes);
			assert(playing_notes[0].midinote.note == b.byte[1]);
			assert(playing_notes[1].midinote.note == c.byte[1]);
			assert(playing_notes[2].midinote.note == d.byte[1]);
			assert(playing_notes[3].midinote.note == e.byte[1]);
		}
		printf(" success\n");
		printf("\tremoving one note from playing notes");
		b.byte[0] = NOTE_OFF(midi_channel);
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
			mode[playmode].init();
			mode[playmode].update_notes(&note_stack, playing_notes);
			assert(playing_notes[0].midinote.note == a.byte[1]);
			assert(playing_notes[1].midinote.note == c.byte[1]);
			assert(playing_notes[2].midinote.note == d.byte[1]);
			assert(playing_notes[3].midinote.note == e.byte[1]);
		}
		printf(" success\n");
		printf("\tremoving another note and test again");
		d.byte[0] = NOTE_OFF(midi_channel);
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
		assert(playing_notes[2].midinote.note == EMPTY_NOTE);
		assert(playing_notes[3].midinote.note == e.byte[1]);
		printf(" success\n");
		printf("\tinserting a new note");
		b.byte[0] = NOTE_ON(midi_channel);
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
		printf("\ttesting running status ");
		init_variables();
		mode[playmode].init();
		assert(midibuffer_put(&midi_buffer, NOTE_ON(midi_channel))==true);
		assert(midibuffer_put(&midi_buffer, a.byte[1])==true);
		assert(midibuffer_put(&midi_buffer, a.byte[2])==true);
		assert(midibuffer_put(&midi_buffer, b.byte[1])==true);
		assert(midibuffer_put(&midi_buffer, b.byte[2])==true);
		assert(midibuffer_put(&midi_buffer, c.byte[1])==true);
		assert(midibuffer_put(&midi_buffer, c.byte[2])==true);
		uint8_t i=0;
		for(;i<3; i++) {
			assert(midibuffer_tick(&midi_buffer) == true);
		}
		printf("success\n");
	}
	printf("} success\n");
	printf("testing unison mode {");
	{
		printf("\n");
		printf("\tinitializing everything");
		init_variables();
		playmode = UNISON_MODE;
		mode[playmode].init();
		a.byte[0] = NOTE_ON(midi_channel);
		b.byte[0] = NOTE_ON(midi_channel);
		c.byte[0] = NOTE_ON(midi_channel);
		d.byte[0] = NOTE_ON(midi_channel);
		e.byte[0] = NOTE_ON(midi_channel);
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
		d.byte[0] = NOTE_OFF(midi_channel);
		insert_midibuffer_test(d);
		assert(midibuffer_tick(&midi_buffer) == true);
		num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 2, &it, &num_notes) == true);
		assert(it->note == c.byte[1]);
		printf(" success\n");
		printf("\tremoving playing note");
		e.byte[0] = NOTE_OFF(midi_channel);
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
		a.byte[0] = NOTE_OFF(midi_channel);
		b.byte[0] = NOTE_OFF(midi_channel);
		c.byte[0] = NOTE_OFF(midi_channel);
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
		mode[playmode].init();
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
	printf("testing user-input");
	{
		init_input_buffer();
		input_buffer[0] = (input_buffer[0] & 0xf0) | midi_channel;
		SET(input_buffer[0], MODE_BIT0);
		process_user_input();
		assert(playmode == POLYPHONIC_MODE);
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
		get_voltage(127,&out);
		assert(out<=65536);
		// test impossibly high value - though it will never occur in 7-Bit MIDI-Data...
		// but who knows...
		get_voltage(255, &out);
		assert(out<=65536);
	}
	printf(" success\n");
	printf("testing some hardcoded note ");
	{
		init_variables();
		playmode = POLYPHONIC_MODE;
		mode[playmode].init();
		testnote_t z;
		z.byte[0] = NOTE_ON(midi_channel);
		z.byte[1] = 0x3c;
		z.byte[2] = 0x72;
		insert_midibuffer_test(z);
		assert(midibuffer_tick(&midi_buffer) == true);
		midinote_t* it;
		uint8_t num_notes = 0;
		assert(midinote_stack_peek_n(&note_stack, 1, &it, &num_notes) == true);
		assert(playing_notes[0].midinote.note == EMPTY_NOTE);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(must_update_dac == true);
		assert(playing_notes[0].midinote.note == 0x3c);
		assert(gate_port == 0x00);
		update_dac();
		assert(gate_port == 0x01);
	}
	printf(" success\n");
	printf("testing velocity 0 instead of NOTE_OFF");
	{
		init_variables();
		playmode = POLYPHONIC_MODE;
		mode[playmode].init();
		testnote_t z;
		z.byte[0] = NOTE_ON(midi_channel);
		z.byte[1] = 0x3f;
		z.byte[2] = 0x77;
		insert_midibuffer_test(z);
		assert(midibuffer_tick(&midi_buffer) == true);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == 0x3f);
		z.byte[2] = 0x00;
		insert_midibuffer_test(z);
		assert(midibuffer_tick(&midi_buffer) == true);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == EMPTY_NOTE);
	}
	printf(" success\n");
	printf("testing multiple same NOTE_ON and single NOTE_OFF");
	{
		init_variables();
		init_notes();
		playmode = POLYPHONIC_MODE;
		mode[playmode].init();
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) == true);
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) == true);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == a.byte[1]);
		uint8_t i=1;
		for(;i<NUM_PLAY_NOTES; i++) {
			assert(playing_notes[i].midinote.note == EMPTY_NOTE);
		}
		a.byte[0] = NOTE_OFF(midi_channel);
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) == true);
		mode[playmode].update_notes(&note_stack, playing_notes);
		for(i=0; i<NUM_PLAY_NOTES; i++) {
			assert(playing_notes[i].midinote.note == EMPTY_NOTE);
		}
	}
	printf(" success\n");
	printf("test switching MIDI CHANNEL");
	{
		init_variables();
		playmode = POLYPHONIC_MODE;
		mode[playmode].init();
		init_notes();
		init_input_buffer();
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) == true);
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == a.byte[1]);
		// set MIDI_CHANEL to 5, preserving the rest of the byte
		input_buffer[0] = (input_buffer[0] & 0xf0) | 0x05;
		SET(input_buffer[0], MODE_BIT0);
		process_user_input();
		assert(playmode == POLYPHONIC_MODE);
		assert(midi_channel == 5);
		assert(playing_notes[0].midinote.note == EMPTY_NOTE);
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) != true);
		a.byte[0] = NOTE_ON(midi_channel);
		insert_midibuffer_test(a);
		assert(midibuffer_tick(&midi_buffer) == true);
		playmode = POLYPHONIC_MODE;
		mode[playmode].init();
		mode[playmode].update_notes(&note_stack, playing_notes);
		assert(playing_notes[0].midinote.note == a.byte[1]);
		// reset to channel 7 - not to confuse the following tests
		input_buffer[0] = (input_buffer[0] & 0xf0) | 0x07;
		process_user_input();
	}
	printf(" success\n");
	return 0;
}
