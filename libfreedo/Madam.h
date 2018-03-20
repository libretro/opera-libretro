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

// Madam.h: interface for the CMadam class.
//
//////////////////////////////////////////////////////////////////////

#ifndef	MADAM_3DO_HEADER
#define MADAM_3DO_HEADER

#define FSM_IDLE 1
#define FSM_INPROCESS 2
#define FSM_SUSPENDED 3

#ifdef __cplusplus
extern "C" {
#endif

uint32_t Get_madam_FSM(void);
void Set_madam_FSM(uint32_t val);


void _madam_SetMapping(uint32_t flag);
void _madam_Reset(void);
uint32_t _madam_GetCELCycles(void);
uint32_t * _madam_GetRegs(void);
int _madam_HandleCEL(void);
void _madam_Init(uint8_t* memory);
uint8_t *_madam_PBUSData(void);
void _madam_Poke(uint32_t addr, uint32_t val);
uint32_t _madam_Peek(uint32_t addr);

uint32_t _madam_SaveSize(void);
void _madam_Save(void *buff);
void _madam_Load(void *buff);

#ifdef __cplusplus
}
#endif

#endif 
