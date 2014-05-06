#include "defines.h"
#include "uart.h"
#include "dac8568c.h"
#include "midi_datatypes.h"
#include "midibuffer.h"
#include "midinote_stack.h"

#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define LED_PORT	PORTC
#define LED_DDR		DDRC
#define LED_P0		PC0
#define NUM_PLAY_NOTES	4

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
midinote_t playing_notes[NUM_PLAY_NOTES];

bool midi_handler_function(midimessage_t* m);
void get_voltage(uint8_t val, uint32_t* voltage_out);
void update_dac(void);
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
			midinote_stack_push(&note_stack, mnote);
			break;
		case NOTE_OFF:
			midinote_stack_remove(&note_stack, m->byte[1]);
			break;
	}
	return true;
}

void update_notes(void) {
	// worst: O(n) = 3n² // with n = NUM_PLAY_NOTES -> 3*4²*4 CMDs = 192 CMDs
	// at 16 MHz -> 12µs (at 5 CMDs per iteration (240CMDs): 15µs)
	// +3µs per command on deepes loop layer
	midinote_t mnotes[NUM_PLAY_NOTES];
	midinote_t* it = mnotes;
	uint8_t actual_played_notes;
	uint8_t i=0;
	uint8_t j=0;
	bool found = false;
	// get the actually to be played notes
	midinote_stack_peek_n(&note_stack, NUM_PLAY_NOTES, &it, &actual_played_notes);
	// remove nonplaying notes - leave alone the still playing notes
	for(i=0; i<NUM_PLAY_NOTES; i++) {
		found = false;
		for(;j<actual_played_notes; j++) {
			if(mnotes[j].note==playing_notes[i].note) {
				found = true;
				break;
			}
		}
		// reset that note as it isn't played anymore
		if(!found)
			memset(playing_notes+i, 0, sizeof(midinote_t));
	}
	// add new playing notes - leave alone the already playing notes
	for(i=0;i<actual_played_notes; i++) {
		found = false;
		for(j=0;j<NUM_PLAY_NOTES; j++) {
			if(mnotes[j].note==playing_notes[i].note) {
				found = true;
				break;
			}
		}
		if(!found) {
			for(j=0; j<NUM_PLAY_NOTES; j++) {
				if(playing_notes[i].note == 0) {
					playing_notes[i] = mnotes[j];
					break;
				}
			}
		}
	}
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
		unsigned int note = playing_notes[i].note;
		unsigned int velocity = playing_notes[i].velocity;
		uint32_t voltage = 0;
		get_voltage(note, &voltage);
		dac8568c_write(DAC_WRITE_UPDATE_N, i, voltage);
		// Send velocity
		get_voltage(velocity, &voltage);
		dac8568c_write(DAC_WRITE_UPDATE_N, i+NUM_PLAY_NOTES, voltage);
		// TODO: send GATE Voltage
	}
}

void init_variables(void) {
	midinote_stack_init(&note_stack);
	midibuffer_init(&midi_buffer, &midi_handler_function);
	memset(playing_notes, 0, sizeof(midinote_t)*NUM_PLAY_NOTES);
}

void init_io(void) {
	LED_DDR = (1<<LED_P0);
	dac8568c_init();
	uart_init();
}

void long_delay(uint16_t ms) {
	for(;ms>0;ms--) _delay_ms(1);
}

ISR(USART_RXC_vect) {
	cli();
	char a;
	uart_getc(&a);
	// this method only affects the writing position in the midibuffer
	// therefor it's ISR-save as long as the buffer does not run out of
	// space!!! prepare your buffers, everyone!
	midibuffer_put(&midi_buffer, a);
	sei();
}

int main(int argc, char** argv) {
	init_variables();
	init_io();
	sei();
	while(1) {
		if(midibuffer_tick(&midi_buffer)) {
			update_notes();
			update_dac();
		}
	}
	return 0;
}

