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

/* sport.h: interface for the SPORT class. */

#ifndef LIBFREEDO_SPORT_H_INCLUDED
#define LIBFREEDO_SPORT_H_INCLUDED

#include "extern_c.h"

#include <stdint.h>

EXTERN_C_BEGIN

void     freedo_sport_init(uint8_t * const vram_);

void     freedo_sport_set_source(const uint32_t idx_);
void     freedo_sport_write_access(const uint32_t idx_, const uint32_t mask_);

uint32_t freedo_sport_state_size(void);
void     freedo_sport_state_save(void *buf_);
void     freedo_sport_state_load(const void *buf_);

EXTERN_C_END

#endif /* LIBFREEDO_SPORT_H_INCLUDED */
