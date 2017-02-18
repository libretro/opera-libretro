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
#include <string.h>

#include <retro_inline.h>

#include "vdlp.h"
#include "arm.h"

extern int HightResMode;

/* === VDL Palette data === */
#define VDL_CONTROL     0x80000000
#define VDL_BACKGROUND	0xE0000000
#define VDL_RGBCTL_MASK 0x60000000
#define VDL_PEN_MASK    0x1F000000
#define VDL_R_MASK      0x00FF0000
#define VDL_G_MASK      0x0000FF00
#define VDL_B_MASK      0x000000FF

#define VDL_B_SHIFT       0
#define VDL_G_SHIFT       8
#define VDL_R_SHIFT       16
#define VDL_PEN_SHIFT     24
#define VDL_RGBSEL_SHIFT  29

/* VDL_RGBCTL_MASK definitions */
#define VDL_FULLRGB     0x00000000
#define VDL_REDONLY     0x60000000
#define VDL_GREENONLY   0x40000000
#define VDL_BLUEONLY    0x20000000


#pragma pack(push,1)

struct cdmaw
{
   uint32_t	lines:9;//0-8
   uint32_t	numword:6;//9-14
   uint32_t	prevover:1;//15
   uint32_t	currover:1;//16
   uint32_t	prevtick:1;//17
   uint32_t  abs:1;//18
   uint32_t  vmode:1;//19
   uint32_t  pad0:1;//20
   uint32_t  enadma:1;//21
   uint32_t  pad1:1;//22
   uint32_t  modulo:3;//23-25
   uint32_t  pad2:6;//26-31
};
union CDMW
{
   uint32_t raw;
   struct cdmaw  dmaw;
};

struct VDLDatum
{
   uint8_t CLUTB[32];
   uint8_t CLUTG[32];
   uint8_t CLUTR[32];
   uint32_t BACKGROUND;
   uint32_t HEADVDL;
   uint32_t MODULO;
   uint32_t CURRENTVDL;
   uint32_t CURRENTBMP;
   uint32_t PREVIOUSBMP;
   uint32_t OUTCONTROLL;
   union CDMW CLUTDMA;
   int linedelay;
};
#pragma pack(pop)

static struct VDLDatum vdl;
static uint8_t *vram;

uint32_t _vdl_SaveSize(void)
{
   return sizeof(struct VDLDatum);
}

void _vdl_Save(void *buff)
{
   memcpy(buff,&vdl,sizeof(struct VDLDatum));
}

void _vdl_Load(void *buff)
{
   memcpy(&vdl,buff,sizeof(struct VDLDatum));
}

#define CLUTB vdl.CLUTB
#define CLUTG vdl.CLUTG
#define CLUTR vdl.CLUTR
#define BACKGROUND vdl.BACKGROUND
#define HEADVDL vdl.HEADVDL
#define MODULO vdl.MODULO
#define CURRENTVDL vdl.CURRENTVDL
#define CURRENTBMP vdl.CURRENTBMP
#define PREVIOUSBMP vdl.PREVIOUSBMP
#define OUTCONTROLL vdl.OUTCONTROLL
#define CLUTDMA vdl.CLUTDMA
#define linedelay vdl.linedelay


uint32_t vmreadw(uint32_t addr);

void _vdl_ProcessVDL( uint32_t addr)
{
   HEADVDL=addr;
}

static const uint32_t HOWMAYPIXELEXPECTPERLINE[8] =
{320, 384, 512, 640, 1024, 320, 320, 320};

// ###### Per line implementation ######

bool doloadclut=false;

