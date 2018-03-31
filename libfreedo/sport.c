/*
  www.freedo.org
  The first and only working 3DO multiplayer emulator.

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

#include <stdint.h>
#include <string.h>

#include "sport.h"

struct sport_s
{
  uint32_t color;
  uint32_t source;
  uint32_t destination;
};

typedef struct sport_s sport_t;

extern int HightResMode;

static sport_t  SPORT = {0};
static uint8_t *VRAM;

uint32_t freedo_sport_state_size(void)
{
  return sizeof(sport_t);
}

void freedo_sport_state_save(void *buf_)
{
  memcpy(buf_,&SPORT,sizeof(sport_t));
}

void freedo_sport_state_load(const void *buf_)
{
  memcpy(&SPORT,buf_,sizeof(sport_t));
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void freedo_sport_init(uint8_t * const vram_)
{
  VRAM=vram_;
}

int freedo_sport_set_source(const uint32_t idx_) //take source for SPORT
{
  SPORT.source=(idx_<<7);
  return 0;
}

void freedo_sport_write_access(const uint32_t idx_, const uint32_t mask_)
{
  int i;
  uint32_t tmp,ctmp;
  uint32_t idx = idx_;

  if((idx & ~0x1FFF)==0x4000) //SPORT flash write
    {
      idx&=0x7ff;
      idx<<=7;
      if(mask_ == 0xFFFFffff)
        {
          for(i = 0; i < 512; i++)
            ((uint32_t*)VRAM)[idx+i]=SPORT.color;
        }
      else  // mask_ is not 0xFFFFffff
        {
          for(i=0;i<512;i++)
            {
              tmp=((uint32_t*)VRAM)[idx+i];
              tmp=((tmp^SPORT.color)&mask_)^SPORT.color;
              ((uint32_t*)VRAM)[idx+i]=tmp;
            }
        }
      if(!HightResMode)
        return;
      memcpy(&((uint32_t*)VRAM)[idx+1024*256], &((uint32_t*)VRAM)[idx], 2048);
      memcpy(&((uint32_t*)VRAM)[idx+2*1024*256], &((uint32_t*)VRAM)[idx], 2048);
      memcpy(&((uint32_t*)VRAM)[idx+3*1024*256], &((uint32_t*)VRAM)[idx], 2048);
      return;
    }


  if(!(idx & ~0x1FFF)) //SPORT copy page
    {
      SPORT.destination=(idx &0x7ff)<<7;
      if(mask_ == 0xFFFFffff)
        {
          memcpy(&((uint32_t*)VRAM)[SPORT.destination],&((uint32_t*)VRAM)[SPORT.source],512*4);
          if(!HightResMode)
            return;
          memcpy(&((uint32_t*)VRAM)[SPORT.destination+1024*256], &((uint32_t*)VRAM)[SPORT.source+1024*256], 2048);
          memcpy(&((uint32_t*)VRAM)[SPORT.destination+2*1024*256], &((uint32_t*)VRAM)[SPORT.source+2*1024*256], 2048);
          memcpy(&((uint32_t*)VRAM)[SPORT.destination+3*1024*256], &((uint32_t*)VRAM)[SPORT.source+3*1024*256], 2048);
        }
      else  // mask_ != 0xFFFFffff
        {
          for(i=0;i<512;i++)
            {
              tmp=((uint32_t*)VRAM)[SPORT.destination+i];
              ctmp=((uint32_t*)VRAM)[SPORT.source+i];
              tmp=((tmp^ctmp)&mask_)^ctmp;
              ((uint32_t*)VRAM)[SPORT.destination+i]=tmp;
            }
          if(!HightResMode)
            return;
          memcpy(&((uint32_t*)VRAM)[SPORT.destination+1024*256], &((uint32_t*)VRAM)[SPORT.destination], 2048);
          memcpy(&((uint32_t*)VRAM)[SPORT.destination+2*1024*256], &((uint32_t*)VRAM)[SPORT.destination], 2048);
          memcpy(&((uint32_t*)VRAM)[SPORT.destination+3*1024*256], &((uint32_t*)VRAM)[SPORT.destination], 2048);
        }
      return;
    }

  if((idx & ~0x1FFF)==0x2000) //SPORT set color!!!
    {
      SPORT.color=mask_;
      return;
    }
}
