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
#include "Clio.h"
#include "Madam.h"
#include "XBUS.h"
#include "arm.h"
#include "DSP.h"
#include "quarz.h"

#define DECREMENT    0x1
#define RELOAD       0x2
#define CASCADE      0x4
#define FLABLODE     0x8
int lsize, flagtime;
int TIMER_VAL=0; //0x415
extern int FMVFIX;

#define RELOAD_VAL   0x10

extern int jw;

void HandleDMA(uint32_t val);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#pragma pack(push,1)
struct FIFOt{

   uint32_t StartAdr;
   int StartLen;
   uint32_t NextAdr;
   int NextLen;
};
struct CLIODatum
{
   uint32_t cregs[65536];
   int DSPW1;
   int DSPW2;
   int DSPA;
   int PTRI[13];
   int PTRO[4];
   struct FIFOt FIFOI[13];
   struct FIFOt FIFOO[4];
};
#pragma pack(pop)

static uint32_t * Mregs;

static struct CLIODatum clio;

#define cregs clio.cregs
#define DSPW1 clio.DSPW1
#define DSPW2 clio.DSPW2
#define DSPA clio.DSPA
#define PTRI clio.PTRI
#define PTRO clio.PTRO
#define FIFOI clio.FIFOI
#define FIFOO clio.FIFOO

uint32_t _clio_SaveSize(void)
{
   return sizeof(struct CLIODatum);
}

void _clio_Save(void *buff)
{
   memcpy(buff,&clio,sizeof(struct CLIODatum));
}

void _clio_Load(void *buff)
{
   TIMER_VAL=0;

   memcpy(&clio,buff,sizeof(struct CLIODatum));
}

#define CURADR Mregs[base]
#define CURLEN Mregs[base+4]
#define RLDADR Mregs[base+8]
#define RLDLEN Mregs[base+0xc]

extern int fastrand(void);

int _clio_v0line(void)
{
   return cregs[8]&0x7ff;
}

int _clio_v1line(void)
{
   return cregs[12]&0x7ff;
}

bool _clio_NeedFIQ(void)
{
   if( (cregs[0x40]&cregs[0x48]) || (cregs[0x60]&cregs[0x68]) ) return true;
   return false;
}

void _clio_GenerateFiq(uint32_t reason1, uint32_t reason2)
{
   cregs[0x40] |= reason1;
   cregs[0x60] |= reason2;
   if(cregs[0x60])
      cregs[0x40] |= 0x80000000;	// irq31 if exist irq32 and high
}

#include "freedocore.h"
extern _ext_Interface  io_interface;
void _clio_SetTimers(uint32_t v200, uint32_t v208);
void _clio_ClearTimers(uint32_t v204, uint32_t v20c);

