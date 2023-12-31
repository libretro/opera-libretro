#include "opera_clio.h"
#include "opera_clock.h"
#include "opera_core.h"
#include "opera_vdlp.h"

#define DEFAULT_TIMER_DELAY  0x150
#define DEFAULT_CPU_FREQ     12500000UL
#define MIN_CPU_FREQ         1000000UL
#define SND_FREQ             44100UL
#define NTSC_FIELD_SIZE      263UL
#define PAL_FIELD_SIZE       312UL
#define NTSC_FIELD_RATE_1616 3928227UL
#define PAL_FIELD_RATE_1616  3276800UL

#define CYCLES_PER_SND(FQ,FS) ((((uint64_t)FQ) << 16) / ((uint64_t)FS))
#define CYCLES_PER_SCANLINE(FQ,FR,FS) ((((uint64_t)FQ)<<32)/(((uint64_t)FR)*((uint64_t)FS)))
#define CYCLES_PER_TIMER(FQ,TD) ((((uint64_t)FQ)<<32)/((((uint64_t)21000000ULL)<<16)/((uint64_t)TD)))

#define DEFAULT_TIMER_DELAY 0x150
#define DEFAULT_CPSL CYCLES_PER_SCANLINE(DEFAULT_CPU_FREQ, \
                                         NTSC_FIELD_RATE_1616,  \
                                         NTSC_FIELD_SIZE)

typedef struct opera_clock_s opera_clock_t;
struct opera_clock_s
{
  uint32_t cpu_freq;
  int32_t  dsp_acc;
  int32_t  vdl_acc;
  int32_t  timer_acc;
  uint32_t timer_delay;
  uint32_t field_size;
  uint32_t field_rate;
  int32_t  cycles_per_snd;
  int32_t  cycles_per_scanline;
  int32_t  cycles_per_timer;
};

static opera_clock_t g_CLOCK =
  {
    /*.cpu_freq            =*/ DEFAULT_CPU_FREQ,
    /*.dsp_acc             =*/ 0,
    /*.vdl_acc             =*/ 0,
    /*.timer_acc           =*/ 0,
    /*.timer_delay         =*/ DEFAULT_TIMER_DELAY,
    /*.field_size          =*/ NTSC_FIELD_SIZE,
    /*.field_rate          =*/ NTSC_FIELD_RATE_1616,
    /*.cycles_per_snd      =*/ CYCLES_PER_SND(DEFAULT_CPU_FREQ,SND_FREQ),
    /*.cycles_per_scanline =*/ DEFAULT_CPSL,
    /*.cycles_per_timer    =*/ CYCLES_PER_TIMER(DEFAULT_CPU_FREQ,DEFAULT_TIMER_DELAY)
  };

static
uint32_t
calc_cycles_per_snd(void)
{
  uint64_t rv;

  rv = CYCLES_PER_SND(g_CLOCK.cpu_freq,SND_FREQ);

  return rv;
}

/*
  For greater percision the field rate is stored as 16.16 so cpu_freq
  requires the 32 rather than 16 LSL
*/
static
uint32_t
calc_cycles_per_scanline(void)
{
  uint64_t rv;

  rv = CYCLES_PER_SCANLINE(g_CLOCK.cpu_freq,
                           g_CLOCK.field_rate,
                           g_CLOCK.field_size);

  return rv;
}

/*
  Don't know where the 21,000,000 comes from but it seems like the
  only reasonable value given the OS sets CLIO 0x220 to 0x150 and the
  system timer to 62499. (21000000 / 0x150) = 62500.
*/
static
uint32_t
calc_cycles_per_timer(void)
{
  uint64_t rv;

  rv = CYCLES_PER_TIMER(g_CLOCK.cpu_freq,
                        g_CLOCK.timer_delay);

  return rv;
}

static
void
recalculate_cycles_per(void)
{
  g_CLOCK.cycles_per_snd      = calc_cycles_per_snd();
  g_CLOCK.cycles_per_scanline = calc_cycles_per_scanline();
  g_CLOCK.cycles_per_timer    = calc_cycles_per_timer();
}

void
opera_clock_cpu_set_freq(const uint32_t freq_)
{
  uint32_t freq;

  freq = ((freq_ < MIN_CPU_FREQ) ? MIN_CPU_FREQ : freq_);

  g_CLOCK.cpu_freq = freq;

  recalculate_cycles_per();
}

void
opera_clock_cpu_set_freq_mul(const float mul_)
{
  float freq;

  freq = (DEFAULT_CPU_FREQ * mul_);

  opera_clock_cpu_set_freq((uint32_t)freq);
}

uint32_t
opera_clock_cpu_get_freq(void)
{
  return g_CLOCK.cpu_freq;
}

uint32_t
opera_clock_cpu_get_default_freq(void)
{
  return DEFAULT_CPU_FREQ;
}

int
opera_clock_vdl_queued(void)
{
  if(g_CLOCK.vdl_acc >= g_CLOCK.cycles_per_scanline)
    {
      g_CLOCK.vdl_acc -= g_CLOCK.cycles_per_scanline;
      return 1;
    }

  return 0;
}

int
opera_clock_dsp_queued(void)
{
  if(g_CLOCK.dsp_acc >= g_CLOCK.cycles_per_snd)
    {
      g_CLOCK.dsp_acc -= g_CLOCK.cycles_per_snd;
      return 1;
    }

  return 0;
}

int
opera_clock_timer_queued(void)
{
  if(g_CLOCK.timer_acc >= g_CLOCK.cycles_per_timer)
    {
      g_CLOCK.timer_acc -= g_CLOCK.cycles_per_timer;
      return 1;
    }

  return 0;
}

void
opera_clock_push_cycles(const uint32_t clks_)
{
  uint32_t clks1616;

  clks1616 = (clks_ << 16);
  g_CLOCK.dsp_acc   += clks1616;
  g_CLOCK.vdl_acc   += clks1616;
  g_CLOCK.timer_acc += clks1616;
}

void
opera_clock_region_set_ntsc(void)
{
  g_CLOCK.field_rate = NTSC_FIELD_RATE_1616;
  g_CLOCK.field_size = NTSC_FIELD_SIZE;

  recalculate_cycles_per();
}

void
opera_clock_region_set_pal(void)
{
  g_CLOCK.field_rate = PAL_FIELD_RATE_1616;
  g_CLOCK.field_size = PAL_FIELD_SIZE;

  recalculate_cycles_per();
}

void
opera_clock_timer_set_delay(const uint32_t td_)
{
  g_CLOCK.timer_delay = (td_ ? td_ : 1);

  recalculate_cycles_per();
}
