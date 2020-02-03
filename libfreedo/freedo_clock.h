#ifndef LIBFREEDO_CLOCK_H_INCLUDED
#define LIBFREEDO_CLOCK_H_INCLUDED

#include "extern_c.h"

#include <stdint.h>

EXTERN_C_BEGIN

void     freedo_clock_init(void);

int      freedo_clock_vdl_queued(void);
int      freedo_clock_dsp_queued(void);
int      freedo_clock_timer_queued(void);

void     freedo_clock_push_cycles(const uint32_t clks);

void     freedo_clock_cpu_set_freq(const uint32_t freq);
void     freedo_clock_cpu_set_freq_mul(const float mul);
uint32_t freedo_clock_cpu_get_freq(void);
uint32_t freedo_clock_cpu_get_default_freq(void);
uint64_t freedo_clock_cpu_cycles_per_field(void);

uint32_t freedo_clock_state_size(void);
void     freedo_clock_state_save(void *buf);
void     freedo_clock_state_load(const void *buf);

void     freedo_clock_region_set_ntsc(void);
void     freedo_clock_region_set_pal(void);

void     freedo_clock_timer_set_delay(const uint32_t td);

EXTERN_C_END

#endif /* LIBFREEDO_CLOCK_H_INCLUDED */
