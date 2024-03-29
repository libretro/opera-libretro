/*
  www.freedo.org
  The first working 3DO multiplayer emulator.

  The FreeDO licensed under modified GNU LGPL, with following notes:

  *   The owners and original authors of the FreeDO have full right to
  *   develop closed source derivative work.

  *   Any non-commercial uses of the FreeDO sources or any knowledge
  *   obtained by studying or reverse engineering of the sources, or
  *   any other material published by FreeDO have to be accompanied
  *   with full credits.

  *   Any commercial uses of FreeDO sources or any knowledge obtained
  *   by studying or reverse engineering of the sources, or any other
  *   material published by FreeDO is strictly forbidden without
  *   owners approval.

  The above notes are taking precedence over GNU LGPL in conflicting
  situations.

  Project authors:
  *  Alexander Troosh
  *  Maxim Grishin
  *  Allen Wright
  *  John Sammons
  *  Felix Lazarev
*/

#include "inline.h"
#include "opera_core.h"
#include "opera_mem.h"
#include "opera_state.h"

#include <stdint.h>
#include <string.h>

#define SPORT_IDX_MASK   0x7FF
#define SPORT_IDX_SHIFT  9
#define SPORT_ELEM_COUNT 512
#define SPORT_BUFSIZE    (SPORT_ELEM_COUNT * sizeof(uint32_t))
#define SPORT_PAGE_TO_BYTE_OFFSET(IDX) (((IDX) & SPORT_IDX_MASK) << SPORT_IDX_SHIFT)

struct sport_s
{
  uint32_t color;
  uint32_t source;
  uint32_t destination;
};

typedef struct sport_s sport_t;

static sport_t SPORT = {0};

void
opera_sport_init()
{

}

void
opera_sport_set_source(const uint32_t rawidx_)
{
  SPORT.source = SPORT_PAGE_TO_BYTE_OFFSET(rawidx_);
}

static
INLINE
void
sport_set_color(const uint32_t idx_)
{
  int i;
  uint32_t * const vram = (uint32_t * const)&VRAM[idx_];

  for(i = 0; i < SPORT_ELEM_COUNT; i++)
    vram[i] = SPORT.color;
}

static
INLINE
void
sport_set_color_with_mask(const uint32_t idx_,
                          const uint32_t mask_)
{
  int i;
  uint32_t * const vram = (uint32_t * const)&VRAM[idx_];

  for(i = 0; i < SPORT_ELEM_COUNT; i++)
    vram[i] = (((vram[i] ^ SPORT.color) & mask_) ^ SPORT.color);
}

static
INLINE
void
sport_memcpy(const uint32_t didx_,
             const uint32_t sidx_)
{
  memcpy(&VRAM[didx_],&VRAM[sidx_],SPORT_BUFSIZE);
}

static
INLINE
void
sport_memcpy_highres(const uint32_t didx_,
                     const uint32_t sidx_)
{
  sport_memcpy(didx_ + (1*VRAM_SIZE),sidx_);
  sport_memcpy(didx_ + (2*VRAM_SIZE),sidx_);
  sport_memcpy(didx_ + (3*VRAM_SIZE),sidx_);
}

static
INLINE
void
sport_flash_write(const uint32_t rawidx_,
                  const uint32_t mask_)
{
  uint32_t idx;

  idx = SPORT_PAGE_TO_BYTE_OFFSET(rawidx_);
  if(mask_ == 0xFFFFFFFF)
    sport_set_color(idx);
  else
    sport_set_color_with_mask(idx,mask_);

  if(!HIRESMODE)
    return;

  sport_memcpy_highres(idx,idx);
}

static
INLINE
void
sport_copy_page_color(void)
{
  sport_memcpy(SPORT.destination,SPORT.source);

  if(!HIRESMODE)
    return;

  sport_memcpy_highres(SPORT.destination,SPORT.source);
}

static
INLINE
void
sport_copy_page_color_with_mask(const uint32_t mask_)
{
  int i;
  uint32_t const * const svram = (uint32_t const * const)&VRAM[SPORT.source];
  uint32_t * const       dvram = (uint32_t * const)&VRAM[SPORT.destination];

  for(i = 0; i < SPORT_ELEM_COUNT; i++)
    dvram[i] = (((dvram[i] ^ svram[i]) & mask_) ^ svram[i]);

  if(!HIRESMODE)
    return;

  sport_memcpy_highres(SPORT.destination,SPORT.destination);
}

static
INLINE
void
sport_copy_page(const uint32_t rawidx_,
                const uint32_t mask_)
{
  SPORT.destination = SPORT_PAGE_TO_BYTE_OFFSET(rawidx_);
  if(mask_ == 0xFFFFFFFF)
    sport_copy_page_color();
  else
    sport_copy_page_color_with_mask(mask_);
}

void
opera_sport_write_access(const uint32_t idx_,
                          const uint32_t mask_)
{
  switch(idx_ & 0x0000E000)
    {
    case 0x00000000:
      sport_copy_page(idx_,mask_);
      break;
    case 0x00002000:
      SPORT.color = mask_;
      break;
    case 0x00004000:
      sport_flash_write(idx_,mask_);
      break;
    default:
      /* TODO: What happens with other values? */
      sport_copy_page(idx_,mask_);
      break;
    }
}

uint32_t
opera_sport_state_size(void)
{
  return opera_state_save_size(sizeof(sport_t));
}

uint32_t
opera_sport_state_save(void *buf_)
{
  return opera_state_save(buf_,"SPRT",&SPORT,sizeof(sport_t));
}

uint32_t
opera_sport_state_load(const void *buf_)
{
  return opera_state_load(&SPORT,"SPRT",buf_,sizeof(sport_t));
}
