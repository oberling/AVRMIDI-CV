#ifndef _MIDI_DATATYPES_H_
#define _MIDI_DATATYPES_H_
#include <stdint.h>

//
// midi-definitions from http://www.midi.org/techspecs/midimessages.php
//

#define STATUS_NIBBLE(x)	(x>>4)
// sysex-messages are ignored
#define SYSEX_BEGIN			(0xF0)
#define SYSEX_END			(0xF7)
#define CLOCK_SIGNAL		(0xF8)
#define CLOCK_START			(0xFA)
#define CLOCK_CONTINUE		(0xFB)
#define CLOCK_STOP			(0xFC)
#define NOTE_OFF(x)			((0x80)|(x))
#define NOTE_ON(x)			((0x90)|(x))
#define NOTE_ON_NIBBLE		(0x09)
#define NOTE_OFF_NIBBLE		(0x08)
#define CONTROL_CHANGE(x)	((0xB0)|(x))
#define PITCH_BEND(x)		((0xE0)|(x))
#define ALL_NOTES_OFF(x)	((x>=123) && (x<=127))
#define MOD_WHEEL			(0x01)

typedef uint8_t note_t;
typedef uint8_t vel_t;

typedef struct {
	note_t note;
	vel_t velocity;
} midinote_t;

/**
 * \brief struct for a 3-byte MIDI-Message
 * \description All MIDI-Messages in the Buffer are read back into
 * this type of midimessage.
 */
typedef struct {
	uint8_t byte[3];
} midimessage_t;

#endif // _MIDI_DATATYPES_H_
