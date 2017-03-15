#include "midinote_stack.h"
#include <string.h>

bool midinote_stack_init(midinote_stack_t* s) {
	memset(s->data, EMPTY_NOTE, sizeof(midinote_t)*MIDINOTE_STACK_SIZE);
	s->position = 0;
	return true;
}

bool midinote_stack_push(midinote_stack_t* s, midinote_t d) {
	uint8_t i=0;
	// search for note and update it if found
	for(;i<s->position;i++) {
		if(s->data[i].note == d.note) {
			s->data[i] = d;
			return true;
		}
	}
	// if no update of existing note then i is now at s->position
	// if stack is full: forget oldest note - shift all notes one position to front
	if(s->position>=MIDINOTE_STACK_SIZE) {
		for(i=0;i<MIDINOTE_STACK_SIZE-1; i++) {
			s->data[i] = s->data[i+1];
		}
		s->position--; // decrease position only to increase it again :)
	}
	// finally put in new note and increase position pointer
	s->data[s->position++] = d;
	return true;
}

bool midinote_stack_pop(midinote_stack_t* s, midinote_t* out) {
	if(s->position == 0) {
		return false;
	}
	*out = s->data[s->position--];
	return true;
}

bool midinote_stack_remove(midinote_stack_t* s, note_t remnote) {
	uint8_t i=0;
	while(s->data[i].note != remnote && i<MIDINOTE_STACK_SIZE)
		i++;
	if(i>=MIDINOTE_STACK_SIZE)
		return false;
	while(i<MIDINOTE_STACK_SIZE) {
		s->data[i] = s->data[i+1];
		i++;
	}
	s->position--;
	return true;
}

bool midinote_stack_peek_n(midinote_stack_t* s, uint8_t num_req, midinote_t** first, uint8_t* num_ret) {
	// if no notes where requested or the stack is empty return nothing and false
	if(num_req == 0 || s->position == 0) {
		*first = NULL;
		*num_ret = 0;
		return false;
	}
	// if more or equal notes are requested than are on the stack 
	// return all notes from stack and true
	if(num_req >= s->position) {
		*first = s->data;
		*num_ret = s->position;
		return true;
	}
	// otherwise return as many as requested taking the most recent added ones
	*first = (s->data)+(s->position)-num_req;
	*num_ret = num_req;
	return true;
}

