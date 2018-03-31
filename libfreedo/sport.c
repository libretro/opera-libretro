/*
  www.freedo.org
  The first and only working 3DO multiplayer emulator.

  The FreeDO licensed under modified GNU LGPL, with following notes:

  *   The owners and original authors of the FreeDO have full right to develop closed source derivative work.
  *   Any non-commercial uses of the FreeDO sources or any knowledge obtained by studying or reverse engineering
  of the sources, or any other material published by FreeDO have to be accompanied with full credits.
  *   Any commercial uses of FreeDO sources or any knowledge obtained by studying or reverse engineering of the sources,
  or any other material published by FreeDO is strictly forbidden without owners approval.

  The above notes are taking precedence over GNU LGPL in conflicting situations.

  Project authors:

  Alexander Troosh
  Maxim Grishin
  Allen Wright
  John Sammons
  Felix Lazarev
*/

#include <stdint.h>
#include <string.h>

#include "sport.h"

static uint32_t gSPORTCOLOR;
static uint32_t gSPORTSOURCE=0;
static uint32_t gSPORTDESTINATION=0;
static uint8_t  *VRAM;

extern int HightResMode;

uint32_t _sport_SaveSize(void)
{
  return 12;
}

void _sport_Save(void *buff)
{
  ((uint32_t*)buff)[0]=gSPORTCOLOR;
  ((uint32_t*)buff)[1]=gSPORTSOURCE;
  ((uint32_t*)buff)[2]=gSPORTDESTINATION;
}

void _sport_Load(void *buff)
{
  gSPORTCOLOR=((uint32_t*)buff)[0];
  gSPORTSOURCE=((uint32_t*)buff)[1];
  gSPORTDESTINATION=((uint32_t*)buff)[2];
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void _sport_Init(uint8_t *vmem)
{
  VRAM=vmem;
}

int _sport_SetSource(uint32_t index) //take source for SPORT
{
  gSPORTSOURCE=(index<<7);
  return 0;
}

void _sport_WriteAccess(uint32_t index, uint32_t mask)
{
  int i;
  uint32_t tmp,ctmp;

  if((index & ~0x1FFF)==0x4000) //SPORT flash write
    {
      index&=0x7ff;
      index<<=7;
      if(mask == 0xFFFFffff)
        {
          for(i = 0; i < 512; i++)
            ((uint32_t*)VRAM)[index+i]=gSPORTCOLOR;
        }
      else  // mask is not 0xFFFFffff
        {
          for(i=0;i<512;i++)
            {
              tmp=((uint32_t*)VRAM)[index+i];
              tmp=((tmp^gSPORTCOLOR)&mask)^gSPORTCOLOR;
              ((uint32_t*)VRAM)[index+i]=tmp;
            }
        }
      if(!HightResMode)
        return;
      memcpy(&((uint32_t*)VRAM)[index+1024*256], &((uint32_t*)VRAM)[index], 2048);
      memcpy(&((uint32_t*)VRAM)[index+2*1024*256], &((uint32_t*)VRAM)[index], 2048);
      memcpy(&((uint32_t*)VRAM)[index+3*1024*256], &((uint32_t*)VRAM)[index], 2048);
      return;
    }


  if(!(index & ~0x1FFF)) //SPORT copy page
    {
      gSPORTDESTINATION=(index &0x7ff)<<7;
      if(mask == 0xFFFFffff)
        {
          memcpy(&((uint32_t*)VRAM)[gSPORTDESTINATION],&((uint32_t*)VRAM)[gSPORTSOURCE],512*4);
          if(!HightResMode)
            return;
          memcpy(&((uint32_t*)VRAM)[gSPORTDESTINATION+1024*256], &((uint32_t*)VRAM)[gSPORTSOURCE+1024*256], 2048);
          memcpy(&((uint32_t*)VRAM)[gSPORTDESTINATION+2*1024*256], &((uint32_t*)VRAM)[gSPORTSOURCE+2*1024*256], 2048);
          memcpy(&((uint32_t*)VRAM)[gSPORTDESTINATION+3*1024*256], &((uint32_t*)VRAM)[gSPORTSOURCE+3*1024*256], 2048);
        }
      else  // mask != 0xFFFFffff
        {
          for(i=0;i<512;i++)
            {
              tmp=((uint32_t*)VRAM)[gSPORTDESTINATION+i];
              ctmp=((uint32_t*)VRAM)[gSPORTSOURCE+i];
              tmp=((tmp^ctmp)&mask)^ctmp;
              ((uint32_t*)VRAM)[gSPORTDESTINATION+i]=tmp;
            }
          if(!HightResMode)
            return;
          memcpy(&((uint32_t*)VRAM)[gSPORTDESTINATION+1024*256], &((uint32_t*)VRAM)[gSPORTDESTINATION], 2048);
          memcpy(&((uint32_t*)VRAM)[gSPORTDESTINATION+2*1024*256], &((uint32_t*)VRAM)[gSPORTDESTINATION], 2048);
          memcpy(&((uint32_t*)VRAM)[gSPORTDESTINATION+3*1024*256], &((uint32_t*)VRAM)[gSPORTDESTINATION], 2048);
        }
      return;
    }

  if((index & ~0x1FFF)==0x2000) //SPORT set color!!!
    {
      gSPORTCOLOR=mask;
      return;
    }
}
