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

#include <stdint.h>
#include <boolean.h>

#include "extern_c.h"

#pragma pack(push,1)

struct BitmapCrop
{
	int left;
	int top;
	int bottom;
	int right;
};

struct GetFrameBitmapParams
{
	struct VDLFrame* sourceFrame;
	void* destinationBitmap;
	int destinationBitmapWidthPixels;
	struct BitmapCrop* bitmapCrop;
	int copyWidthPixels;
	int copyHeightPixels;
	bool addBlackBorder;
	bool copyPointlessAlphaByte;
	bool allowCrop;
	int resultingWidth;
	int resultingHeight;
};

#pragma pack(pop)

#define EXT_READ_ROMS           1
#define EXT_SWAPFRAME           5       //frame swap (in mutlithreaded) or frame draw(single treaded)
#define EXT_PUSH_SAMPLE         6       //sends sample to the buffer
#define EXT_KPRINT              9
#define EXT_DEBUG_PRINT         10
#define EXT_FRAMETRIGGER_MT     12      //multitasking
#define EXT_READ2048            14      //for XBUS Plugin
#define EXT_GET_DISC_SIZE       15
#define EXT_ON_SECTOR           16
#define EXT_ARM_SYNC            17

typedef void* (*freedo_ext_interface_t)(int, void*);

#define FDP_FREEDOCORE_VERSION   0
#define FDP_DO_FRAME_MT          4      //multitasking

#define BIOS_ANVIL (0x40)

EXTERN_C_BEGIN

extern int HIRESMODE;
extern int CNBFIX;
extern int FIXMODE;

EXTERN_C_END

#endif
