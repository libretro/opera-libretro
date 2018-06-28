#include "lr_input.h"
#include "retro_callbacks.h"
#include "lr_input_crosshair.h"

#include <libretro.h>

#include <string.h>

static
uint32_t
setup_joypad_desc(struct retro_input_descriptor *desc_,
                  const unsigned                 port_,
                  const unsigned                 id_,
                  const char                    *str_)
{
  desc_->port        = port_;
  desc_->device      = RETRO_DEVICE_JOYPAD;
  desc_->index       = 0;
  desc_->id          = id_;
  desc_->description = str_;

  return 1;
}

static
uint32_t
setup_analog_left_desc(struct retro_input_descriptor *desc_,
                       const unsigned                 port_,
                       const unsigned                 id_,
                       const char                    *str_)
{
  desc_->port        = port_;
  desc_->device      = RETRO_DEVICE_ANALOG;
  desc_->index       = RETRO_DEVICE_INDEX_ANALOG_LEFT;
  desc_->id          = id_;
  desc_->description = str_;

  return 1;
}

static
uint32_t
setup_analog_right_desc(struct retro_input_descriptor *desc_,
                        const unsigned                 port_,
                        const unsigned                 id_,
                        const char                    *str_)
{
  desc_->port        = port_;
  desc_->device      = RETRO_DEVICE_ANALOG;
  desc_->index       = RETRO_DEVICE_INDEX_ANALOG_RIGHT;
  desc_->id          = id_;
  desc_->description = str_;

  return 1;
}

static
uint32_t
setup_mouse_desc(struct retro_input_descriptor *desc_,
                 const unsigned                 port_,
                 const unsigned                 id_,
                 const char                    *str_)
{
  desc_->port        = port_;
  desc_->device      = RETRO_DEVICE_MOUSE;
  desc_->index       = 0;
  desc_->id          = id_;
  desc_->description = str_;

  return 1;
}

static
uint32_t
setup_lightgun_desc(struct retro_input_descriptor *desc_,
                    const unsigned                 port_,
                    const unsigned                 id_,
                    const char                    *str_)
{
  desc_->port        = port_;
  desc_->device      = RETRO_DEVICE_LIGHTGUN;
  desc_->index       = 0;
  desc_->id          = id_;
  desc_->description = str_;

  return 1;
}

static
uint32_t
setup_joypad_descs(struct retro_input_descriptor *desc_,
                   const unsigned                 port_)
{
  uint32_t rv;

  rv = 0;
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_LEFT,"D-Pad Left");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_UP,"D-Pad Up");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_DOWN,"D-Pad Down");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_RIGHT,"D-Pad Right");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_Y,"A");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_B,"B");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_A,"C");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_L,"L");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_R,"R");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_SELECT,"X (Stop)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_START,"P (Play/Pause)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_X,"P (Play/Pause)");

  return rv;
}

static
uint32_t
setup_flightstick_descs(struct retro_input_descriptor *desc_,
                        const unsigned                 port_)
{
  int rv;

  rv = 0;
  rv += setup_analog_left_desc(desc_++,port_,RETRO_DEVICE_ID_ANALOG_X,"Horizontal (X)");
  rv += setup_analog_left_desc(desc_++,port_,RETRO_DEVICE_ID_ANALOG_Y,"Vertical (Y)");
  rv += setup_analog_right_desc(desc_++,port_,RETRO_DEVICE_ID_ANALOG_Y,"Depth (Z)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_LEFT,"D-Pad Left");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_UP,"D-Pad Up");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_DOWN,"D-Pad Down");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_RIGHT,"D-Pad Right");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_Y,"A");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_B,"B");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_A,"C");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_L,"L");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_R,"R");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_R2,"Fire");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_SELECT,"X (Stop)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_START,"P (Play/Pause)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_X,"P (Play/Pause)");

  return rv;
}

static
uint32_t
setup_mouse_descs(struct retro_input_descriptor *desc_,
                  const unsigned                 port_)
{
  uint32_t rv;

  rv = 0;
  rv += setup_mouse_desc(desc_++,port_,RETRO_DEVICE_ID_MOUSE_X,"Horizontal Axis");
  rv += setup_mouse_desc(desc_++,port_,RETRO_DEVICE_ID_MOUSE_Y,"Vertical Axis");
  rv += setup_mouse_desc(desc_++,port_,RETRO_DEVICE_ID_MOUSE_LEFT,"Left Button");
  rv += setup_mouse_desc(desc_++,port_,RETRO_DEVICE_ID_MOUSE_MIDDLE,"Middle Button");
  rv += setup_mouse_desc(desc_++,port_,RETRO_DEVICE_ID_MOUSE_RIGHT,"Right Button");

  return rv;
}

static
uint32_t
setup_lightgun_descs(struct retro_input_descriptor *desc_,
                     const unsigned                 port_)
{
  uint32_t rv;

  rv = 0;
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X,"X Coord");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y,"Y Coord");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER,"Trigger");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SELECT,"Option");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD,"Reload");

  return rv;
}

static
uint32_t
setup_lightgun_arcade_descs(struct retro_input_descriptor *desc_,
                            const unsigned                 port_)
{
  uint32_t rv;

  rv = 0;
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X,"X Coord");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y,"Y Coord");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER,"Trigger");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_AUX_A,"Service");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SELECT,"Coins");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_START,"Start");
  rv += setup_lightgun_desc(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD,"Holster");

  return rv;
}

static
uint32_t
setup_orbatak_descs(struct retro_input_descriptor *desc_,
                    const unsigned                 port_)
{
  uint32_t rv;

  rv = 0;
  rv += setup_analog_left_desc(desc_++,port_,RETRO_DEVICE_ID_ANALOG_X,"Trackball (Horizontal)");
  rv += setup_analog_left_desc(desc_++,port_,RETRO_DEVICE_ID_ANALOG_Y,"Trackball (Vertical)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_SELECT,"Start (P1)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_START,"Start (P2)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_L,"Coin (P1)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_R,"Coin (P2)");
  rv += setup_joypad_desc(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_R2,"Service");

  return rv;
}

void
lr_input_device_set_with_descs(const uint32_t port_,
                               const uint32_t device_)
{
  uint32_t i;
  uint32_t rv;
  struct retro_input_descriptor desc[256];

  lr_input_device_set(port_,device_);

  rv = 0;
  lr_input_crosshair_reset(port_);
  for(i = 0; i < LR_INPUT_MAX_DEVICES; i++)
    {
      switch(lr_input_device_get(i))
        {
        case RETRO_DEVICE_NONE:
          break;
        default:
        case RETRO_DEVICE_JOYPAD:
          rv += setup_joypad_descs(&desc[rv],i);
          break;
        case RETRO_DEVICE_FLIGHTSTICK:
          rv += setup_flightstick_descs(&desc[rv],i);
          break;
        case RETRO_DEVICE_MOUSE:
          rv += setup_mouse_descs(&desc[rv],i);
          break;
        case RETRO_DEVICE_LIGHTGUN:
          rv += setup_lightgun_descs(&desc[rv],i);
          break;
        case RETRO_DEVICE_ARCADE_LIGHTGUN:
          rv += setup_lightgun_arcade_descs(&desc[rv],i);
          break;
        case RETRO_DEVICE_ORBATAK_TRACKBALL:
          rv += setup_orbatak_descs(&desc[rv],i);
          break;
        }
    }

  memset(&desc[rv],0,sizeof(struct retro_input_descriptor));

  retro_environment_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,desc);
}
