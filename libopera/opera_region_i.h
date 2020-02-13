#ifndef LIBOPERA_REGION_I_H_INCLUDED
#define LIBOPERA_REGION_I_H_INCLUDED

#include <stdint.h>

enum opera_region_e
  {
    OPERA_REGION_NTSC,
    OPERA_REGION_PAL1,
    OPERA_REGION_PAL2
  };

typedef enum opera_region_e opera_region_e;

struct opera_region_s
{
  opera_region_e region;
  uint32_t       width;
  uint32_t       height;
  uint32_t       scanlines;
  uint32_t       start_scanline;
  uint32_t       end_scanline;
  uint32_t       field_rate;
};

typedef struct opera_region_s opera_region_t;

#endif
