#include "inline.h"

#include <stdint.h>

static uint32_t g_PRNG32_STATE = 0xDEADBEEF;

static
INLINE
uint32_t
splitmix32(uint32_t * const v_)
{
  uint32_t z;

  z = (*v_ += 0x9e3779b9);
  z = ((z ^ (z >> 16)) * 0x85ebca6b);
  z = ((z ^ (z >> 13)) * 0xc2b2ae35);

  return (z ^ (z >> 16));
}

void
prng32_seed(uint32_t const seed_)
{
  g_PRNG32_STATE = seed_;
}

uint32_t
prng32(void)
{
  return splitmix32(&g_PRNG32_STATE);
}
