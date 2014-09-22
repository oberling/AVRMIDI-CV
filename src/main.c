#include "analog_in.h"
#include "uart.h"
#include "dac8568c.h"
#include "sr74hc165.h"
#include "datatypes.h"
#include "midi_datatypes.h"
#include "midibuffer.h"
#include "midinote_stack.h"
#include "playmode.h"
#include "polyphonic.h"
#include "unison.h"
#include "lfo.h"

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define GATE_PORT	PORTC
#define GATE_DDR	DDRC
#define GATE1		PC0
#define GATE2		PC1
#define GATE3		PC2
#define GATE4		PC3
#define GATE_OFFSET	(0)

#define LFO_RATE_POTI_CHANNEL	(5)

#define NUM_PLAY_MODES	(2)
#define POLYPHONIC_MODE	(0)
#define UNISON_MODE		(1)

// User Input defines
#define ANALOG_READ_COUNTER (2)
#define SHIFTIN_TRIGGER		(6)
#define NUM_SHIFTIN_REG		(1)

/*
 00000000
 \\\\\\\\_MIDI_CHANNEL_BIT0 \
  \\\\\\\_MIDI_CHANNEL_BIT1  \_MIDI_CHANNEL
   \\\\\\_MIDI_CHANNEL_BIT2  /
    \\\\\_MIDI_CHANNEL_BIT3 /
     \\\\_reserved
      \\\_MODE_BIT0 - polyphonic unison mode
       \\_MODE_BIT1 - yet only reserved
        \_LFO_ENABLE_BIT - lfo enable/velocity disable
*/

// bits in the bytes to represent certain modes
#define MODE_BIT0				(0x20)
#define MODE_BIT1				(0x40)
#define LFO_ENABLE_BIT			(0x80)

