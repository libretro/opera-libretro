#ifndef LIBOPERA_REGION_H_INCLUDED
#define LIBOPERA_REGION_H_INCLUDED

#include "inline.h"

#include "opera_region_i.h"

#include <stdint.h>

extern opera_region_t g_REGION;

void opera_region_set_NTSC(void);
void opera_region_set_PAL1(void);
void opera_region_set_PAL2(void);

opera_region_e opera_region_get(void);

uint32_t opera_region_max_width(void);
uint32_t opera_region_max_height(void);


static
INLINE
uint32_t
opera_region_width(void)
{
  return g_REGION.width;
}

static
INLINE
uint32_t
opera_region_height(void)
{
  return g_REGION.height;
}

static
INLINE
uint32_t
opera_region_scanlines(void)
{
  return g_REGION.scanlines;
}

static
INLINE
uint32_t
opera_region_start_scanline(void)
{
  return g_REGION.start_scanline;
}

static
INLINE
uint32_t
opera_region_end_scanline(void)
{
  return g_REGION.end_scanline;
}

static
INLINE
uint32_t
opera_region_field_rate(void)
{
  return g_REGION.field_rate;
}

#endif
