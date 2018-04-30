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

#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>

#include "freedo_arm.h"
#include "freedo_clio.h"
#include "freedo_core.h"
#include "freedo_diag_port.h"
#include "freedo_dsp.h"
#include "freedo_frame.h"
#include "freedo_madam.h"
#include "freedo_quarz.h"
#include "freedo_sport.h"
#include "freedo_vdlp.h"
#include "freedo_xbus.h"
#include "freedo_xbus_cdrom_plugin.h"
#include "inline.h"

_ext_Interface  io_interface;

extern void* Getp_NVRAM(void);
extern void* Getp_ROMS(void);
extern void* Getp_RAMS(void);
extern int FMVFIX;
extern int lsize;
extern int flagtime;

int __tex__scaler = 0;
int HightResMode=0;
int fixmode=0;
int biosanvil=0;
int isanvil=0;
int sf=0;
int sdf=0;
int unknownflag11=0;
int jw=0;
int cnbfix=0;

static INLINE uint32_t _bswap(uint32_t x)
{
   return (x>>24) | ((x>>8)&0x0000FF00L) | ((x&0x0000FF00L)<<8) | (x<<24);
}

int _3do_Init(void)
{
   int i;
   uint8_t *Memory;
   uint8_t *rom;
   if(isanvil==30)biosanvil=1;
   Memory=_arm_Init();

   io_interface(EXT_READ_ROMS,Getp_ROMS());
   rom=(uint8_t*)Getp_ROMS();
   for(i= (1024*1024*2)-4; i >= 0; i -= 4)
      *(int *)(rom+i) =_bswap(*(int *)(rom+i));

   freedo_vdlp_init(Memory+0x200000);   // Visible only VRAM to it
   freedo_sport_init(Memory+0x200000);  // Visible only VRAM to it
   freedo_madam_init(Memory);

   freedo_xbus_init(xbus_cdrom_plugin);

   freedo_clio_init(0x40); // 0x40 for start from  3D0-CD, 0x01/0x02 from PhotoCD ?? (NO use 0x40/0x02 for BIOS test)
   freedo_dsp_init();
   _diag_Init(-1);  // Select test, use -1 -- if d'nt need tests
   /*
      00	DIAGNOSTICS TEST	(run of test: 1F, 24, 25, 32, 50, 51, 60, 61, 62, 68, 71, 75, 80, 81, 90)
      01	AUTO-DIAG TEST		(run of test: 1F, 24, 25, 32, 50, 51, 60, 61, 62, 68,         80, 81, 90)
      12	DRAM1 DATA TEST
      1A	DRAM2 DATA TEST
      1E	EARLY RAM TEST
      1F	RAM DATA TEST
      22	VRAM1 DATA TEST
      24	VRAM1 FLASH TEST
      25	VRAM1 SPORT TEST
      32	SRAM DATA TEST
      50	MADAM TEST
      51	CLIO TEST
      60	CD-ROM POLL TEST
      61	CD-ROM PATH TEST
      62	CD-ROM READ TEST	???
      63	CD-ROM AutoAdjustValue TEST
      67	CD-ROM#2 AutoAdjustValue TEST
      68  DEV#15 POLL TEST
      71	JOYPAD1 PRESS TEST
      75	JOYPAD1 AUDIO TEST
      80	SIN WAVE TEST
      81	MUTING TEST
      90	COLORBAR
      F0	CHECK TESTTOOL  ???
      F1	REVISION TEST
      FF	TEST END (halt)
      */
   freedo_xbus_device_load(0,NULL);

   freedo_quarz_init();

   return 0;
}

vdlp_frame_t *curr_frame;
bool skipframe;

void _3do_InternalFrame(int cycles)
{
   int line;
   freedo_quarz_push_cycles(cycles);
   if(freedo_quarz_queue_dsp())
      io_interface(EXT_PUSH_SAMPLE,(void*)(uintptr_t)freedo_dsp_loop());
   if(freedo_quarz_queue_timer())
      freedo_clio_timer_execute();
   if(freedo_quarz_queue_vdl())
   {
      line=freedo_quarz_vd_current_line();
      freedo_clio_vcnt_update(line, freedo_quarz_vd_half_frame());
      if(!skipframe)
         freedo_vdlp_process_line(line,curr_frame);
      if(line==16 && skipframe)
         io_interface(EXT_FRAMETRIGGER_MT,NULL);

      if(line==freedo_clio_line_v0())
         freedo_clio_fiq_generate(1<<0,0);

      if(line==freedo_clio_line_v1())
      {
         freedo_clio_fiq_generate(1<<1,0);
         //curr_frame->srcw=320;
         //curr_frame->srch=240;
         if(!skipframe)
            curr_frame = (vdlp_frame_t*)io_interface(EXT_SWAPFRAME,curr_frame);
         //if(!scipframe)io_interface(EXT_SWAPFRAME,curr_frame);
      }
   }
}

