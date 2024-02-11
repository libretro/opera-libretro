#ifndef LIBOPERA_CLOCK_H_INCLUDED
#define LIBOPERA_CLOCK_H_INCLUDED

#include "extern_c.h"

#include <stdint.h>

EXTERN_C_BEGIN

int      opera_clock_vdl_queued(void);
int      opera_clock_dsp_queued(void);
int      opera_clock_timer_queued(void);

void     opera_clock_push_cycles(const uint32_t clks);

void     opera_clock_cpu_set_freq(const uint32_t freq);
void     opera_clock_cpu_set_freq_mul(const float mul);
uint32_t opera_clock_cpu_get_freq(void);
uint32_t opera_clock_cpu_get_default_freq(void);
uint64_t opera_clock_cpu_cycles_per_field(void);

void     opera_clock_region_set_ntsc(void);
void     opera_clock_region_set_pal(void);

void     opera_clock_timer_set_delay(const uint32_t td);

EXTERN_C_END

#endif /* LIBOPERA_CLOCK_H_INCLUDED */
