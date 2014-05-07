#ifndef __DEFINES_H_
#define __DEFINES_H_
#ifndef F_CPU
#warning "F_CPU not defined - defining as 16000000UL"
#define F_CPU 16000000UL
#endif

#ifdef DEBUG
#define BAUD	38400UL
#pragma message "BAUD not defined - defining as 38400UL"
#else
#define BAUD	31250UL
#pragma message "BAUD not defined - defining as 31250UL"
#endif

// must be 2^n
#define RINGBUFFER_SIZE 32
#define ISL_DATATYPE	midimessage_t
#define ISL_SIZE		16
#define MIDI_CHANNEL	4
#endif
