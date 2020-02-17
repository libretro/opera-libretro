#ifndef LIBOPERA_PBUS_H_INCLUDED
#define LIBOPERA_PBUS_H_INCLUDED

#include <stdint.h>

struct opera_pbus_joypad_s
{
  uint8_t u;
  uint8_t d;
  uint8_t l;
  uint8_t r;
  uint8_t x;
  uint8_t p;
  uint8_t a;
  uint8_t b;
  uint8_t c;
  uint8_t lt;
  uint8_t rt;
};

struct opera_pbus_flightstick_s
{
  uint8_t fire;
  uint8_t a;
  uint8_t b;
  uint8_t c;
  uint8_t u;
  uint8_t d;
  uint8_t l;
  uint8_t r;
  uint8_t p;
  uint8_t x;
  uint8_t lt;
  uint8_t rt;

  int32_t h_pos;
  int32_t v_pos;
  int32_t z_pos;
};

struct opera_pbus_mouse_s
{
  uint8_t left;
  uint8_t middle;
  uint8_t right;
  uint8_t shift;

  int16_t x;
  int16_t y;
};

struct opera_pbus_lightgun_s
{
  uint8_t trigger;
  uint8_t option;
  uint8_t reload;

  int16_t x;
  int16_t y;
};

struct opera_pbus_arcade_lightgun_s
{
  uint8_t trigger;
  uint8_t service;
  uint8_t coins;
  uint8_t start;
  uint8_t holster;

  int16_t x;
  int16_t y;
};

struct opera_pbus_orbatak_trackball_s
{
  uint8_t start_p1;
  uint8_t start_p2;
  uint8_t coin_p1;
  uint8_t coin_p2;
  uint8_t service;

  int16_t x;
  int16_t y;
};

typedef struct opera_pbus_joypad_s opera_pbus_joypad_t;
typedef struct opera_pbus_flightstick_s opera_pbus_flightstick_t;
typedef struct opera_pbus_mouse_s opera_pbus_mouse_t;
typedef struct opera_pbus_lightgun_s opera_pbus_lightgun_t;
typedef struct opera_pbus_arcade_lightgun_s opera_pbus_arcade_lightgun_t;
typedef struct opera_pbus_orbatak_trackball_s opera_pbus_orbatak_trackball_t;

void*    opera_pbus_buf(void);
uint32_t opera_pbus_size(void);
void     opera_pbus_pad(void);
void     opera_pbus_reset(void);
void     opera_pbus_add_joypad(const opera_pbus_joypad_t *joypad_);
void     opera_pbus_add_flightstick(const opera_pbus_flightstick_t *fs_);
void     opera_pbus_add_mouse(const opera_pbus_mouse_t *mouse_);
void     opera_pbus_add_lightgun(const opera_pbus_lightgun_t *lightgun_);
void     opera_pbus_add_arcade_lightgun(const opera_pbus_arcade_lightgun_t *lightgun_);
void     opera_pbus_add_orbatak_trackball(const opera_pbus_orbatak_trackball_t *tb_);

#endif /* LIBOPERA_PBUS_H_INCLUDED */
