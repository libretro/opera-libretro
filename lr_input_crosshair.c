#include "lr_input.h"

#include <stdint.h>

struct lr_crosshair_s
{
  int32_t  x;
  int32_t  y;
  uint32_t c;
};

typedef struct lr_crosshair_s lr_crosshair_t;

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
lr_input_crosshair_reset(const uint32_t i_)
{
  if(i_ >= LR_INPUT_MAX_DEVICES)
    return;

  CROSSHAIRS[i_].x = 0;
  CROSSHAIRS[i_].y = 0;
  CROSSHAIRS[i_].c = 0;
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
