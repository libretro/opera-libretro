#include "freedoconfig.h"
#include "freedocore.h"
#include "frame.h"

//#include "hqx.h"

unsigned char FIXED_CLUTR[32];
unsigned char FIXED_CLUTG[32];
unsigned char FIXED_CLUTB[32];

void _frame_Init(void)
{
   int j;
	for(j = 0; j < 32; j++)
	{
		FIXED_CLUTR[j] = (unsigned char)(((j & 0x1f) << 3) | ((j >> 2) & 7));
		FIXED_CLUTG[j] = FIXED_CLUTR[j];
		FIXED_CLUTB[j] = FIXED_CLUTR[j];
	}
}

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
	int* resultingHeight)
{
	float maxCropPercent = allowCrop ? .25f : 0;
	int maxCropTall = (int)(copyHeight * maxCropPercent);
	int maxCropWide = (int)(copyWidth * maxCropPercent);

	bitmapCrop->top = maxCropTall;
	bitmapCrop->left = maxCropWide;
	bitmapCrop->right = maxCropWide;
	bitmapCrop->bottom = maxCropTall;

	int pointlessAlphaByte = copyPointlessAlphaByte ? 1 : 0;

	// Destination will be directly changed if there is no scaling algorithm.
	// Otherwise we extract to a temporary buffer.
	unsigned char* destPtr = (unsigned char*)destinationBitmap;

	VDLFrame* framePtr = sourceFrame;
	for (int line = 0; line < copyHeight; line++)
	{
		VDLLine* linePtr = &framePtr->lines[line];
		short* srcPtr = (short*)linePtr;
		bool allowFixedClut = (linePtr->xOUTCONTROLL & 0x2000000) > 0;
		for (int pix = 0; pix < copyWidth; pix++)
		{
			unsigned char bPart = 0;
			unsigned char gPart = 0;
			unsigned char rPart = 0;
			if (*srcPtr == 0)
			{
				bPart = (unsigned char)(linePtr->xBACKGROUND & 0x1F);
				gPart = (unsigned char)((linePtr->xBACKGROUND >> 5) & 0x1F);
				rPart = (unsigned char)((linePtr->xBACKGROUND >> 10) & 0x1F);
			}
			else if (allowFixedClut && (*srcPtr & 0x8000) > 0)
			{
				bPart = FIXED_CLUTB[(*srcPtr) & 0x1F];
				gPart = FIXED_CLUTG[((*srcPtr) >> 5) & 0x1F];
				rPart = FIXED_CLUTR[(*srcPtr) >> 10 & 0x1F];
			}
			else
			{
				bPart = (unsigned char)(linePtr->xCLUTB[(*srcPtr) & 0x1F]);
				gPart = linePtr->xCLUTG[((*srcPtr) >> 5) & 0x1F];
				rPart = linePtr->xCLUTR[(*srcPtr) >> 10 & 0x1F];
			}
			*destPtr++ = bPart;
			*destPtr++ = gPart;
			*destPtr++ = rPart;

			destPtr += pointlessAlphaByte;
			srcPtr++;

			if (line < bitmapCrop->top)
				if (!(rPart < 0xF && gPart < 0xF && bPart < 0xF))
					bitmapCrop->top = line;

			if (pix < bitmapCrop->left )
				if (!(rPart < 0xF && gPart < 0xF && bPart < 0xF))
					bitmapCrop->left = pix;

			if (pix > copyWidth - bitmapCrop->right - 1)
				if (!(rPart < 0xF && gPart < 0xF && bPart < 0xF))
					bitmapCrop->right = copyWidth - pix - 1;

			if (line > copyHeight - bitmapCrop->bottom - 1)
				if (!(rPart < 0xF && gPart < 0xF && bPart < 0xF))
					bitmapCrop->bottom = copyHeight - line - 1;
		}
	}

	int cropAdjust = 1;

	bitmapCrop->top *= cropAdjust;
	bitmapCrop->left *= cropAdjust;
	bitmapCrop->right *= cropAdjust;
	bitmapCrop->bottom *= cropAdjust;

	*resultingWidth = copyWidth * cropAdjust;
	*resultingHeight = copyHeight * cropAdjust;
}
