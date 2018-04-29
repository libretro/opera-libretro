/*
  www.freedo.org
  The first and only working 3DO multiplayer emulator.

  The FreeDO licensed under modified GNU LGPL, with following notes:

  *   The owners and original authors of the FreeDO have full right to
  *   develop closed source derivative work.

  *   Any non-commercial uses of the FreeDO sources or any knowledge
  *   obtained by studying or reverse engineering of the sources, or
  *   any other material published by FreeDO have to be accompanied
  *   with full credits.

  *   Any commercial uses of FreeDO sources or any knowledge obtained
  *   by studying or reverse engineering of the sources, or any other
  *   material published by FreeDO is strictly forbidden without
  *   owners approval.

  The above notes are taking precedence over GNU LGPL in conflicting
  situations.

  Project authors:
  *  Alexander Troosh
  *  Maxim Grishin
  *  Allen Wright
  *  John Sammons
  *  Felix Lazarev
  */

#include "freedo_arm.h"
#include "freedo_vdlp.h"
#include "hack_flags.h"
#include "inline.h"

#include <boolean.h>

#include <stdint.h>
#include <string.h>

extern int HightResMode;
extern int fixmode;

#define VRAM_OFFSET (1024 * 1024 * 2)

/* === VDL Palette data === */
#define VDL_CONTROL     0x80000000
#define VDL_BACKGROUND	0xE0000000
#define VDL_RGBCTL_MASK 0x60000000
#define VDL_PEN_MASK    0x1F000000
#define VDL_R_MASK      0x00FF0000
#define VDL_G_MASK      0x0000FF00
#define VDL_B_MASK      0x000000FF

#define VDL_B_SHIFT      0
#define VDL_G_SHIFT      8
#define VDL_R_SHIFT      16
#define VDL_PEN_SHIFT    24
#define VDL_RGBSEL_SHIFT 29

/* VDL_RGBCTL_MASK definitions */
#define VDL_FULLRGB   0x00000000
#define VDL_REDONLY   0x60000000
#define VDL_GREENONLY 0x40000000
#define VDL_BLUEONLY  0x20000000

struct cdmaw
{
  uint32_t lines:9;             //0-8
  uint32_t numword:6;           //9-14
  uint32_t prevover:1;          //15
  uint32_t currover:1;          //16
  uint32_t prevtick:1;          //17
  uint32_t abs:1;               //18
  uint32_t vmode:1;             //19
  uint32_t pad0:1;              //20
  uint32_t enadma:1;            //21
  uint32_t pad1:1;              //22
  uint32_t modulo:3;            //23-25
  uint32_t pad2:6;              //26-31
};

union CDMW
{
  uint32_t     raw;
  struct cdmaw dmaw;
};

struct vdlp_datum_s
{
  uint8_t    CLUTB[32];
  uint8_t    CLUTG[32];
  uint8_t    CLUTR[32];
  uint32_t   BACKGROUND;
  uint32_t   HEADVDL;
  uint32_t   MODULO;
  uint32_t   CURRENTVDL;
  uint32_t   CURRENTBMP;
  uint32_t   PREVIOUSBMP;
  uint32_t   OUTCONTROLL;
  union CDMW CLUTDMA;
  int        line_delay;
};

typedef struct vdlp_datum_s vdlp_datum_t;

#define CLUTB       vdl.CLUTB
#define CLUTG       vdl.CLUTG
#define CLUTR       vdl.CLUTR
#define BACKGROUND  vdl.BACKGROUND
#define HEADVDL     vdl.HEADVDL
#define MODULO      vdl.MODULO
#define CURRENTVDL  vdl.CURRENTVDL
#define CURRENTBMP  vdl.CURRENTBMP
#define PREVIOUSBMP vdl.PREVIOUSBMP
#define OUTCONTROLL vdl.OUTCONTROLL
#define CLUTDMA     vdl.CLUTDMA
#define LINE_DELAY  vdl.line_delay

static vdlp_datum_t vdl;
static uint8_t *VRAM = NULL;
static bool LOAD_CLUT = false;
static const uint32_t PIXELS_PER_LINE_MODULO[8] =
  {320, 384, 512, 640, 1024, 320, 320, 320};

static
INLINE
uint32_t
vram_read32(const uint32_t addr_)
{
  return _mem_read32(VRAM_OFFSET + (addr_ & 0x000FFFFF));
}

static
INLINE
void
vram_write32(const uint32_t addr_,
             const uint32_t datum_)
{
  _mem_write32((VRAM_OFFSET + (addr_ & 0x000FFFFF)),datum_);
}

