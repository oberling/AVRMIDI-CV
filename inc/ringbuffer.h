#ifndef __RINGBUFFER_H_
#define __RINGBUFFER_H_
#include <stdint.h>
#include <stdbool.h>

#ifndef RINGBUFFER_SIZE
#pragma message "RINGBUFFER_SIZE not defined - defaulting to 8"
#define RINGBUFFER_SIZE (8)
#endif
#define RINGBUFFER_MASK (RINGBUFFER_SIZE-1)

#define ringbuffer_empty(b) ((b)->pos_read == (b)->pos_write)

/**
 * \brief Datastructure for a simple ringbuffer
 * \description Our ringbuffer consists of a reading position,
 * a writing position and the buffer itself.
 * That buffer has to be sized n^2 (e.g. 8, 16, 32) defining
 * RINGBUFFER_SIZE (default is 8)
 */
typedef struct {
	uint8_t pos_read;
	uint8_t pos_write;
	unsigned char buffer[RINGBUFFER_SIZE];
} ringbuffer_t;

/**
 * \brief Function to initialize the ringbuffer
 * \description This function initializes the ringbuffer (setting
 * reading and writing position to 0).
 * \param in b pointer to the memory of the ringbuffer
 * \return for now always true
 */
bool ringbuffer_init(ringbuffer_t* b);

/**
 * \brief Function to get the next value from the ringbuffer
 * \description This function returns the next character from the buffer
 * if there is any.
 * \param in b the buffer to get the next byte from
 * \oaram out out pointer to the location in memory where to store the byte
 * \return wether or not a byte could be returned
 */
bool ringbuffer_get(ringbuffer_t* b, unsigned char* out);

/**
 * \brief Function to take a look at the next byte to be taken
 * \description This function returns the next character from the buffer
 * if there is any without removing it from the buffer - it stays the
 * next byte to be taken from the buffer
 * \param in b the buffer to get a look at the next byte from
 * \param out out pointer to the location to store the next byte to
 * \return wether or not a byte could be returned
 */
bool ringbuffer_peek(ringbuffer_t* b, unsigned char* out);

/**
 * \brief Function to put a byte into the buffer
 * \description If the buffer has space the provided byte will be stored to it.
 * \param in b the buffer to put the byte into
 * \param in a the character to put into the buffer
 * \return wether or not the byte was stored in the buffer
 */
bool ringbuffer_put(ringbuffer_t* b, unsigned char a);

/**
 * \brief returns n bytes if available or fails returning false
 * \description This function is specifically designed to return n bytes from
 * the given buffer if present or nothing.
 * \param in b the buffer to take the n bytes from
 * \param out out pointer to the location to store the n bytes to (has to be
 * sized accordingly)
 * \param in num_bytes number of bytes to try to read from the buffer
 * \return wether or not the number of bytes could be read to out
 */
bool ringbuffer_getn_or_nothing(ringbuffer_t* b, unsigned char* out, uint8_t num_bytes);

#endif // __RINGBUFFER_H_
