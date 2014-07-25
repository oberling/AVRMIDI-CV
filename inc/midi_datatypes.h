#ifndef _MIDI_DATATYPES_H_
#define _MIDI_DATATYPES_H_
#include <stdint.h>

#ifndef MIDI_CHANNEL
#pragma message	"MIDI_CHANNEL not defined - defining as Channel 4"
#define MIDI_CHANNEL		(4)
#endif

#define STATUS_NIBBLE(x)	(x>>4)
// sysex-messages are ignored
#define SYSEX_BEGIN			(0xF0)
#define SYSEX_END			(0xF7)
#define NOTE_ON				((0x80)|(MIDI_CHANNEL))
#define NOTE_OFF			((0x90)|(MIDI_CHANNEL))
#define NOTE_ON_NIBBLE		(0x08)
#define NOTE_OFF_NIBBLE		(0x09)

#define TRIGGER_FLAG		(0x01)

typedef uint8_t note_t;
typedef uint8_t vel_t;
typedef uint8_t	flag_t;

typedef struct {
	note_t note;
	vel_t velocity;
	flag_t flags;
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
