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

#include "freedo_clio.h"
#include "freedo_clock.h"
#include "freedo_core.h"
#include "freedo_vdlp.h"

#include <string.h>

//#define NTSC_CLOCK      12270000
//#define PAL_CLOCK       14750000

#define DEFAULT_CPU_FREQUENCY 12500000
#define SND_CLOCK             44100

typedef struct clock_s clock_t;
struct clock_s
{
  uint32_t dsp_acc;
  uint32_t vdl_acc;
  uint32_t timer_acc;
  uint32_t fps;
  uint32_t vdlline;
  uint32_t frame_size;
};

static clock_t  g_CLOCK         = {0};
static uint32_t g_CPU_FREQUENCY = DEFAULT_CPU_FREQUENCY;

void
freedo_clock_cpu_set_freq(const uint32_t freq_)
{
  g_CPU_FREQUENCY = freq_;
}

void
freedo_clock_cpu_set_freq_mul(const float mul_)
{
  g_CPU_FREQUENCY = (uint32_t)(DEFAULT_CPU_FREQUENCY * mul_);
}

uint32_t
freedo_clock_cpu_get_freq(void)
{
  return g_CPU_FREQUENCY;
}

uint32_t
freedo_clock_cpu_get_default_freq(void)
{
  return DEFAULT_CPU_FREQUENCY;
}

/* for backwards compatibility */
uint32_t
freedo_clock_state_size(void)
{
  return (sizeof(uint32_t) * 8);
}

void
freedo_clock_state_save(void *buf_)
{
  //memcpy(buf_,&g_CLOCK,sizeof(clock_t));
}

void
freedo_clock_state_load(const void *buf_)
{
  //memcpy(&g_CLOCK,buf_,sizeof(clock_t));
}

void
freedo_clock_init(void)
{
  g_CLOCK.dsp_acc    = 0;
  g_CLOCK.vdl_acc    = 0;
  g_CLOCK.timer_acc  = 0;
  g_CLOCK.fps        = 30;
  g_CLOCK.vdlline    = 0;
  g_CLOCK.frame_size = 526;
}

int
freedo_clock_vdl_current_line(void)
{
  return (g_CLOCK.vdlline % (g_CLOCK.frame_size >> 1));
}

int
freedo_clock_vdl_half_frame(void)
{
  return (g_CLOCK.vdlline / (g_CLOCK.frame_size >> 1));
}

int
freedo_clock_vdl_current_overline(void)
{
  return g_CLOCK.vdlline;
}

bool
freedo_clock_vdl_queued(void)
{
  const uint32_t limit = (g_CPU_FREQUENCY / (g_CLOCK.frame_size * g_CLOCK.fps));

  if(g_CLOCK.vdl_acc >= limit)
    {
      g_CLOCK.vdl_acc -= limit;
      g_CLOCK.vdlline = ((g_CLOCK.vdlline + 1) % g_CLOCK.frame_size);

      return true;
    }

  return false;
}

bool
freedo_clock_dsp_queued(void)
{
  const uint32_t limit = (g_CPU_FREQUENCY / SND_CLOCK);

  if(g_CLOCK.dsp_acc >= limit)
    {
      g_CLOCK.dsp_acc -= limit;

      return true;
    }

  return false;
}

/* Need to find out where the value 21000000 comes from. */
bool
freedo_clock_timer_queued(void)
{
  uint32_t limit;
  uint32_t timer_delay;

  timer_delay = freedo_clio_timer_get_delay();
  if(timer_delay == 0)
    return false;

  limit = (g_CPU_FREQUENCY / (21000000 / timer_delay));
  if(g_CLOCK.timer_acc >= limit)
    {
      g_CLOCK.timer_acc -= limit;

      return true;
    }

  return false;
}

void
freedo_clock_push_cycles(const uint32_t clks_)
{
  g_CLOCK.dsp_acc   += clks_;
  g_CLOCK.vdl_acc   += clks_;
  g_CLOCK.timer_acc += clks_;
}
