#ifndef LIBOPERA_PRNG16_H_INCLUDED
#define LIBOPERA_PRNG16_H_INCLUDED

#include <stdint.h>

void     prng16_seed(uint32_t const seed);
uint32_t prng16();

#endif