static INLINE void VDLExec(void)
{
   int i;
   uint32_t NEXTVDL;
   uint8_t ifgnorflag=0;
   uint32_t tmp = vmreadw(CURRENTVDL);

   if(tmp==0) // End of list
   {
      linedelay=511;
      doloadclut=false;
   }
   else
   {
      CLUTDMA.raw=tmp;

      if(CLUTDMA.dmaw.currover)
      {
         if(fixmode&FIX_BIT_TIMING_5)
            CURRENTBMP=vmreadw(CURRENTVDL+8);
         else
            CURRENTBMP=vmreadw(CURRENTVDL+4);
      }
      if(CLUTDMA.dmaw.prevover)
      {
         if(fixmode&FIX_BIT_TIMING_5)
            PREVIOUSBMP=vmreadw(CURRENTVDL+4);
         else
            PREVIOUSBMP=vmreadw(CURRENTVDL+8);
      }
      if(CLUTDMA.dmaw.abs)
      {
         NEXTVDL=(CURRENTVDL+vmreadw(CURRENTVDL+12)+16);
         //CDebug::DPrint("Relative offset??\n");
      }
      else
         NEXTVDL=vmreadw(CURRENTVDL+12);

      CURRENTVDL+=16;

      int nmcmd=CLUTDMA.dmaw.numword;	//nmcmd-=4;?
      for(i = 0; i < nmcmd; i++)
      {
         int cmd=vmreadw(CURRENTVDL);
         CURRENTVDL+=4;

         if(!(cmd&VDL_CONTROL))
         {	//color value

            uint32_t coloridx=(cmd&VDL_PEN_MASK)>>VDL_PEN_SHIFT;

            if((cmd&VDL_RGBCTL_MASK)==VDL_FULLRGB)
            {
               CLUTR[coloridx]=(cmd&VDL_R_MASK)>>VDL_R_SHIFT;
               CLUTG[coloridx]=(cmd&VDL_G_MASK)>>VDL_G_SHIFT;
               CLUTB[coloridx]=(cmd&VDL_B_MASK)>>VDL_B_SHIFT;
            }
            else if((cmd&VDL_RGBCTL_MASK)==VDL_REDONLY)
               CLUTR[coloridx]=(cmd&VDL_R_MASK)>>VDL_R_SHIFT;
            else if((cmd&VDL_RGBCTL_MASK)==VDL_GREENONLY)
               CLUTG[coloridx]=(cmd&VDL_G_MASK)>>VDL_G_SHIFT;
            else if((cmd&VDL_RGBCTL_MASK)==VDL_BLUEONLY)
               CLUTB[coloridx]=(cmd&VDL_B_MASK)>>VDL_B_SHIFT;
         }
         else if((cmd&0xff000000)==VDL_BACKGROUND)
         {
            if(ifgnorflag)continue;
            BACKGROUND=((     cmd&0xFF    )<<16)|
               (( cmd&0xFF00 )) |
               (((cmd>>16)&0xFF) );
         }
         else if((cmd&0xE0000000)==0xc0000000)
         {
            if(ifgnorflag)continue;
            OUTCONTROLL=cmd;

            ifgnorflag=OUTCONTROLL&2;
         }
         else if((uint32_t)cmd==0xffffffff)
         {
            uint32_t j;
            if (ifgnorflag)
               continue;
            for(j = 0;j < 32; j++)
               CLUTB[j]=CLUTG[j]=CLUTR[j]=((j&0x1f)<<3)|((j>>2)&7);
         }
      }//for(i<nmcmd)
      CURRENTVDL=NEXTVDL;

      MODULO=HOWMAYPIXELEXPECTPERLINE[CLUTDMA.dmaw.modulo];
      doloadclut=((linedelay=CLUTDMA.dmaw.lines)!=0);
   }
}

static INLINE uint32_t VRAMOffEval(uint32_t addr, uint32_t line)
{
   return ((((~addr)&2)<<(18+HightResMode))+((addr>>2)<<1)+1024*512*line)<<HightResMode;
}

