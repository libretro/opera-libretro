#ifndef LIBRETRO_LR_INPUT_CROSSHAIR_H_INCLUDED
#define LIBRETRO_LR_INPUT_CROSSHAIR_H_INCLUDED

#include <stdint.h>

void lr_input_crosshair_reset(const uint32_t i_);
void lr_input_crosshair_set(const uint32_t i_,
                            const int32_t  x_,
                            const int32_t  y_);
void lr_input_crosshairs_draw(uint32_t       *buf_,
                              const uint32_t  width_,
                              const uint32_t  height_);

#endif
