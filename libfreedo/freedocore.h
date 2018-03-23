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

#pragma pack(push,1)

struct VDLLine
{
	uint16_t line[320*4];
	uint8_t xCLUTB[32];
	uint8_t xCLUTG[32];
	uint8_t xCLUTR[32];
	uint32_t xOUTCONTROLL;
	uint32_t xCLUTDMA;
	uint32_t xBACKGROUND;
};

struct VDLFrame
{
	struct VDLLine lines[240*4];
	unsigned int srcw,srch;
};

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
typedef void* (*_ext_Interface)(int, void*);

#define FDP_FREEDOCORE_VERSION  0
#define FDP_INIT                1    //set ext_interface
#define FDP_DESTROY             2
#define FDP_DO_EXECFRAME        3       //execute 1/60 of second
#define FDP_DO_FRAME_MT         4      //multitasking
#define FDP_DO_EXECFRAME_MT     5      //multitasking
#define FDP_DO_LOAD             6       //load state from buffer, returns !NULL if everything went smooth
#define FDP_GET_SAVE_SIZE       7       //return size of savestatemachine
#define FDP_DO_SAVE             8       //save state to buffer
#define FDP_GETP_RAMS           10       //returns ptr to RAM 3M
#define FDP_GETP_ROMS           11       //returns ptr to ROM 2M
#define FDP_GETP_PROFILE        12       //returns profile pointer, sizeof = 3M/4
#define FDP_BUGTEMPORALFIX      13
#define FDP_SET_ARMCLOCK        14
#define FDP_SET_TEXQUALITY      15
#define FDP_GETP_WRCOUNT        16
#define FDP_SET_FIX_MODE        17
#define FDP_GET_FRAME_BITMAP    18
#define FDP_GET_BIOS_TYPE       19
#define FDP_SET_ANVIL           20

#define FIX_BIT_TIMING_1        (0x00000001)
//#define FIX_BIT_TIMING_2        (0x00000002)
#define FIX_BIT_TIMING_3        (0x00000004)
//#define FIX_BIT_TIMING_4        (0x00000008)
#define FIX_BIT_TIMING_5        (0x00000010)
#define FIX_BIT_TIMING_6        (0x00000020)
//#define FIX_BIT_TIMING_7        (0x00000040)
#define FIX_BIT_GRAPHICS_STEP_Y (0x00080000) // Preserve Y coordinate rather than X between CELs.

#define BIOS_ANVIL (0x40)

#ifdef __cplusplus
extern "C"
{
#endif

void *_freedo_Interface(int procedure, void *datum);

extern int fixmode;
extern int biosanvil;
extern int isanvil;

#ifdef __cplusplus
};
#endif

#endif
