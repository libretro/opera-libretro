/*
  www.freedo.org
  The first working 3DO multiplayer emulator.

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

#ifndef LIBOPERA_DSP_H_INCLUDED
#define LIBOPERA_DSP_H_INCLUDED

#include "extern_c.h"

#include <stdint.h>

EXTERN_C_BEGIN

uint32_t opera_dsp_loop(void);

uint16_t opera_dsp_imem_read(uint16_t addr_);
void     opera_dsp_imem_write(uint16_t addr_, uint16_t val_);

void     opera_dsp_mem_write(uint16_t addr_, uint16_t val_);

void     opera_dsp_set_running(int val_);

void     opera_dsp_arm_semaphore_write(uint32_t val_);
uint32_t opera_dsp_arm_semaphore_read(void);

void     opera_dsp_init(void);
void     opera_dsp_reset(void);

uint32_t opera_dsp_state_size(void);
void     opera_dsp_state_save(void *buf_);
void     opera_dsp_state_load(const void *buf_);

EXTERN_C_END

#endif /* LIBOPERA_DSP_H_INCLUDED */
