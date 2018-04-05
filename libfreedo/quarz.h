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

#ifndef LIBFREEDO_QUARZ_H_INCLUDED
#define LIBFREEDO_QUARZ_H_INCLUDED

#include "extern_c.h"

#include <boolean.h>

#include <stdint.h>

EXTERN_C_BEGIN

void     freedo_quarz_init(void);

int      freedo_quarz_vd_current_line(void);
int      freedo_quarz_vd_half_frame(void);
int      freedo_quarz_vd_current_overline(void);

bool     freedo_quarz_queue_vdl(void);
bool     freedo_quarz_queue_dsp(void);
bool     freedo_quarz_queue_timer(void);

void     freedo_quarz_push_cycles(const uint32_t clks_);

void     freedo_quarz_cpu_set_freq(const uint32_t freq_);
void     freedo_quarz_cpu_set_freq_mul(const float mul_);
uint32_t freedo_quarz_cpu_get_freq(void);
uint32_t freedo_quarz_cpu_get_default_freq(void);

uint32_t freedo_quarz_state_size(void);
void     freedo_quarz_state_save(void *buf_);
void     freedo_quarz_state_load(const void *buf_);

EXTERN_C_END

#endif /* LIBFREEDO_QUARZ_H_INCLUDED */
