#ifndef _MIDINOTE_STACK_H_
#define _MIDINOTE_STACK_H_
#include "midi_datatypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef MIDINOTE_STACK_SIZE
#pragma message "MIDINOTE_STACK_SIZE not defined - defaulting to 8"
#define MIDINOTE_STACK_SIZE	8
#endif

/**
 * \brief stack datatype for MIDI-Notes
 * \description This stack is specifically designed to handle
 * MIDI Notes. It utilises an fixed-size Array
 * (MIDINOTE_STACK_SIZE) and contains a current position
 * to write the new element to.
 */
typedef struct {
	midinote_t data[MIDINOTE_STACK_SIZE];
	uint8_t position;
} midinote_stack_t;

/**
 * \brief Function to initialise the stack.
 * \description This Function initialises the given midinote_stack
 * by overriding the whole array with 0 and putting the position to
 * write to the first element
 * \param in s the midinotestack to be initialised
 * \return always true
 */
bool midinote_stack_init(midinote_stack_t* s);

/**
 * \brief Function to push a midinote to the stack
 * \description This function pushes a midinote on top of the
 * stack if there is any space left.
 * \param in s thie midinotestack
 * \param in d the midinote to push to the stack
 * \return whether or not the element has been pushed
 */
bool midinote_stack_push(midinote_stack_t* s, midinote_t d);

/**
 * \brief function to get the topmost midinote from the stack
 * \description This function writes the topmost midinote
 * of the stack to the provided midinote out variable
 * \param in s the midinote stack
 * \param out out the midinote to be fill with that data
 * \return whether or not there was data to return
 */
bool midinote_stack_pop(midinote_stack_t* s, midinote_t* out);

/**
 * \brief function to remove a certain note from the stack
 * \description This function removes a given note from the stack
 * regardless of its position and reorders the stack accordingly
 * (all following elements move one position down)
 * \param s the stack to take that element from
 * \param remnote the note that shall be removed
 * \return whether or not the note was there and has been deleted
 */
bool midinote_stack_remove(midinote_stack_t* s, note_t remnote);

/**
 * \brief take a sneak peek at the first n elements
 * \description This function fills the output array with the
 * maximum num_req elements but does not delete them from the stack.
 * \param in s the stack
 * \param in num_req the number of notes you would like to read
 * \param out first the first element of num_ret elements
 * \param out num_ret the number of elements actually returned
 * \return whether or not a midinote could be returned
 */
bool midinote_stack_peek_n(midinote_stack_t* s, uint8_t num_req, midinote_t** first, uint8_t* num_ret);

#endif
