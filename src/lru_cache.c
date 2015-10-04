#include "lru_cache.h"

bool lru_cache_init(lru_cache* cache, uint8_t num_elements) {
	uint8_t i=0;
	for(;i<num_elements;i++) {
		cache[i] = i;
	}
	return true;
}

bool lru_cache_use(lru_cache* cache, uint8_t element_index, uint8_t num_elements) {
	uint8_t i = element_index;
	uint8_t content = cache[element_index];
	for(; i<num_elements-1; i++) {
		cache[i] = cache[i+1];
	}
	cache[i] = content;
	return true;
}

