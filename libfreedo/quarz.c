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

#include "freedocore.h"

#include "clio.h"
#include "quarz.h"
#include "vdlp.h"

#include <string.h>

//#define NTSC_CLOCK      12270000
//#define PAL_CLOCK       14750000

#define DEFAULT_CPU_FREQUENCY 12500000
#define SND_CLOCK             44100

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

static quarz_datum_t QUARZ         = {0};
static uint32_t      CPU_FREQUENCY = DEFAULT_CPU_FREQUENCY;

void
freedo_quarz_cpu_set_freq(const uint32_t freq_)
{
  CPU_FREQUENCY = freq_;
}

void
freedo_quarz_cpu_set_freq_mul(const float mul_)
{
  CPU_FREQUENCY = (uint32_t)(DEFAULT_CPU_FREQUENCY * mul_);
}

uint32_t
freedo_quarz_cpu_get_freq(void)
{
  return CPU_FREQUENCY;
}

uint32_t
freedo_quarz_cpu_get_default_freq(void)
{
  return DEFAULT_CPU_FREQUENCY;
}

uint32_t
freedo_quarz_state_size(void)
{
  return sizeof(quarz_datum_t);
}

void
freedo_quarz_state_save(void *buf_)
{
  memcpy(buf_,&QUARZ,sizeof(quarz_datum_t));
}

void
freedo_quarz_state_load(const void *buf_)
{
  memcpy(&QUARZ,buf_,sizeof(quarz_datum_t));
}

void
freedo_quarz_init(void)
{
  QUARZ.qrz_AccVDL  = 0;
  QUARZ.qrz_AccDSP  = 0;
  QUARZ.qrz_AccARM  = 0;
  QUARZ.VDL_FS      = 526;
  QUARZ.VDL_CLOCK   = QUARZ.VDL_FS * 30;
  QUARZ.VDL_HS      = QUARZ.VDL_FS / 2;
  QUARZ.qrz_TCount  = 0;
  QUARZ.qrz_vdlline = 0;
}

int
freedo_quarz_vd_current_line(void)
{
  return (QUARZ.qrz_vdlline % QUARZ.VDL_HS);
  /* return (QUARZ.qrz_vdlline % QUARZ.VDL_HS + (VDL_HS / 2)); */
}

int
freedo_quarz_vd_half_frame(void)
{
  return (QUARZ.qrz_vdlline / QUARZ.VDL_HS);
}

int
freedo_quarz_vd_current_overline(void)
{
  return QUARZ.qrz_vdlline;
}

bool
freedo_quarz_queue_vdl(void)
{
  if(QUARZ.qrz_AccVDL >= 0x01000000)
    {
      QUARZ.qrz_AccVDL -= 0x01000000;
      QUARZ.qrz_vdlline = ((QUARZ.qrz_vdlline + 1) % QUARZ.VDL_FS);

      return true;
    }

  return false;
}

bool
freedo_quarz_queue_dsp(void)
{
  if(QUARZ.qrz_AccDSP >= 0x01000000)
    {
      /* if(HightResMode!=0) */
      /*   QUARZ.qrz_AccDSP -= (0x01000000 / 1.3); */
      /* else */
        QUARZ.qrz_AccDSP -= 0x01000000;

      return true;
    }

  return false;
}

bool
freedo_quarz_queue_timer(void)
{
  /* uint32_t cnt=_clio_GetTimerDelay(); */

  if(QUARZ.qrz_TCount >= 0x01000000) /* >= cnt */
    {
      QUARZ.qrz_TCount -= 0x01000000; /* -= cnt */

      return true;
    }

  return false;
}

void
freedo_quarz_push_cycles(const uint32_t clks_)
{
  int      timers = 21000000;   /* default */
  uint32_t arm    = ((clks_ << 24) / CPU_FREQUENCY);

  QUARZ.qrz_AccARM += (arm * CPU_FREQUENCY);
  if((QUARZ.qrz_AccARM >> 24) != clks_)
    {
      arm++;
      QUARZ.qrz_AccARM += CPU_FREQUENCY;
      QUARZ.qrz_AccARM &= 0x00FFFFFF;
    }

  QUARZ.qrz_AccDSP += (arm * SND_CLOCK);
  QUARZ.qrz_AccVDL += (arm * QUARZ.VDL_CLOCK);

  if(freedo_clio_timer_get_delay())
    QUARZ.qrz_TCount += (arm * (timers / freedo_clio_timer_get_delay()));
}
