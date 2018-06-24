#ifndef LIBFREEDO_PBUS_H_INCLUDED
#define LIBFREEDO_PBUS_H_INCLUDED

#include <stdint.h>

struct freedo_pbus_joypad_s
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

typedef struct freedo_pbus_joypad_s freedo_pbus_joypad_t;

struct freedo_pbus_mouse_s
{
  uint8_t left;
  uint8_t middle;
  uint8_t right;
  uint8_t shift;
  int16_t x;
  int16_t y;
};

typedef struct freedo_pbus_mouse_s freedo_pbus_mouse_t;

struct freedo_pbus_lightgun_s
{
  uint8_t trigger;
  uint8_t option;
  uint8_t reload;

  int16_t x;
  int16_t y;
};

typedef struct freedo_pbus_lightgun_s freedo_pbus_lightgun_t;

struct freedo_pbus_saot_lightgun_s
{
  uint8_t trigger;
  uint8_t service;
  uint8_t coins;
  uint8_t start;
  uint8_t holster;

  int16_t x;
  int16_t y;
};

typedef struct freedo_pbus_saot_lightgun_s freedo_pbus_saot_lightgun_t;

void*    freedo_pbus_buf(void);
uint32_t freedo_pbus_size(void);
void     freedo_pbus_reset(void);
void     freedo_pbus_add_joypad(const freedo_pbus_joypad_t *joypad_);
void     freedo_pbus_add_mouse(const freedo_pbus_mouse_t *mouse_);
void     freedo_pbus_add_lightgun(const freedo_pbus_lightgun_t *lightgun_);
void     freedo_pbus_add_saot_lightgun(const freedo_pbus_saot_lightgun_t *lightgun_);

#endif /* LIBFREEDO_PBUS_H_INCLUDED */