void _3do_Frame(vdlp_frame_t *frame, bool __skipframe)
{
   int i   = 0;
   int cnt = 0;

   curr_frame = frame;
   skipframe = __skipframe;
   if(flagtime)flagtime--;

   do
   {
      if(freedo_madam_fsm_get()==FSM_INPROCESS)
      {
         freedo_madam_cel_handle();
         freedo_madam_fsm_set(FSM_IDLE);
         continue;
      }

      cnt+=_arm_Execute();

      if(cnt >> 4)
      {
         _3do_InternalFrame(cnt);
         i += cnt;
         cnt = 0;
      }
   }while(i < (freedo_quarz_cpu_get_freq()/60));
}

void _3do_Destroy()
{
   _arm_Destroy();
   freedo_xbus_destroy();
}

uint32_t _3do_SaveSize(void)
{
   uint32_t tmp=_arm_SaveSize();

   tmp+=freedo_vdlp_state_size();
   tmp+=freedo_dsp_state_size();
   tmp+=freedo_clio_state_size();
   tmp+=freedo_quarz_state_size();
   tmp+=freedo_sport_state_size();
   tmp+=freedo_madam_state_size();
   tmp+=freedo_xbus_state_size();
   tmp+=16*4;
   return tmp;
}

void _3do_Save(void *buff)
{
   uint8_t *data=(uint8_t*)buff;
   int *indexes=(int*)buff;

   indexes[0]=0x97970101;
   indexes[1]=16*4;
   indexes[2]=indexes[1]+_arm_SaveSize();
   indexes[3]=indexes[2]+freedo_vdlp_state_size();
   indexes[4]=indexes[3]+freedo_dsp_state_size();
   indexes[5]=indexes[4]+freedo_clio_state_size();
   indexes[6]=indexes[5]+freedo_quarz_state_size();
   indexes[7]=indexes[6]+freedo_sport_state_size();
   indexes[8]=indexes[7]+freedo_madam_state_size();
   indexes[9]=indexes[8]+freedo_xbus_state_size();

   _arm_Save(&data[indexes[1]]);
   freedo_vdlp_state_save(&data[indexes[2]]);
   freedo_dsp_state_save(&data[indexes[3]]);
   freedo_clio_state_save(&data[indexes[4]]);
   freedo_quarz_state_save(&data[indexes[5]]);
   freedo_sport_state_save(&data[indexes[6]]);
   freedo_madam_state_save(&data[indexes[7]]);
   freedo_xbus_state_save(&data[indexes[8]]);

}

bool _3do_Load(void *buff)
{
   uint8_t *data=(uint8_t*)buff;
   int *indexes=(int*)buff;
   if((uint32_t)indexes[0]!=0x97970101)
      return false;

   _arm_Load(&data[indexes[1]]);
   freedo_vdlp_state_load(&data[indexes[2]]);
   freedo_dsp_state_load(&data[indexes[3]]);
   freedo_clio_state_load(&data[indexes[4]]);
   freedo_quarz_state_load(&data[indexes[5]]);
   freedo_sport_state_load(&data[indexes[6]]);
   freedo_madam_state_load(&data[indexes[7]]);
   freedo_xbus_state_load(&data[indexes[8]]);

   return true;
}

void *_freedo_Interface(int procedure, void *datum)
{
   int line;

   switch(procedure)
   {
      case FDP_INIT:
         cnbfix=0;
         sf=5000000;
         io_interface=(_ext_Interface)datum;
         _3do_Init();
         break;
      case FDP_DESTROY:
         _3do_Destroy();
         break;
      case FDP_DO_EXECFRAME:
         _3do_Frame((vdlp_frame_t*)datum, false);
         break;
      case FDP_DO_EXECFRAME_MT:
         _3do_Frame((vdlp_frame_t*)datum, true);
         break;
      case FDP_DO_FRAME_MT:
         line=0;
         while(line<256)freedo_vdlp_process_line(line++,(vdlp_frame_t*)datum);
         break;
      case FDP_GET_SAVE_SIZE:
         _3do_SaveSize();
         break;
      case FDP_DO_SAVE:
         _3do_Save(datum);
         break;
      case FDP_DO_LOAD:
         cnbfix=1;
         sf=0;
         _3do_Load(datum);
         break;
      case FDP_GETP_RAMS:
         Getp_RAMS();
         break;
      case FDP_GETP_ROMS:
         Getp_ROMS();
         break;
      case FDP_GETP_PROFILE:
         break;
      case FDP_SET_TEXQUALITY:
         __tex__scaler=(intptr_t)datum;
         break;
         //	case FDP_SET_FIX_MODE:
         //		fixmode=(intptr_t)datum;
         //		break;
         //	case FDP_GET_FRAME_BITMAP:
         //		GetFrameBitmapParams* param = (GetFrameBitmapParams*)datum;
         //		Get_Frame_Bitmap(
         //			param->sourceFrame
         //			, param->destinationBitmap
         //			, param->destinationBitmapWidthPixels
         //			, param->bitmapCrop
         //			, param->copyWidthPixels
         //			, param->copyHeightPixels
         //			, param->addBlackBorder
         //			, param->copyPointlessAlphaByte
         //			, param->allowCrop
         //			, (ScalingAlgorithm)param->scalingAlgorithm
         //			, &param->resultingWidth
         //			, &param->resultingHeight);
         //		break;
      case FDP_GET_BIOS_TYPE:
         return (void*)&isanvil;
         break;
      case FDP_SET_ANVIL:
         isanvil=(int)datum;
         break;
   }

   return NULL;
}
