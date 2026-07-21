#ifndef LIBOPERA_PRNG32_H_INCLUDED
#define LIBOPERA_PRNG32_H_INCLUDED

#include <stdint.h>

void     prng32_seed(uint32_t const seed);
uint32_t prng32(void);
uint32_t prng32_state_get(void);
void     prng32_state_set(uint32_t state);

#endif
