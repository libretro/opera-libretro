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

// CPU.h: interface for the CCPU class.
//
//////////////////////////////////////////////////////////////////////

#ifndef ARM_3DO_HEADER
#define ARM_3DO_HEADER

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int _arm_Execute(void);
void _arm_Reset(void);
void _arm_Destroy(void);
uint8_t *_arm_Init(void);

//for mas
void _mem_write8(unsigned int addr, uint8_t val);
void  _mem_write16(unsigned int addr, uint16_t val);
void _mem_write32(unsigned int addr, unsigned int val);
uint8_t  _mem_read8(unsigned int addr);
uint16_t _mem_read16(unsigned int addr);
unsigned int  _mem_read32(unsigned int addr);

void WriteIO(unsigned int addr, unsigned int val);
unsigned int ReadIO(unsigned int addr);
void SelectROM(int n);

unsigned int _arm_SaveSize(void);
void _arm_Save(void *buff);
void _arm_Load(void *buff);

#ifdef __cplusplus
}
#endif

#endif 
