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
#include "clock_trigger.h"

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define GATE_PORT	PORTD
#define GATE_DDR	DDRD
#define GATE1		PD2
#define GATE2		PD3
#define GATE3		PD4
#define GATE4		PD5
#define GATE_OFFSET	(2)

#define LFO_RATE_POTI0	(4)
#define LFO_RATE_ANALOG_TOLERANCE	(5)

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
uint16_t clock_limit[11] = {
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
uint16_t last_analog_values[2] = {
	0,
	0
};

volatile bool update_clock = false;

uint32_t midiclock_counter = 0;
uint32_t current_midiclock_tick = 0;
uint32_t last_midiclock_tick = 0;

uint32_t last_single_bar_completed_tick = 0;
uint32_t last_eight_bars_completed_tick = 0;

uint8_t old_midi_channel = 4;
uint8_t midi_channel = 4;

uint32_t ticks = 0;
uint8_t ticks_correction = 0;
uint16_t correction_counter = 0;
uint16_t ccorrection_counter = 0;
// with our 8 bit timer and prescaler 256 we have
//		1000/((1/(16MHz/256/256))*1000)=244,140625 interrupts per second -> taking 244
// which means that if we take 244 we have to take into account that
// we have to do some error correction here:
//		1/0,140625=7,11111 -> after 7*244 interrupts we have to increment by 2 instead of 1
#define TPSEC		(244)
#define TPSEC_CORR	(7)
// now we still would have 7*244 = 1708 where we would increment to 1709
// but 244,140625*7 = 1708,984375
// resulting in an error of 1709-1708,984375 = 0,015625
// That means we are overcorrecting slightly and now could recorrect
// every 1/0,015625 = 64 steps with simply not incrementing to correct
// that will result 244,140625*7*64 = 109375 = 1709*64-1 -> error of 0,0 - yay
#define TTPSEC		(64)

#define NUM_LFO		(2)
lfo_t lfo[NUM_LFO];
volatile bool must_update_lfo = false;

#define LFO_AND_CLOCK_OUT_ENABLE			(0x04)

uint8_t program_options = 0x00;

#define NUM_CLOCK_OUTPUTS	(2)
#define CLOCK_TRIGGER_COUNTDOWN_INIT	(3)
clock_trigger_t clock_output[NUM_CLOCK_OUTPUTS];
volatile bool must_update_clock_output = false;

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
		if(!ISSET(program_options, LFO_AND_CLOCK_OUT_ENABLE)) {
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
}

void process_analog_in(void) {
	if(ISSET(program_options, LFO_AND_CLOCK_OUT_ENABLE)) {
		uint8_t i=0;
		for(;i<NUM_LFO; i++) {
			uint16_t analog_value = analog_read(LFO_RATE_POTI0+i);
			if(!(
				 (analog_value < last_analog_values[i]+LFO_RATE_ANALOG_TOLERANCE) &&
				 ((analog_value + LFO_RATE_ANALOG_TOLERANCE) > (last_analog_values[i]+LFO_RATE_ANALOG_TOLERANCE*2))
			)) {
				if(lfo[i].clock_sync) {
					lfo[i].clock_mode = analog_value/64; // now we efficiently have 16 modes
					lfo[i].clock_mode = (lfo[i].clock_mode > sizeof(clock_limit)-1) ? sizeof(clock_limit)-1 : lfo[i].clock_mode;
					lfo[i].stepwidth = LFO_TABLE_LENGTH / ((current_midiclock_tick - last_midiclock_tick)*clock_limit[lfo[i].clock_mode]);
				} else {
					lfo[i].stepwidth = ((analog_value+1)*4);
				}
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
			}
			if(midiclock_counter % SINGLE_BAR_COMPLETED == 0) {
				last_single_bar_completed_tick = ticks;
			}
			if(midiclock_counter % EIGHT_BARS_COMPLETED == 0) {
				last_eight_bars_completed_tick = ticks;
			}
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
	memset(playing_notes, 0, sizeof(playingnote_t)*NUM_PLAY_NOTES);
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
	// setting gate and trigger pins as output pins
	GATE_DDR |= (1<<GATE1)|(1<<GATE2)|(1<<GATE3)|(1<<GATE4);

	// setting trigger timer
	TCCR0 = (1<<CS02)|(1<<CS00); // set prescaler to 1024 -> ~16ms (@16MHz Clock)
	// calculation: 16000000Hz/1024 = 15625Hz trigger-rate
	//              15625Hz/256 = 61.035Hz overflow-rate (8-bit timer)
	//              1/61.035Hz = 16.3ms per overflow
	// setting lfo timer
	TCCR2 = (1<<CS22)|(1<<CS21); // set prescaler to 256 -> 4,096ms (@16MHz Clock)
	TIMSK |= (1<<TOIE0)|(1<<TOIE2); // enable overflow timer interrupt for timer 0 and 2
	dac8568c_init();
	sr74hc165_init();
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
	// tick correction first
	correction_counter++;
	if(correction_counter == TPSEC) {
		ticks_correction++;
		if(ticks_correction == TPSEC_CORR) {
			ccorrection_counter++;
			if(ccorrection_counter == TTPSEC) {
				// not correcting here - see explaination at definition of TTPSEC
				ccorrection_counter = 0;
			} else {
				ticks++;
			}
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

	for(i=0;i<NUM_CLOCK_OUTPUTS;i++) {
		if(clock_output[i].active_countdown > 0) {
			clock_output[i].active_countdown--;
			must_update_clock_output = true;
		}
	}
}

int main(int argc, char** argv) {
	cli();
	init_variables();
	init_lfo();
	init_io();
	mode[playmode].init();
	sei();
//	uint16_t j=0;
	while(1) {
		// <NORMAL FUNCTION>
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
		if(must_update_clock_output) {
			must_update_clock_output = false;
			update_clock_output();
		}
		// </NORMAL FUNCTION>

//		// <RAMP UP TEST_CASE> - use to verify all DAC-channels work
//		cli();
//		uint8_t i=0;
//		for(i=0; i<8; i++) {
//			dac8568c_write(DAC_WRITE_UPDATE_N, i, j);
//		}
//		j++;
//		for(i=0; i<100; i++) {
//			__asm("nop\n");
//		}
//		// </RAMP UP TEST_CASE>

//		// <FIXED VOLTAGES TEST_CASE> - use for calibration of DAC voltage doubling circuit
//		cli();
//		uint8_t i=0;
//		for(i=0; i<8; i++) {
//			dac8568c_write(DAC_WRITE_UPDATE_N, i, voltage[j]);
//		}
//		j++;
//		j%=10;
//		uint32_t k=0;
//		for(; k<400000000; k++) {
//			__asm("nop\n");
//		}
//		// </FIXED VOLTAGES TEST_CASE>
	}
	return 0;
}

