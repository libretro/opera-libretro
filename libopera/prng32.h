#ifndef LIBOPERA_PRNG32_H_INCLUDED
#define LIBOPERA_PRNG32_H_INCLUDED

#include <stdint.h>

void     prng32_seed(uint32_t const seed);
uint32_t prng32(void);

#endif
