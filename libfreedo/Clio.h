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

// Clio.h: interface for the CClio class.
//
//////////////////////////////////////////////////////////////////////

#ifndef	CLIO_3DO_HEADER
#define CLIO_3DO_HEADER

#include <stdint.h>

#include <boolean.h>

#include "extern_c.h"

EXTERN_C_BEGIN

int _clio_v0line(void);
int _clio_v1line(void);
bool _clio_NeedFIQ(void);

uint32_t _clio_FIFOStruct(uint32_t addr);
void _clio_Reset(void);
void _clio_SetFIFO(uint32_t adr, uint32_t val);
uint16_t  _clio_GetEOFIFOStat(uint8_t channel);
uint16_t  _clio_GetEIFIFOStat(uint8_t channel);
uint16_t  _clio_EIFIFONI(uint16_t channel);
void  _clio_EOFIFO(uint16_t channel, uint16_t val);
uint16_t  _clio_EIFIFO(uint16_t channel);

void _clio_Init(int ResetReson);

void _clio_DoTimers(void);
uint32_t _clio_Peek(uint32_t addr);
int _clio_Poke(uint32_t addr, uint32_t val);
void _clio_UpdateVCNT(int line, int halfframe);
void _clio_GenerateFiq(uint32_t reason1, uint32_t reason2);

uint32_t _clio_GetTimerDelay(void);

uint32_t _clio_SaveSize(void);
void _clio_Save(void *buff);
void _clio_Load(void *buff);

EXTERN_C_END

#endif
