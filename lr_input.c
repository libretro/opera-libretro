#include "lr_input.h"
#include "lr_input_crosshair.h"

#include "opera_lr_callbacks.h"

#include <libopera/opera_pbus.h>

#include <libretro.h>

static uint32_t ACTIVE_DEVICES = 0;
static unsigned PBUS_DEVICES[LR_INPUT_MAX_DEVICES] = {0};

static
uint8_t
poll_joypad(const int port_,
            const int id_)
{
  return retro_input_state_cb(port_,RETRO_DEVICE_JOYPAD,0,id_);
}

static
int16_t
poll_analog_lx(const int port_)
{
  return retro_input_state_cb(port_,
                              RETRO_DEVICE_ANALOG,
                              RETRO_DEVICE_INDEX_ANALOG_LEFT,
                              RETRO_DEVICE_ID_ANALOG_X);
}

static
int16_t
poll_analog_ly(const int port_)
{
  return retro_input_state_cb(port_,
                              RETRO_DEVICE_ANALOG,
                              RETRO_DEVICE_INDEX_ANALOG_LEFT,
                              RETRO_DEVICE_ID_ANALOG_Y);
}

static
int16_t
poll_analog_rx(const int port_)
{
  return retro_input_state_cb(port_,
                              RETRO_DEVICE_ANALOG,
                              RETRO_DEVICE_INDEX_ANALOG_RIGHT,
                              RETRO_DEVICE_ID_ANALOG_X);
}

static
int16_t
poll_analog_ry(const int port_)
{
  return retro_input_state_cb(port_,
                              RETRO_DEVICE_ANALOG,
                              RETRO_DEVICE_INDEX_ANALOG_RIGHT,
                              RETRO_DEVICE_ID_ANALOG_Y);
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
  opera_pbus_joypad_t jp;

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

  opera_pbus_add_joypad(&jp);
}

static
void
lr_input_poll_flightstick(const int port_)
{
  opera_pbus_flightstick_t fs;

  fs.h_pos = poll_analog_lx(port_);
  fs.v_pos = poll_analog_ly(port_);
  fs.z_pos = poll_analog_ry(port_);

  fs.fire = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_R2);
  fs.a    = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_Y);
  fs.b    = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_B);
  fs.c    = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_A);
  fs.u    = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_UP);
  fs.d    = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_DOWN);
  fs.l    = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_LEFT);
  fs.r    = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_RIGHT);
  fs.p    = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_START);
  fs.x    = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_SELECT);
  fs.lt   = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_L);
  fs.rt   = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_R);

  opera_pbus_add_flightstick(&fs);
}

static
void
lr_input_poll_mouse(const int port_)
{
  opera_pbus_mouse_t m;

  m.x      = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_X);
  m.y      = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_Y);
  m.left   = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_LEFT);
  m.middle = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_MIDDLE);
  m.right  = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_RIGHT);
  m.shift  = poll_mouse(port_,RETRO_DEVICE_ID_MOUSE_BUTTON_4);

  opera_pbus_add_mouse(&m);
}

static
void
lr_input_poll_lightgun(const int port_)
{
  opera_pbus_lightgun_t lg;

  lg.x       = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
  lg.y       = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);
  lg.trigger = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
  lg.option  = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SELECT);
  lg.reload  = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD);
            
  if(poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN))
  {
    lg.trigger = 0;
    lg.reload  = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) || poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD);
  }

  lr_input_crosshair_set(port_,lg.x,lg.y);

  opera_pbus_add_lightgun(&lg);
}

static
void
lr_input_poll_arcade_lightgun(const int port_)
{
  opera_pbus_arcade_lightgun_t lg;

  lg.x       = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X);
  lg.y       = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y);
  lg.trigger = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER);
  lg.service = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_AUX_A);
  lg.coins   = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_SELECT);
  lg.start   = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_START);
  lg.holster = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD);
            
  if(poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN))
  {
    lg.trigger = 0;
    lg.holster = poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) || poll_lightgun(port_,RETRO_DEVICE_ID_LIGHTGUN_RELOAD);
  }

  lr_input_crosshair_set(port_,lg.x,lg.y);

  opera_pbus_add_arcade_lightgun(&lg);
}

static
void
lr_input_poll_orbatak_trackball(const int port_)
{
  opera_pbus_orbatak_trackball_t tb;

  tb.x = (poll_analog_lx(port_) / (32768 / 24));
  tb.y = (poll_analog_ly(port_) / (32768 / 24));
  if(tb.x == 0)
    {
      if(poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_LEFT))
        tb.x = -24;
      else if(poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_RIGHT))
        tb.x = 24;
    }

  if(tb.y == 0)
    {
      if(poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_UP))
        tb.y = -24;
      else if(poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_DOWN))
        tb.y = 24;
    }

  tb.start_p1 = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_SELECT);
  tb.start_p2 = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_START);
  tb.coin_p1  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_L);
  tb.coin_p2  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_R);
  tb.service  = poll_joypad(port_,RETRO_DEVICE_ID_JOYPAD_R2);

  opera_pbus_add_orbatak_trackball(&tb);
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
    case RETRO_DEVICE_FLIGHTSTICK:
      lr_input_poll_flightstick(port_);
      break;
    case RETRO_DEVICE_MOUSE:
      lr_input_poll_mouse(port_);
      break;
    case RETRO_DEVICE_LIGHTGUN:
      lr_input_poll_lightgun(port_);
      break;
    case RETRO_DEVICE_ARCADE_LIGHTGUN:
      lr_input_poll_arcade_lightgun(port_);
      break;
    case RETRO_DEVICE_ORBATAK_TRACKBALL:
      lr_input_poll_orbatak_trackball(port_);
      break;
    }
}

void
lr_input_device_set(const uint32_t port_,
                    const uint32_t device_)
{
  PBUS_DEVICES[port_] = device_;
}

uint32_t
lr_input_device_get(const uint32_t port_)
{
  return PBUS_DEVICES[port_];
}

void
lr_input_update(const uint32_t active_devices_)
{
  int i;

  opera_pbus_reset();
  retro_input_poll_cb();
  for(i = 0; i < active_devices_; i++)
    {
      if(PBUS_DEVICES[i] == RETRO_DEVICE_NONE)
        continue;
      lr_input_poll(i);
    }
}
