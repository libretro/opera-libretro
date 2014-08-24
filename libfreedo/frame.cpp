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
   unsigned char* destPtr = (unsigned char*)destinationBitmap;
   VDLFrame* framePtr     = (VDLFrame*)sourceFrame;

   for (int i = 0; i < copyHeight; i++)
   {
      VDLLine* linePtr = (VDLLine*)&framePtr->lines[i];
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

         destPtr++;
         srcPtr++;
      }
   }

   *resultingWidth = copyWidth;
   *resultingHeight = copyHeight;
}
