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

#include <string.h>

#include "quarz.h"
#include "Clio.h"
#include "vdlp.h"

//#define NTSC_CLOCK      12270000        //818*500(строк)  //15  √ц
//#define PAL_CLOCK       14750000        //944*625(строк)  //15625 √ц

#define SND_CLOCK       44100

int ARM_CLOCK = ARM_FREQUENCY;
int THE_ARM_CLOCK=0;

extern _ext_Interface  io_interface;

extern int sdf;
extern int sf;
extern int unknownflag11;
extern int speedfixes;
extern int fixmode;

#pragma pack(push,1)
struct QDatum
{
   uint32_t qrz_AccARM;
   uint32_t qrz_AccDSP;
   uint32_t qrz_AccVDL;
   uint32_t qrz_TCount;
   uint32_t VDL_CLOCK, qrz_vdlline, VDL_HS,VDL_FS;
};
#pragma pack(pop)

static struct QDatum quarz;

uint32_t _qrz_SaveSize(void)
{
   return sizeof(struct QDatum);
}

void _qrz_Save(void *buff)
{
   memcpy(buff,&quarz,sizeof(struct QDatum));
}

void _qrz_Load(void *buff)
{
   memcpy(&quarz,buff,sizeof(struct QDatum));
}

#define qrz_AccARM quarz.qrz_AccARM
#define qrz_AccDSP quarz.qrz_AccDSP
#define qrz_AccVDL quarz.qrz_AccVDL
#define qrz_TCount quarz.qrz_TCount
#define VDL_CLOCK quarz.VDL_CLOCK
#define qrz_vdlline quarz.qrz_vdlline
#define VDL_HS quarz.VDL_HS
#define VDL_FS quarz.VDL_FS

void  _qrz_Init(void)
{
   qrz_AccVDL=qrz_AccDSP=0;
   qrz_AccARM=0;

   VDL_FS=526;
   VDL_CLOCK=VDL_FS*30;
   VDL_HS=VDL_FS/2;

   qrz_TCount=0;
   qrz_vdlline=0;
}

int  _qrz_VDCurrLine(void)
{
   return qrz_vdlline%(VDL_HS/*+(VDL_HS/2)*/);
}

int  _qrz_VDHalfFrame(void)
{
   return qrz_vdlline/(VDL_HS);
}

int  _qrz_VDCurrOverline(void)
{
   return qrz_vdlline;
}

bool  _qrz_QueueVDL(void)
{
   if(qrz_AccVDL>>24)
   {
      qrz_AccVDL-=0x1000000;
      qrz_vdlline++;
      qrz_vdlline%=VDL_FS;
      return true;
   }
   return false;
}

bool  _qrz_QueueDSP(void)
{
   if(qrz_AccDSP>>24)
   {
      //if(HightResMode!=0) qrz_AccDSP-=0x1000000/1.3;
      //else
      qrz_AccDSP-=0x1000000;
      return true;
   }
   return false;
}

bool  _qrz_QueueTimer(void)
{
   //uint32_t cnt=_clio_GetTimerDelay();
   if(qrz_TCount>>24)//=cnt)
   {
      qrz_TCount-=0x1000000;//cnt;
      return true;
   }
   return false;
}

void  _qrz_PushARMCycles(uint32_t clks)
{
   int timers = 21000000; /* default */
   int     sp = 0;
   uint32_t arm=(clks<<24)/(ARM_CLOCK);
   qrz_AccARM+=arm*(ARM_CLOCK);
   if( (qrz_AccARM>>24) != clks )
   {
      arm++;
      qrz_AccARM+=ARM_CLOCK;
      qrz_AccARM&=0xffffff;
   }
   qrz_AccDSP+=arm*SND_CLOCK;
   qrz_AccVDL+=arm*(VDL_CLOCK);

   if(_clio_GetTimerDelay())
      qrz_TCount += arm*((timers)/(_clio_GetTimerDelay()));
}
