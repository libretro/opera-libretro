// Frame.h - For frame extraction and filtering options.

#ifndef	FRAME_3DO_HEADER
#define FRAME_3DO_HEADER

#include "extern_c.h"

EXTERN_C_BEGIN

void _frame_Init(void);

void Get_Frame_Bitmap(
	struct VDLFrame* sourceFrame,
	void* destinationBitmap,
	int copyWidth,
	int copyHeight);

EXTERN_C_END

#endif
