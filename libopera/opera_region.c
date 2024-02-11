#include "opera_clock.h"

#include "opera_region_i.h"

#define NTSC_WIDTH 320
#define NTSC_HEIGHT 240
#define NTSC_SCANLINES 262
#define NTSC_START_SCANLINE 21
#define NTSC_END_SCANLINE (NTSC_START_SCANLINE + NTSC_HEIGHT)
#define NTSC_FIELD_RATE 60

#define PAL1_WIDTH 320
#define PAL1_HEIGHT 288
#define PAL1_SCANLINES 312
#define PAL1_START_SCANLINE 21
#define PAL1_END_SCANLINE (PAL1_START_SCANLINE + PAL1_HEIGHT)
#define PAL1_FIELD_RATE 50

#define PAL2_WIDTH 384
#define PAL2_HEIGHT 288
#define PAL2_SCANLINES 312
#define PAL2_START_SCANLINE 22
#define PAL2_END_SCANLINE (PAL2_START_SCANLINE + PAL2_HEIGHT)
#define PAL2_FIELD_RATE 50

opera_region_t g_REGION =
  {
    OPERA_REGION_NTSC,
    NTSC_WIDTH,
    NTSC_HEIGHT,
    NTSC_SCANLINES,
    NTSC_START_SCANLINE,
    NTSC_END_SCANLINE,
    NTSC_FIELD_RATE
  };

void
opera_region_set_NTSC(void)
{
  g_REGION.region         = OPERA_REGION_NTSC;
  g_REGION.width          = NTSC_WIDTH;
  g_REGION.height         = NTSC_HEIGHT;
  g_REGION.scanlines      = NTSC_SCANLINES;
  g_REGION.start_scanline = NTSC_START_SCANLINE;
  g_REGION.end_scanline   = NTSC_END_SCANLINE;
  g_REGION.field_rate     = NTSC_FIELD_RATE;

  opera_clock_region_set_ntsc();
}

void
opera_region_set_PAL1(void)
{
  g_REGION.region         = OPERA_REGION_PAL1;
  g_REGION.width          = PAL1_WIDTH;
  g_REGION.height         = PAL1_HEIGHT;
  g_REGION.scanlines      = PAL1_SCANLINES;
  g_REGION.start_scanline = PAL1_START_SCANLINE;
  g_REGION.end_scanline   = PAL1_END_SCANLINE;
  g_REGION.field_rate     = PAL1_FIELD_RATE;

  opera_clock_region_set_pal();
}

void
opera_region_set_PAL2(void)
{
  g_REGION.region         = OPERA_REGION_PAL2;
  g_REGION.width          = PAL2_WIDTH;
  g_REGION.height         = PAL2_HEIGHT;
  g_REGION.scanlines      = PAL2_SCANLINES;
  g_REGION.start_scanline = PAL2_START_SCANLINE;
  g_REGION.end_scanline   = PAL2_END_SCANLINE;
  g_REGION.field_rate     = PAL2_FIELD_RATE;

  opera_clock_region_set_pal();
}

opera_region_e
opera_region_get(void)
{
  return g_REGION.region;
}

uint32_t
opera_region_min_width(void)
{
  return NTSC_WIDTH;
}

uint32_t
opera_region_min_height(void)
{
  return NTSC_HEIGHT;
}

uint32_t
opera_region_max_width(void)
{
  return PAL2_WIDTH;
}

uint32_t
opera_region_max_height(void)
{
  return PAL2_HEIGHT;
}
