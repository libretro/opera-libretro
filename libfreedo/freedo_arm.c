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
#include <string.h>

#include <boolean.h>

#include "freedo_arm.h"
#include "freedo_clio.h"
#include "freedo_core.h"
#include "freedo_diag_port.h"
#include "freedo_madam.h"
#include "freedo_sport.h"
#include "hack_flags.h"
#include "inline.h"

extern int fixmode;
extern int cnbfix;
extern int HightResMode;

extern _ext_Interface  io_interface;

#define ARM_MUL_MASK    0x0fc000f0
#define ARM_MUL_SIGN    0x00000090
#define ARM_SDS_MASK    0x0fb00ff0
#define ARM_SDS_SIGN    0x01000090
#define ARM_UND_MASK    0x0e000010
#define ARM_UND_SIGN    0x06000010
#define ARM_MRS_MASK    0x0fbf0fff
#define ARM_MRS_SIGN    0x010f0000
#define ARM_MSR_MASK    0x0fbffff0
#define ARM_MSR_SIGN    0x0129f000
#define ARM_MSRF_MASK   0x0dbff000
#define ARM_MSRF_SIGN   0x0128f000

//режимы процессора----------------------------------------------------------
#define ARM_MODE_USER   0
#define ARM_MODE_FIQ    1
#define ARM_MODE_IRQ    2
#define ARM_MODE_SVC    3
#define ARM_MODE_ABT    4
#define ARM_MODE_UND    5
#define ARM_MODE_UNK    0xff

//static AString str;
const static uint8_t arm_mode_table[]=
{
   ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,
   ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,
   ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,
   ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,
   ARM_MODE_USER,    ARM_MODE_FIQ,     ARM_MODE_IRQ,     ARM_MODE_SVC,
   ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_ABT,
   ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UND,
   ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK,     ARM_MODE_UNK
};

//для посчета тактов------------------------------------------------------------
#define NCYCLE 4
#define SCYCLE 1
#define ICYCLE 1

//--------------------------Conditions-------------------------------------------
//flags - N Z C V  -  31...28
const static uint16_t cond_flags_cross[]={   //((cond_flags_cross[cond_feald]>>flags)&1)  -- пример проверки
   0xf0f0, //EQ - Z set (equal)
   0x0f0f, //NE - Z clear (not equal)
   0xcccc, //CS - C set (unsigned higher or same)
   0x3333, //CC - C clear (unsigned lower)
   0xff00, //N set (negative)
   0x00ff, //N clear (positive or zero)
   0xaaaa, //V set (overflow)
   0x5555, //V clear (no overflow)
   0x0c0c, //C set and Z clear (unsigned higher)
   0xf3f3, //C clear or Z set (unsigned lower or same)
   0xaa55, //N set and V set, or N clear and V clear (greater or equal)
   0x55aa, //N set and V clear, or N clear and V set (less than)
   0x0a05, //Z clear, and either N set and V set, or N clear and V clear (greater than)
   0xf5fa, //Z set, or N set and V clear, or N clear and V set (less than or equal)
   0xffff, //always
   0x0000  //never
};


///////////////////////////////////////////////////////////////
// Global variables;
///////////////////////////////////////////////////////////////
#define RAMSIZE     3*1024*1024 //dram1+dram2+vram
#define ROMSIZE     1*1024*1024 //rom
#define NVRAMSIZE   (65536>>1)		//nvram at 0x03140000...0x317FFFF
#define REG_PC	RON_USER[15]
#define UNDEFVAL 0xBAD12345

#pragma pack(push,1)

struct ARM_CoreState
{
   //console memories------------------------
   uint8_t *Ram;//[RAMSIZE];
   uint8_t *Rom;//[ROMSIZE*2];
   uint8_t *NVRam;//[NVRAMSIZE];

   //ARM60 registers
   uint32_t USER[16];
   uint32_t CASH[7];
   uint32_t SVC[2];
   uint32_t ABT[2];
   uint32_t FIQ[7];
   uint32_t IRQ[2];
   uint32_t UND[2];
   uint32_t SPSR[6];
   uint32_t CPSR;

   bool nFIQ; //external interrupt
   bool SecondROM;	//ROM selector
   bool MAS_Access_Exept;	//memory exceptions
};
#pragma pack(pop)

static struct ARM_CoreState arm;
static int CYCLES;	//cycle counter

//forward decls
uint32_t rreadusr(uint32_t rn);
void loadusr(uint32_t rn, uint32_t val);
uint32_t mreadb(uint32_t addr);
void mwriteb(uint32_t addr, uint32_t val);
uint32_t mreadw(uint32_t addr);
void mwritew(uint32_t addr,uint32_t val);

#define MAS_Access_Exept	arm.MAS_Access_Exept
#define pRam	arm.Ram
#define pRom	arm.Rom
#define pNVRam	arm.NVRam
#define RON_USER	arm.USER
#define RON_CASH	arm.CASH
#define RON_SVC	arm.SVC
#define RON_ABT	arm.ABT
#define RON_FIQ	arm.FIQ
#define RON_IRQ	arm.IRQ
#define RON_UND	arm.UND
#define SPSR	arm.SPSR
#define CPSR	arm.CPSR
#define gFIQ	arm.nFIQ
#define gSecondROM	arm.SecondROM

void* Getp_NVRAM(void)
{
   return pNVRam;
}

void* Getp_ROMS(void)
{
   return pRom;
}

void* Getp_RAMS(void)
{
   return pRam;
}

uint32_t _arm_SaveSize(void)
{
   return sizeof(struct ARM_CoreState) + RAMSIZE + (ROMSIZE * 2) + NVRAMSIZE;
}
void _arm_Save(void *buff)
{
   memcpy(buff,&arm,sizeof(struct ARM_CoreState));
   memcpy(((uint8_t*)buff)+sizeof(struct ARM_CoreState),pRam,RAMSIZE);
   memcpy(((uint8_t*)buff)+sizeof(struct ARM_CoreState)+RAMSIZE,pRom,ROMSIZE*2);
   memcpy(((uint8_t*)buff)+sizeof(struct ARM_CoreState)+RAMSIZE+ROMSIZE*2,pNVRam,NVRAMSIZE);
}

