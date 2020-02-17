#ifndef LIBRETRO_LR_INPUT_H_INCLUDED
#define LIBRETRO_LR_INPUT_H_INCLUDED

#include <stdint.h>

#define LR_INPUT_MAX_DEVICES 8

#define RETRO_DEVICE_FLIGHTSTICK RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD,0)
#define RETRO_DEVICE_ARCADE_LIGHTGUN RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN,0)
#define RETRO_DEVICE_ORBATAK_TRACKBALL RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD,1)

void     lr_input_device_set(const uint32_t port_,
                             const uint32_t device_);
uint32_t lr_input_device_get(const uint32_t port_);

void     lr_input_update(const uint32_t active_devices_);

#endif
