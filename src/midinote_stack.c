#include "midinote_stack.h"
#include <string.h>

bool midinote_stack_init(midinote_stack_t* s) {
	memset(s->data, 0, sizeof(midinote_t)*MIDINOTE_STACK_SIZE);
	s->position = 0;
	return true;
}

bool midinote_stack_push(midinote_stack_t* s, midinote_t d) {
	if(s->position>MIDINOTE_STACK_SIZE) {
		uint8_t i=0;
		for(;i<MIDINOTE_STACK_SIZE-1; i++) {
			s->data[i] = s->data[i+1];
		}
		s->position = i;
	}
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
	if(num_req == 0 || s->position == 0) {
		*first = NULL;
		*num_ret = 0;
		return false;
	}
	if(num_req >= s->position) {
		*first = s->data;
		*num_ret = s->position;
		return true;
	}
	*first = (s->data)+(s->position)-num_req;
	*num_ret = num_req;
	return true;
}

