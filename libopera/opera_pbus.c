#include "opera_pbus.h"

#include <stdint.h>
#include <string.h>

#define PBUS_BUF_SIZE 256

#define PBUS_FLIGHTSTICK_ID_0       0x01
#define PBUS_FLIGHTSTICK_ID_1       0x7B
#define PBUS_JOYPAD_ID              0x80
#define PBUS_MOUSE_ID               0x49
#define PBUS_LIGHTGUN_ID            0x4D
#define PBUS_ORBATAK_TRACKBALL_ID   PBUS_MOUSE_ID
#define PBUS_ORBATAK_BUTTONS_ID     0xC0
#define PBUS_END_CHAIN_ID           0xFF

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

#define PBUS_LG_NTSC_XSCANTIME      1030
#define PBUS_LG_NTSC_YSCANTIME      12707
#define PBUS_LG_NTSC_TIMEOFFSET     (-12835)
#define PBUS_LG_NTSC_WIDTH          320
#define PBUS_LG_COUNTER_MASK        0x000FFFFF
#define PBUS_LG_HIT_COUNT           0x1F

#define PBUS_ORBATAK_SHIFT_COIN_P1  5
#define PBUS_ORBATAK_SHIFT_COIN_P2  4
#define PBUS_ORBATAK_SHIFT_START_P1 1
#define PBUS_ORBATAK_SHIFT_START_P2 2
#define PBUS_ORBATAK_SHIFT_SERVICE  0

struct pbus_s
{
  uint32_t idx;
  uint32_t finalized;
  uint8_t  buf[PBUS_BUF_SIZE];
};

typedef struct pbus_s pbus_t;

static pbus_t PBUS = {0,0,{0}};

static
uint32_t
pbus_reserve(const uint32_t len_)
{
  if(PBUS.finalized)
    return 0;

  return ((PBUS.idx + len_) <= PBUS_BUF_SIZE);
}

static
uint16_t
pbus_pos10(const int32_t pos_)
{
  int32_t rv;

  rv = ((pos_ + 32768) / 64);
  if(rv < 0)
    return 0;
  if(rv > 1023)
    return 1023;

  return rv;
}

static
uint32_t
pbus_lightgun_counter(const int32_t x_,
                      const int32_t y_)
{
  int32_t x;
  int32_t y;
  int32_t counter;

  x = x_;
  y = y_;
  if(x < 0)
    x = 0;
  if(x > 319)
    x = 319;
  if(y < 0)
    y = 0;
  if(y > 239)
    y = 239;

  counter = (((y * PBUS_LG_NTSC_YSCANTIME) +
              ((x * PBUS_LG_NTSC_XSCANTIME * 10) / PBUS_LG_NTSC_WIDTH)) / 10);

  return ((uint32_t)(counter - PBUS_LG_NTSC_TIMEOFFSET) & PBUS_LG_COUNTER_MASK);
}

