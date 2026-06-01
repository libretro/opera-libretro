#include "opera_clio.h"
#include "opera_clock.h"
#include "opera_core.h"
#include "opera_timing.h"
#include "opera_vdlp.h"

#define DEFAULT_TIMER_SLACK  64UL
#define DEFAULT_CPU_FREQ     12500000UL
#define MIN_CPU_FREQ         1000000UL
#define SND_FREQ             44100UL

/* CLIO timers scan 16 slots at 4 M25M ticks per slot, then wait SLACK ticks. */
#define M25M_FREQ            25000000ULL
#define TIMER_SLOT_TICKS     4ULL
#define TIMER_SLOT_COUNT     16UL

#define CYCLES_PER_SND(FQ,FS) ((((uint64_t)FQ) << 16) / ((uint64_t)FS))
#define CYCLES_PER_SCANLINE(FQ,FR,FS) ((((uint64_t)FQ)<<32)/(((uint64_t)FR)*((uint64_t)FS)))
#define CYCLES_PER_M25M(FQ,TICKS) \
  (((uint64_t)FQ * ((uint64_t)TICKS) << 16) / M25M_FREQ)
#define CYCLES_PER_TIMER_SLOT(FQ) CYCLES_PER_M25M(FQ,TIMER_SLOT_TICKS)
#define CYCLES_PER_TIMER_GAP(FQ,SLACK) \
  CYCLES_PER_M25M(FQ,(TIMER_SLOT_TICKS + ((uint64_t)SLACK)))

#define DEFAULT_CPSL CYCLES_PER_SCANLINE(DEFAULT_CPU_FREQ, \
                                         OPERA_NTSC_FIELD_RATE_1616,  \
                                         OPERA_NTSC_FIELD_SIZE)

typedef struct opera_clock_s opera_clock_t;
struct opera_clock_s
{
  uint32_t cpu_freq;
  uint64_t cpu_cycles;
  int32_t  dsp_acc;
  int32_t  vdl_acc;
  int32_t  timer_acc;
  uint32_t timer_slack;
  uint32_t timer_slot;
  uint32_t timer_in_gap;
  uint32_t field_size;
  uint32_t field_rate;
  int32_t  cycles_per_snd;
  int32_t  cycles_per_scanline;
  int32_t  cycles_per_timer_slot;
  int32_t  cycles_per_timer_gap;
  int32_t  cycles_until_timer;
};

static opera_clock_t g_CLOCK =
  {
    /*.cpu_freq            =*/ DEFAULT_CPU_FREQ,
    /*.cpu_cycles          =*/ 0,
    /*.dsp_acc             =*/ 0,
    /*.vdl_acc             =*/ 0,
    /*.timer_acc           =*/ 0,
    /*.timer_slack         =*/ DEFAULT_TIMER_SLACK,
    /*.timer_slot          =*/ 0,
    /*.timer_in_gap        =*/ 0,
    /*.field_size          =*/ OPERA_NTSC_FIELD_SIZE,
    /*.field_rate          =*/ OPERA_NTSC_FIELD_RATE_1616,
    /*.cycles_per_snd      =*/ CYCLES_PER_SND(DEFAULT_CPU_FREQ,SND_FREQ),
    /*.cycles_per_scanline =*/ DEFAULT_CPSL,
    /*.cycles_per_timer_slot=*/ CYCLES_PER_TIMER_SLOT(DEFAULT_CPU_FREQ),
    /*.cycles_per_timer_gap =*/ CYCLES_PER_TIMER_GAP(DEFAULT_CPU_FREQ,DEFAULT_TIMER_SLACK),
    /*.cycles_until_timer  =*/ CYCLES_PER_TIMER_SLOT(DEFAULT_CPU_FREQ)
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

static
uint32_t
calc_cycles_per_timer_slot(void)
{
  uint64_t rv;

  rv = CYCLES_PER_TIMER_SLOT(g_CLOCK.cpu_freq);

  return rv;
}

static
uint32_t
calc_cycles_per_timer_gap(void)
{
  uint64_t rv;

  rv = CYCLES_PER_TIMER_GAP(g_CLOCK.cpu_freq,
                            g_CLOCK.timer_slack);

  return rv;
}

static
void
recalculate_cycles_per(void)
{
  g_CLOCK.cycles_per_snd        = calc_cycles_per_snd();
  g_CLOCK.cycles_per_scanline   = calc_cycles_per_scanline();
  g_CLOCK.cycles_per_timer_slot = calc_cycles_per_timer_slot();
  g_CLOCK.cycles_per_timer_gap  = calc_cycles_per_timer_gap();
  g_CLOCK.cycles_until_timer    = (g_CLOCK.timer_in_gap ?
                                   g_CLOCK.cycles_per_timer_gap :
                                   g_CLOCK.cycles_per_timer_slot);
}

void
opera_clock_reset(void)
{
  g_CLOCK.cpu_cycles = 0;
  g_CLOCK.dsp_acc    = 0;
  g_CLOCK.vdl_acc    = 0;
  g_CLOCK.timer_acc  = 0;
  g_CLOCK.timer_slack = DEFAULT_TIMER_SLACK;
  g_CLOCK.timer_slot = 0;
  g_CLOCK.timer_in_gap = 0;
  recalculate_cycles_per();
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

double
opera_clock_field_rate(void)
{
  return ((double)g_CLOCK.field_rate / OPERA_FIELD_RATE_ONE_1616);
}

uint64_t
opera_clock_cpu_get_cycles(void)
{
  return g_CLOCK.cpu_cycles;
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
opera_clock_timer_queued(uint32_t *timer_)
{
  if(g_CLOCK.timer_acc >= g_CLOCK.cycles_until_timer)
    {
      g_CLOCK.timer_acc -= g_CLOCK.cycles_until_timer;
      *timer_ = g_CLOCK.timer_slot;

      g_CLOCK.timer_slot++;
      if(g_CLOCK.timer_slot >= TIMER_SLOT_COUNT)
        {
          g_CLOCK.timer_slot         = 0;
          g_CLOCK.timer_in_gap       = 1;
          g_CLOCK.cycles_until_timer = g_CLOCK.cycles_per_timer_gap;
        }
      else
        {
          g_CLOCK.timer_in_gap       = 0;
          g_CLOCK.cycles_until_timer = g_CLOCK.cycles_per_timer_slot;
        }

      return 1;
    }

  return 0;
}

void
opera_clock_push_cycles(const uint32_t clks_)
{
  uint32_t clks1616;

  g_CLOCK.cpu_cycles += clks_;

  clks1616 = (clks_ << 16);
  g_CLOCK.dsp_acc   += clks1616;
  g_CLOCK.vdl_acc   += clks1616;
  g_CLOCK.timer_acc += clks1616;
}

void
opera_clock_region_set_ntsc(void)
{
  g_CLOCK.field_rate = OPERA_NTSC_FIELD_RATE_1616;
  g_CLOCK.field_size = OPERA_NTSC_FIELD_SIZE;

  recalculate_cycles_per();
}

void
opera_clock_region_set_pal(void)
{
  g_CLOCK.field_rate = OPERA_PAL_FIELD_RATE_1616;
  g_CLOCK.field_size = OPERA_PAL_FIELD_SIZE;

  recalculate_cycles_per();
}

void
opera_clock_timer_set_delay(const uint32_t td_)
{
  g_CLOCK.timer_slack = td_;

  recalculate_cycles_per();
}