static
INLINE
void
vdlp_clut_reset(void)
{
  int i;

  for(i = 0; i < 32; i++)
    CLUTB[i] = CLUTG[i] = CLUTR[i] = (((i & 0x1F) << 3) | ((i >> 2) & 7));
}

static
INLINE
void
vdlp_execute_last_vdl(void)
{
  LINE_DELAY = 511;
  LOAD_CLUT  = false;
}

static
INLINE
void
vdlp_execute_next_vdl(const uint32_t vdl_)
{
  int i;
  int numcmd;
  uint32_t NEXTVDL;
  uint32_t ignore_flag;

  ignore_flag = 0;
  CLUTDMA.raw = vdl_;

  if(CLUTDMA.dmaw.currover)
    CURRENTBMP = ((fixmode & FIX_BIT_TIMING_5) ?
                  vram_read32(CURRENTVDL+8) :
                  vram_read32(CURRENTVDL+4));

  if(CLUTDMA.dmaw.prevover)
    PREVIOUSBMP = ((fixmode & FIX_BIT_TIMING_5) ?
                   vram_read32(CURRENTVDL+4) :
                   vram_read32(CURRENTVDL+8));

  NEXTVDL = ((CLUTDMA.dmaw.abs) ?
             (CURRENTVDL+vram_read32(CURRENTVDL+12)+16) :
             vram_read32(CURRENTVDL+12));

  CURRENTVDL += 16;

  numcmd = CLUTDMA.dmaw.numword; //numcmd-=4;?

  for(i = 0; i < numcmd; i++)
    {
      uint32_t cmd;

      cmd = vram_read32(CURRENTVDL);
      CURRENTVDL += 4;

      if(!(cmd & VDL_CONTROL))
        {
          const uint32_t idx = ((cmd & VDL_PEN_MASK) >> VDL_PEN_SHIFT);

          switch(cmd & VDL_RGBCTL_MASK)
            {
            case VDL_FULLRGB:
              CLUTR[idx] = ((cmd & VDL_R_MASK) >> VDL_R_SHIFT);
              CLUTG[idx] = ((cmd & VDL_G_MASK) >> VDL_G_SHIFT);
              CLUTB[idx] = ((cmd & VDL_B_MASK) >> VDL_B_SHIFT);
              break;
            case VDL_REDONLY:
              CLUTR[idx] = ((cmd & VDL_R_MASK) >> VDL_R_SHIFT);
              break;
            case VDL_GREENONLY:
              CLUTG[idx] = ((cmd & VDL_G_MASK) >> VDL_G_SHIFT);
              break;
            case VDL_BLUEONLY:
              CLUTB[idx] = ((cmd & VDL_B_MASK) >> VDL_B_SHIFT);
              break;
            }
        }
      else if((cmd & 0xFF000000) == VDL_BACKGROUND)
        {
          if(ignore_flag)
            continue;
          BACKGROUND = (((cmd & 0x000000FF) << 16) |
                        ((cmd & 0x0000FF00) <<  0) |
                        ((cmd & 0x00FF0000) >> 16));
        }
      else if((cmd & 0xE0000000) == 0xC0000000)
        {
          if(ignore_flag)
            continue;
          OUTCONTROLL = cmd;
          ignore_flag = (OUTCONTROLL & 2);
        }
      else if(cmd == 0xFFFFFFFF)
        {
          if(ignore_flag)
            continue;
          vdlp_clut_reset();
        }
    }

  CURRENTVDL = NEXTVDL;
  MODULO     = PIXELS_PER_LINE_MODULO[CLUTDMA.dmaw.modulo];
  LOAD_CLUT = ((LINE_DELAY = CLUTDMA.dmaw.lines) != 0);
}

static
INLINE
void
vdlp_execute(void)
{
  uint32_t tmp;

  tmp = vram_read32(CURRENTVDL);
  if(tmp == 0)  /* End of list */
    vdlp_execute_last_vdl();
  else
    vdlp_execute_next_vdl(tmp);
}

static
INLINE
void
vdlp_process_line_320(int           line_,
                      vdlp_frame_t *frame_)
{
  vdlp_line_t *line;

  line = &frame_->lines[line_];
  if(CLUTDMA.dmaw.enadma)
    {
      int i;
      uint16_t *dst;
      uint32_t *src;

      dst = line->line;
      src = (uint32_t*)(VRAM + ((PREVIOUSBMP^2) & 0x0FFFFF));

      for(i = 0; i < 320; i++)
        *dst++ = *(uint16_t*)(src++);

      memcpy(line->xCLUTR,CLUTR,32);
      memcpy(line->xCLUTG,CLUTG,32);
      memcpy(line->xCLUTB,CLUTB,32);
    }

  line->xOUTCONTROLL = OUTCONTROLL;
  line->xCLUTDMA     = CLUTDMA.raw;
  line->xBACKGROUND  = BACKGROUND;
}