int _clio_Poke(uint32_t addr, uint32_t val)
{
   int base;
   int i;

   if(!flagtime){TIMER_VAL=lsize=0;}

   if( (addr& ~0x2C) == 0x40 ) // 0x40..0x4C, 0x60..0x6C case
   {
      if(addr==0x40)
      {
         cregs[0x40] |= val;
         if(cregs[0x60])
            cregs[0x40] |= 0x80000000;
         //if(cregs[0x40]&cregs[0x48]) _arm_SetFIQ();
         return 0;
      }
      else if(addr==0x44)
      {
         cregs[0x40]&=~val;
         if(!cregs[0x60]) cregs[0x40]&=~0x80000000;
         return 0;
      }
      else if(addr==0x48)
      {
         cregs[0x48]|=val;
         //if(cregs[0x40]&cregs[0x48]) _arm_SetFIQ();
         return 0;
      }
      else if(addr==0x4c)
      {
         cregs[0x48]&=~val;
         cregs[0x48]|=0x80000000; // always one for irq31
         return 0;
      }
#if 0
      else if(addr==0x50)
      {
         cregs[0x50]|=val&0x3fff0000;
         return 0;
      }
      else if(addr==0x54)
      {
         cregs[0x50]&=~val;
         return 0;
      }
#endif
      else if(addr==0x60)
      {
         cregs[0x60]|=val;
         if(cregs[0x60]) cregs[0x40]|=0x80000000;
         //if(cregs[0x60]&cregs[0x68])	_arm_SetFIQ();
         return 0;
      }
      else if(addr==0x64)
      {
         cregs[0x60]&=~val;
         if(!cregs[0x60]) cregs[0x40]&=~0x80000000;
         return 0;
      }
      else if(addr==0x68)
      {
         cregs[0x68]|=val;
         //if(cregs[0x60]&cregs[0x68]) _arm_SetFIQ();
         return 0;
      }
      else if(addr==0x6c)
      {
         cregs[0x68]&=~val;
         return 0;
      }
   }
   else if(addr==0x84)
   {
      cregs[0x84]=val&0xf;
      SelectROM((val&4)? 1:0 );
      return 0;
   }
   else if(addr==0x300)
   {
      //clear down the fifos and stop them
      base=0;
      cregs[0x304]&=~val;

      for(i=0;i<13;i++)
      {
         if(val&(1<<i))
         {
            base=0x400+(i<<4);
            RLDADR=CURADR=0;
            RLDLEN=CURLEN=0;
            _clio_SetFIFO(base,0);
            _clio_SetFIFO(base+4,0);
            _clio_SetFIFO(base+8,0);
            _clio_SetFIFO(base+0xc,0);
            val&=~(1<<i);
            PTRI[i]=0;
         }

      }

      for(i=0;i<4;i++)
      {
         if(val&(1<<(i+16)))
         {
            base=0x500+(i<<4);
            RLDADR=CURADR=0;
            RLDLEN=CURLEN=0;
            _clio_SetFIFO(base,0);
            _clio_SetFIFO(base+4,0);
            _clio_SetFIFO(base+8,0);
            _clio_SetFIFO(base+0xc,0);

            val&=~(1<<(i+16));
            PTRO[i]=0;

         }

      }


      return 0;
   }
   else if(addr==0x304) // Dma Starter!!!!! P/A !!!! need to create Handler.
   {
      HandleDMA(val);
      switch(val)
      {
         case 0x100000:
            if(TIMER_VAL<5800)
               TIMER_VAL+=0x33;
            lsize+=0x33;
            flagtime=(freedo_quarz_cpu_get_freq()/2000000);
            break;
         default:
            if(!cregs[0x304])
               TIMER_VAL=lsize=0;
            break;
      }
      return 0;
   }
   else if(addr==0x308) //Dma Stopper!!!!
   {
      cregs[0x304]&=~val;
      return 0;
   }
   else if(addr==0x400) //XBUS direction
   {
      if(val&0x800)
         return 0;

      cregs[0x400]=val;
      return 0;
   }
   else if((addr>=0x500) && (addr<0x540))
   {
      _xbus_SetSEL(val);

      return 0;
   }
   else if((addr>=0x540) && (addr<0x580))
   {
      _xbus_SetPoll(val);
      return 0;
   }
   else if((addr>=0x580) && (addr<0x5c0))
   {
      _xbus_SetCommandFIFO(val); // on FIFO Filled execute the command
      return 0;
   }
   else if((addr>=0x5c0) && (addr<0x600))
   {
      _xbus_SetDataFIFO(val); // on FIFO Filled execute the command
      return 0;
   }
   else if(addr==0x28)
   {
      cregs[addr]=val;
      if(val==0x30)
         return 1;
      else
         return 0;
   }else if((addr>=0x1800)&&(addr<=0x1fff))//0x0340 1800 … 0x0340 1BFF && 0x0340 1C00 … 0x0340 1FFF
   {
      addr&=~0x400; //mirrors
      DSPW1=val>>16;
      DSPW2=val&0xffff;
      DSPA=(addr-0x1800)>>1;
      _dsp_WriteMemory(DSPA,DSPW1);
      _dsp_WriteMemory(DSPA+1,DSPW2);
      return 0;
      //DSPNRAMWrite 2 DSPW per 1ARMW
   }else if((addr>=0x2000)&&(addr<=0x2fff))
   {
      addr&=~0x800;//mirrors
      DSPW1=val&0xffff;
      DSPA=(addr-0x2000)>>2;
      _dsp_WriteMemory(DSPA,DSPW1);
      return 0;
   }else if((addr>=0x3000)&&(addr<=0x33ff)) //0x0340 3000 … 0x0340 33FF
   {
      DSPA=(addr-0x3000)>>1;
      DSPA&=0xff;
      DSPW1=val>>16;
      DSPW2=val&0xffff;
      _dsp_WriteIMem(DSPA,DSPW1);
      _dsp_WriteIMem(DSPA+1,DSPW2);
      return 0;
   }else if((addr>=0x3400)&&(addr<=0x37ff))//0x0340 3400 … 0x0340 37FF
   {
      DSPA=(addr-0x3400)>>2;
      DSPA&=0xff;
      DSPW1=val&0xffff;
      _dsp_WriteIMem(DSPA,DSPW1);
      return 0;
   }
   else if(addr==0x17E8)//Reset
   {
      _dsp_Reset();
      return 0;
   }
   else if(addr==0x17D0)//Write DSP/ARM Semaphore
   {
      _dsp_ARMwrite2sema4(val);
      return 0;
   }
   else if(addr==0x17FC)//start/stop
   {
      _dsp_SetRunning(val>0);
      return 0;
   }
   else if(addr==0x200)
   {
      cregs[0x200]|=val;
      _clio_SetTimers(val, 0);
      return 0;
   }
   else if(addr==0x204)
   {
      cregs[0x200]&=~val;
      _clio_ClearTimers(val, 0);
      return 0;
   }
   else if(addr==0x208)
   {
      cregs[0x208]|=val;
      _clio_SetTimers(0, val);
      return 0;
   }
   else if(addr==0x20c)
   {
      cregs[0x208]&=~val;
      _clio_ClearTimers(0, val);
      return 0;
   }
   else if(addr==0x220)
   {
      //if(val<64)val=64;
      cregs[addr]=val&0x3ff;
      return 0;
   }
   else if(addr==0x120)
   {
      cregs[addr]=((TIMER_VAL>800)?TIMER_VAL+(val/0x30):val); //316 or 800?
      return 0;
   }

   if(addr==0x128&&val==0x0)
      jw=17000000;//val=1;

   cregs[addr]=val;
   return 0;
}



