#include "lr_input.h"
#include "retro_callbacks.h"

#include <libfreedo/freedo_pbus.h>

#include <libretro.h>

#include <string.h>

static const uint32_t COLORS[LR_INPUT_MAX_DEVICES] =
  {
    0x000000FF,
    0x00FF0000,
    0x0000FF00,
    0x00FFFFFF,
    0x00FF00FF,
    0x00FFFF00,
    0x0000FFFF,
    0x00000001
  };

static uint32_t ACTIVE_DEVICES = 0;
static unsigned PBUS_DEVICES[LR_INPUT_MAX_DEVICES]     = {0};
static lr_crosshair_t CROSSHAIRS[LR_INPUT_MAX_DEVICES] = {{0,0,0}};

static
void
lr_input_crosshair_draw(const lr_crosshair_t *crosshair_,
                        uint32_t             *buf_,
                        const int32_t         width_,
                        const int32_t         height_)
{
  int32_t x;
  int32_t y;
  uint32_t *p;

  x = ((crosshair_->x + 32768) / (65535 / width_));
  y = ((crosshair_->y + 32768) / (65535 / height_));

  p = &buf_[x + (y * width_)];

  *p = crosshair_->c;
  if(x >= 1)
    *(p -1) = crosshair_->c;
  if(x < (width_ - 1))
    *(p + 1) = crosshair_->c;
  if(y >= 1)
    *(p - width_) = crosshair_->c;
  if(y < (height_ - 1))
    *(p + width_) = crosshair_->c;
}

void
lr_input_crosshair_set(const uint32_t i_,
                       const int32_t  x_,
                       const int32_t  y_)
{
  if(i_ >= LR_INPUT_MAX_DEVICES)
    return;

  CROSSHAIRS[i_].x = x_;
  CROSSHAIRS[i_].y = y_;
  CROSSHAIRS[i_].c = COLORS[i_];
}

void
lr_input_crosshairs_draw(uint32_t       *buf_,
                         const uint32_t  width_,
                         const uint32_t  height_)
{
  int i;

  for(i = 0; i < LR_INPUT_MAX_DEVICES; i++)
    {
      if(CROSSHAIRS[i].c == 0)
        continue;

      lr_input_crosshair_draw(&CROSSHAIRS[i],buf_,width_,height_);
    }
}

static
uint8_t
poll_joypad(const int port_,
            const int id_)
{
  return retro_input_state_cb(port_,RETRO_DEVICE_JOYPAD,0,id_);
}

static
uint32_t
poll_mouse(const int port_,
           const int id_)
{
  return retro_input_state_cb(port_,RETRO_DEVICE_MOUSE,0,id_);
}

static
uint32_t
poll_lightgun(const int port_,
              const int id_)
{
  return retro_input_state_cb(port_,RETRO_DEVICE_LIGHTGUN,0,id_);
}

static
void
lr_input_poll_joypad(const int port_)
{
  freedo_pbus_joypad_t jp;

  jp.u  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_UP);
  jp.d  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_DOWN);
  jp.l  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_LEFT);
  jp.r  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_RIGHT);
  jp.lt = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_L);
  jp.rt = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_R);
  jp.x  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_SELECT);
  jp.p  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_START) |
          poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_X);
  jp.a  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_Y);
  jp.b  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_B);
  jp.c  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_A);

  freedo_pbus_add_joypad(&jp);
}

static
void
lr_input_poll_mouse(const int port_)
{
  freedo_pbus_mouse_t m;

  m.x      = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_X);
  m.y      = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_Y);
  m.left   = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_LEFT);
  m.middle = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_MIDDLE);
  m.right  = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_RIGHT);
  m.shift  = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_BUTTON_4);

  freedo_pbus_add_mouse(&m);
}

static
void
lr_input_poll_lightgun(const int port_)
{
  freedo_pbus_lightgun_t lg;

  lg.x       = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
  lg.y       = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);
  lg.trigger = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
  lg.option  = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SELECT);
  lg.reload  = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD);

  lr_input_crosshair_set(port_,lg.x,lg.y);

  freedo_pbus_add_lightgun(&lg);
}

static
void
lr_input_poll_saot_lightgun(const int port_)
{
  freedo_pbus_saot_lightgun_t lg;

  lg.x       = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
  lg.y       = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);
  lg.trigger = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
  lg.service = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_AUX_A);
  lg.coins   = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SELECT);
  lg.start   = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_START);
  lg.holster = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD);

  lr_input_crosshair_set(port_,lg.x,lg.y);

  freedo_pbus_add_saot_lightgun(&lg);
}

