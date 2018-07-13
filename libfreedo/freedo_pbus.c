#include "freedo_pbus.h"

#include <stdint.h>

#define PBUS_BUF_SIZE 256

#define PBUS_FLIGHTSTICK_ID       0x01
#define PBUS_JOYPAD_ID            0x80
#define PBUS_MOUSE_ID             0x49
#define PBUS_LIGHTGUN_ID          0x4D
#define PBUS_ORBATAK_TRACKBALL_ID PBUS_MOUSE_ID
#define PBUS_ORBATAK_BUTTONS_ID   0xC0

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

#define PBUS_ORBATAK_SHIFT_COIN_P1  5
#define PBUS_ORBATAK_SHIFT_COIN_P2  4
#define PBUS_ORBATAK_SHIFT_START_P1 1
#define PBUS_ORBATAK_SHIFT_START_P2 2
#define PBUS_ORBATAK_SHIFT_SERVICE  0

struct pbus_s
{
  uint32_t idx;
  uint8_t  buf[PBUS_BUF_SIZE];
};

typedef struct pbus_s pbus_t;

static pbus_t PBUS = {0,{0}};

/*
  It's possible that with multiple joypads should be packed into
  32bits and intertwined in the format of:

  CDAB GHEF

  A = P1 MSB
  B = P1 LSB
  C = P2 MSB
  D = P2 LSB
  etc.
*/
static
void
pbus_add_joypad(const freedo_pbus_joypad_t *joypad_,
                uint8_t                    *buf_)
{
  buf_[3] = ((PBUS_JOYPAD_ID)                      |
             (joypad_->d  << PBUS_JOYPAD_SHIFT_D)  |
             (joypad_->u  << PBUS_JOYPAD_SHIFT_U)  |
             (joypad_->r  << PBUS_JOYPAD_SHIFT_R)  |
             (joypad_->l  << PBUS_JOYPAD_SHIFT_L)  |
             (joypad_->a  << PBUS_JOYPAD_SHIFT_A));
  buf_[2] = ((joypad_->b  << PBUS_JOYPAD_SHIFT_B)  |
             (joypad_->c  << PBUS_JOYPAD_SHIFT_C)  |
             (joypad_->p  << PBUS_JOYPAD_SHIFT_P)  |
             (joypad_->x  << PBUS_JOYPAD_SHIFT_X)  |
             (joypad_->rt << PBUS_JOYPAD_SHIFT_RT) |
             (joypad_->lt << PBUS_JOYPAD_SHIFT_LT));
  buf_[1] = 0xFF;
  buf_[0] = 0xFF;
}

static
void
pbus_add_flightstick(const freedo_pbus_flightstick_t *fs_,
                     uint8_t                         *buf_)
{
  /* TODO */
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
pbus_add_lightgun(const freedo_pbus_lightgun_t *lg_,
                  uint8_t                      *buf_)
{
  int32_t x;
  int32_t y;
  int32_t r;
  uint8_t trigger;

  trigger = lg_->trigger;
  if(lg_->reload)
    {
      x = 320;
      y = 0;
      trigger = 1;
    }
  else
    {
      x = (lg_->x + (32*1024));
      y = (lg_->y + (32*1024));
      x = (x / (65535.0 / 640.0));
      y = (y / (65535.0 / 240.0));
    }

  r = (((y * 794.386) + x) / 5.0);

  buf_[3] = PBUS_LIGHTGUN_ID;
  buf_[2] = ((trigger      << PBUS_LG_SHIFT_TRIGGER) |
             (lg_->option  << PBUS_LG_SHIFT_OPTION)  |
             ((r & 0x10000) >> 16));
  buf_[1] = ((r & 0xFF00) >> 8);
  buf_[0] = (r & 0xFF);
}

static
void
pbus_add_arcade_lightgun(const freedo_pbus_arcade_lightgun_t *lg_,
                         uint8_t                             *buf_)
{
  int32_t x;
  int32_t y;
  int32_t r;

  if(lg_->holster)
    {
      x = 320;
      y = 0;
    }
  else
    {
      x = (lg_->x + (32*1024));
      y = (lg_->y + (32*1024));
      x = (x / (65535.0 / 640.0));
      y = (y / (65535.0 / 240.0));
    }

  r = (((y * 794.386) + x) / 5.0);

  buf_[3] = PBUS_LIGHTGUN_ID;
  buf_[2] = ((lg_->trigger << PBUS_LG_SHIFT_TRIGGER) |
             (lg_->service << PBUS_LG_SHIFT_SERVICE) |
             (lg_->coins   << PBUS_LG_SHIFT_COINS)   |
             (lg_->start   << PBUS_LG_SHIFT_START)   |
             (lg_->holster << PBUS_LG_SHIFT_HOLSTER) |
             ((r & 0x10000) >> 16));
  buf_[1] = ((r & 0xFF00) >> 8);
  buf_[0] = (r & 0xFF);
}

static
void
pbus_add_orbatak_trackball(const freedo_pbus_orbatak_trackball_t *tb_,
                           uint8_t                               *buf_)
{
  buf_[3] = PBUS_ORBATAK_TRACKBALL_ID;
  buf_[2] = ((tb_->y & 0x3C0) >> 6);
  buf_[1] = (((tb_->y & 0x03F) << 2) |
             ((tb_->x & 0x300) >> 8));
  buf_[0] = (tb_->x & 0xFF);

  buf_[7] = PBUS_ORBATAK_BUTTONS_ID;
  buf_[6] = 0;
  buf_[5] = ((tb_->coin_p1  << PBUS_ORBATAK_SHIFT_COIN_P1)  |
             (tb_->coin_p2  << PBUS_ORBATAK_SHIFT_COIN_P2)  |
             (tb_->start_p1 << PBUS_ORBATAK_SHIFT_START_P1) |
             (tb_->start_p2 << PBUS_ORBATAK_SHIFT_START_P2) |
             (tb_->service  << PBUS_ORBATAK_SHIFT_SERVICE));
  buf_[4] = 0;
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
freedo_pbus_add_lightgun(const freedo_pbus_lightgun_t *lg_)
{
  if((PBUS.idx + 4) >= PBUS_BUF_SIZE)
    return;

  pbus_add_lightgun(lg_,&PBUS.buf[PBUS.idx]);
  PBUS.idx += 4;
}

void
freedo_pbus_add_arcade_lightgun(const freedo_pbus_arcade_lightgun_t *lg_)
{
  if((PBUS.idx + 4) >= PBUS_BUF_SIZE)
    return;

  pbus_add_arcade_lightgun(lg_,&PBUS.buf[PBUS.idx]);
  PBUS.idx += 4;
}

void
freedo_pbus_add_flightstick(const freedo_pbus_flightstick_t *fs_)
{
  /* TODO */
  return;

  if((PBUS.idx + 8) >= PBUS_BUF_SIZE)
    return;

  pbus_add_flightstick(fs_,&PBUS.buf[PBUS.idx]);
  PBUS.idx += 8;
}

void
freedo_pbus_add_orbatak_trackball(const freedo_pbus_orbatak_trackball_t *tb_)
{
  if((PBUS.idx + 8) >= PBUS_BUF_SIZE)
    return;

  pbus_add_orbatak_trackball(tb_,&PBUS.buf[PBUS.idx]);
  PBUS.idx += 8;
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