void
opera_pbus_add_joypad(const opera_pbus_joypad_t *jp_)
{
  if(!pbus_reserve(2))
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
opera_pbus_add_flightstick(const opera_pbus_flightstick_t *fs_)
{
  uint16_t x;
  uint16_t y;
  uint16_t z;

  if(!pbus_reserve(9))
    return;

  x = pbus_pos10(fs_->h_pos);
  y = pbus_pos10(fs_->v_pos);
  z = pbus_pos10(fs_->z_pos);

  PBUS.buf[PBUS.idx++] = PBUS_FLIGHTSTICK_ID_0;
  PBUS.buf[PBUS.idx++] = PBUS_FLIGHTSTICK_ID_1;
  PBUS.buf[PBUS.idx++] = 0x08;
  PBUS.buf[PBUS.idx++] = (x >> 2);
  PBUS.buf[PBUS.idx++] = (((x & 0x003) << 6) | ((y & 0x3F0) >> 4));
  PBUS.buf[PBUS.idx++] = (((y & 0x00F) << 4) | ((z & 0x3C0) >> 6));
  PBUS.buf[PBUS.idx++] = (((z & 0x03F) << 2) | 0x02);
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
opera_pbus_add_mouse(const opera_pbus_mouse_t *mouse_)
{
  if(!pbus_reserve(4))
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

/* Match the Portfolio OS lightgun driver: 20-bit scan counter + hit count. */
void
opera_pbus_add_lightgun(const opera_pbus_lightgun_t *lg_)
{
  int32_t x;
  int32_t y;
  uint32_t counter;
  uint8_t hit_count;
  uint8_t trigger;

  if(!pbus_reserve(5))
    return;

  trigger = lg_->trigger;
  if(lg_->reload)
    {
      x = 0;
      y = 0;
      trigger = 1;
      hit_count = 0;
    }
  else
    {
      x = (lg_->x + (32*1024));
      y = (lg_->y + (32*1024));
      x = (x / (65535.0 / 320.0));
      y = (y / (65535.0 / 240.0));
      hit_count = PBUS_LG_HIT_COUNT;
    }

  counter = pbus_lightgun_counter(x,y);

  PBUS.buf[PBUS.idx++] = PBUS_LIGHTGUN_ID;
  PBUS.buf[PBUS.idx++] = ((trigger      << PBUS_LG_SHIFT_TRIGGER) |
                          (lg_->option  << PBUS_LG_SHIFT_OPTION)  |
                          ((counter & 0x80000) >> 19));
  PBUS.buf[PBUS.idx++] = ((counter & 0x7F800) >> 11);
  PBUS.buf[PBUS.idx++] = ((counter & 0x007F8) >> 3);
  PBUS.buf[PBUS.idx++] = (((counter & 0x00007) << 5) | hit_count);
}

void
opera_pbus_add_arcade_lightgun(const opera_pbus_arcade_lightgun_t *lg_)
{
  int32_t x;
  int32_t y;
  uint32_t counter;
  uint8_t hit_count;

  if(!pbus_reserve(5))
    return;

  if(lg_->holster)
    {
      x = 0;
      y = 0;
      hit_count = 0;
    }
  else
    {
      x = (lg_->x + (32*1024));
      y = (lg_->y + (32*1024));
      x = (x / (65535.0 / 320.0));
      y = (y / (65535.0 / 240.0));
      hit_count = PBUS_LG_HIT_COUNT;
    }

  counter = pbus_lightgun_counter(x,y);

  PBUS.buf[PBUS.idx++] = PBUS_LIGHTGUN_ID;
  PBUS.buf[PBUS.idx++] = ((lg_->trigger << PBUS_LG_SHIFT_TRIGGER) |
                          (lg_->service << PBUS_LG_SHIFT_SERVICE) |
                          (lg_->coins   << PBUS_LG_SHIFT_COINS)   |
                          (lg_->start   << PBUS_LG_SHIFT_START)   |
                          (lg_->holster << PBUS_LG_SHIFT_HOLSTER) |
                          ((counter & 0x80000) >> 19));
  PBUS.buf[PBUS.idx++] = ((counter & 0x7F800) >> 11);
  PBUS.buf[PBUS.idx++] = ((counter & 0x007F8) >> 3);
  PBUS.buf[PBUS.idx++] = (((counter & 0x00007) << 5) | hit_count);
}

void
opera_pbus_add_orbatak_trackball(const opera_pbus_orbatak_trackball_t *tb_)
{
  if(!pbus_reserve(8))
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
opera_pbus_buf(void)
{
  return PBUS.buf;
}

uint32_t
opera_pbus_size(void)
{
  return PBUS.idx;
}

void
opera_pbus_pad(void)
{
  if(PBUS.finalized)
    return;

  if(PBUS.idx >= PBUS_BUF_SIZE)
    {
      PBUS.finalized = 1;
      return;
    }

  do
    PBUS.buf[PBUS.idx++] = PBUS_END_CHAIN_ID;
  while(((PBUS.idx & 3) != 0) && (PBUS.idx < PBUS_BUF_SIZE));

  PBUS.finalized = 1;
}

void
opera_pbus_reset(void)
{
  memset(PBUS.buf,0,sizeof(PBUS.buf));
  PBUS.idx = 0;
  PBUS.finalized = 0;
}
