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

// DiagPort.h: interface for the DiagPort class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _DIAG_PORT_HEADER_DEFINITION_
#define _DIAG_PORT_HEADER_DEFINITION_

#include <stdint.h>

#include "extern_c.h"

EXTERN_C_BEGIN

void _diag_Init(int testcode);

unsigned int  _diag_Get(void);
void _diag_Send(unsigned int val);

EXTERN_C_END

#endif