void _arm_Load(void *buff)
{
   uint8_t *tRam=pRam;
   uint8_t *tRom=pRom;
   uint8_t *tNVRam=pNVRam;
   memcpy(&arm,buff,sizeof(struct ARM_CoreState));
   memcpy(tRam,((uint8_t*)buff)+sizeof(struct ARM_CoreState),RAMSIZE);
   memcpy(tRom,((uint8_t*)buff)+sizeof(struct ARM_CoreState)+RAMSIZE,ROMSIZE*2);
   memcpy(tNVRam,((uint8_t*)buff)+sizeof(struct ARM_CoreState)+RAMSIZE+ROMSIZE*2,NVRAMSIZE);

   memcpy(tRam+3*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+4*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+5*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+6*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+7*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+8*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+9*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+10*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+11*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+12*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+13*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+14*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+15*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+16*1024*1024,tRam+2*1024*1024, 1024*1024);
   memcpy(tRam+17*1024*1024,tRam+2*1024*1024, 1024*1024);

   pRom=tRom;
   pRam=tRam;
   pNVRam=tNVRam;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static INLINE void load(uint32_t rn, uint32_t val)
{
   RON_USER[rn]=val;
}

void ARM_RestUserRONS(void)
{
   switch(arm_mode_table[(CPSR&0x1f)])
   {
      case ARM_MODE_USER:
         break;
      case ARM_MODE_FIQ:
         memcpy(RON_FIQ,&RON_USER[8],7<<2);
         memcpy(&RON_USER[8],RON_CASH,7<<2);
         break;
      case ARM_MODE_IRQ:
         RON_IRQ[0]=RON_USER[13];
         RON_IRQ[1]=RON_USER[14];
         RON_USER[13]=RON_CASH[5];
         RON_USER[14]=RON_CASH[6];
         break;
      case ARM_MODE_SVC:
         RON_SVC[0]=RON_USER[13];
         RON_SVC[1]=RON_USER[14];
         RON_USER[13]=RON_CASH[5];
         RON_USER[14]=RON_CASH[6];
         break;
      case ARM_MODE_ABT:
         RON_ABT[0]=RON_USER[13];
         RON_ABT[1]=RON_USER[14];
         RON_USER[13]=RON_CASH[5];
         RON_USER[14]=RON_CASH[6];
         break;
      case ARM_MODE_UND:
         RON_UND[0]=RON_USER[13];
         RON_UND[1]=RON_USER[14];
         RON_USER[13]=RON_CASH[5];
         RON_USER[14]=RON_CASH[6];
         break;
   }
}

void ARM_RestFiqRONS(void)
{
   switch(arm_mode_table[(CPSR&0x1f)])
   {
      case ARM_MODE_USER:
         memcpy(RON_CASH,&RON_USER[8],7<<2);
         memcpy(&RON_USER[8],RON_FIQ,7<<2);
         break;
      case ARM_MODE_FIQ:
         break;
      case ARM_MODE_IRQ:
         memcpy(RON_CASH,&RON_USER[8],5<<2);
         RON_IRQ[0]=RON_USER[13];
         RON_IRQ[1]=RON_USER[14];
         memcpy(&RON_USER[8],RON_FIQ,7<<2);
         break;
      case ARM_MODE_SVC:
         memcpy(RON_CASH,&RON_USER[8],5<<2);
         RON_SVC[0]=RON_USER[13];
         RON_SVC[1]=RON_USER[14];
         memcpy(&RON_USER[8],RON_FIQ,7<<2);
         break;
      case ARM_MODE_ABT:
         memcpy(RON_CASH,&RON_USER[8],5<<2);
         RON_ABT[0]=RON_USER[13];
         RON_ABT[1]=RON_USER[14];
         memcpy(&RON_USER[8],RON_FIQ,7<<2);
         break;
      case ARM_MODE_UND:
         memcpy(RON_CASH,&RON_USER[8],5<<2);
         RON_UND[0]=RON_USER[13];
         RON_UND[1]=RON_USER[14];
         memcpy(&RON_USER[8],RON_FIQ,7<<2);
         break;
   }
}

void ARM_RestIrqRONS(void)
{
   switch(arm_mode_table[(CPSR&0x1f)])
   {
      case ARM_MODE_USER:
         RON_CASH[5]=RON_USER[13];
         RON_CASH[6]=RON_USER[14];
         RON_USER[13]=RON_IRQ[0];
         RON_USER[14]=RON_IRQ[1];
         break;
      case ARM_MODE_FIQ:
         memcpy(RON_FIQ,&RON_USER[8],7<<2);
         memcpy(&RON_USER[8],RON_CASH,5<<2);
         RON_USER[13]=RON_IRQ[0];
         RON_USER[14]=RON_IRQ[1];
         break;
      case ARM_MODE_IRQ:
         break;
      case ARM_MODE_SVC:
         RON_SVC[0]=RON_USER[13];
         RON_SVC[1]=RON_USER[14];
         RON_USER[13]=RON_IRQ[0];
         RON_USER[14]=RON_IRQ[1];
         break;
      case ARM_MODE_ABT:
         RON_ABT[0]=RON_USER[13];
         RON_ABT[1]=RON_USER[14];
         RON_USER[13]=RON_IRQ[0];
         RON_USER[14]=RON_IRQ[1];
         break;
      case ARM_MODE_UND:
         RON_UND[0]=RON_USER[13];
         RON_UND[1]=RON_USER[14];
         RON_USER[13]=RON_IRQ[0];
         RON_USER[14]=RON_IRQ[1];
         break;
   }
}

void ARM_RestSvcRONS(void)
{
   switch(arm_mode_table[(CPSR&0x1f)])
   {
      case ARM_MODE_USER:
         RON_CASH[5]=RON_USER[13];
         RON_CASH[6]=RON_USER[14];
         RON_USER[13]=RON_SVC[0];
         RON_USER[14]=RON_SVC[1];
         break;
      case ARM_MODE_FIQ:
         memcpy(RON_FIQ,&RON_USER[8],7<<2);
         memcpy(&RON_USER[8],RON_CASH,5<<2);
         RON_USER[13]=RON_SVC[0];
         RON_USER[14]=RON_SVC[1];
         break;
      case ARM_MODE_IRQ:
         RON_IRQ[0]=RON_USER[13];
         RON_IRQ[1]=RON_USER[14];
         RON_USER[13]=RON_SVC[0];
         RON_USER[14]=RON_SVC[1];
         break;
      case ARM_MODE_SVC:
         break;
      case ARM_MODE_ABT:
         RON_ABT[0]=RON_USER[13];
         RON_ABT[1]=RON_USER[14];
         RON_USER[13]=RON_SVC[0];
         RON_USER[14]=RON_SVC[1];
         break;
      case ARM_MODE_UND:
         RON_UND[0]=RON_USER[13];
         RON_UND[1]=RON_USER[14];
         RON_USER[13]=RON_SVC[0];
         RON_USER[14]=RON_SVC[1];
         break;
   }
}

void ARM_RestAbtRONS(void)
{
   switch(arm_mode_table[(CPSR&0x1f)])
   {
      case ARM_MODE_USER:
         RON_CASH[5]=RON_USER[13];
         RON_CASH[6]=RON_USER[14];
         RON_USER[13]=RON_ABT[0];
         RON_USER[14]=RON_ABT[1];
         break;
      case ARM_MODE_FIQ:
         memcpy(RON_FIQ,&RON_USER[8],7<<2);
         memcpy(&RON_USER[8],RON_CASH,5<<2);
         RON_USER[13]=RON_ABT[0];
         RON_USER[14]=RON_ABT[1];
         break;
      case ARM_MODE_IRQ:
         RON_IRQ[0]=RON_USER[13];
         RON_IRQ[1]=RON_USER[14];
         RON_USER[13]=RON_ABT[0];
         RON_USER[14]=RON_ABT[1];
         break;
      case ARM_MODE_SVC:
         RON_SVC[0]=RON_USER[13];
         RON_SVC[1]=RON_USER[14];
         RON_USER[13]=RON_ABT[0];
         RON_USER[14]=RON_ABT[1];
         break;
      case ARM_MODE_ABT:
         break;
      case ARM_MODE_UND:
         RON_UND[0]=RON_USER[13];
         RON_UND[1]=RON_USER[14];
         RON_USER[13]=RON_ABT[0];
         RON_USER[14]=RON_ABT[1];
         break;
   }
}

void ARM_RestUndRONS(void)
{
   switch(arm_mode_table[(CPSR&0x1f)])
   {
      case ARM_MODE_USER:
         RON_CASH[5]=RON_USER[13];
         RON_CASH[6]=RON_USER[14];
         RON_USER[13]=RON_UND[0];
         RON_USER[14]=RON_UND[1];
         break;
      case ARM_MODE_FIQ:
         memcpy(RON_FIQ,&RON_USER[8],7<<2);
         memcpy(&RON_USER[8],RON_CASH,5<<2);
         RON_USER[13]=RON_UND[0];
         RON_USER[14]=RON_UND[1];
         break;
      case ARM_MODE_IRQ:
         RON_IRQ[0]=RON_USER[13];
         RON_IRQ[1]=RON_USER[14];
         RON_USER[13]=RON_UND[0];
         RON_USER[14]=RON_UND[1];
         break;
      case ARM_MODE_SVC:
         RON_SVC[0]=RON_USER[13];
         RON_SVC[1]=RON_USER[14];
         RON_USER[13]=RON_UND[0];
         RON_USER[14]=RON_UND[1];
         break;
      case ARM_MODE_ABT:
         RON_ABT[0]=RON_USER[13];
         RON_ABT[1]=RON_USER[14];
         RON_USER[13]=RON_UND[0];
         RON_USER[14]=RON_UND[1];
         break;
      case ARM_MODE_UND:
         break;
   }
}

void ARM_Change_ModeSafe(uint32_t mode)
{
   switch(arm_mode_table[mode&0x1f])
   {
      case ARM_MODE_USER:
         ARM_RestUserRONS();
         break;
      case ARM_MODE_FIQ:
         ARM_RestFiqRONS();
         break;
      case ARM_MODE_IRQ:
         ARM_RestIrqRONS();
         break;
      case ARM_MODE_SVC:
         ARM_RestSvcRONS();
         break;
      case ARM_MODE_ABT:
         ARM_RestAbtRONS();
         break;
      case ARM_MODE_UND:
         ARM_RestUndRONS();
         break;
   }
}

void SelectROM(int n)
{
   gSecondROM = (n>0)? true:false;
}

void _arm_SetCPSR(uint32_t a)
{
#if 0
   if(arm_mode_table[a&0x1f]==ARM_MODE_UNK)
   {
      //!!Exeption!!
   }
#endif
   a|=0x10;
   ARM_Change_ModeSafe(a);
   CPSR=a&0xf00000df;
}


static INLINE void SETM(uint32_t a)
{
#if 0
   if(arm_mode_table[a&0x1f]==ARM_MODE_UNK)
   {
      //!!Exeption!!
   }
#endif
   a|=0x10;
   ARM_Change_ModeSafe(a);
   CPSR=(CPSR&0xffffffe0)|(a & 0x1F);
}

// This functions d'nt change mode bits, then need no update regcur
static INLINE void SETN(bool a) { CPSR=(CPSR&0x7fffffff)|((a?1<<31:0)); }
static INLINE void SETZ(bool a) { CPSR=(CPSR&0xbfffffff)|((a?1<<30:0)); }
static INLINE void SETC(bool a) { CPSR=(CPSR&0xdfffffff)|((a?1<<29:0)); }
static INLINE void SETV(bool a) { CPSR=(CPSR&0xefffffff)|((a?1<<28:0)); }
static INLINE void SETI(bool a) { CPSR=(CPSR&0xffffff7f)|((a?1<<7:0)); }
static INLINE void SETF(bool a) { CPSR=(CPSR&0xffffffbf)|((a?1<<6:0)); }


///////////////////////////////////////////////////////////////
// Macros
///////////////////////////////////////////////////////////////
#define ISN  ((CPSR>>31)&1)
#define ISZ  ((CPSR>>30)&1)
#define ISC  ((CPSR>>29)&1)
#define ISV  ((CPSR>>28)&1)

#define MODE ((CPSR&0x1f))
#define ISI	 ((CPSR>>7)&1)
#define ISF  ((CPSR>>6)&1)

#define ROTR(val, shift) ((shift)) ? (((val) >> (shift)) | ((val) << (32 - (shift)))) : (val)

uint8_t * _arm_Init(void)
{
   int i;

   MAS_Access_Exept=false;

   CYCLES=0;
   for(i=0;i<16;i++)
      RON_USER[i]=0;
   for(i=0;i<2;i++)
   {
      RON_SVC[i]=0;
      RON_ABT[i]=0;
      RON_IRQ[i]=0;
      RON_UND[i]=0;
   }
   for(i=0;i<7;i++)
      RON_CASH[i]=RON_FIQ[i]=0;

   gSecondROM=0;
   pRam   = malloc(RAMSIZE+1024*1024*16 * sizeof(uint8_t));
   pRom   = malloc(ROMSIZE*2 * sizeof(uint8_t));
   pNVRam = malloc(NVRAMSIZE * sizeof(uint8_t));

   memset( pRam, 0, RAMSIZE+1024*1024*16);
   memset( pRom, 0, ROMSIZE*2);
   memset( pNVRam,0, NVRAMSIZE);
   gFIQ=false;

   // Endian swap for loaded ROM image

   REG_PC=0x03000000;
   _arm_SetCPSR(0x13); //set svc mode

   return (uint8_t *)pRam;
}

void _arm_Destroy(void)
{
   if (pNVRam)
      free(pNVRam);
   pNVRam = NULL;

   if (pRom)
      free(pRom);
   pRom = NULL;

   if (pRam)
      free(pRam);
   pRam = NULL;
}

void _arm_Reset(void)
{
   int i;
   gSecondROM=0;
   CYCLES=0;
   for(i=0;i<16;i++)
      RON_USER[i]=0;
   for(i=0;i<2;i++)
   {
      RON_SVC[i]=0;
      RON_ABT[i]=0;
      RON_IRQ[i]=0;
      RON_UND[i]=0;
   }
   for(i=0;i<7;i++)
      RON_CASH[i]=RON_FIQ[i]=0;

   MAS_Access_Exept=false;

   REG_PC=0x03000000;
   _arm_SetCPSR(0x13); //set svc mode
   gFIQ=false;		//no FIQ!!!
   gSecondROM=0;

   freedo_clio_reset();
   freedo_madam_reset();
}

int addrr=0;
int vall=0;
int inuse=0;

void ldm_accur(uint32_t opc, uint32_t base, uint32_t rn_ind)
{
   uint16_t x=opc&0xffff;
   uint16_t list=opc&0xffff;
   uint32_t base_comp,	//по ней шагаем
                i=0,tmp;

   x = (x & 0x5555) + ((x >> 1) & 0x5555);
   x = (x & 0x3333) + ((x >> 2) & 0x3333);
   x = (x & 0xff) + (x >> 8);
   x = (x & 0xf) + (x >> 4);

   switch((opc>>23)&3)
   {
      case 0:
         base-=(x<<2);
         base_comp=base+4;
         break;
      case 1:
         base_comp=base;
         base+=(x<<2);
         break;
      case 2:
         base_comp=base=base-(x<<2);
         break;
      case 3:
         base_comp=base+4;
         base+=(x<<2);
         break;
   }

   //base_comp&=~3;

   //if(opc&(1<<21))RON_USER[rn_ind]=base;

   if((opc&(1<<22)) && !(opc&0x8000))
   {
      if(opc&(1<<21))loadusr(rn_ind,base);
      while(list)
      {
         if(list&1)
         {
            tmp=mreadw(base_comp);
            /*if(MAS_Access_Exept)
              {
              if(opc&(1<<21))RON_USER[rn_ind]=base;
              break;
              } */
            loadusr(i,tmp);
            base_comp+=4;
         }
         i++;
         list>>=1;
      }
   }
   else
   {
      if(opc&(1<<21))RON_USER[rn_ind]=base;
      while(list)
      {
         if(list&1)
         {
            tmp=mreadw(base_comp);
            if (tmp==0xF1000&&i==0x1&&RON_USER[2]!=0xF0000&&cnbfix==0&&(fixmode&FIX_BIT_TIMING_1))
            {
               tmp+=0x1000;
            }
            //if(i==0x1&&tmp==0xF1000&&RON_USER[0]==RON_USER[i]){tmp+=0x1000;cnbfix=1;}
            if(inuse==1&&base_comp&0x1FFFFF)
            {
               if(base_comp==addrr)
                  inuse=0;
               if(tmp!=vall)
               {
                  if(tmp==0xEFE54&&i==0x4&&cnbfix==0&&(fixmode&FIX_BIT_TIMING_1))
                     tmp-=0xF;
                  //if(tmp==0xF1014)tmp=0x25000;
               }}
            RON_USER[i]=tmp;
            base_comp+=4;
         }
         i++;
         list>>=1;
      }
      if((opc&(1<<22)) && arm_mode_table[MODE] /*&& !MAS_Access_Exept*/)
         _arm_SetCPSR(SPSR[arm_mode_table[MODE]]);
   }

   CYCLES-=(x-1)*SCYCLE+NCYCLE+ICYCLE;

}


void stm_accur(uint32_t opc, uint32_t base, uint32_t rn_ind)
{
   uint16_t x=opc&0xffff;
   uint16_t list=opc&0x7fff;
   uint32_t base_comp,	//по ней шагаем
                i=0;

   x = (x & 0x5555) + ((x >> 1) & 0x5555);
   x = (x & 0x3333) + ((x >> 2) & 0x3333);
   x = (x & 0xff) + (x >> 8);
   x = (x & 0xf) + (x >> 4);

   switch((opc>>23)&3)
   {
      case 0:
         base-=(x<<2);
         base_comp=base+4;
         break;
      case 1:
         base_comp=base;
         base+=(x<<2);
         break;
      case 2:
         base_comp=base=base-(x<<2);
         break;
      case 3:
         base_comp=base+4;
         base+=(x<<2);
         break;
   }

   //base_comp&=~3;


   if((opc&(1<<22)))
   {
      if((opc&(1<<21)) && (opc&((1<<rn_ind)-1)) )loadusr(rn_ind,base);
      while(list)
      {
         if(list&1)
         {
            mwritew(base_comp,rreadusr(i));
            //if(MAS_Access_Exept)break;
            base_comp+=4;
         }
         i++;
         list>>=1;
      }
      if(opc&(1<<21))loadusr(rn_ind,base);
   }
   else
   {
      if((opc&(1<<21)) && (opc&((1<<rn_ind)-1)) )RON_USER[rn_ind]=base;
      while(list)
      {
         if(list&1)
         {
            int aac=RON_USER[i];
            mwritew(base_comp,aac);
            if(base_comp&0x1FFFFF){ addrr=base_comp; vall=aac; inuse=1;}
            base_comp+=4;
         }
         i++;
         list>>=1;
      }
      if(opc&(1<<21))RON_USER[rn_ind]=base;
   }

   if((opc&0x8000) /*&& !MAS_Access_Exept*/)mwritew(base_comp,RON_USER[15]+8);

   CYCLES-=(x-2)*SCYCLE+NCYCLE+NCYCLE;

}



void  bdt_core(uint32_t opc)
{
   uint32_t base;
   uint32_t rn_ind=(opc>>16)&0xf;

   if(rn_ind==0xf)
      base=RON_USER[rn_ind]+8;
   else
      base=RON_USER[rn_ind];

   if(opc&(1<<20))	//memory or register?
   {
      if(opc&0x8000)
         CYCLES-=SCYCLE+NCYCLE;

      ldm_accur(opc,base,rn_ind);

   }
   else //из регистра в память
      stm_accur(opc,base,rn_ind);
}

//------------------------------math SWI------------------------------------------------
typedef struct TagArg
{
   uint32_t Type;
   uint32_t Arg;
} TagItem;

void decode_swi(uint32_t i)
{

   (void) i;
   SPSR[arm_mode_table[0x13]]=CPSR;

   SETI(1);
   SETM(0x13);

   load(14,REG_PC);

   REG_PC=0x00000008;
   CYCLES-=SCYCLE+NCYCLE; // +2S+1N
}


uint32_t carry_out=0;

void ARM_SET_C(uint32_t x)
{
   //old_C=(CPSR>>29)&1;

   CPSR=((CPSR&0xdfffffff)|(((x)&1)<<29));
}

#define ARM_SET_Z(x)    (CPSR=((CPSR&0xbfffffff)|((x)==0?0x40000000:0)))
#define ARM_SET_N(x)    (CPSR=((CPSR&0x7fffffff)|((x)&0x80000000)))
#define ARM_GET_C       ((CPSR>>29)&1)

static INLINE void ARM_SET_ZN(uint32_t val)
{
   if(val)
      CPSR=((CPSR&0x3fffffff)|(val&0x80000000));
   else
      CPSR=((CPSR&0x3fffffff)|0x40000000);
}

static INLINE void ARM_SET_CV(uint32_t rd, uint32_t op1, uint32_t op2)
{
   //old_C=(CPSR>>29)&1;

   CPSR=(CPSR&0xcfffffff)|
      ((((op1 & op2) | ((~rd) & (op1 | op2)))&0x80000000)>>2) |
      (((((op1&(op2&(~rd))) | ((~op1)&(~op2)&rd)))&0x80000000)>>3);

}

static INLINE void ARM_SET_CV_sub(uint32_t rd, uint32_t op1, uint32_t op2)
{
   //old_C=(CPSR>>29)&1;

   CPSR=(CPSR&0xcfffffff)|
      //(( ( ~( ((~op1) & op2) | (rd&((~op1)|op2))) )&0x80000000)>>2) |
      ((((op1 & (~op2)) | ((~rd) & (op1 | (~op2))))&0x80000000)>>2) |
      (((((op1&((~op2)&(~rd))) | ((~op1)&op2&rd)))&0x80000000)>>3);
}


static INLINE bool ARM_ALU_Exec(uint32_t inst, uint8_t opc, uint32_t op1, uint32_t op2, uint32_t *Rd)
{
   switch(opc)
   {
      case 0:
         *Rd=op1&op2;
         break;
      case 2:
         *Rd=op1^op2;
         break;
      case 4:
         *Rd=op1-op2;
         break;
      case 6:
         *Rd=op2-op1;
         break;
      case 8:
         *Rd=op1+op2;
         break;
      case 10:
         *Rd=op1+op2+ARM_GET_C;
         break;
      case 12:
         *Rd=op1-op2-(ARM_GET_C^1);
         break;
      case 14:
         *Rd=op2-op1-(ARM_GET_C^1);
         break;
      case 16:
      case 20:
         if((inst>>22)&1)
            RON_USER[(inst>>12)&0xf]=SPSR[arm_mode_table[CPSR&0x1f]];
         else
            RON_USER[(inst>>12)&0xf]=CPSR;

         return true;
      case 18:
      case 22:
         if(!((inst>>16)&0x1) || !(arm_mode_table[MODE]))
         {
            if((inst>>22)&1)
               SPSR[arm_mode_table[MODE]]=(SPSR[arm_mode_table[MODE]]&0x0fffffff)|(op2&0xf0000000);
            else
               CPSR=(CPSR&0x0fffffff)|(op2&0xf0000000);
         }
         else
         {
            if((inst>>22)&1)
               SPSR[arm_mode_table[MODE]]=op2&0xf00000df;
            else
               _arm_SetCPSR(op2);
         }
         return true;
      case 24:
         *Rd=op1|op2;
         break;
      case 26:
         *Rd=op2;
         break;
      case 28:
         *Rd=op1&(~op2);
         break;
      case 30:
         *Rd=~op2;
         break;
      case 1:
         *Rd=op1&op2;
         ARM_SET_ZN(*Rd);
         break;
      case 3:
         *Rd=op1^op2;
         ARM_SET_ZN(*Rd);
         break;
      case 5:
         *Rd=op1-op2;
         ARM_SET_ZN(*Rd);
         ARM_SET_CV_sub(*Rd,op1,op2);
         break;
      case 7:
         *Rd=op2-op1;
         ARM_SET_ZN(*Rd);
         ARM_SET_CV_sub(*Rd,op2,op1);
         break;
      case 9:
         *Rd=op1+op2;
         ARM_SET_ZN(*Rd);
         ARM_SET_CV(*Rd,op1,op2);
         break;

      case 11:
         *Rd=op1+op2+ARM_GET_C;
         ARM_SET_ZN(*Rd);
         ARM_SET_CV(*Rd,op1,op2);
         break;
      case 13:
         *Rd=op1-op2-(ARM_GET_C^1);
         ARM_SET_ZN(*Rd);
         ARM_SET_CV_sub(*Rd,op1,op2);
         break;
      case 15:
         *Rd=op2-op1-(ARM_GET_C^1);
         ARM_SET_ZN(*Rd);
         ARM_SET_CV_sub(*Rd,op2,op1);
         break;//*/
      case 17:
         op1&=op2;
         ARM_SET_ZN(op1);
         return true;
      case 19:
         op1^=op2;
         ARM_SET_ZN(op1);
         return true;
      case 21:
         ARM_SET_CV_sub(op1-op2,op1,op2);
         ARM_SET_ZN(op1-op2);
         return true;
      case 23:
         ARM_SET_CV(op1+op2,op1,op2);
         ARM_SET_ZN(op1+op2);
         return true;
      case 25:
         *Rd=op1|op2;
         ARM_SET_ZN(*Rd);
         break;
      case 27:
         *Rd=op2;
         ARM_SET_ZN(*Rd);
         break;
      case 29:
         *Rd=op1&(~op2);
         ARM_SET_ZN(*Rd);
         break;
      case 31:
         *Rd=~op2;
         ARM_SET_ZN(*Rd);
         break;
   };
   return false;
}


uint32_t ARM_SHIFT_NSC(uint32_t value, uint8_t shift, uint8_t type)
{
   switch(type)
   {
      case 0:
         if(shift)
         {
            if(shift>32) carry_out=(0);
            else carry_out=(((value<<(shift-1))&0x80000000)>>31);
         }
         else carry_out=ARM_GET_C;

         if(shift==0)return value;
         if(shift>31)return 0;
         return value<<shift;
      case 1:

         if(shift)
         {
            if(shift>32) carry_out=(0);
            else carry_out=((value>>(shift-1))&1);
         }
         else carry_out=ARM_GET_C;

         if(shift==0)return value;
         if(shift>31)return 0;
         return value>>shift;
      case 2:

         if(shift)
         {
            if(shift>32) carry_out=((((signed int)value)>>31)&1);
            else carry_out=((((signed int)value)>>(shift-1))&1);
         }
         else carry_out=ARM_GET_C;

         if(shift==0)return value;
         if(shift>31)return (((signed int)value)>>31);
         return (((signed int)value)>>shift);
      case 3:

         if(shift)
         {
            if(shift&31)carry_out=((value>>(shift-1))&1);
            else carry_out=((value>>31)&1);
         }
         else carry_out=ARM_GET_C;

         shift&=31;
         if(shift==0)return value;
         return ROTR(value, shift);
      case 4:
         carry_out=value&1;
         return (value>>1)|(ARM_GET_C<<31);
   }
   return 0;
}

uint32_t  ARM_SHIFT_SC(uint32_t value, uint8_t shift, uint8_t type)
{
   uint32_t tmp;
   switch(type)
   {
      case 0:
         if(shift)
         {
            if(shift>32) ARM_SET_C(0);
            else ARM_SET_C(((value<<(shift-1))&0x80000000)>>31);
         }
         else
            return value;
         if(shift>31)
            return 0;
         return value<<shift;
      case 1:
         if(shift)
         {
            if(shift>32)
               ARM_SET_C(0);
            else
               ARM_SET_C((value>>(shift-1))&1);
         }
         else
            return value;
         if(shift>31)
            return 0;
         return value>>shift;
      case 2:
         if(shift)
         {
            if(shift>32) ARM_SET_C((((signed int)value)>>31)&1);
            else ARM_SET_C((((signed int)value)>>(shift-1))&1);
         }
         else
            return value;
         if(shift>31)
            return (((signed int)value)>>31);
         return ((signed int)value)>>shift;
      case 3:
         if(shift)
         {
            shift=((shift)&31);
            if(shift)
            {
               ARM_SET_C((value>>(shift-1))&1);
            }
            else
            {
               ARM_SET_C((value>>31)&1);
            }
         }
         else
            return value;
         return ROTR(value, shift);
      case 4:
         tmp=ARM_GET_C<<31;
         ARM_SET_C(value&1);
         return (value>>1)|(tmp);
   }

   return 0;
}



void ARM_SWAP(uint32_t cmd)
{
   uint32_t tmp, addr;

   REG_PC+=4;
   addr=RON_USER[(cmd>>16)&0xf];
   REG_PC+=4;

   if(cmd&(1<<22))
   {
      tmp=mreadb(addr);
      //	if(MAS_Access_Exept)return true;
      mwriteb(addr, RON_USER[cmd&0xf]);
      REG_PC-=8;
      //	if(MAS_Access_Exept)return true;
      RON_USER[(cmd>>12)&0xf]=tmp;
   }
   else
   {
      tmp=mreadw(addr);
      //if(MAS_Access_Exept)return true;
      mwritew(addr, RON_USER[cmd&0xf]);
      REG_PC-=8;
      //	if(MAS_Access_Exept)return true;
      if(addr&3)tmp=(tmp>>((addr&3)<<3))|(tmp<<(32-((addr&3)<<3)));
      RON_USER[(cmd>>12)&0xf]=tmp;
   }
}

static INLINE uint32_t calcbits(uint32_t num)
{
   uint32_t retval;

   if(!num)
      return 1;

   if(num>>16)
   {
      num>>=16;
      retval=16;
   }
   else
      retval=0;

   if(num>>8)
   {
      num>>=8;
      retval+=8;
   }
   if(num>>4)
   {
      num>>=4;
      retval+=4;
   }
   //if(num>>6){num>>=6;retval+=6;} //JMK NOTE: I saw this in 3DOplay... why was it here? It was not in the FreeDO core I got.
   if(num>>2)
   {
      num>>=2;
      retval+=2;
   }
   if(num>>1)
   {
      num>>=1;
      retval+=2;
   }
   else if(num)
      retval++;

   return retval;
}

uint32_t curr_pc;

const bool is_logic[]={
   true,true,false,false,
   false,false,false,false,
   true,true,false,false,
   true,true,true,true
};

int _arm_Execute(void)
{
   uint32_t op2,op1;
   uint8_t shift;
   uint8_t shtype;
   uint32_t cmd,pc_tmp;
   bool isexeption=false;
   if(biosanvil==1)
   {
      REG_PC+=64;
      biosanvil=2;
      isanvil=1;
   }

   //for(; CYCLES>0; CYCLES-=SCYCLE)
   {
      if(REG_PC==0x94D60&&RON_USER[0]==0x113000&&RON_USER[1]==0x113000&&cnbfix==0&&(fixmode&FIX_BIT_TIMING_1))
      {
         REG_PC=0x9E9CC;
         cnbfix=1;
      }
      cmd=mreadw(REG_PC);

      curr_pc=REG_PC;


      REG_PC+=4;

      CYCLES=-SCYCLE;
      if(cmd==0xE5101810&&CPSR==0x80000093)
         isexeption=true;
      //	if(REG_PC==0x9E9F0){isexeption=true; RON_USER[5]=0xE2998; fix=1;}
      if(((cond_flags_cross[(((uint32_t)cmd)>>28)]>>((CPSR)>>28))&1)&&isexeption==false)
      {
         switch((cmd>>24)&0xf)  //разбор типа команды
         {
            case 0x0:	//Multiply

               if ((cmd & ARM_MUL_MASK) == ARM_MUL_SIGN)
               {
                  uint32_t res = ((calcbits(RON_USER[(cmd>>8)&0xf])+5)>>1)-1;
                  if(res>16)
                     CYCLES-=16;
                  else
                     CYCLES-=res;

                  if(((cmd>>16)&0xf)==(cmd&0xf))
                  {
                     if (cmd&(1<<21))
                     {
                        REG_PC+=8;
                        res=RON_USER[(cmd>>12)&0xf];
                        REG_PC-=8;
                     }
                     else
                        res=0;
                  }
                  else
                  {
                     if (cmd&(1<<21))
                     {
                        res=RON_USER[cmd&0xf]*RON_USER[(cmd>>8)&0xf];
                        REG_PC+=8;
                        res+=RON_USER[(cmd>>12)&0xf];
                        REG_PC-=8;
                     }
                     else
                        res=RON_USER[cmd&0xf]*RON_USER[(cmd>>8)&0xf];
                  }
                  if(cmd&(1<<20))
                  {
                     ARM_SET_ZN(res);
                  }

                  RON_USER[(cmd>>16)&0xf]=res;
                  break;
               }
            case 0x1:	//Single Data Swap
               if ((cmd & ARM_SDS_MASK) == ARM_SDS_SIGN)
               {
                  ARM_SWAP(cmd);
                  //if(MAS_Access_Exept)
                  CYCLES-=2*NCYCLE+ICYCLE;
                  break;
               }
            case 0x2:	//ALU
            case 0x3:
               {

                  if((cmd&0x2000090)!=0x90)
                  {
                     /////////////////////////////////////////////SHIFT
                     pc_tmp=REG_PC;
                     REG_PC+=4;
                     if (cmd&(1<<25))
                     {
                        op2=cmd&0xff;
                        if(((cmd>>7)&0x1e))
                        {
                           op2= ROTR(op2, (cmd>>7)&0x1e);
                           //if((cmd&(1<<20))) SETC(((cmd&0xff)>>(((cmd>>7)&0x1e)-1))&1);
                        }
                        op1=RON_USER[(cmd>>16)&0xf];
                     }
                     else
                     {
                        shtype=(cmd>>5)&0x3;
                        if(cmd&(1<<4))
                        {
                           shift=((cmd>>8)&0xf);
                           shift=(RON_USER[shift])&0xff;
                           REG_PC+=4;
                           op2=RON_USER[cmd&0xf];
                           op1=RON_USER[(cmd>>16)&0xf];
                           CYCLES-=ICYCLE;
                        }
                        else
                        {
                           shift=(cmd>>7)&0x1f;

                           if(!shift)
                           {
                              if(shtype)
                              {
                                 if(shtype==3)shtype++;
                                 else shift=32;
                              }
                           }
                           op2=RON_USER[cmd&0xf];
                           op1=RON_USER[(cmd>>16)&0xf];
                        }


                        //if((cmd&(1<<20)) && is_logic[((cmd>>21)&0xf)] ) op2=ARM_SHIFT_SC(op2, shift, shtype);
                        //else
                        op2=ARM_SHIFT_NSC(op2, shift, shtype);

                     }

                     REG_PC=pc_tmp;

                     if((cmd&(1<<20)) && is_logic[((cmd>>21)&0xf)] ) ARM_SET_C(carry_out);


                     if(ARM_ALU_Exec(cmd, (cmd>>20)&0x1f ,op1,op2,&RON_USER[(cmd>>12)&0xf]))
                        break;

                     if(((cmd>>12)&0xf)==0xf) //destination = pc, take care of cpsr
                     {
                        if(cmd&(1<<20))
                        {
                           _arm_SetCPSR(SPSR[arm_mode_table[MODE]]);
                        }

                        CYCLES-=ICYCLE+NCYCLE;

                     }
                     break;
                  }
               }
            case 0x6:	//Undefined
            case 0x7:

Undefine:
               if((cmd&ARM_UND_MASK)==ARM_UND_SIGN)
               {
                  //!!Exeption!!
                  //         io_interface(EXT_DEBUG_PRINT,(void*)str.print("*PC: 0x%8.8X undefined\n",REG_PC).CStr());

                  SPSR[arm_mode_table[0x1b]]=CPSR;
                  SETI(1);
                  SETM(0x1b);
                  load(14,REG_PC);
                  REG_PC=0x00000004;  // (-4) fetch!!!
                  CYCLES-=SCYCLE+NCYCLE; // +2S+1N
                  break;
               }
            case 0x4:	//Single Data Transfer
            case 0x5:

               if((cmd&0x2000090)!=0x2000090)
               {
                  uint32_t base,tbas;
                  uint32_t oper2;
                  uint32_t val, rora;

                  pc_tmp=REG_PC;
                  REG_PC+=4;
                  if(cmd&(1<<25))
                  {
                     shtype=(cmd>>5)&0x3;
                     if(cmd&(1<<4))
                     {
                        shift=((cmd>>8)&0xf);
                        shift=(RON_USER[shift])&0xff;
                        REG_PC+=4;
                     }
                     else
                     {
                        shift=(cmd>>7)&0x1f;
                        if(!shift)
                        {
                           if(shtype)
                           {
                              if(shtype==3)shtype++;
                              else shift=32;
                           }
                        }
                     }
                     oper2=ARM_SHIFT_NSC(RON_USER[cmd&0xf], shift, shtype);

                  }
                  else
                     oper2=(cmd&0xfff);


                  tbas=base=RON_USER[((cmd>>16)&0xf)];


                  if(!(cmd&(1<<23)))
                     oper2=0-oper2;

                  if(cmd&(1<<24))
                     tbas=base=base+oper2;
                  else
                     base=base+oper2;


                  if(cmd&(1<<20)) //load
                  {
                     if(cmd&(1<<22))//bytes
                        val=mreadb(tbas)&0xff;
                     else //words/halfwords
                     {
                        unsigned rora = tbas & 3;
                        val           = mreadw(tbas);

                        if((rora))
                           val = ROTR(val,rora*8);
                     }

                     if(((cmd>>12)&0xf)==0xf)
                        CYCLES-=SCYCLE+NCYCLE;   // +1S+1N if R15 load

                     CYCLES-=NCYCLE+ICYCLE;  // +1N+1I
                     REG_PC=pc_tmp;

                     if ((cmd&(1<<21)) || (!(cmd&(1<<24)))) load((cmd>>16)&0xf,base);

                     if((cmd&(1<<21)) && !(cmd&(1<<24)))
                        loadusr((cmd>>12)&0xf,val);//privil mode
                     else
                        load((cmd>>12)&0xf,val);

                  }
                  else
                  { // store

                     if((cmd&(1<<21)) && !(cmd&(1<<24)))
                        val=rreadusr((cmd>>12)&0xf);// privil mode
                     else
                        val=RON_USER[(cmd>>12)&0xf];

                     //if(((cmd>>12)&0xf)==0xf)val+=delta;
                     REG_PC=pc_tmp;
                     CYCLES-=-SCYCLE+2*NCYCLE;  // 2N

                     if(cmd&(1<<22))//bytes/words
                        mwriteb(tbas,val);
                     else //words/halfwords
                        mwritew(tbas,val);

                     if ( (cmd&(1<<21)) || !(cmd&(1<<24)) ) load((cmd>>16)&0xf,base);

                  }

                  //if(MAS_Access_Exept)


                  break;
               }
               else goto Undefine;

            case 0x8:	//Block Data Transfer
            case 0x9:

               bdt_core(cmd);
               /*if(MAS_Access_Exept)
                 {
               //sprintf(str,"*PC: 0x%8.8X DataAbort!!!\n",REG_PC);
               //CDebug::DPrint(str);
               //!!Exeption!!

               SPSR[arm_mode_table[0x17]]=CPSR;
               SETI(1);
               SETM(0x17);
               load(14,REG_PC+4);
               REG_PC=0x00000010;
               CYCLES-=SCYCLE+NCYCLE;
               MAS_Access_Exept=false;
               break;
               } */


               break;

            case 0xa:	//BRANCH
            case 0xb:
               if(cmd&(1<<24))
                  RON_USER[14]=REG_PC;
               REG_PC+=(((cmd&0xffffff)|((cmd&0x800000)?0xff000000:0))<<2)+4;

               CYCLES-=SCYCLE+NCYCLE; //2S+1N

               break;

            case 0xf:	//SWI
               decode_swi(cmd);
               break;
               //---------
            default:	//coprocessor
               //!!Exeption!!
               //io_interface(EXT_DEBUG_PRINT,(void*)str.print("*PC: 0x%8.8X undefined\n",REG_PC).CStr());

               SPSR[arm_mode_table[0x1b]]=CPSR;
               SETI(1);
               SETM(0x1b);
               load(14,REG_PC);
               REG_PC=0x00000004;
               CYCLES-=SCYCLE+NCYCLE;
               break;

         };


      }	//condition

      if(!ISF && freedo_clio_fiq_needed()/*gFIQ*/)
      {

         //Set_madam_FSM(FSM_SUSPENDED);
         gFIQ=0;

         SPSR[arm_mode_table[0x11]]=CPSR;
         SETF(1);
         SETI(1);
         SETM(0x11);
         load(14,REG_PC+4);
         REG_PC=0x0000001c;//1c
      }

   } // for(CYCLES)


   return -CYCLES;
}


void _mem_write8(uint32_t addr, uint8_t val)
{
   pRam[addr]=val;
   if(addr<0x200000 || !HightResMode)
      return;
   pRam[addr+1024*1024]=val;
   pRam[addr+2*1024*1024]=val;
   pRam[addr+3*1024*1024]=val;
}
void  _mem_write16(uint32_t addr, uint16_t val)
{
   *((uint16_t*)&pRam[addr])=val;
   if(addr<0x200000 || !HightResMode) return;
   *((uint16_t*)&pRam[addr+1024*1024])=val;
   *((uint16_t*)&pRam[addr+2*1024*1024])=val;
   *((uint16_t*)&pRam[addr+3*1024*1024])=val;
}
void _mem_write32(uint32_t addr, uint32_t val)
{
   *((uint32_t*)&pRam[addr])=val;
   if(addr<0x200000 || !HightResMode) return;
   *((uint32_t*)&pRam[addr+1024*1024])=val;
   *((uint32_t*)&pRam[addr+2*1024*1024])=val;
   *((uint32_t*)&pRam[addr+3*1024*1024])=val;
}

uint16_t _mem_read16(uint32_t addr)
{
   return *((uint16_t*)&pRam[addr]);
}

uint32_t _mem_read32(uint32_t addr)
{
   return *((uint32_t*)&pRam[addr]);
}

uint8_t _mem_read8(uint32_t addr)
{
   return pRam[addr];
}


void mwritew(uint32_t addr, uint32_t val)
{
   //to do -- wipe out all HW part
   //to do -- add proper loging
   uint32_t index;

   addr&=~3;


   if (addr<0x00300000) //dram1&dram2&vram
   {
      _mem_write32(addr,val);
      return;
   }

   if (!((index=(addr^0x03300000)) & ~0x7FF)) //madam
      //  if((addr & ~0xFFFFF)==0x03300000) //madam
   {
      freedo_madam_poke(index,val);

      return;
   }


   if (!((index=(addr^0x03400000)) & ~0xFFFF)) //clio
      //  if((addr & ~0xFFFFF)==0x03400000) //clio
   {
      if(freedo_clio_poke(index,val))
         REG_PC+=4;  // ???
      return;
   }

   if(!((index=(addr^0x03200000)) & ~0xFFFFF)) //SPORT
   {
      freedo_sport_write_access(index,val);
      return;
   }


   if (!((index=(addr^0x03100000)) & ~0xFFFFF)) // NVRAM & DiagPort
   {
      if(index & 0x80000) //if (addr>=0x03180000)
      {
         _diag_Send(val);
         return;
      }
      else if(index & 0x40000)       //else if ((addr>=0x03140000) && (addr<0x03180000))
      {
         //  sprintf(str,":NVRAM Write [0x%X] = 0x%8.8X\n",addr,val);
         //  CDebug::DPrint(str);
         pNVRam[(index>>2) & 32767]=(uint8_t)val;
         //CConfig::SetNVRAMData(pNVRam);
      }
      return;
   }

   /*
      if ((addr>=0x03000000) && (addr<0x03100000)) //rom
      {
      return;
      }*/
   //io_interface(EXT_DEBUG_PRINT,(void*)str.print("0x%8.8X:  WriteWord???  0x%8.8X=0x%8.8X\n",REG_PC,addr,val).CStr());


}

uint32_t mreadw(uint32_t addr)
{
   //to do -- wipe out all HW
   //to do -- add abort (may be in HW)
   //to do -- proper loging
   int index;

   addr&=~3;

   if (addr<0x00300000) //dram1&dram2&vram
      return _mem_read32(addr);

   if (!((index=(addr^0x03300000)) & ~0xFFFFF)) //madam
      return freedo_madam_peek(index);


   if (!((index=(addr^0x03400000)) & ~0xFFFFF)) //clio
      return freedo_clio_peek(index);

   if (!((index=(addr^0x03200000)) & ~0xFFFFF)) // read acces to SPORT
   {
      if (!((index=(addr^0x03200000)) & ~0x1FFF))
         return (freedo_sport_set_source(index),0);
         //          io_interface(EXT_DEBUG_PRINT,(void*)str.print("0x%8.8X:  Unknow read access to SPORT  0x%8.8X=0x%8.8X\n",REG_PC,addr,0xBADACCE5).CStr());
         //!!Exeption!!
      return 0xBADACCE5;
   }

   if (!((index=(addr^0x03000000)) & ~0xFFFFF)) //rom
   {
      if(!gSecondROM) // 2nd rom
         return *(uint32_t*)(pRom+index);
      return *(uint32_t*)(pRom+index+1024*1024);
   }


   if (!((index=(addr^0x03100000)) & ~0xFFFFF)) // NVRAM & DiagPort
   {
      if(index & 0x80000) //if (addr>=0x03180000)
         return _diag_Get();
      else if(index & 0x40000)       //else if ((addr>=0x03140000) && (addr<0x03180000))
         return (uint32_t)pNVRam[(index>>2)&32767];
   }

   //   io_interface(EXT_DEBUG_PRINT,(void*)str.print("0x%8.8X:  ReadWord???  0x%8.8X=0x%8.8X\n",REG_PC,addr,0xBADACCE5).CStr());

   //MAS_Access_Exept=true;
   return 0xBADACCE5;///data abort
}


void mwriteb(uint32_t addr, uint32_t val)
{
   int index; // for avoid bad compiler optimization

   val&=0xff;


   if (addr<0x00300000) //dram1&dram2&vram
   {
      _mem_write8(addr^3,val);
      return;
   }

   else if (!((index=(addr^0x03100003)) & ~0xFFFFF)) //NVRAM
   {
      if((index & 0x40000)==0x40000)
      {
         //if((addr&3)==3)
         {
            pNVRam[(index>>2)&32767]=val;
         }
         return;
      }
   }
#if 0
   else if (!((index=(addr^0x03000003)) & ~0xFFFFF)) //rom
   {
      return;
   }
#endif

   //io_interface(EXT_DEBUG_PRINT,(void*)str.print("0x%8.8X:  WritetByte???  0x%8.8X=0x%8.8X\n",REG_PC,addr,val).CStr());

}



uint32_t mreadb(uint32_t addr)
{

   int index; // for avoid bad compiler optimization

   if (addr<0x00300000) //dram1&dram2&vram
      return _mem_read8(addr^3);
   else if (!((index=(addr^0x03000003)) & ~0xFFFFF)) //rom
   {
      if(gSecondROM) // 2nd rom
         return pRom[index+1024*1024];
      return pRom[index];
   }
   else if (!((index=(addr^0x03100003)) & ~0xFFFFF)) //NVRAM
   {
      if((index & 0x40000)==0x40000)
      {
         //if((addr&3)!=3)return 0;
         //else
         return pNVRam[(index>>2)&32767];
      }
   }

   //MAS_Access_Exept=true;
   //    io_interface(EXT_DEBUG_PRINT,(void*)str.print("0x%8.8X:  ReadByte???  0x%8.8X=0x%8.8X\n",REG_PC,addr,0xBADACCE5).CStr());

   return 0xBADACCE5;///data abort
}


void  loadusr(uint32_t n, uint32_t val)
{
   if(n==15)
   {
      RON_USER[15]=val;
      return;
   }

   switch(arm_mode_table[(CPSR&0x1f)|0x10])
   {
      case ARM_MODE_USER:
         RON_USER[n]=val;
         break;
      case ARM_MODE_FIQ:
         if(n>7)
            RON_CASH[n-8]=val;
         else
            RON_USER[n]=val;
         break;
      case ARM_MODE_IRQ:
      case ARM_MODE_ABT:
      case ARM_MODE_UND:
      case ARM_MODE_SVC:
         if(n>12)
            RON_CASH[n-8]=val;
         else
            RON_USER[n]=val;
         break;
   }
}


uint32_t rreadusr(uint32_t n)
{
   if(n==15)
      return RON_USER[15];

   switch(arm_mode_table[(CPSR&0x1f)])
   {
      case ARM_MODE_USER:
         return RON_USER[n];
      case ARM_MODE_FIQ:
         if(n>7)
            return RON_CASH[n-8];
         return RON_USER[n];
      case ARM_MODE_IRQ:
      case ARM_MODE_ABT:
      case ARM_MODE_UND:
      case ARM_MODE_SVC:
         if(n>12)
            return RON_CASH[n-8];
         return RON_USER[n];
   }
   return 0;
}

uint32_t ReadIO(uint32_t addr)
{
   return mreadw(addr);
}

void WriteIO(uint32_t addr, uint32_t val)
{
   mwritew(addr,val);
}
