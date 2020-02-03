#include "freedo_clock.h"

#include "freedo_region_i.h"

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

freedo_region_t g_REGION =
  {
    FREEDO_REGION_NTSC,
    NTSC_WIDTH,
    NTSC_HEIGHT,
    NTSC_SCANLINES,
    NTSC_START_SCANLINE,
    NTSC_END_SCANLINE,
    NTSC_FIELD_RATE
  };

void
freedo_region_set_NTSC(void)
{
  g_REGION.region         = FREEDO_REGION_NTSC;
  g_REGION.width          = NTSC_WIDTH;
  g_REGION.height         = NTSC_HEIGHT;
  g_REGION.scanlines      = NTSC_SCANLINES;
  g_REGION.start_scanline = NTSC_START_SCANLINE;
  g_REGION.end_scanline   = NTSC_END_SCANLINE;
  g_REGION.field_rate     = NTSC_FIELD_RATE;

  freedo_clock_region_set_ntsc();
}

void
freedo_region_set_PAL1(void)
{
  g_REGION.region         = FREEDO_REGION_PAL1;
  g_REGION.width          = PAL1_WIDTH;
  g_REGION.height         = PAL1_HEIGHT;
  g_REGION.scanlines      = PAL1_SCANLINES;
  g_REGION.start_scanline = PAL1_START_SCANLINE;
  g_REGION.end_scanline   = PAL1_END_SCANLINE;
  g_REGION.field_rate     = PAL1_FIELD_RATE;

  freedo_clock_region_set_pal();
}

void
freedo_region_set_PAL2(void)
{
  g_REGION.region         = FREEDO_REGION_PAL2;
  g_REGION.width          = PAL2_WIDTH;
  g_REGION.height         = PAL2_HEIGHT;
  g_REGION.scanlines      = PAL2_SCANLINES;
  g_REGION.start_scanline = PAL2_START_SCANLINE;
  g_REGION.end_scanline   = PAL2_END_SCANLINE;
  g_REGION.field_rate     = PAL2_FIELD_RATE;

  freedo_clock_region_set_pal();
}

freedo_region_e
freedo_region_get(void)
{
  return g_REGION.region;
}

uint32_t
freedo_region_max_width(void)
{
  return PAL2_WIDTH;
}

uint32_t
freedo_region_max_height(void)
{
  return PAL2_HEIGHT;
}
