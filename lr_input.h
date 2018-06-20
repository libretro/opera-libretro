#ifndef LIBRETRO_4DO_LR_INPUT_H_INCLUDED
#define LIBRETRO_4DO_LR_INPUT_H_INCLUDED

#include <stdint.h>

#define LR_INPUT_MAX_DEVICES 8

#define RETRO_DEVICE_SAOT_LIGHTGUN RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN,0)

struct lr_crosshair_s
{
  int32_t  x;
  int32_t  y;
  uint32_t c;
};

typedef struct lr_crosshair_s lr_crosshair_t;

void lr_input_crosshair_set(const uint32_t i_,
                            const int32_t  x_,
                            const int32_t  y_);
void lr_input_crosshairs_draw(uint32_t       *buf_,
                              const uint32_t  width_,
                              const uint32_t  height_);

void lr_input_set_controller_port_device(const uint32_t port_,
                                         const uint32_t device_);

void lr_input_update(const uint32_t active_devices_);

#endif /* LIBRETRO_4DO_LR_INPUT_H_INCLUDED */
