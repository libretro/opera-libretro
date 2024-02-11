/*
  www.freedo.org
  The first 3DO multiplayer emulator.

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

#ifndef LIBOPERA_SPORT_H_INCLUDED
#define LIBOPERA_SPORT_H_INCLUDED

#include "extern_c.h"

#include <stdint.h>

EXTERN_C_BEGIN

void     opera_sport_init();

void     opera_sport_set_source(const uint32_t idx_);
void     opera_sport_write_access(const uint32_t idx_, const uint32_t mask_);

uint32_t opera_sport_state_size(void);
uint32_t opera_sport_state_save(void *buf_);
uint32_t opera_sport_state_load(void const *buf_);

EXTERN_C_END

#endif /* LIBOPERA_SPORT_H_INCLUDED */
