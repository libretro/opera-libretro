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

// SPORT.h: interface for the SPORT class.
//
//////////////////////////////////////////////////////////////////////

#ifndef	SPORT_3DO_HEADER
#define  SPORT_3DO_HEADER

#ifdef __cplusplus
extern "C" {
#endif

void _sport_Init(uint8_t *vmem);

int _sport_SetSource(uint32_t index);

void _sport_WriteAccess(uint32_t index, uint32_t mask);

uint32_t _sport_SaveSize(void);
void _sport_Save(void *buff);
void _sport_Load(void *buff);

#ifdef __cplusplus
}
#endif

#endif 