uint32_t _clio_Peek(uint32_t addr)
{
   if( (addr& ~0x2C) == 0x40 ) // 0x40..0x4C, 0x60..0x6C case
   {
      addr&=~4;	// By read 40 and 44, 48 and 4c, 60 and 64, 68 and 6c same
      if(addr==0x40)
         return cregs[0x40];
      else if(addr==0x48)
         return cregs[0x48]|0x80000000;
      else if(addr==0x60)
         return cregs[0x60];
      else if(addr==0x68)
         return cregs[0x68];
      return 0; // for skip warning C4715
   }
   else if(addr==0x204)
      return cregs[0x200];
   else if(addr==0x20c)
      return cregs[0x208];
   else if(addr==0x308)
      return cregs[0x304];
   else if (addr==0x414)
      return 0x4000; //TO CHECK!!! requested by CDROMDIPIR
   else if((addr>=0x500) && (addr<0x540))
      return _xbus_GetRes();
   else if((addr>=0x540) && (addr<0x580))
   {
      return _xbus_GetPoll();
   }
   else if((addr>=0x580) && (addr<0x5c0))
      return _xbus_GetStatusFIFO();
   else if((addr>=0x5c0) && (addr<0x600))
      return _xbus_GetDataFIFO();
   else if(addr==0x0)
      return 0x02020000;
   else if((addr>=0x3800)&&(addr<=0x3bff))//0x0340 3800 … 0x0340 3BFF
   {
      //2DSPW per 1ARMW
      DSPA=(addr-0x3800)>>1;
      DSPA&=0xff;
      DSPA+=0x300;
      DSPW1=_dsp_ReadIMem(DSPA);
      DSPW2=_dsp_ReadIMem(DSPA+1);
      return ((DSPW1<<16)|DSPW2);
   }
   else if((addr>=0x3c00)&&(addr<=0x3fff))//0x0340 3C00 … 0x0340 3FFF
   {
      DSPA=(addr-0x3c00)>>2;
      DSPA&=0xff;
      DSPA+=0x300;
      return (_dsp_ReadIMem(DSPA));
   }
   else if(addr==0x17F0)
      return fastrand();
   else if(addr==0x17D0)//Read DSP/ARM Semaphore
      return _dsp_ARMread2sema4();

   return cregs[addr];
}

void _clio_UpdateVCNT(int line, int halfframe)
{
   //	Poke(0x34,Peek(0x34)+1);
   cregs[0x34]=(halfframe<<11)+line;
}

void _clio_SetTimers(uint32_t v200, uint32_t v208)
{
   (void) v200;
   (void) v208;
}

void _clio_ClearTimers(uint32_t v204, uint32_t v20c)
{
   (void) v204;
   (void) v20c;
}

