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

#ifndef LIBRETRO_DSP_H_INCLUDED
#define LIBRETRO_DSP_H_INCLUDED

#include <stdint.h>
#include <boolean.h>

#include "extern_c.h"

EXTERN_C_BEGIN

uint32_t freedo_dsp_loop(void);

uint16_t freedo_dsp_imem_read(uint16_t addr_);
void     freedo_dsp_imem_write(uint16_t addr_, uint16_t val_);

void     freedo_dsp_mem_write(uint16_t addr_, uint16_t val_);

void     freedo_dsp_set_running(bool val_);

void     freedo_dsp_arm_semaphore_write(uint32_t val_);
uint32_t freedo_dsp_arm_semaphore_read(void);

void     freedo_dsp_init(void);
void     freedo_dsp_reset(void);

uint32_t freedo_dsp_state_size(void);
void     freedo_dsp_state_save(void *buf_);
void     freedo_dsp_state_load(const void *buf_);

EXTERN_C_END

#endif /* LIBRETRO_DSP_H_INCLUDED */
