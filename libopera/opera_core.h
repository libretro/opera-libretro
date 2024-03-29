/*
  www.freedo.org
  The first and only working 3DO multiplayer emulator.

  The FreeDO licensed under modified GNU LGPL, with following notes:

  *   The owners and original authors of the FreeDO have full right to develop closed source derivative work.
  *   Any non-commercial uses of the FreeDO sources or any knowledge obtained by studying or reverse engineering
  of the sources, or any other material published by FreeDO have to be accompanied with full credits.
  *   Any commercial uses of FreeDO sources or any knowledge obtained by studying or reverse engineering of the sources,
  or any other material published by FreeDO is strictly forbidden without owners approval.

  The above notes are taking precedence over GNU LGPL in conflicting situations.

  Project authors:

  Alexander Troosh
  Maxim Grishin
  Allen Wright
  John Sammons
  Felix Lazarev
*/

#ifndef LIBOPERA_CORE_H_INCLUDED
#define LIBOPERA_CORE_H_INCLUDED

#include "extern_c.h"

#include <stdint.h>

#define EXT_DSP_TRIGGER   2  /* DSP should be triggered */

typedef void* (*opera_ext_interface_t)(int, void*);

EXTERN_C_BEGIN

extern uint32_t FIXMODE;
extern int      CNBFIX;

EXTERN_C_END

#endif