void _clio_DoTimers(void)
{
   uint32_t timer;
   uint16_t counter;
   bool NeedDecrementNextTimer=true;   // Need decrement for next timer

   for (timer=0;timer<16;timer++)
   {
      unsigned flag = cregs[(timer<8)?0x200:0x208]>>((timer*4)&31);

      if( !(flag & CASCADE) ) NeedDecrementNextTimer=true;

      if( NeedDecrementNextTimer && (flag & DECREMENT) )
      {
         counter=cregs[0x100+timer*8];
         if((NeedDecrementNextTimer=(counter--==0)))
         {
            if((timer&1))   // Only odd timers can generate
            {  // generate the interrupts because be overflow
               _clio_GenerateFiq(1<<(10-timer/2),0);
            }
            if(flag & RELOAD)
            {  // reload timer by reload value
               counter=cregs[0x100+timer*8+4];
               //return;
            }
            else
            {  // timer stopped -> reset it's flag DECREMENT
               cregs[(timer<8)?0x200:0x208] &= ~( DECREMENT<<((timer*4)&31) );
            }
         }
         cregs[0x100+timer*8]=counter;
      }
      else
         NeedDecrementNextTimer=false;
   }
}

uint32_t _clio_GetTimerDelay(void)
{
   return cregs[0x220];
}


void HandleDMA(uint32_t val)
{
   cregs[0x304]|=val;

   if(val&0x00100000)
   {
      unsigned src;
      unsigned trg;
      int len;
      uint8_t b0,b1,b2,b3;

      cregs[0x304]&=~0x00100000;
      src=_madam_Peek(0x540);
      trg=src;
      len=_madam_Peek(0x544);

      cregs[0x400]&=~0x80;

      if((cregs[0x404])&0x200)
      {
         while(len>=0)
         {
            b3=_xbus_GetDataFIFO();
            b2=_xbus_GetDataFIFO();
            b1=_xbus_GetDataFIFO();
            b0=_xbus_GetDataFIFO();

#ifdef MSB_FIRST
            _mem_write8(trg,b3);
            _mem_write8(trg+1,b2);
            _mem_write8(trg+2,b1);
            _mem_write8(trg+3,b0);
#else
            _mem_write8(trg,b0);
            _mem_write8(trg+1,b1);
            _mem_write8(trg+2,b2);
            _mem_write8(trg+3,b3);
#endif

            trg+=4;
            len-=4;
         }
         cregs[0x400]|=0x80;

      }
      else
      {
         while(len>=0)
         {
            b3=_xbus_GetDataFIFO();
            b2=_xbus_GetDataFIFO();
            b1=_xbus_GetDataFIFO();
            b0=_xbus_GetDataFIFO();

#ifdef MSB_FIRST
            _mem_write8(trg,b3);
            _mem_write8(trg+1,b2);
            _mem_write8(trg+2,b1);
            _mem_write8(trg+3,b0);
#else
            _mem_write8(trg,b0);
            _mem_write8(trg+1,b1);
            _mem_write8(trg+2,b2);
            _mem_write8(trg+3,b3);
#endif

            trg+=4;
            len-=4;
         }
         cregs[0x400]|=0x80;

      }
      len=0xFFFFFFFC;
      _madam_Poke(0x544,len);
      _clio_GenerateFiq(1<<29,0);

      return;
   }//XBDMA transfer
}

void _clio_Init(int ResetReson)
{
   unsigned i;
   for(i = 0; i < 32768; i++)
      cregs[i]=0;

   //cregs[8]=240;

   cregs[0x0028]=ResetReson;
   cregs[0x0400]=0x80;
   cregs[0x220]=64;
   Mregs=_madam_GetRegs();
   TIMER_VAL=0;

}
uint16_t  _clio_EIFIFO(uint16_t channel)
{
   unsigned base = 0x400+(channel*16);
   unsigned mask = 1<<channel;

   (void)base;
   (void)mask;

   if(FIFOI[channel].StartAdr!=0)//channel enabled
   {
      uint32_t val;

      if( (FIFOI[channel].StartLen-PTRI[channel])>0 )
      {
#ifdef MSB_FIRST
         val=_mem_read16( ((FIFOI[channel].StartAdr+PTRI[channel])) );
#else
         val=_mem_read16( ((FIFOI[channel].StartAdr+PTRI[channel])^2) );
#endif
         PTRI[channel]+=2;
      }
      else
      {
         PTRI[channel]=0;
         _clio_GenerateFiq(1<<(channel+16),0);//generate fiq
         if(FIFOI[channel].NextAdr!=0)// reload enabled see patent WO09410641A1, 49.16
         {
            FIFOI[channel].StartAdr=FIFOI[channel].NextAdr;
            FIFOI[channel].StartLen=FIFOI[channel].NextLen;
#ifdef MSB_FIRST
            val=_mem_read16(((FIFOI[channel].StartAdr+PTRI[channel]))); //get the value!!!
#else
            val=_mem_read16(((FIFOI[channel].StartAdr+PTRI[channel])^2)); //get the value!!!
#endif
            PTRI[channel]+=2;
         }
         else
         {
            FIFOI[channel].StartAdr=0;
            val=0;
         }
      }

      return val;
   }

   // JMK SEZ: What is this? It was commented out along with this whole "else"
   //          block, but I had to bring this else block back from the dead
   //          in order to initialize val appropriately.

   // _clio_GenerateFiq(1<<(channel+16),0);
   return 0;
}

