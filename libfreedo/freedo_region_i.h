#ifndef FREEDO_REGION_I_H_INCLUDED
#define FREEDO_REGION_I_H_INCLUDED

#include <stdint.h>

enum freedo_region_e
  {
    FREEDO_REGION_NTSC,
    FREEDO_REGION_PAL1,
    FREEDO_REGION_PAL2
  };

typedef enum freedo_region_e freedo_region_e;

struct freedo_region_s
{
  freedo_region_e region;
  uint32_t        width;
  uint32_t        height;
  uint32_t        scanlines;
  uint32_t        start_scanline;
  uint32_t        end_scanline;
  uint32_t        field_rate;
};

typedef struct freedo_region_s freedo_region_t;

#endif
