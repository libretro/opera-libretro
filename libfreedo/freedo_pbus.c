#include "freedo_pbus.h"

#include <stdint.h>

#define PBUS_BUF_SIZE 256

#define PBUS_JOYPAD_ID   0x80
#define PBUS_MOUSE_ID    0x49
#define PBUS_LIGHTGUN_ID 0x4D

#define PBUS_JOYPAD_SHIFT_LT       0x02
#define PBUS_JOYPAD_SHIFT_RT       0x03
#define PBUS_JOYPAD_SHIFT_X        0x04
#define PBUS_JOYPAD_SHIFT_P        0x05
#define PBUS_JOYPAD_SHIFT_C        0x06
#define PBUS_JOYPAD_SHIFT_B        0x07
#define PBUS_JOYPAD_SHIFT_A        0x00
#define PBUS_JOYPAD_SHIFT_L        0x01
#define PBUS_JOYPAD_SHIFT_R        0x02
#define PBUS_JOYPAD_SHIFT_U        0x03
#define PBUS_JOYPAD_SHIFT_D        0x04
#define PBUS_JOYPAD_CONNECTED_MASK 0x80

#define PBUS_MOUSE_SHIFT_LEFT   7
#define PBUS_MOUSE_SHIFT_MIDDLE 6
#define PBUS_MOUSE_SHIFT_RIGHT  5
#define PBUS_MOUSE_SHIFT_SHIFT  4

#define PBUS_LG_SHIFT_TRIGGER 7
#define PBUS_LG_SHIFT_SERVICE 6
#define PBUS_LG_SHIFT_COINS   5
#define PBUS_LG_SHIFT_START   4
#define PBUS_LG_SHIFT_HOLSTER 3
#define PBUS_LG_SHIFT_OPTION  3

struct pbus_s
{
  uint32_t idx;
  uint8_t  buf[PBUS_BUF_SIZE];
};

typedef struct pbus_s pbus_t;

static pbus_t PBUS = {0,{0}};

static
void
pbus_add_joypad(const freedo_pbus_joypad_t *joypad_,
                uint8_t                    *buf_)
{
  buf_[0] = 0x00;
  buf_[1] = PBUS_JOYPAD_ID;
  buf_[2] = ((joypad_->lt << PBUS_JOYPAD_SHIFT_LT) |
             (joypad_->rt << PBUS_JOYPAD_SHIFT_RT) |
             (joypad_->x  << PBUS_JOYPAD_SHIFT_X)  |
             (joypad_->p  << PBUS_JOYPAD_SHIFT_P)  |
             (joypad_->c  << PBUS_JOYPAD_SHIFT_C)  |
             (joypad_->b  << PBUS_JOYPAD_SHIFT_B));
  buf_[3] = ((joypad_->a  << PBUS_JOYPAD_SHIFT_A)  |
             (joypad_->l  << PBUS_JOYPAD_SHIFT_L)  |
             (joypad_->r  << PBUS_JOYPAD_SHIFT_R)  |
             (joypad_->u  << PBUS_JOYPAD_SHIFT_U)  |
             (joypad_->d  << PBUS_JOYPAD_SHIFT_D)  |
             (PBUS_JOYPAD_CONNECTED_MASK));
}

static
void
pbus_add_mouse(const freedo_pbus_mouse_t *mouse_,
               uint8_t                   *buf_)
{
  buf_[3] = PBUS_MOUSE_ID;
  buf_[2] = ((mouse_->left   << PBUS_MOUSE_SHIFT_LEFT)   |
             (mouse_->middle << PBUS_MOUSE_SHIFT_MIDDLE) |
             (mouse_->right  << PBUS_MOUSE_SHIFT_RIGHT)  |
             (mouse_->shift  << PBUS_MOUSE_SHIFT_SHIFT) |
             ((mouse_->y & 0x3C0) >> 6));
  buf_[1] = (((mouse_->y & 0x03F) << 2) |
             ((mouse_->x & 0x300) >> 8));
  buf_[0] = (mouse_->x & 0xFF);
}

