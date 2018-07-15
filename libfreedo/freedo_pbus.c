#include "freedo_pbus.h"

#include <stdint.h>

#define PBUS_BUF_SIZE 256

#define PBUS_FLIGHTSTICK_ID_0       0x01
#define PBUS_FLIGHTSTICK_ID_1       0x7B
#define PBUS_JOYPAD_ID              0x80
#define PBUS_MOUSE_ID               0x49
#define PBUS_LIGHTGUN_ID            0x4D
#define PBUS_ORBATAK_TRACKBALL_ID   PBUS_MOUSE_ID
#define PBUS_ORBATAK_BUTTONS_ID     0xC0

#define PBUS_JOYPAD_SHIFT_LT        0x02
#define PBUS_JOYPAD_SHIFT_RT        0x03
#define PBUS_JOYPAD_SHIFT_X         0x04
#define PBUS_JOYPAD_SHIFT_P         0x05
#define PBUS_JOYPAD_SHIFT_C         0x06
#define PBUS_JOYPAD_SHIFT_B         0x07
#define PBUS_JOYPAD_SHIFT_A         0x00
#define PBUS_JOYPAD_SHIFT_L         0x01
#define PBUS_JOYPAD_SHIFT_R         0x02
#define PBUS_JOYPAD_SHIFT_U         0x03
#define PBUS_JOYPAD_SHIFT_D         0x04

#define PBUS_FLIGHTSTICK_SHIFT_FIRE 0x07
#define PBUS_FLIGHTSTICK_SHIFT_A    0x06
#define PBUS_FLIGHTSTICK_SHIFT_B    0x05
#define PBUS_FLIGHTSTICK_SHIFT_C    0x04
#define PBUS_FLIGHTSTICK_SHIFT_U    0x03
#define PBUS_FLIGHTSTICK_SHIFT_D    0x02
#define PBUS_FLIGHTSTICK_SHIFT_R    0x01
#define PBUS_FLIGHTSTICK_SHIFT_L    0x00
#define PBUS_FLIGHTSTICK_SHIFT_P    0x07
#define PBUS_FLIGHTSTICK_SHIFT_X    0x06
#define PBUS_FLIGHTSTICK_SHIFT_LT   0x05
#define PBUS_FLIGHTSTICK_SHIFT_RT   0x04

#define PBUS_MOUSE_SHIFT_LEFT       7
#define PBUS_MOUSE_SHIFT_MIDDLE     6
#define PBUS_MOUSE_SHIFT_RIGHT      5
#define PBUS_MOUSE_SHIFT_SHIFT      4

#define PBUS_LG_SHIFT_TRIGGER       7
#define PBUS_LG_SHIFT_SERVICE       6
#define PBUS_LG_SHIFT_COINS         5
#define PBUS_LG_SHIFT_START         4
#define PBUS_LG_SHIFT_HOLSTER       3
#define PBUS_LG_SHIFT_OPTION        3

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

void
freedo_pbus_add_joypad(const freedo_pbus_joypad_t *jp_)
{
  if((PBUS.idx + 2) >= PBUS_BUF_SIZE)
    return;

  PBUS.buf[PBUS.idx++] = ((PBUS_JOYPAD_ID)                  |
                          (jp_->d  << PBUS_JOYPAD_SHIFT_D)  |
                          (jp_->u  << PBUS_JOYPAD_SHIFT_U)  |
                          (jp_->r  << PBUS_JOYPAD_SHIFT_R)  |
                          (jp_->l  << PBUS_JOYPAD_SHIFT_L)  |
                          (jp_->a  << PBUS_JOYPAD_SHIFT_A));
  PBUS.buf[PBUS.idx++] = ((jp_->b  << PBUS_JOYPAD_SHIFT_B)  |
                          (jp_->c  << PBUS_JOYPAD_SHIFT_C)  |
                          (jp_->p  << PBUS_JOYPAD_SHIFT_P)  |
                          (jp_->x  << PBUS_JOYPAD_SHIFT_X)  |
                          (jp_->rt << PBUS_JOYPAD_SHIFT_RT) |
                          (jp_->lt << PBUS_JOYPAD_SHIFT_LT));
}

void
freedo_pbus_add_flightstick(const freedo_pbus_flightstick_t *fs_)
{
  uint8_t x;
  uint8_t y;
  uint8_t z;

  if((PBUS.idx + 9) >= PBUS_BUF_SIZE)
    return;

  x = ((fs_->h_pos + 32768) / (65536 / 256));
  y = ((fs_->v_pos + 32768) / (65536 / 256));
  z = ((fs_->z_pos + 32768) / (65536 / 256));

  PBUS.buf[PBUS.idx++] = PBUS_FLIGHTSTICK_ID_0;
  PBUS.buf[PBUS.idx++] = PBUS_FLIGHTSTICK_ID_1;
  PBUS.buf[PBUS.idx++] = 0x08;
  PBUS.buf[PBUS.idx++] = x;

  PBUS.buf[PBUS.idx++] = (y >> 2);
  PBUS.buf[PBUS.idx++] = (((y & 0x03) << 6) | ((z & 0xF0) >> 4));
  PBUS.buf[PBUS.idx++] = (((z & 0x0F) << 4) | 0x02);
  PBUS.buf[PBUS.idx++] = ((fs_->fire << PBUS_FLIGHTSTICK_SHIFT_FIRE) |
                          (fs_->a    << PBUS_FLIGHTSTICK_SHIFT_A)    |
                          (fs_->b    << PBUS_FLIGHTSTICK_SHIFT_B)    |
                          (fs_->c    << PBUS_FLIGHTSTICK_SHIFT_C)    |
                          (fs_->u    << PBUS_FLIGHTSTICK_SHIFT_U)    |
                          (fs_->d    << PBUS_FLIGHTSTICK_SHIFT_D)    |
                          (fs_->r    << PBUS_FLIGHTSTICK_SHIFT_R)    |
                          (fs_->l    << PBUS_FLIGHTSTICK_SHIFT_L));

  PBUS.buf[PBUS.idx++] = ((fs_->p    << PBUS_FLIGHTSTICK_SHIFT_P)    |
                          (fs_->x    << PBUS_FLIGHTSTICK_SHIFT_X)    |
                          (fs_->lt   << PBUS_FLIGHTSTICK_SHIFT_LT)   |
                          (fs_->rt   << PBUS_FLIGHTSTICK_SHIFT_RT));
}

