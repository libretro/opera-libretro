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

#ifndef DSP_3DO_HEADER
#define DSP_3DO_HEADER

#include <stdint.h>
#include <boolean.h>

#include "extern_c.h"

EXTERN_C_BEGIN

uint32_t _dsp_Loop(void);

uint16_t  _dsp_ReadIMem(uint16_t addr);
void  _dsp_WriteIMem(uint16_t addr, uint16_t val);
void  _dsp_WriteMemory(uint16_t addr, uint16_t val);
void  _dsp_SetRunning(bool val);
void  _dsp_ARMwrite2sema4(unsigned int val);
unsigned int _dsp_ARMread2sema4(void);

void _dsp_Init(void);
void _dsp_Reset(void);

unsigned int _dsp_SaveSize(void);
void _dsp_Save(void *buff);
void _dsp_Load(void *buff);

EXTERN_C_END

#endif
