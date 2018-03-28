// Frame.h - For frame extraction and filtering options.

#ifndef	FRAME_3DO_HEADER
#define FRAME_3DO_HEADER

#include "extern_c.h"

#include "vdlp.h"

EXTERN_C_BEGIN

void _frame_Init(void);

void Get_Frame_Bitmap(
	vdlp_frame_t* sourceFrame,
	void* destinationBitmap,
	int copyWidth,
	int copyHeight);

EXTERN_C_END

#endif
