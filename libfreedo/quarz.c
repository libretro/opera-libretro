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

#include <string.h>

#include "quarz.h"
#include "Clio.h"
#include "vdlp.h"

#include "freedocore.h"

//#define NTSC_CLOCK      12270000        //818*500(строк)  //15  √ц
//#define PAL_CLOCK       14750000        //944*625(строк)  //15625 √ц

#define SND_CLOCK       44100

int ARM_CLOCK = ARM_FREQUENCY;
int THE_ARM_CLOCK=0;

extern _ext_Interface  io_interface;

extern int sdf;
extern int sf;
extern int unknownflag11;
extern int speedfixes;
extern int fixmode;

struct quarz_datum_s
{
  uint32_t qrz_AccARM;
  uint32_t qrz_AccDSP;
  uint32_t qrz_AccVDL;
  uint32_t qrz_TCount;
  uint32_t VDL_CLOCK;
  uint32_t qrz_vdlline;
  uint32_t VDL_HS;
  uint32_t VDL_FS;
};

typedef struct quarz_datum_s quarz_datum_t;

static quarz_datum_t QUARZ;

uint32_t freedo_quarz_state_size(void)
{
  return sizeof(quarz_datum_t);
}

void freedo_quarz_state_save(void *buf_)
{
  memcpy(buf_,&QUARZ,sizeof(quarz_datum_t));
}

void freedo_quarz_state_load(const void *buf_)
{
  memcpy(&QUARZ,buf_,sizeof(quarz_datum_t));
}

void freedo_quarz_init(void)
{
  QUARZ.qrz_AccVDL=QUARZ.qrz_AccDSP=0;
  QUARZ.qrz_AccARM=0;

  QUARZ.VDL_FS=526;
  QUARZ.VDL_CLOCK=QUARZ.VDL_FS*30;
  QUARZ.VDL_HS=QUARZ.VDL_FS/2;

  QUARZ.qrz_TCount=0;
  QUARZ.qrz_vdlline=0;
}

int freedo_quarz_vd_current_line(void)
{
  return QUARZ.qrz_vdlline%(QUARZ.VDL_HS/*+(VDL_HS/2)*/);
}

int freedo_quarz_vd_half_frame(void)
{
  return QUARZ.qrz_vdlline/(QUARZ.VDL_HS);
}

int freedo_quarz_vd_current_overline(void)
{
  return QUARZ.qrz_vdlline;
}

bool freedo_quarz_queue_vdl(void)
{
  if(QUARZ.qrz_AccVDL>>24)
    {
      QUARZ.qrz_AccVDL-=0x1000000;
      QUARZ.qrz_vdlline++;
      QUARZ.qrz_vdlline%=QUARZ.VDL_FS;
      return true;
    }
  return false;
}

bool freedo_quarz_queue_dsp(void)
{
  if(QUARZ.qrz_AccDSP>>24)
    {
      //if(HightResMode!=0) qrz_AccDSP-=0x1000000/1.3;
      //else
      QUARZ.qrz_AccDSP-=0x1000000;
      return true;
    }
  return false;
}

bool freedo_quarz_queue_timer(void)
{
  //uint32_t cnt=_clio_GetTimerDelay();
  if(QUARZ.qrz_TCount>>24)//=cnt)
    {
      QUARZ.qrz_TCount-=0x1000000;//cnt;
      return true;
    }
  return false;
}

void freedo_quarz_push_cycles(const uint32_t clks_)
{
  int timers = 21000000; /* default */
  int     sp = 0;
  uint32_t arm=(clks_<<24)/(ARM_CLOCK);
  QUARZ.qrz_AccARM+=arm*(ARM_CLOCK);
  if( (QUARZ.qrz_AccARM>>24) != clks_ )
    {
      arm++;
      QUARZ.qrz_AccARM+=ARM_CLOCK;
      QUARZ.qrz_AccARM&=0xffffff;
    }
  QUARZ.qrz_AccDSP+=arm*SND_CLOCK;
  QUARZ.qrz_AccVDL+=arm*(QUARZ.VDL_CLOCK);

  if(_clio_GetTimerDelay())
    QUARZ.qrz_TCount += arm*((timers)/(_clio_GetTimerDelay()));
}
