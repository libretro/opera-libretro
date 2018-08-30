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

#ifndef __3DO_SYSTEM_HEADER_DEFINITION
#define __3DO_SYSTEM_HEADER_DEFINITION

#include "extern_c.h"

#include <stdint.h>
#include <boolean.h>

#define EXT_SWAPFRAME     1  /* frame should be read */
#define EXT_DSP_TRIGGER   2  /* DSP should be triggered
                                (freedo_dsp_loop) */

typedef void* (*freedo_ext_interface_t)(int, void*);

EXTERN_C_BEGIN

extern int HIRESMODE;
extern int CNBFIX;
extern int FIXMODE;

EXTERN_C_END

#endif