#define MIDI_CHANNEL_MASK		(0x0f)

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
// 384 - 4 bar; 96 - 1 bar or 1 full note; 48 - half note; ... 3 - 32th note
uint16_t clock_limit[10] = {
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
uint32_t current_midiclock_tick = 0;
uint32_t last_midiclock_tick = 0;

uint8_t old_midi_channel = 4;
uint8_t midi_channel = 4;

uint32_t ticks = 0;
uint8_t ticks_correction = 0;
uint16_t correction_counter = 0;
// with our 8 bit timer and prescaler 256 we have
//		1000/((1/(16MHz/256/256))*1000)=244,140625 interrupts per second -> taking 244
// which means that if we take 244 we have to take into account that
// we have to do some error correction here:
//		1/0,140625=7,11111 -> after 7*244 interrupts we have to increment by 2 instead of 1
#define TPSEC		(244)
#define TPSEC_CORR	(7)

#define NUM_LFO		(2)
lfo_t lfo[NUM_LFO];
volatile bool must_update_lfo = false;

#define LFO_ENABLE			(0x04)

uint8_t program_options = 0x00;


bool midi_handler_function(midimessage_t* m);
void get_voltage(uint8_t val, uint32_t* voltage_out);
void update_dac(void);
void process_user_input(void);
void process_analog_in(void);
void init_variables(void);
void init_lfo(void);
void init_io(void);

bool midi_handler_function(midimessage_t* m) {
	midinote_t mnote;
	if(m->byte[0] == NOTE_ON(midi_channel)) {
		mnote.note = m->byte[1];
		mnote.velocity = m->byte[2];
		if(mnote.velocity != 0x00) {
			midinote_stack_push(&note_stack, mnote);
		} else {
			midinote_stack_remove(&note_stack, m->byte[1]);
		}
		return true;
	} else if (m->byte[0] == NOTE_OFF(midi_channel)) {
		midinote_stack_remove(&note_stack, m->byte[1]);
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
			last_midiclock_tick = 0;
			cli();
			last_midiclock_tick = current_midiclock_tick = 0;
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
		dac8568c_write(DAC_WRITE_UPDATE_N, i, voltage);
		if(!ISSET(program_options, LFO_ENABLE)) {
			// Send velocity
			get_voltage(velocity, &voltage);
			dac8568c_write(DAC_WRITE_UPDATE_N, i+NUM_PLAY_NOTES, voltage);
		}

		// not putting this if-clause at start because we would have to reset all
		// other pins/dac-outputs anyway... but as of memset to 0 in update_notes
		// they are already 0 here if this note is not playing and will get reset
		// implicitly here
		if(playing_notes[i].midinote.note != 0) {
			GATE_PORT |= (1<<(i+(GATE_OFFSET)));
		} else {
			GATE_PORT &= ~(1<<(i+(GATE_OFFSET)));
		}
	}
}

void update_lfo(void) {
	if(ISSET(program_options, LFO_ENABLE)) {
		uint8_t i=0;
		for(;i<NUM_LFO;i++) {
			uint32_t voltage = lfo[i].get_value(lfo+i);
			dac8568c_write(DAC_WRITE_UPDATE_N, i+NUM_PLAY_NOTES, voltage);
		}
	}
}

void process_user_input(void) {
	uint8_t input[NUM_SHIFTIN_REG];
	sr74hc165_read(input, NUM_SHIFTIN_REG);
	if(ISSET(input[0], MODE_BIT0)) {
		playmode = POLYPHONIC_MODE;
	} else {
		playmode = UNISON_MODE;
	}
	if(ISSET(input[0], LFO_ENABLE_BIT)) {
		SET(program_options, LFO_ENABLE);
	} else {
		UNSET(program_options, LFO_ENABLE);
	}
	midi_channel = (input[0] & MIDI_CHANNEL_MASK);
	if(midi_channel != old_midi_channel) {
		cli();
		init_variables();
		must_update_dac = true;
		sei();
		old_midi_channel = midi_channel;
	}
}

void process_analog_in(void) {
	uint8_t i=0;
	for(;i<NUM_LFO; i++) {
		lfo[i].stepwidth = ((analog_read(LFO_RATE_POTI_CHANNEL))*4)+1;
	}
}

// INFO: assure that this function is called on each increment of midiclock_counter
//       otherwise we may lose trigger-points
void update_clock_trigger(void) {
	uint8_t i;
	for(i=0;i<NUM_LFO;i++) {
		if((midiclock_counter % clock_limit[lfo[i].clock_mode]) == 0 &&
			(ISSET(program_options, LFO_ENABLE))) {
			lfo[i].last_cycle_completed_tick = ticks;
		}
	}
}

void init_variables(void) {
	midinote_stack_init(&note_stack);
	midibuffer_init(&midi_buffer, &midi_handler_function);
	memset(playing_notes, 0, sizeof(playingnote_t)*NUM_PLAY_NOTES);
	memset(mode, 0, sizeof(playmode_t)*NUM_PLAY_MODES);
	mode[POLYPHONIC_MODE].update_notes = update_notes_polyphonic;
	mode[UNISON_MODE].update_notes = update_notes_unison;
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
	// setting gate and trigger pins as output pins
	GATE_DDR = (1<<GATE1)|(1<<GATE2)|(1<<GATE3)|(1<<GATE4);

	// setting trigger timer
	TCCR0 = (1<<CS02)|(1<<CS00); // set prescaler to 1024 -> ~16ms (@16MHz Clock)
	// calculation: 16000000Hz/1024 = 15625Hz trigger-rate
	//              15625Hz/256 = 61.035Hz overflow-rate (8-bit timer)
	//              1/61.035Hz = 16.3ms per overflow
	// setting lfo timer
	TCCR2 = (1<<CS22)|(1<<CS21); // set prescaler to 256 -> 4,096ms (@16MHz Clock)
	TIMSK |= (1<<TOIE0)|(1<<TOIE2); // enable overflow timer interrupt for timer 0 and 2
	dac8568c_init();
	sr74hc165_init(NUM_SHIFTIN_REG);
	init_analogin();
	uart_init();
}

ISR(USART_RXC_vect) {
	char a;
	uart_getc(&a);
	// this method only affects the writing position in the midibuffer
	// therefor it's ISR-save as long as the buffer does not run out of
	// space!!! prepare your buffers, everyone!
	midibuffer_put(&midi_buffer, a);
}

// ISR for timer 0 overflow - every ~16ms (calculation see init_io())
ISR(TIMER0_OVF_vect) {
	if(shift_in_trigger_counter-- == 0) {
		shift_in_trigger_counter = SHIFTIN_TRIGGER;
		get_shiftin = true;
	}
	if(analog_in_counter-- == 0) {
		analog_in_counter = ANALOG_READ_COUNTER;
		get_analogin = true;
	}
}

ISR(TIMER2_OVF_vect) {
	correction_counter++;
	if(correction_counter == TPSEC) {
		ticks_correction++;
		if(ticks_correction == TPSEC_CORR) {
			ticks++;
			ticks_correction = 0;
		}
		correction_counter = 0;
	}
	ticks++;
	uint8_t i=0;
	for(;i<NUM_LFO;i++) {
		lfo[i].position += lfo[i].stepwidth;
		if(!lfo[i].clock_sync) {
			lfo[i].position %= LFO_TABLE_LENGTH;
		} else {
			// avoid division by 0
			if(current_midiclock_tick != 0) {
				lfo[i].stepwidth = LFO_TABLE_LENGTH / ((current_midiclock_tick - last_midiclock_tick)*clock_limit[lfo[i].clock_mode]);
				lfo[i].position = (ticks - lfo[i].last_cycle_completed_tick) * lfo[i].stepwidth;
			}
		}
	}
	must_update_lfo = true;
}

int main(int argc, char** argv) {
	cli();
	init_variables();
	init_lfo();
	init_io();
	sei();
	while(1) {
		// handle midibuffer - update playing_notes accordingly
		if(midibuffer_tick(&midi_buffer)) {
			mode[playmode].update_notes(&note_stack, playing_notes);
			must_update_dac = true;
		}

		// as our TIMER_Interupt might change must_update_dac
		// during update_dac() - which is fine - we have to
		// eventually repeat update_dac() right away
		while(must_update_dac) {
			must_update_dac = false;
			update_dac();
		}
		if(get_shiftin) {
			get_shiftin = false;
			process_user_input();
		}
		if(get_analogin) {
			get_analogin = false;
			process_analog_in();
		}
		if(update_clock) {
			update_clock = false;
			update_clock_trigger();
		}
		if(must_update_lfo) {
			must_update_lfo = false;
			update_lfo();
		}
	}
	return 0;
}

