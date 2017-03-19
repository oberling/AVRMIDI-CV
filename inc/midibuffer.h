#ifndef __MIDIBUFFER_H_
#define __MIDIBUFFER_H_

// using this ringbuffer we implement our midibufer
#include "midi_datatypes.h"
#include "ringbuffer.h"

/**
 * \brief handler function definition for midimessage handling
 * \description This function is called back whenever \a midibuffer_tick
 * is called and there is a midimessage in the buffer to be handled
 * \return wether or not your dispatch was successfull; read \a midibuffer_tick
 * for mor information on its default returns
 * \param m The midimessage to dispatch right now
 */
typedef bool (*midimessage_handler)(midimessage_t* m);

/**
 * \brief buffer definition containing a ringbuffer and the
 * dispatcher-function
 * \description This is the general description of a midibuffer. It
 * consissts of a handler-function called when midibuffer_tick is called
 * on the buffer and the buffer itself. The buffer is a simple ringbuffer.
 * Therefore its size (RINGBUFFER_SIZE) has to be a number n^2.
 */
typedef struct {
	midimessage_handler f;
	ringbuffer_t buffer;
} midibuffer_t;

/**
 * \brief Function to init the Buffer.
 * \description Call this function at the very beginning of your program
 * before you start using the midibuffer.
 * \param in b the reference to the midibuffer
 * \param in h the handler function for midimessages on this buffer
 * \return wether or not initialization went ok
 */
bool midibuffer_init(midibuffer_t* b, midimessage_handler h);

/**
 * \brief Function to get a midimessage from the buffer
 * \description This Function is intended to use the buffer with rather than
 * the \a midibuffer_tick approach
 * \param in b the midibuffer to get the midimessage from
 * \param out m the midimessage memory to be filled
 * \return wether or not a message could be stored in m
 */
bool midibuffer_get(midibuffer_t* b, midimessage_t* m);

/**
 * \brief Function to put a new byte into the midibuffer
 * \description Using a UART you would put each received byte into
 * the buffer using this function
 * \param in b the midibuffer to put that character in to
 * \param in a the character to put into the buffer
 * \return wether or not the character could be put into the buffer
 */
bool midibuffer_put(midibuffer_t* b, unsigned char a);

/**
 * \brief function to be called in your applications main loop
 * \description This function calls the provided callback function
 * but only if there is an message to dispatch. It only calls that
 * callback function once per call
 * \param in b the buffer to 'tick'
 * \returns wether or not some data had to be dispatched
 */
bool midibuffer_tick(midibuffer_t* b);

#endif // __MIDIBUFFER_H_
