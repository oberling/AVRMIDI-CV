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

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define GATE_PORT	PORTC
#define GATE_DDR	DDRC
#define GATE1		PC0
#define GATE2		PC1
#define GATE3		PC2
#define GATE4		PC3
#define GATE_OFFSET	(0)

#define TRIGGER_PORT	PORTD
#define TRIGGER_DDR		DDRD
#define TRIGGER1		PD2
#define TRIGGER2		PD3
#define TRIGGER3		PD4
#define TRIGGER4		PD5
#define TRIGGER_OFFSET	(2)

#define SET(x,y)	(x |= (y))
#define ISSET(x,y)	(x & y)
#define UNSET(x,y)	(x &= ~(y))

#define NUM_PLAY_MODES 2
#define POLYPHONIC_MODE 0
#define UNISON_MODE 1

#define SHIFTIN_TRIGGER		(6)
#define NUM_SHIFTIN_REG		(1)

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

// dunno where these numbers come from
//unsigned int voltage1 = 13400;
//unsigned int voltage2 = 26500;
//unsigned int voltage3 = 39550;
//unsigned int voltage4 = 52625;
//unsigned long voltage5 = 65735;

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
volatile bool must_update_dac = false;
uint8_t shift_in_trigger_counter = SHIFTIN_TRIGGER;
volatile bool get_shiftin = false;

bool midi_handler_function(midimessage_t* m);
void get_voltage(uint8_t val, uint32_t* voltage_out);
void update_dac(void);
void process_user_input(void);
void init_variables(void);
void init_io(void);
void long_delay(uint16_t ms);
void update_notes(void);

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
	// only use a input as a uint8_t if NUM_SHIFTIN_REG equals 1...
	uint8_t input = 0;
	sr74hc165_read(&input, NUM_SHIFTIN_REG);
	// TODO: now check which bits are set and do something with it :-)
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
	// setting gate and trigger pins as output pins
	GATE_DDR = (1<<GATE1)|(1<<GATE2)|(1<<GATE3)|(1<<GATE4);
	TRIGGER_DDR = (1<<TRIGGER1)|(1<<TRIGGER2)|(1<<TRIGGER3)|(1<<TRIGGER4);

	// setting trigger timer
	TCCR0 = (1<<CS02)|(1<<CS00); // set prescaler to 1024 -> ~16ms (@16MHz Clock)
	TIMSK |= (1<<TOIE0); // enable overflow timer interrupt
	dac8568c_init();
	sr74hc165_init(NUM_SHIFTIN_REG);
	uart_init();
}

void long_delay(uint16_t ms) {
	for(;ms>0;ms--) _delay_ms(1);
}

ISR(USART_RXC_vect) {
	char a;
	uart_getc(&a);
	// this method only affects the writing position in the midibuffer
	// therefor it's ISR-save as long as the buffer does not run out of
	// space!!! prepare your buffers, everyone!
	midibuffer_put(&midi_buffer, a);
}

ISR(TIMER1_OVF_vect) {
	uint8_t i=0;
	for(;i<NUM_PLAY_NOTES; i++) {
		if(playing_notes[i].trigger_counter > 0) {
			playing_notes[i].trigger_counter--;
			if(playing_notes[i].trigger_counter-- == 0) {
				must_update_dac = true;
			}
		}
	}
	if(shift_in_trigger_counter-- == 0) {
		shift_in_trigger_counter = SHIFTIN_TRIGGER;
		get_shiftin = true;
	}
}

int main(int argc, char** argv) {
	uint8_t i = 0;
	init_variables();
	init_io();
	sei();
	while(1) {
		// handle midibuffer - update playing_notes accordingly
		if(midibuffer_tick(&midi_buffer)) {
			mode[playmode].update_notes(&note_stack, playing_notes);
			update_dac();
		}

		// handle newly triggered notes
		for(i=0;i<NUM_PLAY_NOTES; ++i) {
			if(ISSET(playing_notes[i].midinote.flags, TRIGGER_FLAG)) {
				UNSET(playing_notes[i].midinote.flags, TRIGGER_FLAG);
				playing_notes[i].trigger_counter = TRIGGER_COUNTER_INIT;
				must_update_dac = true;
			}
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
	}
	return 0;
}

