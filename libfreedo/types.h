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

/*****************************************************************************
Copyright (C) 2007-2008 Maxim Grishin (http://altmer.arts-union.ru)
*****************************************************************************/

#ifndef TYPES_HEAD_DEFINITION
#define TYPES_HEAD_DEFINITION

#include <stdint.h>

typedef  unsigned int uint32;
typedef  unsigned char uint8;
typedef  unsigned short uint16;
typedef  unsigned long long uint64;
typedef  unsigned long long __uint64;

typedef  int int32;
typedef  char int8;
typedef  short int16;
typedef  signed long long int64;

#ifndef WIN32
	#ifndef __int64
		typedef  signed long long __int64;
	#endif
#endif

#endif
