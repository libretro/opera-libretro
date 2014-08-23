// Frame.h - For frame extraction and filtering options.

#ifndef	FRAME_3DO_HEADER
#define FRAME_3DO_HEADER

void _frame_Init();

void Get_Frame_Bitmap(
	VDLFrame* sourceFrame,
	void* destinationBitmap,
	int destinationBitmapWidth,
	BitmapCrop* bitmapCrop,
	int copyWidth,
	int copyHeight,
	bool addBlackBorder,
	bool copyPointlessAlphaByte,
	bool allowCrop,
	ScalingAlgorithm scalingAlgorithm,
	int* resultingWidth,
	int* resultingHeight);

#endif 