void  _clio_EOFIFO(uint16_t channel, uint16_t val)
{
   /* Channel disabled? */
   if(FIFOO[channel].StartAdr == 0)
      return;

   if( (FIFOO[channel].StartLen-PTRO[channel])>0 )
   {
#ifdef MSB_FIRST
      _mem_write16(((FIFOO[channel].StartAdr+PTRO[channel])),val);
#else
      _mem_write16(((FIFOO[channel].StartAdr+PTRO[channel])^2),val);
#endif
      PTRO[channel] += 2;
   }
   else
   {
      PTRO[channel]=0;
      _clio_GenerateFiq(1<<(channel+12),0);//generate fiq

      if(FIFOO[channel].NextAdr!=0) //reload enabled?
      {
         FIFOO[channel].StartAdr=FIFOO[channel].NextAdr;
         FIFOO[channel].StartLen=FIFOO[channel].NextLen;
      }
      else
         FIFOO[channel].StartAdr = 0;
   }
}

uint16_t  _clio_EIFIFONI(uint16_t channel)
{
#ifdef MSB_FIRST
   return _mem_read16(((FIFOI[channel].StartAdr+PTRI[channel])));
#else
   return _mem_read16(((FIFOI[channel].StartAdr+PTRI[channel])^2));
#endif
}

uint16_t   _clio_GetEIFIFOStat(uint8_t channel)
{
   if( FIFOI[channel].StartAdr!=0 )
      return 2;// 2fixme

   return 0;
}

uint16_t   _clio_GetEOFIFOStat(uint8_t channel)
{
   if( FIFOO[channel].StartAdr!=0 )
      return 1;
   return 0;
}

void _clio_SetFIFO(uint32_t adr, uint32_t val)
{
   if( (adr&0x500) == 0x400)
   {

      switch (adr & 0xf)
      {
         case 0:
            FIFOI[(adr>>4)&0xf].StartAdr=val;
            FIFOI[(adr>>4)&0xf].NextAdr=0;//see patent WO09410641A1, 46.25
            break;
         case 4:
            FIFOI[(adr>>4)&0xf].StartLen=val+4;
            if(val==0)
               FIFOI[(adr>>4)&0xf].StartLen=0;

            FIFOI[(adr>>4)&0xf].NextLen=0;//see patent WO09410641A1, 46.25
            break;
         case 8:
            FIFOI[(adr>>4)&0xf].NextAdr=val;
            break;
         case 0xc:
            if(val != 0)
               FIFOI[(adr>>4)&0xf].NextLen=val+4;
            else
               FIFOI[(adr>>4)&0xf].NextLen=0;
            break;
      }
   }
   else
   {
      switch (adr & 0xf)
      {
         case 0:
            FIFOO[(adr>>4)&0xf].StartAdr=val;
            break;
         case 4:
            FIFOO[(adr>>4)&0xf].StartLen=val+4;
            break;
         case 8:
            FIFOO[(adr>>4)&0xf].NextAdr=val;
            break;
         case 0xc:
            FIFOO[(adr>>4)&0xf].NextLen=val+4;
            break;
      }
   }
}

void _clio_Reset(void)
{
   int i;
   for(i = 0;i < 65536; i++)
      cregs[i]=0;
}

uint32_t _clio_FIFOStruct(uint32_t addr)
{
   if((addr&0x500)==0x400)
   {
      switch (addr&0xf)
      {
         case 0:
            return FIFOI[(addr>>4)&0xf].StartAdr+PTRI[(addr>>4)&0xf];
         case 4:
            return FIFOI[(addr>>4)&0xf].StartLen-PTRI[(addr>>4)&0xf];
         case 8:
            return FIFOI[(addr>>4)&0xf].NextAdr;
         case 0xc:
            return FIFOI[(addr>>4)&0xf].NextLen;
      }
   }

   switch (addr&0xf)
   {
      case 0:
         return FIFOO[(addr>>4)&0xf].StartAdr+PTRO[(addr>>4)&0xf];
      case 4:
         return FIFOO[(addr>>4)&0xf].StartLen-PTRO[(addr>>4)&0xf];
      case 8:
         return FIFOO[(addr>>4)&0xf].NextAdr;
      case 0xc:
         return FIFOO[(addr>>4)&0xf].NextLen;
   }

   return 0;

}
