#include "inline.h"

#include <stdint.h>

static uint32_t g_PRNG16_STATE = 0xDEADBEEF;

static
INLINE
uint32_t
hash16(uint32_t const input_,
       uint32_t const key_)
{
  uint32_t const hash = (input_ * key_);

  return (((hash >> 16) ^ hash) & 0xFFFF);
}

void
prng16_seed(uint32_t const seed_)
{
  g_PRNG16_STATE = seed_;
}

uint32_t
prng16()
{
  g_PRNG16_STATE += 0xFC15;

  return hash16(g_PRNG16_STATE,0x02AB);
}