static
INLINE
void
vdlp_process_line_640(int           line_,
                      vdlp_frame_t *frame_)
{
  vdlp_line_t *line0;
  vdlp_line_t *line1;

  line0 = &frame_->lines[(line_ << 1) + 0];
  line1 = &frame_->lines[(line_ << 1) + 1];
  if(CLUTDMA.dmaw.enadma)
    {
      int i;
      uint16_t *dst1;
      uint16_t *dst2;
      uint32_t *src1;
      uint32_t *src2;
      uint32_t *src3;
      uint32_t *src4;

      dst1 = line0->line;
      dst2 = line1->line;
      src1 = (uint32_t*)(VRAM + ((PREVIOUSBMP^2) & 0x0FFFFF) + (0*1024*1024));
      src2 = (uint32_t*)(VRAM + ((PREVIOUSBMP^2) & 0x0FFFFF) + (1*1024*1024));
      src3 = (uint32_t*)(VRAM + ((PREVIOUSBMP^2) & 0x0FFFFF) + (2*1024*1024));
      src4 = (uint32_t*)(VRAM + ((PREVIOUSBMP^2) & 0x0FFFFF) + (3*1024*1024));

      for(i = 0; i < 320; i++)
        {
          *dst1++ = *(uint16_t*)(src1++);
          *dst1++ = *(uint16_t*)(src2++);
          *dst2++ = *(uint16_t*)(src3++);
          *dst2++ = *(uint16_t*)(src4++);
        }

      memcpy(line0->xCLUTR,CLUTR,32);
      memcpy(line0->xCLUTG,CLUTG,32);
      memcpy(line0->xCLUTB,CLUTB,32);
      memcpy(line1->xCLUTR,CLUTR,32);
      memcpy(line1->xCLUTG,CLUTG,32);
      memcpy(line1->xCLUTB,CLUTB,32);
    }

  line0->xOUTCONTROLL = line1->xOUTCONTROLL = OUTCONTROLL;
  line0->xCLUTDMA     = line1->xCLUTDMA     = CLUTDMA.raw;
  line0->xBACKGROUND  = line1->xBACKGROUND  = BACKGROUND;
}

static
INLINE
void
vdlp_process_line(int           line_,
                  vdlp_frame_t *frame_)
{
  if(HightResMode)
    vdlp_process_line_640(line_,frame_);
  else
    vdlp_process_line_320(line_,frame_);
}

void
freedo_vdlp_process_line(int           line_,
                         vdlp_frame_t *frame_)
{
  int y;

  line_ &= 0x07FF;
  if(line_ == 0)
    {
      LOAD_CLUT  = true;
      LINE_DELAY = 0;
      CURRENTVDL = HEADVDL;
      vdlp_execute();
    }

  y = (line_ - 16);

  if(LINE_DELAY == 0 /*&& LOAD_CLUT*/)
    vdlp_execute();

  if((y >= 0) && (y < 240))  // 256???
    vdlp_process_line(y,frame_);

  if(CURRENTBMP & 2)
    CURRENTBMP += ((MODULO * 4) - 2);
  else
    CURRENTBMP += 2;

  if(!CLUTDMA.dmaw.prevtick)
    {
      PREVIOUSBMP = CURRENTBMP;
    }
  else
    {
      if(PREVIOUSBMP & 2)
        PREVIOUSBMP += ((MODULO * 4) - 2);
      else
        PREVIOUSBMP += 2;
    }

  LINE_DELAY--;
  OUTCONTROLL &= ~1; //Vioff1ln
}


void
freedo_vdlp_init(uint8_t *vram_)
{
  uint32_t i;

  static const uint32_t StartupVDL[]=
    { // Startup VDL at address 0x2B0000
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

  VRAM    = vram_;
  HEADVDL = 0xB0000;

  for(i = 0; i < (sizeof(StartupVDL)/4); i++)
    vram_write32((HEADVDL + (i * 4)),StartupVDL[i]);

  vdlp_clut_reset();
}

void
freedo_vdlp_process(const uint32_t addr_)
{
  HEADVDL = addr_;
}

uint32_t
freedo_vdlp_state_size(void)
{
  return sizeof(vdlp_datum_t);
}

void
freedo_vdlp_state_save(void *buf_)
{
  memcpy(buf_,&vdl,sizeof(vdlp_datum_t));
}

void
freedo_vdlp_state_load(const void *buf_)
{
  memcpy(&vdl,buf_,sizeof(vdlp_datum_t));
}
