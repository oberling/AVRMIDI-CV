#ifndef __LRU_CACHE_H_
#define __LRU_CACHE_H_

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t lru_cache;

bool lru_cache_init(lru_cache* cache, uint8_t num_elements);
bool lru_cache_use(lru_cache* cache, uint8_t element_index, uint8_t num_elements);

#endif