/*
  The algo below is derived from FreeDO's lightgun.cpp. It's not clear
  where all the constants come from or refer to.
*/
static
void
pbus_add_lightgun(const freedo_pbus_lightgun_t *lightgun_,
                  uint8_t                      *buf_)
{
  int32_t x;
  int32_t y;
  int32_t r;
  uint8_t trigger;

  trigger = lightgun_->trigger;
  if(lightgun_->reload)
    {
      x = 320;
      y = 0;
      trigger = 1;
    }
  else
    {
      x = (lightgun_->x + (32*1024));
      y = (lightgun_->y + (32*1024));
      x = (x / (65535.0 / 640.0));
      y = (y / (65535.0 / 240.0));
    }

  r = (((y * 794.386) + x) / 5.0);

  buf_[3] = PBUS_LIGHTGUN_ID;
  buf_[2] = ((trigger            << PBUS_LG_SHIFT_TRIGGER) |
             (lightgun_->option  << PBUS_LG_SHIFT_OPTION)  |
             ((r & 0x10000) >> 16));
  buf_[1] = ((r & 0xFF00) >> 8);
  buf_[0] = (r & 0xFF);
}

static
void
pbus_add_saot_lightgun(const freedo_pbus_saot_lightgun_t *lightgun_,
                       uint8_t                           *buf_)
{
  int32_t x;
  int32_t y;
  int32_t r;

  x = (lightgun_->x + (32*1024));
  y = (lightgun_->y + (32*1024));
  x = (x / (65535.0 / 640.0));
  y = (y / (65535.0 / 240.0));

  r = (((y * 794.386) + x) / 5.0);

  buf_[3] = PBUS_LIGHTGUN_ID;
  buf_[2] = ((lightgun_->trigger << PBUS_LG_SHIFT_TRIGGER) |
             (lightgun_->service << PBUS_LG_SHIFT_SERVICE) |
             (lightgun_->coins   << PBUS_LG_SHIFT_COINS)   |
             (lightgun_->start   << PBUS_LG_SHIFT_START)   |
             (lightgun_->holster << PBUS_LG_SHIFT_HOLSTER) |
             ((r & 0x10000) >> 16));
  buf_[1] = ((r & 0xFF00) >> 8);
  buf_[0] = (r & 0xFF);
}

void
freedo_pbus_add_joypad(const freedo_pbus_joypad_t *joypad_)
{
  if((PBUS.idx + 4) >= PBUS_BUF_SIZE)
    return;

  pbus_add_joypad(joypad_,&PBUS.buf[PBUS.idx]);
  PBUS.idx += 4;
}

void
freedo_pbus_add_mouse(const freedo_pbus_mouse_t *mouse_)
{
  if((PBUS.idx + 4) >= PBUS_BUF_SIZE)
    return;

  pbus_add_mouse(mouse_,&PBUS.buf[PBUS.idx]);
  PBUS.idx += 4;
}

void
freedo_pbus_add_lightgun(const freedo_pbus_lightgun_t *lightgun_)
{
  if((PBUS.idx + 4) >= PBUS_BUF_SIZE)
    return;

  pbus_add_lightgun(lightgun_,&PBUS.buf[PBUS.idx]);
  PBUS.idx += 4;
}

void
freedo_pbus_add_saot_lightgun(const freedo_pbus_saot_lightgun_t *lightgun_)
{
  if((PBUS.idx + 4) >= PBUS_BUF_SIZE)
    return;

  pbus_add_saot_lightgun(lightgun_,&PBUS.buf[PBUS.idx]);
  PBUS.idx += 4;
}

void*
freedo_pbus_buf(void)
{
  return PBUS.buf;
}

uint32_t
freedo_pbus_size(void)
{
  return PBUS.idx;
}

void
freedo_pbus_reset(void)
{
  PBUS.idx = 0;
}