static
void
lr_input_poll(const int port_)
{
  switch(PBUS_DEVICES[port_])
    {
    case RETRO_DEVICE_NONE:
      break;
    default:
    case RETRO_DEVICE_JOYPAD:
      lr_input_poll_joypad(port_);
      return;
    case RETRO_DEVICE_MOUSE:
      lr_input_poll_mouse(port_);
      break;
    case RETRO_DEVICE_LIGHTGUN:
      lr_input_poll_lightgun(port_);
      break;
    case RETRO_DEVICE_SAOT_LIGHTGUN:
      lr_input_poll_saot_lightgun(port_);
      break;
    }
}

void
lr_input_update(const uint32_t active_devices_)
{
  int i;

  freedo_pbus_reset();
  retro_input_poll_cb();
  for(i = 0; i < active_devices_; i++)
    {
      if(PBUS_DEVICES[i] == RETRO_DEVICE_NONE)
        continue;
      lr_input_poll(i);
    }
}

static
uint32_t
setup_joypad_description(struct retro_input_descriptor *desc_,
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
setup_joypad_descriptions(struct retro_input_descriptor *desc_,
                          const unsigned                 port_)
{
  uint32_t rv;

  rv = 0;
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_LEFT,"D-Pad Left");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_UP,"D-Pad Up");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_DOWN,"D-Pad Down");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_RIGHT,"D-Pad Right");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_Y,"A");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_B,"B");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_A,"C");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_L,"L");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_R,"R");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_SELECT,"X (Stop)");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_START,"P (Play/Pause)");
  rv += setup_joypad_description(desc_++,port_,RETRO_DEVICE_ID_JOYPAD_X,"P (Play/Pause)");

  return rv;
}

static
uint32_t
setup_mouse_description(struct retro_input_descriptor *desc_,
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
setup_mouse_descriptions(struct retro_input_descriptor *desc_,
                         const unsigned                 port_)
{
  uint32_t rv;

  rv = 0;
  rv += setup_mouse_description(desc_++,port_,RETRO_DEVICE_ID_MOUSE_X,"Horizontal Axis");
  rv += setup_mouse_description(desc_++,port_,RETRO_DEVICE_ID_MOUSE_Y,"Vertical Axis");
  rv += setup_mouse_description(desc_++,port_,RETRO_DEVICE_ID_MOUSE_LEFT,"Left Button");
  rv += setup_mouse_description(desc_++,port_,RETRO_DEVICE_ID_MOUSE_MIDDLE,"Middle Button");
  rv += setup_mouse_description(desc_++,port_,RETRO_DEVICE_ID_MOUSE_RIGHT,"Right Button");

  return rv;
}

static
uint32_t
setup_lightgun_description(struct retro_input_descriptor *desc_,
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
setup_lightgun_descriptions(struct retro_input_descriptor *desc_,
                            const unsigned                 port_)
{
  uint32_t rv;

  rv = 0;
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X,"X Coord");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y,"Y Coord");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER,"Trigger");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SELECT,"Option");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD,"Reload");

  return rv;
}

static
uint32_t
setup_lightgun_saot_descriptions(struct retro_input_descriptor *desc_,
                                 const unsigned                 port_)
{
  uint32_t rv;

  rv = 0;
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X,"X Coord");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y,"Y Coord");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER,"Trigger");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_AUX_A,"Service");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_SELECT,"Coins");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_START,"Start");
  rv += setup_lightgun_description(desc_++,port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD,"Holster");

  return rv;
}

void
lr_input_set_controller_port_device(const uint32_t port_,
                                    const uint32_t device_)
{
  uint32_t i;
  uint32_t rv;
  struct retro_input_descriptor desc[256];

  PBUS_DEVICES[port_] = device_;

  rv = 0;
  for(i = 0; i < LR_INPUT_MAX_DEVICES; i++)
    {
      switch(PBUS_DEVICES[i])
        {
        case RETRO_DEVICE_NONE:
          break;
        default:
        case RETRO_DEVICE_JOYPAD:
          rv += setup_joypad_descriptions(&desc[rv],i);
          break;
        case RETRO_DEVICE_MOUSE:
          rv += setup_mouse_descriptions(&desc[rv],i);
          break;
        case RETRO_DEVICE_LIGHTGUN:
          rv += setup_lightgun_descriptions(&desc[rv],i);
          break;
        case RETRO_DEVICE_SAOT_LIGHTGUN:
          rv += setup_lightgun_saot_descriptions(&desc[rv],i);
        }
    }

  memset(&desc[rv],0,sizeof(struct retro_input_descriptor));

  retro_environment_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,desc);
}
