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

int _clio_v0line(void);
int _clio_v1line(void);
bool _clio_NeedFIQ(void);

unsigned int _clio_FIFOStruct(unsigned int addr);
void _clio_Reset(void);
void _clio_SetFIFO(unsigned int adr, unsigned int val);
unsigned short  _clio_GetEOFIFOStat(unsigned char channel);
unsigned short  _clio_GetEIFIFOStat(unsigned char channel);
unsigned short  _clio_EIFIFONI(unsigned short channel);
void  _clio_EOFIFO(unsigned short channel, unsigned short val);
unsigned short  _clio_EIFIFO(unsigned short channel);

void _clio_Init(int ResetReson);

void _clio_DoTimers(void);
unsigned int _clio_Peek(unsigned int addr);
int _clio_Poke(unsigned int addr, unsigned int val);
void _clio_UpdateVCNT(int line, int halfframe);
void _clio_GenerateFiq(unsigned int reason1, unsigned int reason2);

unsigned int _clio_GetTimerDelay(void);

unsigned int _clio_SaveSize(void);
void _clio_Save(void *buff);
void _clio_Load(void *buff);

#endif 