void _vdl_DoLineNew(int line2x, struct VDLFrame *frame)
{
   int y,i;
   int line=line2x&0x7ff;

   if(line==0)
   {
      doloadclut=true;
      linedelay=0;
      CURRENTVDL=HEADVDL;
      VDLExec();
   }

   y=(line-(16));

   if(linedelay==0 /*&& doloadclut*/)
      VDLExec();

   if((y>=0) && (y<240))  // 256???
   {
      if(CLUTDMA.dmaw.enadma)
      {
         if(HightResMode)
         {
            uint16_t *dst1,*dst2;
            uint32_t *src1,*src2,*src3,*src4;
            dst1=frame->lines[(y<<1)].line;
            dst2=frame->lines[(y<<1)+1].line;
            src1=(uint32_t*)(vram+((PREVIOUSBMP^2) & 0x0FFFFF));
            src2=(uint32_t*)(vram+((PREVIOUSBMP^2) & 0x0FFFFF)+1024*1024);
            src3=(uint32_t*)(vram+((CURRENTBMP^2) & 0x0FFFFF)+2*1024*1024);
            src4=(uint32_t*)(vram+((CURRENTBMP^2) & 0x0FFFFF)+3*1024*1024);
            i=320;
            while(i--)
            {
               *dst1++=*(uint16_t*)(src1++);
               *dst1++=*(uint16_t*)(src2++);
               *dst2++=*(uint16_t*)(src3++);
               *dst2++=*(uint16_t*)(src4++);
            }
         }
         else
         {
            uint16_t *dst;
            uint32_t *src;
            dst=frame->lines[y].line;
            src=(uint32_t*)(vram+((PREVIOUSBMP^2) & 0x0FFFFF));
            i=320;
            while(i--)
               *dst++=*(uint16_t*)(src++);
         }
         memcpy(frame->lines[(y<<HightResMode)].xCLUTB,CLUTB,32);
         memcpy(frame->lines[(y<<HightResMode)].xCLUTG,CLUTG,32);
         memcpy(frame->lines[(y<<HightResMode)].xCLUTR,CLUTR,32);
         if(HightResMode)
            memcpy(frame->lines[(y<<HightResMode)+1].xCLUTB,frame->lines[(y<<HightResMode)].xCLUTB,32*3);
      }
      frame->lines[(y<<HightResMode)].xOUTCONTROLL=OUTCONTROLL;
      frame->lines[(y<<HightResMode)].xCLUTDMA=CLUTDMA.raw;
      frame->lines[(y<<HightResMode)].xBACKGROUND=BACKGROUND;
      if(HightResMode)
      {
         frame->lines[(y<<HightResMode)+1].xOUTCONTROLL=OUTCONTROLL;
         frame->lines[(y<<HightResMode)+1].xCLUTDMA=CLUTDMA.raw;
         frame->lines[(y<<HightResMode)+1].xBACKGROUND=BACKGROUND;
      }

   } // //if((y>=0) && (y<240))

   if(CURRENTBMP & 2)
      CURRENTBMP+=MODULO*4 - 2;
   else
      CURRENTBMP+=2;

   if(!CLUTDMA.dmaw.prevtick)
   {
      PREVIOUSBMP=CURRENTBMP;
   }
   else
   {
      if(PREVIOUSBMP & 2)
         PREVIOUSBMP+=MODULO*4 - 2;
      else
         PREVIOUSBMP+=2;
   }


   linedelay--;
   OUTCONTROLL&=~1; //Vioff1ln
}


void _vdl_Init(uint8_t *vramstart)
{
   uint32_t i;
   vram = vramstart;

   static const uint32_t StartupVDL[]=
   { // Startup VDL at addres 0x2B0000
      0x00004410, 0x002C0000, 0x002C0000, 0x002B0098,
      0x00000000, 0x01080808, 0x02101010, 0x03191919,
      0x04212121, 0x05292929, 0x06313131, 0x073A3A3A,
      0x08424242, 0x094A4A4A, 0x0A525252, 0x0B5A5A5A,
      0x0C636363, 0x0D6B6B6B, 0x0E737373, 0x0F7B7B7B,
      0x10848484, 0x118C8C8C, 0x12949494, 0x139C9C9C,
      0x14A5A5A5, 0x15ADADAD, 0x16B5B5B5, 0x17BDBDBD,
      0x18C5C5C5, 0x19CECECE, 0x1AD6D6D6, 0x1BDEDEDE,
      0x1CE6E6E6, 0x1DEFEFEF, 0x1EF8F8F8, 0x1FFFFFFF,
      0xE0010101, 0xC001002C, 0x002180EF, 0x002C0000,
      0x002C0000, 0x002B00A8, 0x00000000, 0x002C0000,
      0x002C0000, 0x002B0000
   };
   HEADVDL=0xB0000;

   for(i = 0;i < (sizeof(StartupVDL)/4); i++)
      _mem_write32(HEADVDL+i*4+1024*1024*2,StartupVDL[i]);

   //memcpy(vram+HEADVDL, StartupVDL, sizeof(StartupVDL));


   for(i = 0; i < 32; i++)
      CLUTB[i] = CLUTG[i] = CLUTR[i] = ((i&0x1f)<<3) | ((i>>2)&7);
}

uint32_t vmreadw(uint32_t addr)
{
   return _mem_read32((addr&0xfffff)+1024*1024*2);
}
