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

#ifndef QUARZ_3DO_HEADER_DEFINTION
#define QUARZ_3DO_HEADER_DEFINTION

#include <stdint.h>

#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

void  _qrz_Init(void);

int  _qrz_VDCurrLine(void);
int  _qrz_VDHalfFrame(void);
int  _qrz_VDCurrOverline(void);

bool  _qrz_QueueVDL(void);
bool  _qrz_QueueDSP(void);
bool  _qrz_QueueTimer(void);

void  _qrz_PushARMCycles(unsigned int clks);

unsigned int _qrz_SaveSize(void);
void _qrz_Save(void *buff);
void _qrz_Load(void *buff);

#ifdef __cplusplus
}
#endif

#endif
 