void
freedo_pbus_add_mouse(const freedo_pbus_mouse_t *mouse_)
{
  if((PBUS.idx + 4) >= PBUS_BUF_SIZE)
    return;

  PBUS.buf[PBUS.idx++] = PBUS_MOUSE_ID;
  PBUS.buf[PBUS.idx++] = ((mouse_->left   << PBUS_MOUSE_SHIFT_LEFT)   |
                          (mouse_->middle << PBUS_MOUSE_SHIFT_MIDDLE) |
                          (mouse_->right  << PBUS_MOUSE_SHIFT_RIGHT)  |
                          (mouse_->shift  << PBUS_MOUSE_SHIFT_SHIFT)  |
                          ((mouse_->y & 0x3C0) >> 6));
  PBUS.buf[PBUS.idx++] = (((mouse_->y & 0x03F) << 2) |
                          ((mouse_->x & 0x300) >> 8));
  PBUS.buf[PBUS.idx++] = (mouse_->x & 0xFF);
}

/*
  The algo below is derived from FreeDO's lightgun.cpp. It's not clear
  where all the constants come from or refer to.
*/
void
freedo_pbus_add_lightgun(const freedo_pbus_lightgun_t *lg_)
{
  int32_t x;
  int32_t y;
  int32_t r;
  uint8_t trigger;

  if((PBUS.idx + 4) >= PBUS_BUF_SIZE)
    return;

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

  PBUS.buf[PBUS.idx++] = PBUS_LIGHTGUN_ID;
  PBUS.buf[PBUS.idx++] = ((trigger      << PBUS_LG_SHIFT_TRIGGER) |
                          (lg_->option  << PBUS_LG_SHIFT_OPTION)  |
                          ((r & 0x10000) >> 16));
  PBUS.buf[PBUS.idx++] = ((r & 0xFF00) >> 8);
  PBUS.buf[PBUS.idx++] = (r & 0xFF);
}

void
freedo_pbus_add_arcade_lightgun(const freedo_pbus_arcade_lightgun_t *lg_)
{
  int32_t x;
  int32_t y;
  int32_t r;

  if((PBUS.idx + 4) >= PBUS_BUF_SIZE)
    return;

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

  PBUS.buf[PBUS.idx++] = PBUS_LIGHTGUN_ID;
  PBUS.buf[PBUS.idx++] = ((lg_->trigger << PBUS_LG_SHIFT_TRIGGER) |
                          (lg_->service << PBUS_LG_SHIFT_SERVICE) |
                          (lg_->coins   << PBUS_LG_SHIFT_COINS)   |
                          (lg_->start   << PBUS_LG_SHIFT_START)   |
                          (lg_->holster << PBUS_LG_SHIFT_HOLSTER) |
                          ((r & 0x10000) >> 16));
  PBUS.buf[PBUS.idx++] = ((r & 0xFF00) >> 8);
  PBUS.buf[PBUS.idx++] = (r & 0xFF);
}

void
freedo_pbus_add_orbatak_trackball(const freedo_pbus_orbatak_trackball_t *tb_)
{
  if((PBUS.idx + 8) >= PBUS_BUF_SIZE)
    return;

  PBUS.buf[PBUS.idx++] = PBUS_ORBATAK_TRACKBALL_ID;
  PBUS.buf[PBUS.idx++] = ((tb_->y & 0x3C0) >> 6);
  PBUS.buf[PBUS.idx++] = (((tb_->y & 0x03F) << 2) |
                          ((tb_->x & 0x300) >> 8));
  PBUS.buf[PBUS.idx++] = (tb_->x & 0xFF);

  PBUS.buf[PBUS.idx++] = PBUS_ORBATAK_BUTTONS_ID;
  PBUS.buf[PBUS.idx++] = 0;
  PBUS.buf[PBUS.idx++] = ((tb_->coin_p1  << PBUS_ORBATAK_SHIFT_COIN_P1)  |
                          (tb_->coin_p2  << PBUS_ORBATAK_SHIFT_COIN_P2)  |
                          (tb_->start_p1 << PBUS_ORBATAK_SHIFT_START_P1) |
                          (tb_->start_p2 << PBUS_ORBATAK_SHIFT_START_P2) |
                          (tb_->service  << PBUS_ORBATAK_SHIFT_SERVICE));
  PBUS.buf[PBUS.idx++] = 0;
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
freedo_pbus_pad(void)
{
  int i;

  for(i = 0; ((i < 8) && (PBUS.idx < PBUS_BUF_SIZE)); i++)
    PBUS.buf[PBUS.idx++] = 0xFF;
}

void
freedo_pbus_reset(void)
{
  PBUS.idx = 0;
}
