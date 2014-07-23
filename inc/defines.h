#ifndef __DEFINES_H_
#define __DEFINES_H_
#ifndef F_CPU
#warning "F_CPU not defined - defining as 16000000UL"
#define F_CPU (16000000UL)
#endif

#ifndef BAUD
#ifdef DEBUG
#pragma message "BAUD not defined - defining as 38400UL"
#define BAUD	(38400UL)
#else
#pragma message "BAUD not defined - defining as 31250UL"
#define BAUD	(31250UL)
#endif // DEBUG
#endif // BAUD

#ifndef NUM_PLAY_NOTES
#pragma message "NUM_PLAY_NOTES not defined - defining 4 Notes"
#define NUM_PLAY_NOTES	(4)
#endif

// must be 2^n
#define RINGBUFFER_SIZE (32)
#define ISL_DATATYPE	midimessage_t
#define ISL_SIZE		(16)
#define MIDI_CHANNEL	(4)
#endif //__DEFINES_H_
