/*
  www.freedo.org
  The first working 3DO multiplayer emulator.

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

#include "boolean.h"
#include "endianness.h"
#include "flags.h"
#include "hack_flags.h"
#include "inline.h"
#include "opera_arm.h"
#include "opera_bitop.h"
#include "opera_clio.h"
#include "opera_core.h"
#include "opera_madam.h"
#include "opera_mem.h"
#include "opera_pbus.h"
#include "opera_state.h"
#include "opera_vdlp.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static struct BitReaderBig bitoper;

/* === CCB control word flags === */
#define CCB_SKIP        0x80000000
#define CCB_LAST        0x40000000
#define CCB_NPABS       0x20000000
#define CCB_SPABS       0x10000000
#define CCB_PPABS       0x08000000
#define CCB_LDSIZE      0x04000000
#define CCB_LDPRS       0x02000000
#define CCB_LDPPMP      0x01000000
#define CCB_LDPLUT      0x00800000
#define CCB_CCBPRE      0x00400000
#define CCB_YOXY        0x00200000
#define CCB_ACSC        0x00100000
#define CCB_ALSC        0x00080000
#define CCB_ACW         0x00040000
#define CCB_ACCW        0x00020000
#define CCB_TWD         0x00010000
#define CCB_LCE         0x00008000
#define CCB_ACE         0x00004000
#define CCB_reserved13  0x00002000
#define CCB_MARIA       0x00001000
#define CCB_PXOR        0x00000800
#define CCB_USEAV       0x00000400
#define CCB_PACKED      0x00000200
#define CCB_POVER_MASK  0x00000180
#define CCB_PLUTPOS     0x00000040
#define CCB_BGND        0x00000020
#define CCB_NOBLK       0x00000010
#define CCB_PLUTA_MASK  0x0000000F

#define CCB_POVER_SHIFT  7
#define CCB_PLUTA_SHIFT  0

#define PMODE_PDC   ((0x00000000)<<CCB_POVER_SHIFT) /* Normal */
#define PMODE_ZERO  ((0x00000002)<<CCB_POVER_SHIFT)
#define PMODE_ONE   ((0x00000003)<<CCB_POVER_SHIFT)

/* === CCBCTL0 flags === */
#define B15POS_MASK   0xC0000000
#define B0POS_MASK    0x30000000
#define SWAPHV        0x08000000
#define ASCALL        0x04000000
#define _CCBCTL0_u25  0x02000000
#define CFBDSUB       0x01000000
#define CFBDLSB_MASK  0x00C00000
#define PDCLSB_MASK   0x00300000

#define B15POS_SHIFT  30
#define B0POS_SHIFT   28
#define CFBD_SHIFT    22
#define PDCLSB_SHIFT  20

/* B15POS_MASK definitions */
#define B15POS_0    0x00000000
#define B15POS_1    0x40000000
#define B15POS_PDC  0xC0000000

/* B0POS_MASK definitions */
#define B0POS_0     0x00000000
#define B0POS_1     0x10000000
#define B0POS_PPMP  0x20000000
#define B0POS_PDC   0x30000000

/* CFBDLSB_MASK definitions */
#define CFBDLSB_0      0x00000000
#define CFBDLSB_CFBD0  0x00400000
#define CFBDLSB_CFBD4  0x00800000
#define CFBDLSB_CFBD5  0x00C00000

/* PDCLSB_MASK definitions */
#define PDCLSB_0     0x00000000
#define PDCLSB_PDC0  0x00100000
#define PDCLSB_PDC4  0x00200000
#define PDCLSB_PDC5  0x00300000

/* === Cel first preamble word flags === */
#define PRE0_LITERAL    0x80000000
#define PRE0_BGND       0x40000000
#define PREO_reservedA  0x30000000
#define PRE0_SKIPX_MASK 0x0F000000
#define PREO_reservedB  0x00FF0000
#define PRE0_VCNT_MASK  0x0000FFC0
#define PREO_reservedC  0x00000020
#define PRE0_LINEAR     0x00000010
#define PRE0_REP8       0x00000008
#define PRE0_BPP_MASK   0x00000007

#define PRE0_SKIPX_SHIFT 24
#define PRE0_VCNT_SHIFT  6
#define PRE0_BPP_SHIFT   0

/* PRE0_BPP_MASK definitions */
#define PRE0_BPP_1   0x00000001
#define PRE0_BPP_2   0x00000002
#define PRE0_BPP_4   0x00000003
#define PRE0_BPP_6   0x00000004
#define PRE0_BPP_8   0x00000005
#define PRE0_BPP_16  0x00000006

/* Subtract this value from the actual vertical source line count */
#define PRE0_VCNT_PREFETCH    1

/* === Cel second preamble word flags === */
#define PRE1_WOFFSET8_MASK   0xFF000000
#define PRE1_WOFFSET10_MASK  0x03FF0000
#define PRE1_NOSWAP          0x00004000
#define PRE1_TLLSB_MASK      0x00003000
#define PRE1_LRFORM          0x00000800
#define PRE1_TLHPCNT_MASK    0x000007FF

#define PRE1_WOFFSET8_SHIFT   24
#define PRE1_WOFFSET10_SHIFT  16
#define PRE1_TLLSB_SHIFT      12
#define PRE1_TLHPCNT_SHIFT    0

#define PRE1_TLLSB_0     0x00000000
#define PRE1_TLLSB_PDC0  0x00001000 /* Normal */
#define PRE1_TLLSB_PDC4  0x00002000
#define PRE1_TLLSB_PDC5  0x00003000

/* Subtract this value from the actual word offset */
#define PRE1_WOFFSET_PREFETCH 2
/* Subtract this value from the actual pixel count */
#define PRE1_TLHPCNT_PREFETCH 1

#define PPMP_0_SHIFT 0
#define PPMP_1_SHIFT 16

#define PPMPC_1S_MASK  0x00008000
#define PPMPC_MS_MASK  0x00006000
#define PPMPC_MF_MASK  0x00001C00
#define PPMPC_SF_MASK  0x00000300
#define PPMPC_2S_MASK  0x000000C0
#define PPMPC_AV_MASK  0x0000003E
#define PPMPC_2D_MASK  0x00000001

#define PPMPC_MS_SHIFT  13
#define PPMPC_MF_SHIFT  10
#define PPMPC_SF_SHIFT  8
#define PPMPC_2S_SHIFT  6
#define PPMPC_AV_SHIFT  1

/* PPMPC_1S_MASK definitions */
#define PPMPC_1S_PDC   0x00000000
#define PPMPC_1S_CFBD  0x00008000

/* PPMPC_MS_MASK definitions */
#define PPMPC_MS_CCB         0x00000000
#define PPMPC_MS_PIN         0x00002000
#define PPMPC_MS_PDC_MFONLY  0x00004000
#define PPMPC_MS_PDC         0x00004000

/* PPMPC_MF_MASK definitions */
#define PPMPC_MF_1  0x00000000
#define PPMPC_MF_2  0x00000400
#define PPMPC_MF_3  0x00000800
#define PPMPC_MF_4  0x00000C00
#define PPMPC_MF_5  0x00001000
#define PPMPC_MF_6  0x00001400
#define PPMPC_MF_7  0x00001800
#define PPMPC_MF_8  0x00001C00

/* PPMPC_SF_MASK definitions */
#define PPMPC_SF_2   0x00000100
#define PPMPC_SF_4   0x00000200
#define PPMPC_SF_8   0x00000300
#define PPMPC_SF_16  0x00000000

/* PPMPC_2S_MASK definitions */
#define PPMPC_2S_0     0x00000000
#define PPMPC_2S_CCB   0x00000040
#define PPMPC_2S_CFBD  0x00000080
#define PPMPC_2S_PDC   0x000000C0

/* PPMPC_2D_MASK definitions */
#define PPMPC_2D_1  0x00000000
#define PPMPC_2D_2  0x00000001

#define MASK_24BIT 0x00FFFFFF

#pragma pack(push,1)

struct cp1btag_s
{
  uint16_t c:1;
  uint16_t pad:15;
};

typedef struct cp1btag_s cp1btag_t;

struct cp2btag_s
{
  uint16_t c:2;
  uint16_t pad:14;
};

typedef struct cp2btag_s cp2btag_t;

struct cp4btag_s
{
  uint16_t c:4;
  uint16_t pad:12;
};

typedef struct cp4btag_s cp4btag_t;

struct cp6btag_s
{
  uint16_t c:5;
  uint16_t pw:1;
  uint16_t pad:10;
};

typedef struct cp6btag_s cp6btag_t;

struct cp8btag_s
{
  uint16_t c:5;
  uint16_t mpw:1;
  uint16_t m:2;
  uint16_t pad:8;
};

typedef struct cp8btag_s cp8btag_t;

struct cp16btag_s
{
  uint16_t c:5;
  uint16_t mb:3;
  uint16_t mg:3;
  uint16_t mr:3;
  uint16_t pad:1;
  uint16_t pw:1;
};

typedef struct cp16btag_s cp16btag_t;

struct up8btag_s
{
  uint16_t b:2;
  uint16_t g:3;
  uint16_t r:3;
  uint16_t pad:8;
};

typedef struct up8btag_s up8btag_t;

struct up16btag_s
{
  uint16_t bw:1;
  uint16_t b:4;
  uint16_t g:5;
  uint16_t r:5;
  uint16_t p:1;
};

typedef struct up16btag_s up16btag_t;

struct res16btag_s
{
  uint16_t b:5;
  uint16_t g:5;
  uint16_t r:5;
  uint16_t p:1;
};

typedef struct res16btag_s res16btag_t;

union pdeco_u
{
  uint32_t    raw;
  cp1btag_t   c1b;
  cp2btag_t   c2b;
  cp4btag_t   c4b;
  cp6btag_t   c6b;
  cp8btag_t   c8b;
  cp16btag_t  c16b;
  up8btag_t   u8b;
  up16btag_t  u16b;
  res16btag_t r16b;
};

typedef union pdeco_u pdeco_t;

struct avtag_s
{
  uint8_t NEG:1;
  uint8_t XTEND:1;
  uint8_t nCLIP:1;
  uint8_t dv3:2;
  uint8_t pad:3;
};

typedef struct avtag_s avtag_t;

union AVS_u
{
  avtag_t  avsignal;
  uint32_t raw;
};

typedef union AVS_u AVS_t;

struct pixctag_s
{
  uint8_t dv2:1;
  uint8_t av:5;
  uint8_t s2:2;
  uint8_t dv1:2;
  uint8_t mxf:3;
  uint8_t ms:2;
  uint8_t s1:1;
};

typedef struct pixctag_s pixctag_t;

union PXC_u
{
  pixctag_t meaning;
  uint32_t  raw;
};

typedef union PXC_u PXC_t;

#define MADAM_REGISTER_COUNT   2048
#define MADAM_PLUT_COUNT       32

#pragma pack(pop)

struct madam_s
{
  uint32_t mregs[MADAM_REGISTER_COUNT+64];
  uint16_t PLUT[MADAM_PLUT_COUNT];
  int32_t  rmod;
  int32_t  wmod;
  int32_t  clipx;
  int32_t  clipy;
  uint32_t FSM;
};

typedef struct madam_s madam_t;

#define ME_MODE_HARDWARE 0
#define ME_MODE_SOFTWARE 1
#define MADAM_ID_RED_HARDWARE   0x01000000
#define MADAM_ID_GREEN_HARDWARE 0x01020000
#define MADAM_ID_GREEN_SOFTWARE 0x01020001

static madam_t MADAM;
static int KPRINT  = 0;
static int ME_MODE = ME_MODE_HARDWARE;

void
opera_madam_kprint_enable(void)
{
  KPRINT = 1;
}

void
opera_madam_kprint_disable(void)
{
  KPRINT = 0;
}

void
opera_madam_me_mode_hardware(void)
{
  ME_MODE = ME_MODE_HARDWARE;
}

void
opera_madam_me_mode_software(void)
{
  ME_MODE = ME_MODE_SOFTWARE;
}

uint32_t
opera_madam_fsm_get(void)
{
  return MADAM.FSM;
}

void
opera_madam_fsm_set(uint32_t val_)
{
  MADAM.FSM = val_;
}

uint32_t
opera_madam_state_size(void)
{
  return opera_state_save_size(sizeof(madam_t));
}

uint32_t
opera_madam_state_save(void *buf_)
{
  return opera_state_save(buf_,"MDAM",&MADAM,sizeof(MADAM));
}

uint32_t
opera_madam_state_load(const void *buf_)
{
  return opera_state_load(&MADAM,"MDAM",buf_,sizeof(MADAM));
}

#define PACKED   true
#define UNPACKED false

static bool    TestInitVisual(bool const packed);
static bool    InitLineMap(void);
static void    InitScaleMap(void);
static void    InitArbitraryMap(void);
static void    TexelDrawLine(uint16_t CURPIX, uint16_t LAMV, int32_t xcur, int32_t ycur, int32_t cnt);
static int32_t TexelDrawScale(uint16_t CURPIX, uint16_t LAMV, int32_t xcur, int32_t ycur, int32_t deltax, int32_t deltay);
static int32_t TexelDrawArbitrary(uint16_t CURPIX, uint16_t LAMV, int32_t xA, int32_t yA, int32_t xB, int32_t yB, int32_t xC, int32_t yC, int32_t xD, int32_t yD);
static void    DrawPackedCel(void);
static void    DrawLiteralCel(void);
static void    DrawLRCel(void);
static void    HandleDMA8(void);
static void    DMAPBus(void);

static struct
{
  uint32_t plutaCCBbits;
  uint32_t pixelBitsMask;
  int      tmask;
} pdec;

static struct
{
  uint32_t pmode;
  uint32_t pmodeORmask;
  uint32_t pmodeANDmask;
  int      Transparent;
} pproj;

static uint32_t  CCBFLAGS;
static uint32_t  PIXC;
static uint32_t  PRE0;
static uint32_t  PRE1;
static int32_t   SPRWI;
static int32_t   SPRHI;
static uint32_t  PXOR1;
static uint32_t  PXOR2;

static uint32_t const BPP[8] = {1,1,2,4,6,8,16,1};

static uint8_t PSCALAR[8][4][32];

static uint16_t MAPu8b[256+64];
static uint16_t MAPc8bAMV[256+64];
static uint16_t MAPc16bAMV[8*8*8+64];

/*
  See section "Spryte Rendering Process" of patent
  WO09410642A1 - Method For Controlling A Spryte Rendering Processor
*/

// CelEngine STATBits
#define STATBITS	MADAM.mregs[0x28]

#define SPRON		0x10
#define SPRPAU		0x20

// CelEngine Registers
#define SPRSTRT		0x100
#define SPRSTOP		0x104
#define SPRCNTU		0x108
#define SPRPAUS		0x10c

#define CCBCTL0		MADAM.mregs[0x110]
#define REGCTL0		MADAM.mregs[0x130]
#define REGCTL1		MADAM.mregs[0x134]
#define REGCTL2		MADAM.mregs[0x138]
#define REGCTL3		MADAM.mregs[0x13c]
#define XYPOSL          MADAM.mregs[0x140]
#define XYPOSH          MADAM.mregs[0x144]
#define DXYL            MADAM.mregs[0x148]
#define DXYH            MADAM.mregs[0x14c]
#define LINEDXYL        MADAM.mregs[0x150]
#define LINEDXYH        MADAM.mregs[0x154]
#define DDXYL           MADAM.mregs[0x158]
#define DDXYH           MADAM.mregs[0x15c]

#define CURRENTCCB	MADAM.mregs[0x5a0]
#define NEXTCCB		MADAM.mregs[0x5a4]
#define PLUTDATA	MADAM.mregs[0x5a8]
#define PDATA		MADAM.mregs[0x5ac]
#define ENGAFETCH	MADAM.mregs[0x5b0]
#define ENGALEN		MADAM.mregs[0x5b4]
#define	ENGBFETCH	MADAM.mregs[0x5b8]
#define ENGBLEN		MADAM.mregs[0x5bc]

static
INLINE
const
int32_t
PDV(const int32_t x_)
{
  return (((x_ - 1) & 3) + 1);
}

static
INLINE
const
int32_t
XY2OFF(const int32_t x_,
       const int32_t y_,
       const int32_t m_)
{
  return (((y_ >> 1) * m_) + ((y_ & 1) << 1) + (x_ << 2));
}

static
INLINE
const
bool
clipping(const int32_t x_,
         const int32_t y_)
{
  return (((uint32_t)x_ > MADAM.clipx) ||
          ((uint32_t)y_ > MADAM.clipy));
}

uint32_t
opera_madam_peek(uint32_t addr_)
{
  /* we need to return actual fifo status */
  if((addr_ >= 0x400) && (addr_ <= 0x53F))
    return opera_clio_fifo_read(addr_);

  /* status of CEL */
  if(addr_ == 0x28)
    {
      switch(MADAM.FSM)
        {
        case FSM_IDLE:
          return 0x00;
        case FSM_SUSPENDED:
          return 0x30;
        case FSM_INPROCESS:
          return 0x10;
        }
    }

  return MADAM.mregs[addr_];
}

/* Matrix engine macros */
/* input */
#define MI00 ((int64_t)(int32_t)MADAM.mregs[0x600])
#define MI01 ((int64_t)(int32_t)MADAM.mregs[0x604])
#define MI02 ((int64_t)(int32_t)MADAM.mregs[0x608])
#define MI03 ((int64_t)(int32_t)MADAM.mregs[0x60C])
#define MI10 ((int64_t)(int32_t)MADAM.mregs[0x610])
#define MI11 ((int64_t)(int32_t)MADAM.mregs[0x614])
#define MI12 ((int64_t)(int32_t)MADAM.mregs[0x618])
#define MI13 ((int64_t)(int32_t)MADAM.mregs[0x61C])
#define MI20 ((int64_t)(int32_t)MADAM.mregs[0x620])
#define MI21 ((int64_t)(int32_t)MADAM.mregs[0x624])
#define MI22 ((int64_t)(int32_t)MADAM.mregs[0x628])
#define MI23 ((int64_t)(int32_t)MADAM.mregs[0x62C])
#define MI30 ((int64_t)(int32_t)MADAM.mregs[0x630])
#define MI31 ((int64_t)(int32_t)MADAM.mregs[0x634])
#define MI32 ((int64_t)(int32_t)MADAM.mregs[0x638])
#define MI33 ((int64_t)(int32_t)MADAM.mregs[0x63C])

/* output */
#define MO0 MADAM.mregs[0x660]
#define MO1 MADAM.mregs[0x664]
#define MO2 MADAM.mregs[0x668]
#define MO3 MADAM.mregs[0x66C]

/* vector */
#define MV0 ((int64_t)(int32_t)MADAM.mregs[0x640])
#define MV1 ((int64_t)(int32_t)MADAM.mregs[0x644])
#define MV2 ((int64_t)(int32_t)MADAM.mregs[0x648])
#define MV3 ((int64_t)(int32_t)MADAM.mregs[0x64C])

static int64_t tmpMO0;
static int64_t tmpMO1;
static int64_t tmpMO2;
static int64_t tmpMO3;

#define Nfrac16 (((int64_t)MADAM.mregs[0x680]<<32) |    \
                 (uint32_t)MADAM.mregs[0x684])

static
INLINE
void
madam_matrix_copy(void)
{
  MO0 = tmpMO0;
  MO1 = tmpMO1;
  MO2 = tmpMO2;
  MO3 = tmpMO3;
}

/*
  multiply a 3x3 matrix of 16.16 values by a vector of 16.16 values
*/
static
INLINE
void
madam_matrix_mul3x3(void)
{
  madam_matrix_copy();

  tmpMO0 = (((MI00 * MV0) +
             (MI01 * MV1) +
             (MI02 * MV2)) >> 16);
  tmpMO1 = (((MI10 * MV0) +
             (MI11 * MV1) +
             (MI12 * MV2)) >> 16);
  tmpMO2 = (((MI20 * MV0) +
             (MI21 * MV1) +
             (MI22 * MV2)) >> 16);
}

/*
  multiply a 3x3 matrix of 16.16 values by multiple
  vectors, then multiply x and y by n/z

  return the result vectors {x*n/z, y*n/z, z}
*/
static
INLINE
void
madam_matrix_mul3x3_nz(void)
{
  int64_t M;

  madam_matrix_copy();

  M = Nfrac16;

  tmpMO2 = (((MI20 * MV0) +
             (MI21 * MV1) +
             (MI22 * MV2)) >> 16);

  if(tmpMO2 != 0)
    M /= (int64_t)tmpMO2;

  tmpMO0 = (((MI00 * MV0) +
             (MI01 * MV1) +
             (MI02 * MV2)) >> 16);
  tmpMO1 = (((MI10 * MV0) +
             (MI11 * MV1) +
             (MI12 * MV2)) >> 16);

  tmpMO0 = ((tmpMO0 * M) >> 32);
  tmpMO1 = ((tmpMO1 * M) >> 32);
}

/*
  multiply a 4x4 matrix of 16.16 values by a vector of 16.16 values
*/
static
INLINE
void
madam_matrix_mul4x4(void)
{
  madam_matrix_copy();

  tmpMO0 = (((MI00 * MV0) +
             (MI01 * MV1) +
             (MI02 * MV2) +
             (MI03 * MV3)) >> 16);
  tmpMO1 = (((MI10 * MV0) +
             (MI11 * MV1) +
             (MI12 * MV2) +
             (MI13 * MV3)) >> 16);
  tmpMO2 = (((MI20 * MV0) +
             (MI21 * MV1) +
             (MI22 * MV2) +
             (MI23 * MV3)) >> 16);
  tmpMO3 = (((MI30 * MV0) +
             (MI31 * MV1) +
             (MI32 * MV2) +
             (MI33 * MV3)) >> 16);
}

void
opera_madam_poke(uint32_t addr_,
                 uint32_t val_)
{
  if((addr_ >= 0x400) && (addr_ <= 0x53F))
    {
      opera_clio_fifo_write(addr_,val_);
      return;
    }

  switch(addr_)
    {
    case 0x00:
      if(KPRINT)
        fputc(val_,stderr);
      return;
    case 0x04:
      /* readonly */
      break;
    case 0x08:
      MADAM.mregs[0x08] = val_;
      HandleDMA8();
      break;
    case 0x580:
      opera_vdlp_set_vdl_head(val_);
      return;
    case SPRSTRT:
      if(MADAM.FSM == FSM_IDLE)
        MADAM.FSM = FSM_INPROCESS;
      return;
    case SPRSTOP:
      MADAM.FSM = FSM_IDLE;
      NEXTCCB = 0;
      return;
    case SPRCNTU:
      if(MADAM.FSM == FSM_SUSPENDED)
        MADAM.FSM = FSM_INPROCESS;
      return;
    case SPRPAUS:
      if(MADAM.FSM == FSM_INPROCESS)
        MADAM.FSM = FSM_SUSPENDED;
      return;

      /* Matrix engine */
    case 0x7FC:
      switch(val_)
        {
        case 0:
          madam_matrix_copy();
          return;
        case 1:
          madam_matrix_mul4x4();
          return;
        case 2:
          madam_matrix_mul3x3();
          return;
        case 3:
          madam_matrix_mul3x3_nz();
          return;
        default:
          return;
        }
      break;

      /* REGCTL0 */
    case 0x130:
      MADAM.mregs[0x130] = val_;
      MADAM.rmod = (((val_ & 0x01) << 7) +
                    ((val_ & 0x0C) << 8) +
                    ((val_ & 0x70) << 4));
      val_ >>= 8;
      MADAM.wmod = (((val_ & 0x01) << 7) +
                    ((val_ & 0x0C) << 8) +
                    ((val_ & 0x70) << 4));
      break;

      /* REGCTL1 */
    case 0x134:
      MADAM.mregs[0x134] = val_;
      MADAM.clipx = ((val_ >>  0) & 0x3FF);
      MADAM.clipy = ((val_ >> 16) & 0x3FF);
      break;

    default:
      MADAM.mregs[addr_] = val_;
      break;
    }
}

enum texel_draw_func_e
  {
    TEXEL_DRAW_FUNC_LINE,
    TEXEL_DRAW_FUNC_SCALE,
    TEXEL_DRAW_FUNC_ARBITRARY
  };
typedef enum texel_draw_func_e texel_draw_func_e;

static int32_t  HDDX1616;
static int32_t  HDDY1616;
static int32_t  HDX1616;
static int32_t  HDY1616;
static int32_t  VDX1616;
static int32_t  VDY1616;
static int32_t  XPOS1616;
static int32_t  YPOS1616;
static uint32_t CEL_ORIGIN_VH_VALUE;
static texel_draw_func_e TEXEL_DRAW_FUNC;
static int32_t  TEXTURE_WI_START;
static int32_t  TEXTURE_HI_START;
static int32_t  TEXEL_INCX;
static int32_t  TEXEL_INCY;
static int32_t  TEXTURE_WI_LIM;
static int32_t  TEXTURE_HI_LIM;

static
void
LoadPLUT(uint32_t pnt_,
         int32_t  n_)
{
  int i;

  for(i = 0; i < n_; i++)
    MADAM.PLUT[i] = opera_mem_read16((((pnt_ >> 1) + i)) << 1);
}

void
opera_madam_cel_handle(void)
{
  CCBFLAGS  = 0;
  set_flag(STATBITS,SPRON);

  /*
    When the CPU writes to the SPRSTRT address, after
    the spryte engine gains control of the system data bus
    118, 120, the DMA engine of Figs. 3 and 4 loads in the
    first six words from the first SCoB beginning from the
    system memory address specified in the NEXT SCOB
    register in the DMA stack 312.
  */
  while(NEXTCCB && flag_is_clr(CCBFLAGS,CCB_LAST))
    {
      /*
        To accomplish this, the address of the first word to load is
        read out of the NEXT SCOB register and provided to the memory
        address lines via source multiplexer 314. The address is also
        incremented by adder/clipper 320 and written back via
        multiplexer 310 into the CURRENT SCOB register in DMA stack
        312. All six words are read from memory in this manner, the
        CURRENT SCOB register maintaining the address of each next
        word to load.
      */
      CURRENTCCB = NEXTCCB;
      if(CURRENTCCB >= RAM_SIZE)
        break;

      /*
        The first SCoB word read, FLAGS, is written into a 32-bit
        hardware register in address manipulator chip 106.
      */
      CCBFLAGS = opera_mem_read32(CURRENTCCB);
      CURRENTCCB += 4;

      /*
        The next SCoB word read, NEXTPTR, is written into the NEXT
        SCOB register in DMA stack 312.
      */
      NEXTCCB = opera_mem_read32(CURRENTCCB) & MASK_24BIT;
      if(NEXTCCB && flag_is_clr(CCBFLAGS,CCB_NPABS))
        NEXTCCB += CURRENTCCB + 4;
      CURRENTCCB += 4;

      /*
        SOURCEPTR is written into the SPRYTE DATA ADDRESS register of
        DMA stack 312,
      */
      PDATA = opera_mem_read32(CURRENTCCB) & MASK_24BIT;
      if(flag_is_clr(CCBFLAGS,CCB_SPABS))
        PDATA += CURRENTCCB + 4;
      CURRENTCCB += 4;

      /*
        and PIPPTR is written into the PIP ADDRESS register in DMA
        stack 312.
      */
      PLUTDATA = opera_mem_read32(CURRENTCCB) & MASK_24BIT;
      if(flag_is_clr(CCBFLAGS,CCB_PPABS))
        PLUTDATA += CURRENTCCB + 4;
      CURRENTCCB += 4;

      /*
        XPOS and YPOS are written to two memory mapped hardware
        registers XYPOSH and XYPOSL, each having the format of
        x,y. That is, the high-order 16 bits from XPOS and the
        high-order 16 bits from YPOS are written to the high- and
        low-order half words, respectively, of XYPOSH, and the
        low-order 16 bits of XPOS and the low-order 16 bits of YPOS
        are written to the high and low half words, respectively, of
        XYPOSL.

        Note that if the SKIP bit of the FLAGS word equals one,
        indicating that the present SCoB is to he skipped, or if the
        YOXY bit is zero, then the X and Y values are not written to
        the hardware.
      */
      if(flag_is_clr(CCBFLAGS,CCB_SKIP) && flag_is_set(CCBFLAGS,CCB_YOXY))
        {
          XPOS1616 = opera_mem_read32(CURRENTCCB + 0);
          YPOS1616 = opera_mem_read32(CURRENTCCB + 4);
        }

      CURRENTCCB += 8;

      if(flag_is_set(CCBFLAGS,CCB_PXOR))
        {
          PXOR1 = 0;
          PXOR2 = 0x1F1F1F1F;
        }
      else
        {
          PXOR1 = 0xFFFFFFFF;
          PXOR2 = 0;
        }

      /*
        Get the VH value for this cel. This is done in case the
        cel later decides to use the position as the source of
        its VH values in the projector.
      */
      CEL_ORIGIN_VH_VALUE = ((XPOS1616 & 0x1) | ((YPOS1616 & 0x1) << 15));

      if(flag_is_set(CCBFLAGS,CCB_LDSIZE))
        {
          HDX1616     = ((int32_t)opera_mem_read32(CURRENTCCB)) >> 4;
          CURRENTCCB += 4;
          HDY1616     = ((int32_t)opera_mem_read32(CURRENTCCB)) >> 4;
          CURRENTCCB += 4;
          VDX1616     = opera_mem_read32(CURRENTCCB);
          CURRENTCCB += 4;
          VDY1616     = opera_mem_read32(CURRENTCCB);
          CURRENTCCB += 4;
        }

      if(flag_is_set(CCBFLAGS,CCB_LDPRS))
        {
          HDDX1616    = ((int32_t)opera_mem_read32(CURRENTCCB)) >> 4;
          CURRENTCCB += 4;
          HDDY1616    = ((int32_t)opera_mem_read32(CURRENTCCB)) >> 4;
          CURRENTCCB += 4;
        }

      if(flag_is_set(CCBFLAGS,CCB_LDPPMP))
        {
          PIXC        = opera_mem_read32(CURRENTCCB);
          CURRENTCCB += 4;
        }

      if(flag_is_set(CCBFLAGS,CCB_CCBPRE))
        {
          PRE0        = opera_mem_read32(CURRENTCCB);
          CURRENTCCB += 4;
          if(flag_is_clr(CCBFLAGS,CCB_PACKED))
            {
              PRE1        = opera_mem_read32(CURRENTCCB);
              CURRENTCCB += 4;
            }
        }
      else
        {
          PRE0   = opera_mem_read32(PDATA);
          PDATA += 4;
          if(flag_is_clr(CCBFLAGS,CCB_PACKED))
            {
              PRE1   = opera_mem_read32(PDATA);
              PDATA += 4;
            }
        }

      /* PDEC data compute */
      {
        /* pdec.mode = PRE0 & PRE0_BPP_MASK; */
        switch(PRE0 & PRE0_BPP_MASK)
          {
          case 0:
          case 7:
            continue;
          case 1:
            pdec.plutaCCBbits  = ((CCBFLAGS & 0x0F) * 4);
            pdec.pixelBitsMask = 1; /* 1 bit */
            break;
          case 2:
            pdec.plutaCCBbits  = ((CCBFLAGS & 0x0E) * 4);
            pdec.pixelBitsMask = 3; /* 2 bit */
            break;
          case 3:
          default:
            pdec.plutaCCBbits  = ((CCBFLAGS & 0x08) * 4);
            pdec.pixelBitsMask = 15; /* 4 bit */
            break;
          }

        pdec.tmask = flag_is_clr(CCBFLAGS,CCB_BGND);

        pproj.pmode        = (CCBFLAGS & CCB_POVER_MASK);
        pproj.pmodeORmask  = ((pproj.pmode == PMODE_ONE ) ? 0x8000 : 0x0000);
        pproj.pmodeANDmask = ((pproj.pmode != PMODE_ZERO) ? 0xFFFF : 0x7FFF);
      }

      if(flag_is_set(CCBFLAGS,CCB_LDPLUT))
        {
          switch(PRE0 & PRE0_BPP_MASK)
            {
            case 1:
              LoadPLUT(PLUTDATA,2);
              break;
            case 2:
              LoadPLUT(PLUTDATA,4);
              break;
            case 3:
              LoadPLUT(PLUTDATA,16);
              break;
            default:
              LoadPLUT(PLUTDATA,32);
            };
        }

      if(flag_is_set(CCBFLAGS,CCB_SKIP))
        continue;

      if(flag_is_set(CCBFLAGS,CCB_PACKED))
        DrawPackedCel();
      else if(flag_is_set(PRE1,PRE1_LRFORM) && (BPP[PRE0 & PRE0_BPP_MASK] == 16))
        DrawLRCel();
      else
        DrawLiteralCel();
    }

  clr_flag(STATBITS,SPRON);
  MADAM.FSM = FSM_IDLE;
}

static
void
HandleDMA8(void)
{
  /* pbus transfer */
  if(MADAM.mregs[0x8] & 0x8000)
    {
      DMAPBus();
      MADAM.mregs[0x8] &= ~0x8000; /* dma done */
      opera_clio_fiq_generate(0,1);
    }
}

static
void
DMAPBus(void)
{
  uint32_t *pbus_buf;
  int32_t   pbus_size;

  if((int32_t)MADAM.mregs[0x574] < 0)
    return;

  opera_pbus_pad();

  MADAM.mregs[0x574] -= 4;
  MADAM.mregs[0x570] += 4;
  MADAM.mregs[0x578] += 4;

  pbus_buf  = opera_pbus_buf();
  pbus_size = opera_pbus_size();
  while(((int32_t)MADAM.mregs[0x574] > 0) && (pbus_size > 0))
    {
      opera_io_write(MADAM.mregs[0x570],
                     swap32_if_little_endian(*pbus_buf));
      pbus_buf++;
      pbus_size          -= 4;
      MADAM.mregs[0x574] -= 4;
      MADAM.mregs[0x570] += 4;
      MADAM.mregs[0x578] += 4;
    }

  while((int32_t)MADAM.mregs[0x574] > 0)
    {
      opera_io_write(MADAM.mregs[0x570],0xFFFFFFFF);
      MADAM.mregs[0x574] -= 4;
      MADAM.mregs[0x570] += 4;
      MADAM.mregs[0x578] += 4;
    }

  MADAM.mregs[0x574] = 0xFFFFFFFC;
}

void
opera_madam_init()
{
  int32_t i;
  int32_t j;
  int32_t n;

  opera_madam_reset();

  bitoper.bitset = 1;

  MADAM.FSM = FSM_IDLE;

  MADAM.mregs[0] = ((ME_MODE == ME_MODE_HARDWARE) ?
                    MADAM_ID_GREEN_HARDWARE :
                    MADAM_ID_GREEN_SOFTWARE);

  /* DRAM dux init */
  MADAM.mregs[0x4]   = opera_mem_madam_red_sysbits(0x20);
  MADAM.mregs[0x574] = 0xFFFFFFFC;

  for(i = 0; i < 32; i++)
    for(j = 0; j < 8; j++)
      for(n = 0; n < 4; n++)
        PSCALAR[j][n][i] = ((i * (j + 1)) >> PDV(n));

  for(i = 0; i < 256; i++)
    {
      pdeco_t  pix1;
      pdeco_t  pix2;
      uint16_t pres;
      uint16_t resamv;

      pix1.raw     = i;
      pix2.r16b.b  = (pix1.u8b.b << 3) + (pix1.u8b.b << 1) + (pix1.u8b.b >> 1);
      pix2.r16b.g  = (pix1.u8b.g << 2) + (pix1.u8b.g >> 1);
      pix2.r16b.r  = (pix1.u8b.r << 2) + (pix1.u8b.r >> 1);
      pres         = pix2.raw;
      pres        &= 0x7fff;
      MAPu8b[i]    = pres;

      resamv       = ((pix1.c8b.m << 1) + pix1.c8b.mpw);
      resamv       = ((resamv << 6) + (resamv << 3) + resamv);
      MAPc8bAMV[i] = resamv;
    }

  for(i = 0; i < (8*8*8); i++)
    {
      pdeco_t pix1;

      pix1.raw = i << 5;
      MAPc16bAMV[i] = ((pix1.c16b.mr << 6) + (pix1.c16b.mg << 3) + pix1.c16b.mb);
    }
}

static
uint32_t
PDEC(const uint32_t  pixel_,
     uint16_t       *amv_)
{
  pdeco_t pix1;
  uint16_t resamv;
  uint16_t pres;

  pix1.raw = pixel_;

  switch(PRE0 & PRE0_BPP_MASK)
    {
    default:
    case 1: /* 1 bit  */
    case 2: /* 2 bits */
    case 3: /* 4 bits */
      pres   = MADAM.PLUT[(pdec.plutaCCBbits + ((pix1.raw & pdec.pixelBitsMask) * 2)) >> 1];
      resamv = 0x49;
      break;

    case 4:   /* 6 bits */
      /* pmode = pix1.c6b.pw; ??? */
      pres   = MADAM.PLUT[pix1.c6b.c];
      pres   = (pres & 0x7FFF) + (pix1.c6b.pw << 15);
      resamv = 0x49;
      break;

    case 5:   /* 8 bits */
      if(flag_is_set(PRE0,PRE0_LINEAR))
        {
          pres   = MAPu8b[pix1.raw & 0xFF];
          resamv = 0x49;
        }
      else
        {
          pres   = MADAM.PLUT[pix1.c8b.c];
          resamv = MAPc8bAMV[pix1.raw & 0xFF];
        }
      break;

    case 6:  /* 16 bits */
    case 7:
      if(flag_is_set(PRE0,PRE0_LINEAR))
        {
          pres   = pix1.raw;
          resamv = 0x49;
        }
      else
        {
          pres   = MADAM.PLUT[pix1.c16b.c];
          pres   = ((pres & 0x7FFF) | (pixel_ & 0x8000));
          resamv = MAPc16bAMV[(pix1.raw >> 5) & 0x1FF];
        }

      break;
    }

  *amv_ = resamv;

  /* conceptual end of decoder */

  /*
    TODO: Do PROJECTOR functions now?
    They'll be done before using the PROCESSOR.

    if(!(PRE1&PRE1_NOSWAP) && (CCBCTL0&(1<<27)))
    pres = (pres&0x7ffe)|((pres&0x8000)>>15)|((pres&1)<<15);

    if(!(CCBCTL0 & 0x80000000))
    pres = (pres&0x7fff)|((CCBCTL0>>15)&0x8000);

    pres=(pres|pdec.pmodeORmask)&pdec.pmodeANDmask;
  */

  pproj.Transparent = (((pres & 0x7FFF) == 0x0) & pdec.tmask);

  return pres;
}

static
uint32_t
PPROJ_OUTPUT(uint32_t pdec_output_,
             uint32_t pproc_output_,
             uint32_t pframe_input_)
{
  int32_t  b15mode;
  int32_t  b0mode;
  uint32_t VHOutput;

  /*
    CCB_PLUTPOS flag
    Determine projector's originating source of VH values.
  */

  if(flag_is_set(CCBFLAGS,CCB_PLUTPOS)) /* Use pixel decoder output. */
    VHOutput = (pdec_output_ & 0x8001);
  else /* Use VH values determined from the CEL's origin. */
    VHOutput = CEL_ORIGIN_VH_VALUE;

  /*
    SWAPHV flag
    Swap the H and V values now if requested.
  */
  if(flag_is_set(CCBCTL0,SWAPHV))
    {
      /* TODO: I have read that PRE1 is only set for unpacked CELs.
         So... should this be ignored if using packed CELs? I don't
         know. */
      if(flag_is_clr(PRE1,PRE1_NOSWAP))
        VHOutput = ((VHOutput >> 15) | ((VHOutput & 1) << 15));
    }

#if 0
  /*
    CFBDSUB flag
    Substitute the VH values from the frame buffer if requested.
  */
  if(flag_is_set(CCBCTL0,CFBDSUB))
    {
      /* TODO: This should be re-enabled sometime. However, it currently
         causes the wing commander 3 movies to screw up again! There
         must be some missing mbehavior elsewhere. */
      VHOutput = (pframe_input & 0x8001);
    }
#endif

  /*
    B15POS_MASK settings
    Substitute the V value explicitly if requested.
  */
  b15mode = (CCBCTL0 & B15POS_MASK);
  switch(b15mode)
    {
    case B15POS_PDC:
    default:
      /* do nothing */
      break;
    case B15POS_0:
      VHOutput = (VHOutput & ~0x8000);
      break;
    case B15POS_1:
      VHOutput |= 0x8000;
      break;
    }

  /*
    B15POS_MASK settings
    Substitute the H value explicitly if requested.
  */
  b0mode = (CCBCTL0 & B0POS_MASK);
  switch(b0mode)
    {
    case B0POS_PDC:
      /* do nothing  */
      break;
    case B0POS_PPMP:
      /* Use LSB from pixel processor output. */
      VHOutput = ((VHOutput & ~0x1) | (pproc_output_ & 0x1));
      break;
    case B0POS_0:
      VHOutput = (VHOutput & ~0x1);
      break;
    case B0POS_1:
      VHOutput |= 0x01;
      break;
    }

  return ((pproc_output_ & 0x7FFE) | VHOutput);
}

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(x, a, b) (MIN(b,MAX(x,a)))
#endif

static
FORCEINLINE
int8_t
clamp_i8(const int8_t x_,
         const int8_t min_,
         const int8_t max_)
{
  return CLAMP(x_,min_,max_);
}

static
uint32_t
PPROC(uint32_t pixel_,
      uint32_t fpix_,
      uint32_t amv_)
{
  AVS_t AV;
  PXC_t pixc;

  pdeco_t input1;
  pdeco_t out;
  pdeco_t pix1;

#pragma pack(push,1)
  union
  {
    uint32_t raw;
    struct
    {
      int8_t R;
      int8_t B;
      int8_t G;
      int8_t a;
    };
  } color1, color2, AOP, BOP;
#pragma pack(pop)

  /*
    Set PMODE according to the values set up in the CCBFLAGS word.
    (This merely uses masks here because it's faster).
    This is a duty of the PROJECTOR, but we'll do it here because its
    easier.
  */
  pixel_ = ((pixel_ | pproj.pmodeORmask) & pproj.pmodeANDmask);

  pixc.raw = (PIXC & 0xFFFF);
  if(pixel_ & 0x8000)
    pixc.raw = (PIXC >> 16);

  /*
    now let's select the sources
    1. av
    2. input1
    3. input2
    pixc.raw = 0;
  */

  if(flag_is_set(CCBFLAGS,CCB_USEAV))
    {
      AV.raw = pixc.meaning.av;
    }
  else
    {
      AV.avsignal.dv3   = 0;
      AV.avsignal.nCLIP = 0;
      AV.avsignal.XTEND = 0;
      AV.avsignal.NEG   = 0;
    }

  if(!pixc.meaning.s1)
    input1.raw = pixel_;
  else
    input1.raw = fpix_;

  switch(pixc.meaning.s2)
    {
    case 0:
      color2.raw = 0;
      break;
    case 1:
      color2.R = color2.G = color2.B = (pixc.meaning.av >> AV.avsignal.dv3);
      break;
    case 2:
      pix1.raw = fpix_;
      color2.R = ((pix1.r16b.r) >> AV.avsignal.dv3);
      color2.G = ((pix1.r16b.g) >> AV.avsignal.dv3);
      color2.B = ((pix1.r16b.b) >> AV.avsignal.dv3);
      break;
    case 3:
      pix1.raw = pixel_;
      color2.R = (pix1.r16b.r >> AV.avsignal.dv3);
      color2.G = (pix1.r16b.g >> AV.avsignal.dv3);
      color2.B = (pix1.r16b.b >> AV.avsignal.dv3);
      break;
    }

  switch(pixc.meaning.ms)
    {
    case 0:
      color1.R = PSCALAR[pixc.meaning.mxf][pixc.meaning.dv1][input1.r16b.r];
      color1.G = PSCALAR[pixc.meaning.mxf][pixc.meaning.dv1][input1.r16b.g];
      color1.B = PSCALAR[pixc.meaning.mxf][pixc.meaning.dv1][input1.r16b.b];
      break;
    case 1:
      color1.R = PSCALAR[(amv_ >> 6) & 7][pixc.meaning.dv1][input1.r16b.r];
      color1.G = PSCALAR[(amv_ >> 3) & 7][pixc.meaning.dv1][input1.r16b.g];
      color1.B = PSCALAR[(amv_ >> 0) & 7][pixc.meaning.dv1][input1.r16b.b];
      break;
    case 2:
      pix1.raw = pixel_;
      color1.R = PSCALAR[pix1.r16b.r >> 2][pix1.r16b.r & 3][input1.r16b.r];
      color1.G = PSCALAR[pix1.r16b.g >> 2][pix1.r16b.g & 3][input1.r16b.g];
      color1.B = PSCALAR[pix1.r16b.b >> 2][pix1.r16b.b & 3][input1.r16b.b];
      break;
    case 3:
      color1.R = PSCALAR[4][pixc.meaning.dv1][input1.r16b.r];
      color1.G = PSCALAR[4][pixc.meaning.dv1][input1.r16b.g];
      color1.B = PSCALAR[4][pixc.meaning.dv1][input1.r16b.b];
      break;
    }

  /* Use this to render magenta for testing. */
#if 0
  {
    pdeco_t magenta;

    magenta.r16b.r = 0x1b;
    magenta.r16b.g = 0x00;
    magenta.r16b.b = 0x1b;
    magenta.r16b.p = 1;

    return magenta.raw;
  }
#endif

  /*
    ok -- we got the sources -- now RGB processing
  */

  /* AOP/BOP calculation */
  AOP.raw     = (color1.raw & PXOR1);
  color1.raw &= PXOR2;

  if(AV.avsignal.NEG)
    BOP.raw = (color2.raw ^ 0x00FFFFFF);
  else
    BOP.raw = (color2.raw ^ color1.raw);

  if(AV.avsignal.XTEND)
    {
      BOP.R = ((BOP.R << 3) >> 3);
      BOP.B = ((BOP.B << 3) >> 3);
      BOP.G = ((BOP.G << 3) >> 3);
    }

  color2.R = ((AOP.R + BOP.R + AV.avsignal.NEG) >> pixc.meaning.dv2);
  color2.G = ((AOP.G + BOP.G + AV.avsignal.NEG) >> pixc.meaning.dv2);
  color2.B = ((AOP.B + BOP.B + AV.avsignal.NEG) >> pixc.meaning.dv2);

  if(!AV.avsignal.nCLIP)
    {
      color2.R = clamp_i8(color2.R,0,31);
      color2.G = clamp_i8(color2.G,0,31);
      color2.B = clamp_i8(color2.B,0,31);
    }

  out.raw    = 0;
  out.r16b.r = color2.R;
  out.r16b.g = color2.G;
  out.r16b.b = color2.B;

  /* TODO: Is this something the PROJECTOR should do? */
  if(flag_is_clr(CCBFLAGS,CCB_NOBLK) && (out.raw == 0))
    out.raw = (1 << 10);

  /*
    if(!(PRE1 & PRE1_NOSWAP) && (CCBCTL0 & (1 << 27)))
    out.raw = ((out.raw & 0x7FFE) |
    ((out.raw & 0x8000) >> 15) |
    ((out.raw & 1) << 15));
  */

  /*
    if(!(CCBCTL0 & 0x80000000))
    out.raw = ((out.raw & 0x7FFF) | ((CCBCTL0 >> 15) & 0x8000));
  */

  return out.raw;
}


static
INLINE
void
process_pixel(int32_t x_,
              int32_t y_,
              uint32_t curpix_,
              uint32_t lawv_)
{
  int32_t p;
  int32_t fp;

  fp = opera_mem_read16(REGCTL2 + XY2OFF(x_,y_,MADAM.rmod));
  p  = PPROC(curpix_,fp,lawv_);
  p  = PPROJ_OUTPUT(curpix_,p,fp);
  opera_mem_write16(REGCTL3 + XY2OFF(x_,y_,MADAM.wmod),p);
}

uint32_t*
opera_madam_registers(void)
{
  return MADAM.mregs;
}

static
void
DrawPackedCel(void)
{
  int row;
  bool eor;
  uint16_t CURPIX;
  uint16_t LAMV;
  int32_t lastaddr;
  int32_t xcur;
  int32_t ycur;
  int32_t xvert;
  int32_t yvert;
  int32_t xdown;
  int32_t ydown;
  int32_t hdx;
  int32_t hdy;
  uint32_t bpp;
  uint32_t type;
  uint32_t pdata;
  uint32_t nrows;
  uint32_t offset;
  uint32_t offsetl;

  pdata   = PDATA;
  nrows   = ((PRE0 & PRE0_VCNT_MASK) >> PRE0_VCNT_SHIFT);
  bpp     = BPP[PRE0 & PRE0_BPP_MASK];
  offsetl = ((bpp < 8) ? 1 : 2);
  SPRHI   = nrows + 1;

  if(TestInitVisual(PACKED))
    return;

  xvert = XPOS1616;
  yvert = YPOS1616;

  switch(TEXEL_DRAW_FUNC)
    {
    case TEXEL_DRAW_FUNC_LINE:
      {
        uint32_t pixel_count;
        for(row = 0; row < TEXTURE_HI_LIM; row++)
          {
            int wcnt;
            int scipw;

            BitReaderBig_AttachBuffer(&bitoper,pdata);
            offset = BitReaderBig_Read(&bitoper,(offsetl << 3));

            lastaddr  = (pdata + ((offset + 2) << 2));
            eor       = false;
            xcur      = xvert;
            ycur      = yvert;
            xvert    += VDX1616;
            yvert    += VDY1616;

            if(TEXTURE_HI_START)
              {
                TEXTURE_HI_START--;
                pdata = lastaddr;
                continue;
              }

            scipw = TEXTURE_WI_START;
            wcnt  = scipw;

            /* while not end of row */
            while(!eor)
              {
                type = BitReaderBig_Read(&bitoper,2);
                if((int32_t)(bitoper.point + pdata) >= (lastaddr))
                  type = 0;

                pixel_count = BitReaderBig_Read(&bitoper,6) + 1;

                if(scipw)
                  {
                    if(type == 0)
                      break;
                    if(scipw >= (int32_t)(pixel_count))
                      {
                        scipw -= (pixel_count);
                        if(HDX1616)
                          xcur += (HDX1616 * pixel_count);
                        if(HDY1616)
                          ycur += (HDY1616 * pixel_count);
                        if(type == 1)
                          BitReaderBig_Skip(&bitoper,bpp*pixel_count);
                        else if(type == 3)
                          BitReaderBig_Skip(&bitoper,bpp);
                        continue;
                      }
                    else
                      {
                        if(HDX1616)
                          xcur += (HDX1616 * scipw);
                        if(HDY1616)
                          ycur += (HDY1616 * scipw);
                        pixel_count -= scipw;
                        if(type == 1)
                          BitReaderBig_Skip(&bitoper,bpp*scipw);
                        scipw = 0;
                      }
                  }

                /*
                  if(wcnt >= TEXTURE_WI_LIM)
                  break;
                */
                wcnt += pixel_count;
                if(wcnt > TEXTURE_WI_LIM)
                  {
                    pixel_count -= (wcnt - TEXTURE_WI_LIM);
                    /*
                      if(pixel_count >> 31)
                      break;
                    */
                  }

                switch(type)
                  {
                  case 0: /* end of row */
                    eor = true;
                    break;
                  case 1: /* PACK_LITERAL */
                    {
                      int pix;
                      for(pix = 0; pix < pixel_count; pix++)
                        {
                          CURPIX = PDEC(BitReaderBig_Read(&bitoper,bpp),&LAMV);
                          if(!pproj.Transparent)
                            process_pixel(xcur >> 16,ycur >> 16,CURPIX,LAMV);

                          xcur += HDX1616;
                          ycur += HDY1616;
                        }
                    }
                    break;
                  case 2: /* PACK_TRANSPARENT */
                    if(HDX1616)
                      xcur += (HDX1616 * pixel_count);
                    if(HDY1616)
                      ycur += (HDY1616 * pixel_count);
                    break;
                  case 3: /* PACK_REPEAT */
                    CURPIX = PDEC(BitReaderBig_Read(&bitoper,bpp),&LAMV);

                    if(!pproj.Transparent)
                      TexelDrawLine(CURPIX,LAMV,xcur,ycur,pixel_count);

                    if(HDX1616)
                      xcur += (HDX1616 * pixel_count);
                    if(HDY1616)
                      ycur += (HDY1616 * pixel_count);
                    break;
                  }

                if(wcnt >= TEXTURE_WI_LIM)
                  break;
              }

            pdata = lastaddr;
          }
      }
      break;
    case TEXEL_DRAW_FUNC_SCALE:
      {
        int row;
        int drawHeight;

        drawHeight = VDY1616;
        if(flag_is_set(CCBFLAGS,CCB_MARIA) && (drawHeight > (1 << 16)))
          drawHeight = (1 << 16);

        for(row = 0; row < SPRHI; row++)
          {
            BitReaderBig_AttachBuffer(&bitoper,pdata);
            offset = BitReaderBig_Read(&bitoper,(offsetl << 3));

            lastaddr = (pdata + ((offset + 2) << 2));

            eor = false;

            xcur   = xvert;
            ycur   = yvert;
            xvert += VDX1616;
            yvert += VDY1616;

            /* while not end of row */
            while(!eor)
              {
                int32_t pixel_count;

                type = BitReaderBig_Read(&bitoper,2);
                if((bitoper.point + pdata) >= lastaddr)
                  type = 0;

                pixel_count = (BitReaderBig_Read(&bitoper,6) + 1);

                switch(type)
                  {
                  case 0: /* end of row */
                    eor = true;
                    break;
                  case 1: /* PACK_LITERAL */
                    while(pixel_count)
                      {
                        pixel_count--;
                        CURPIX = PDEC(BitReaderBig_Read(&bitoper,bpp),&LAMV);

                        if(!pproj.Transparent)
                          {
                            if(TexelDrawScale(CURPIX,
                                              LAMV,
                                              xcur >> 16,
                                              ycur >> 16,
                                              ((xcur + (HDX1616 + VDX1616)) >> 16),
                                              ((ycur + (HDY1616 + drawHeight)) >> 16)))
                              break;
                          }

                        xcur += HDX1616;
                        ycur += HDY1616;
                      }
                    break;
                  case 2: /* PACK_TRANSPARENT */
                    xcur  += (HDX1616 * pixel_count);
                    ycur  += (HDY1616 * pixel_count);
                    pixel_count  = 0;
                    break;
                  case 3: /* PACK_REPEAT */
                    CURPIX = PDEC(BitReaderBig_Read(&bitoper,bpp),&LAMV);
                    if(!pproj.Transparent)
                      {
                        if(TexelDrawScale(CURPIX,
                                          LAMV,
                                          xcur >> 16,
                                          ycur >> 16,
                                          ((xcur + (HDX1616 * pixel_count) + VDX1616) >> 16),
                                          ((ycur + (HDY1616 * pixel_count) + drawHeight) >> 16)))
                          break;

                      }

                    xcur += (HDX1616 * pixel_count);
                    ycur += (HDY1616 * pixel_count);
                    pixel_count = 0;
                    break;
                  }

                if(pixel_count)
                  break;
              }

            pdata = lastaddr;
          }
      }
      break;
    case TEXEL_DRAW_FUNC_ARBITRARY:
      {
        int row;
        uint32_t pixel_count;
        for(row = 0; row < SPRHI; row++)
          {
            BitReaderBig_AttachBuffer(&bitoper,pdata);
            offset = BitReaderBig_Read(&bitoper,(offsetl << 3));

            lastaddr = (pdata + ((offset + 2) << 2));

            eor = false;

            xcur = xvert;
            ycur = yvert;
            hdx  = HDX1616;
            hdy  = HDY1616;

            xvert   += VDX1616;
            yvert   += VDY1616;
            HDX1616 += HDDX1616;
            HDY1616 += HDDY1616;

            xdown = xvert;
            ydown = yvert;

            /* while not end of row */
            while(!eor)
              {
                type = BitReaderBig_Read(&bitoper,2);
                if((bitoper.point + pdata) >= lastaddr)
                  type = 0;

                pixel_count = (BitReaderBig_Read(&bitoper,6) + 1);

                switch(type)
                  {
                  case 0: /* end of row */
                    eor = true;
                    break;
                  case 1: /* PACK_LITERAL */
                    while(pixel_count)
                      {
                        CURPIX = PDEC(BitReaderBig_Read(&bitoper,bpp),&LAMV);
                        pixel_count--;

                        if(!pproj.Transparent)
                          {
                            if(TexelDrawArbitrary(CURPIX,
                                                  LAMV,
                                                  xcur,
                                                  ycur,
                                                  xcur + hdx,
                                                  ycur + hdy,
                                                  xdown + HDX1616,
                                                  ydown + HDY1616,
                                                  xdown,
                                                  ydown))
                              break;
                          }

                        xcur  += hdx;
                        ycur  += hdy;
                        xdown += HDX1616;
                        ydown += HDY1616;
                      }
                    break;
                  case 2: /* PACK_TRANSPARENT */
                    xcur  += (hdx * pixel_count);
                    ycur  += (hdy * pixel_count);
                    xdown += (HDX1616 * pixel_count);
                    ydown += (HDY1616 * pixel_count);
                    pixel_count = 0;
                    break;
                  case 3: /* PACK_REPEAT */
                    CURPIX = PDEC(BitReaderBig_Read(&bitoper,bpp),&LAMV);

                    if(!pproj.Transparent)
                      {
                        while(pixel_count)
                          {
                            pixel_count--;
                            if(TexelDrawArbitrary(CURPIX,
                                                  LAMV,
                                                  xcur,
                                                  ycur,
                                                  xcur + hdx,
                                                  ycur + hdy,
                                                  xdown + HDX1616,
                                                  ydown + HDY1616,
                                                  xdown,
                                                  ydown))
                              break;
                            xcur  += hdx;
                            ycur  += hdy;
                            xdown += HDX1616;
                            ydown += HDY1616;
                          }
                      }
                    else
                      {
                        xcur  += (hdx * pixel_count);
                        ycur  += (hdy * pixel_count);
                        xdown += (HDX1616 * pixel_count);
                        ydown += (HDY1616 * pixel_count);
                        pixel_count = 0;
                      }
                    break;
                  }

                if(pixel_count)
                  break;
              }

            pdata = lastaddr;
          }
      }
      break;
    }

  SPRWI++;

  if(flag_is_set(FIXMODE,FIX_BIT_GRAPHICS_STEP_Y))
    YPOS1616 = ycur;
  else
    XPOS1616 = xcur;
}

static
void
DrawLiteralCel(void)
{
  int32_t xcur;
  int32_t ycur;
  int32_t xvert;
  int32_t yvert;
  int32_t xdown;
  int32_t ydown;
  int32_t hdx;
  int32_t hdy;
  uint16_t CURPIX;
  uint16_t LAMV;
  uint32_t bpp;
  uint32_t offset;
  uint32_t offsetl;

  bpp     = BPP[PRE0 & PRE0_BPP_MASK];
  offsetl = ((bpp < 8) ? 1 : 2);
  offset  = ((offsetl == 1) ?
             ((PRE1 & PRE1_WOFFSET8_MASK) >> PRE1_WOFFSET8_SHIFT):
             ((PRE1 & PRE1_WOFFSET10_MASK) >> PRE1_WOFFSET10_SHIFT));

  SPRWI = (1 + (PRE1 & PRE1_TLHPCNT_MASK));
  SPRHI = (1 + ((PRE0 & PRE0_VCNT_MASK) >> PRE0_VCNT_SHIFT));

  if(TestInitVisual(UNPACKED))
    return;

  xvert = XPOS1616;
  yvert = YPOS1616;

  switch(TEXEL_DRAW_FUNC)
    {
    case TEXEL_DRAW_FUNC_LINE:
      {
        uint32_t i;

        SPRWI -= ((PRE0 >> 24) & 0xF);
        xvert += (TEXTURE_HI_START * VDX1616);
        yvert += (TEXTURE_HI_START * VDY1616);
        PDATA += (((offset + 2) << 2) * TEXTURE_HI_START);

        if(SPRWI > TEXTURE_WI_LIM)
          SPRWI = TEXTURE_WI_LIM;

        for(i = TEXTURE_HI_START; i < TEXTURE_HI_LIM; i++)
          {
            uint32_t j;

            BitReaderBig_AttachBuffer(&bitoper,PDATA);
            xcur = (xvert + TEXTURE_WI_START * HDX1616);
            ycur = (yvert + TEXTURE_WI_START * HDY1616);
            BitReaderBig_Skip(&bitoper,(bpp * (((PRE0 >> 24) & 0xF))));
            if(TEXTURE_WI_START)
              BitReaderBig_Skip(&bitoper,(bpp * TEXTURE_WI_START));

            xvert += VDX1616;
            yvert += VDY1616;

            for(j = TEXTURE_WI_START; j < SPRWI; j++)
              {
                CURPIX = PDEC(BitReaderBig_Read(&bitoper,bpp),&LAMV);

                if(!pproj.Transparent)
                  process_pixel(xcur >> 16,ycur >> 16,CURPIX,LAMV);

                xcur += HDX1616;
                ycur += HDY1616;
              }

            PDATA += ((offset+2) << 2);
          }
      }
      break;
    case TEXEL_DRAW_FUNC_SCALE:
      {
        int32_t drawHeight;
        uint32_t i;
        uint32_t j;

        SPRWI -= ((PRE0 >> 24) & 0xF);

        drawHeight = VDY1616;
        if(flag_is_set(CCBFLAGS,CCB_MARIA) && (drawHeight > (1 << 16)))
          drawHeight = (1 << 16);

        for(i = 0; i < SPRHI; i++)
          {
            BitReaderBig_AttachBuffer(&bitoper,PDATA);
            xcur   = xvert;
            ycur   = yvert;
            xvert += VDX1616;
            yvert += VDY1616;
            BitReaderBig_Skip(&bitoper,(bpp * (((PRE0 >> 24) & 0xF))));

            for(j = 0; j < SPRWI; j++)
              {
                CURPIX = PDEC(BitReaderBig_Read(&bitoper,bpp),&LAMV);

                if(!pproj.Transparent)
                  {
                    if(TexelDrawScale(CURPIX,
                                      LAMV,
                                      xcur >> 16,
                                      ycur >> 16,
                                      ((xcur + HDX1616 + VDX1616) >> 16),
                                      ((ycur + HDY1616 + drawHeight) >> 16)))
                      break;
                  }

                xcur += HDX1616;
                ycur += HDY1616;
              }

            PDATA += ((offset + 2) << 2);
          }
      }
      break;
    case TEXEL_DRAW_FUNC_ARBITRARY:
    default:
      {
        uint32_t i;
        uint32_t j;

        SPRWI -= ((PRE0 >> 24) & 0xF);
        for(i = 0; i < SPRHI; i++)
          {
            BitReaderBig_AttachBuffer(&bitoper,PDATA);

            xcur = xvert;
            ycur = yvert;
            hdx  = HDX1616;
            hdy  = HDY1616;

            xvert   += VDX1616;
            yvert   += VDY1616;
            HDX1616 += HDDX1616;
            HDY1616 += HDDY1616;

            BitReaderBig_Skip(&bitoper,(bpp * (((PRE0 >> 24) & 0xF))));

            xdown = xvert;
            ydown = yvert;

            for(j = 0; j < SPRWI; j++)
              {
                CURPIX = PDEC(BitReaderBig_Read(&bitoper,bpp),&LAMV);

                if(!pproj.Transparent)
                  {
                    if(TexelDrawArbitrary(CURPIX,
                                          LAMV,
                                          xcur,
                                          ycur,
                                          xcur + hdx,
                                          ycur + hdy,
                                          xdown + HDX1616,
                                          ydown + HDY1616,
                                          xdown,
                                          ydown))
                      break;
                  }

                xcur  += hdx;
                ycur  += hdy;
                xdown += HDX1616;
                ydown += HDY1616;
              }

            PDATA += (((offset + 2) << 2));
          }
      }
      break;
    }

  if(flag_is_set(FIXMODE,FIX_BIT_GRAPHICS_STEP_Y))
    YPOS1616 = ycur;
  else
    XPOS1616 = xcur;
}

static
void
DrawLRCel(void)
{
  int32_t i;
  int32_t j;
  int32_t xcur;
  int32_t ycur;
  int32_t xvert;
  int32_t yvert;
  int32_t xdown;
  int32_t ydown;
  int32_t hdx;
  int32_t hdy;
  uint16_t CURPIX;
  uint16_t LAMV;
  uint32_t bpp;
  uint32_t offset;
  uint32_t offsetl;

  bpp      = BPP[PRE0 & PRE0_BPP_MASK];
  offsetl  = ((bpp < 8) ? 1 : 2);
  offset   = ((offsetl == 1) ?
              ((PRE1 & PRE1_WOFFSET8_MASK)  >> PRE1_WOFFSET8_SHIFT) :
              ((PRE1 & PRE1_WOFFSET10_MASK) >> PRE1_WOFFSET10_SHIFT));
  offset  += 2;

  SPRWI = (1 + (PRE1 & PRE1_TLHPCNT_MASK));
  SPRHI = ((((PRE0 & PRE0_VCNT_MASK) >> PRE0_VCNT_SHIFT) << 1) + 2); /* doom fix */

  if(TestInitVisual(UNPACKED))
    return;

  xvert = XPOS1616;
  yvert = YPOS1616;

  switch(TEXEL_DRAW_FUNC)
    {
    case TEXEL_DRAW_FUNC_LINE:
      xvert += (TEXTURE_HI_START * VDX1616);
      yvert += (TEXTURE_HI_START * VDY1616);
      /*
        if(SPRHI > TEXTURE_HI_LIM)
        SPRHI = TEXTURE_HI_LIM;
      */

      if(SPRWI > TEXTURE_WI_LIM)
        SPRWI = TEXTURE_WI_LIM;

      for(i = TEXTURE_HI_START; i < TEXTURE_HI_LIM; i++)
        {
          xcur   = (xvert + TEXTURE_WI_START * HDX1616);
          ycur   = (yvert + TEXTURE_WI_START * HDY1616);
          xvert += VDX1616;
          yvert += VDY1616;

          for(j = TEXTURE_WI_START; j < SPRWI; j++)
            {
              CURPIX = PDEC(opera_mem_read16((PDATA + XY2OFF(j,i,offset << 2))),&LAMV);

              if(!pproj.Transparent)
                {
                  uint32_t pixel;
                  uint32_t framePixel;

                  if(flag_is_set(FIXMODE,FIX_BIT_TIMING_6))
                    framePixel = opera_mem_read16((REGCTL2+XY2OFF(xcur >> 16,(ycur>>16)<<1,MADAM.rmod)));
                  else
                    framePixel = opera_mem_read16((REGCTL2+XY2OFF(xcur >> 16,ycur>>16,MADAM.rmod)));

                  pixel = PPROC(CURPIX,framePixel,LAMV);
                  pixel = PPROJ_OUTPUT(CURPIX,pixel,framePixel);
                  opera_mem_write16((REGCTL3+XY2OFF(xcur >> 16,ycur >> 16,MADAM.wmod)),pixel);
                }

              xcur += HDX1616;
              ycur += HDY1616;
            }
        }
      break;
    case TEXEL_DRAW_FUNC_SCALE:
      {
        int32_t drawHeight;

        drawHeight = VDY1616;
        if(flag_is_set(CCBFLAGS,CCB_MARIA) && (drawHeight > (1 << 16)))
          drawHeight = (1 << 16);

        for(i = 0; i < SPRHI; i++)
          {
            xcur   = xvert;
            ycur   = yvert;
            xvert += VDX1616;
            yvert += VDY1616;

            for(j = 0; j < SPRWI; j++)
              {
                CURPIX = PDEC(opera_mem_read16((PDATA+XY2OFF(j,i,offset<<2))),&LAMV);

                if(!pproj.Transparent)
                  {
                    if(TexelDrawScale(CURPIX,
                                      LAMV,
                                      xcur >> 16,
                                      ycur >> 16,
                                      ((xcur+HDX1616+VDX1616)>>16),
                                      ((ycur+HDY1616+drawHeight)>>16)))
                      break;
                  }

                xcur += HDX1616;
                ycur += HDY1616;
              }
          }
      }
      break;
    case TEXEL_DRAW_FUNC_ARBITRARY:
    default:
      for(i = 0; i < SPRHI; i++)
        {
          xcur     = xvert;
          ycur     = yvert;
          xvert   += VDX1616;
          yvert   += VDY1616;
          xdown    = xvert;
          ydown    = yvert;
          hdx      = HDX1616;
          hdy      = HDY1616;
          HDX1616 += HDDX1616;
          HDY1616 += HDDY1616;

          for(j = 0; j < SPRWI; j++)
            {
              CURPIX = PDEC(opera_mem_read16((PDATA+XY2OFF(j,i,offset<<2))),&LAMV);

              if(!pproj.Transparent)
                {
                  if(TexelDrawArbitrary(CURPIX,
                                        LAMV,
                                        xcur,
                                        ycur,
                                        xcur + hdx,
                                        ycur + hdy,
                                        xdown + HDX1616,
                                        ydown + HDY1616,
                                        xdown,
                                        ydown))
                    break;
                }

              xcur  += hdx;
              ycur  += hdy;
              xdown += HDX1616;
              ydown += HDY1616;
            }
        }
      break;
    }

  if(flag_is_set(FIXMODE,FIX_BIT_GRAPHICS_STEP_Y))
    YPOS1616 = ycur;
  else
    XPOS1616 = xcur;
}

void
opera_madam_reset(void)
{
  uint32_t i;

  for(i = 0; i < MADAM_REGISTER_COUNT; i++)
    MADAM.mregs[i] = 0;
}

static
INLINE
uint32_t
TexelCCWTest(int64_t hdx, int64_t hdy, int64_t vdx, int64_t vdy)
{
  if (((hdx + vdx) * (hdy - vdy) + vdx * vdy - hdx * hdy) < 0)
    return CCB_ACCW;
  return CCB_ACW;
}

static
bool
QuadCCWTest(int32_t wdt_)
{
  float wdt;
  uint32_t tmp;

  if(flag_is_set(CCBFLAGS,CCB_ACCW) && flag_is_set(CCBFLAGS,CCB_ACW))
    return false;

  wdt = (float)wdt_;
  tmp = TexelCCWTest(HDX1616, HDY1616, VDX1616, VDY1616);
  if (tmp != TexelCCWTest(HDX1616, HDY1616, VDX1616 + HDDX1616*wdt, VDY1616 + HDDY1616*wdt))
    return false;
  if (tmp != TexelCCWTest(HDX1616 + HDDX1616*SPRHI, HDY1616 + HDDY1616*SPRHI, VDX1616, VDY1616))
    return false;
  if (tmp != TexelCCWTest(HDX1616 + HDDX1616*SPRHI, HDY1616 + HDDY1616*SPRHI, VDX1616 + HDDX1616*SPRHI * wdt, VDY1616 + HDDY1616*SPRHI * wdt))
    return false;
  if(tmp == (CCBFLAGS & (CCB_ACCW | CCB_ACW)))
    return true;
  return false;
}

static
FORCEINLINE
int32_t
ABS(const int32_t val_)
{
  return ((val_ > 0) ? val_ : -val_);
}

static
bool
TestInitVisual(bool const packed_)
{
  int32_t xpoints[4];
  int32_t ypoints[4];

  if(flag_is_clr(CCBFLAGS,CCB_ACCW) && flag_is_clr(CCBFLAGS,CCB_ACW))
    return true;

  if(packed_)
    {
      xpoints[0] = (XPOS1616 >> 16);
      xpoints[1] = ((XPOS1616 + VDX1616 * SPRHI) >> 16);
      if((xpoints[0] < 0) &&
         (xpoints[1] < 0) &&
         (HDX1616   <= 0) &&
         (HDDX1616  <= 0))
        return true;
      if((xpoints[0] > MADAM.clipx) &&
         (xpoints[1] > MADAM.clipx) &&
         (HDX1616   >= 0)           &&
         (HDDX1616  >= 0))
        return true;

      ypoints[0] = (YPOS1616 >> 16);
      ypoints[1] = ((YPOS1616 + VDY1616 * SPRHI) >> 16);
      if((ypoints[0] < 0) &&
         (ypoints[1] < 0) &&
         (HDY1616   <= 0) &&
         (HDDY1616  <= 0))
        return true;
      if((ypoints[0] > MADAM.clipy) &&
         (ypoints[1] > MADAM.clipy) &&
         (HDY1616   >= 0)           &&
         (HDDY1616  >= 0))
        return true;
    }
  else
    {
      xpoints[0] = (XPOS1616 >> 16);
      xpoints[1] = (XPOS1616+HDX1616*SPRWI)>>16;
      xpoints[2] = (XPOS1616+VDX1616*SPRHI)>>16;
      xpoints[3] = ((XPOS1616+VDX1616*SPRHI+
                     (HDX1616+HDDX1616*SPRHI)*SPRWI) >> 16);
      if((xpoints[0] < 0) &&
         (xpoints[1] < 0) &&
         (xpoints[2] < 0) &&
         (xpoints[3] < 0))
        return true;
      if((xpoints[0] > MADAM.clipx) &&
         (xpoints[1] > MADAM.clipx) &&
         (xpoints[2] > MADAM.clipx) &&
         (xpoints[3] > MADAM.clipx))
        return true;

      ypoints[0] = (YPOS1616 >> 16);
      ypoints[1] = ((YPOS1616+HDY1616*SPRWI) >> 16);
      ypoints[2] = ((YPOS1616+VDY1616*SPRHI) >> 16);
      ypoints[3] = ((YPOS1616+VDY1616*SPRHI+
                     (HDY1616+HDDY1616*SPRHI)*SPRWI) >> 16);
      if((ypoints[0] < 0) &&
         (ypoints[1] < 0) &&
         (ypoints[2] < 0) &&
         (ypoints[3] < 0))
        return true;
      if((ypoints[0] > MADAM.clipy) &&
         (ypoints[1] > MADAM.clipy) &&
         (ypoints[2] > MADAM.clipy) &&
         (ypoints[3] > MADAM.clipy))
        return true;
    }

  if((HDDX1616 == 0) && (HDDY1616 == 0))
    {
      if((HDX1616 == 0) && (VDY1616 == 0))
        {
          if(((HDY1616 < 0) && (VDX1616 > 0)) ||
             ((HDY1616 > 0) && (VDX1616 < 0)))
            {
              if(flag_is_set(CCBFLAGS,CCB_ACW))
                {
                  if((ABS(HDY1616) == 0x10000) &&
                     (ABS(VDX1616) == 0x10000) &&
                     !((YPOS1616|XPOS1616)&0xffff))
                    {
                      return InitLineMap();
                    }
                  else
                    {
                      InitScaleMap();
                      return false;
                    }
                }
            }
          else
            {
              if(flag_is_set(CCBFLAGS,CCB_ACCW))
                {
                  if((ABS(HDY1616) == 0x10000) &&
                     (ABS(VDX1616) == 0x10000) &&
                     !((YPOS1616|XPOS1616)&0xffff))
                    {
                      return InitLineMap();
                    }
                  else
                    {
                      InitScaleMap();
                      return false;
                    }
                }
            }

          return true;
        }
      else if((HDY1616 == 0) && (VDX1616 == 0))
        {
          if(((HDX1616 < 0) && (VDY1616 > 0)) ||
             ((HDX1616 > 0) && (VDY1616 < 0)))
            {
              if(flag_is_set(CCBFLAGS,CCB_ACCW))
                {
                  if((ABS(HDX1616) == 0x10000) &&
                     (ABS(VDY1616) == 0x10000) &&
                     !((YPOS1616|XPOS1616)&0xffff))
                    {
                      return InitLineMap();
                    }
                  else
                    {
                      InitScaleMap();
                      return false;
                    }
                }
            }
          else
            {
              if(flag_is_set(CCBFLAGS,CCB_ACW))
                {
                  if((ABS(HDX1616) == 0x10000) &&
                     (ABS(VDY1616) == 0x10000) &&
                     !((YPOS1616|XPOS1616)&0xffff))
                    {
                      return InitLineMap();
                    }
                  else
                    {
                      InitScaleMap();
                      return false;
                    }
                }
            }

          return true;
        }
    }

  if(QuadCCWTest(packed_ ? 2048 : SPRWI))
    return true;

  InitArbitraryMap();

  return false;
}

static
bool
InitLineMap(void)
{
  TEXEL_DRAW_FUNC  = TEXEL_DRAW_FUNC_LINE;
  TEXTURE_WI_START = 0;
  TEXTURE_HI_START = 0;
  TEXTURE_HI_LIM   = SPRHI;

  if((HDX1616 < 0) || (VDX1616 < 0))
    XPOS1616 -= 0x8000;
  if((HDY1616 < 0) || (VDY1616 < 0))
    YPOS1616 -= 0x8000;

  if(VDX1616 < 0)
    {
      if(((XPOS1616 - ((SPRHI - 1) << 16)) >> 16) < 0)
        TEXTURE_HI_LIM = ((XPOS1616 >> 16) + 1);
      if(TEXTURE_HI_LIM > SPRHI)
        TEXTURE_HI_LIM = SPRHI;
    }
  else if(VDX1616 > 0)
    {
      if(((XPOS1616 + (SPRHI << 16)) >> 16) > MADAM.clipx)
        TEXTURE_HI_LIM = (MADAM.clipx - (XPOS1616>>16) + 1);
    }

  if(VDY1616 < 0)
    {
      if(((YPOS1616 - ((SPRHI - 1) << 16)) >> 16) < 0)
        TEXTURE_HI_LIM = ((YPOS1616 >> 16) + 1);
      if(TEXTURE_HI_LIM > SPRHI)
        TEXTURE_HI_LIM = SPRHI;
    }
  else if(VDY1616 > 0)
    {
      if(((YPOS1616 + (SPRHI << 16)) >> 16) > MADAM.clipy)
        TEXTURE_HI_LIM = (MADAM.clipy - (YPOS1616 >> 16) + 1);
    }

  if(HDX1616 < 0)
    TEXTURE_WI_LIM = ((XPOS1616 >> 16) + 1);
  else if(HDX1616 > 0)
    TEXTURE_WI_LIM = (MADAM.clipx - (XPOS1616 >> 16) + 1);

  if(HDY1616 < 0)
    TEXTURE_WI_LIM = ((YPOS1616 >> 16) + 1);
  else if(HDY1616 > 0)
    TEXTURE_WI_LIM = (MADAM.clipy - (YPOS1616 >> 16) + 1);

  if(XPOS1616 < 0)
    {
      if(HDX1616 < 0)
        return true;
      else if(HDX1616 > 0)
        TEXTURE_WI_START = -(XPOS1616 >> 16);

      if(VDX1616 < 0)
        return true;
      else if(VDX1616 > 0)
        TEXTURE_HI_START = -(XPOS1616 >> 16);
    }
  else if((XPOS1616 >> 16) > MADAM.clipx)
    {
      if(HDX1616 > 0)
        return true;
      else if(HDX1616 < 0)
        TEXTURE_WI_START = ((XPOS1616 >> 16) - MADAM.clipx);

      if(VDX1616 > 0)
        return true;
      else if(VDX1616 < 0)
        TEXTURE_HI_START = ((XPOS1616 >> 16) - MADAM.clipx);
    }

  if(YPOS1616 < 0)
    {
      if(HDY1616 < 0)
        return true;
      else if(HDY1616 > 0)
        TEXTURE_WI_START = -(YPOS1616 >> 16);

      if(VDY1616 < 0)
        return true;
      else if(VDY1616 > 0)
        TEXTURE_HI_START = -(YPOS1616 >> 16);
    }
  else if((YPOS1616 >> 16) > MADAM.clipy)
    {
      if(HDY1616 > 0)
        return true;
      else if(HDY1616 < 0)
        TEXTURE_WI_START = ((YPOS1616 >> 16) - MADAM.clipy);

      if(VDY1616 > 0)
        return true;
      else if(VDY1616 < 0)
        TEXTURE_HI_START = ((YPOS1616 >> 16) - MADAM.clipy);
    }

  /*
    if(TEXTURE_WI_START<((PRE0>>24)&0xf))
    TEXTURE_WI_START=((PRE0>>24)&0xf);
    TEXTURE_WI_START+=(PRE0>>24)&0xf;
    if(TEXTURE_WI_START<0)TEXTURE_WI_START=0;
    if(TEXTURE_HI_START<0)TEXTURE_HI_START=0;
    if(TEXTURE_HI_LIM>SPRHI)TEXTURE_HI_LIM=SPRHI;
  */

  if(TEXTURE_WI_LIM <= 0)
    return true;

  return false;
}

static
INLINE
void
InitScaleMap(void)
{
  int32_t deltax;
  int32_t deltay;

  TEXEL_DRAW_FUNC = TEXEL_DRAW_FUNC_SCALE;

  if((HDX1616 < 0) || (VDX1616 < 0))
    XPOS1616 -= 0x8000;
  if((HDY1616 < 0) || (VDY1616 < 0))
    YPOS1616 -= 0x8000;

  deltax = (HDX1616 + VDX1616);
  deltay = (HDY1616 + VDY1616);

  TEXEL_INCX = ((deltax < 0) ? -1 : 1);
  TEXEL_INCY = ((deltay < 0) ? -1 : 1);

  TEXTURE_WI_START = 0;
  TEXTURE_HI_START = 0;
}

static
INLINE
void
InitArbitraryMap(void)
{
  TEXEL_DRAW_FUNC  = TEXEL_DRAW_FUNC_ARBITRARY;
  TEXTURE_WI_START = 0;
  TEXTURE_HI_START = 0;
}

static
void
TexelDrawLine(uint16_t CURPIX_,
              uint16_t LAMV_,
              int32_t  xcur_,
              int32_t  ycur_,
              int32_t  cnt_)
{
  int32_t i;
  uint32_t curr;
  uint32_t pixel;

  xcur_ >>= 16;
  ycur_ >>= 16;
  curr = 0xFFFFFFFF;

  for(i = 0; i < cnt_; i++, xcur_ += (HDX1616 >> 16), ycur_ += (HDY1616 >> 16))
    {
      uint32_t next;

      next = opera_mem_read16(REGCTL2 + XY2OFF(xcur_,ycur_,MADAM.rmod));
      if(next != curr)
        {
          curr  = next;
          pixel = PPROC(CURPIX_,next,LAMV_);
          pixel = PPROJ_OUTPUT(CURPIX_,pixel,next);
        }

      opera_mem_write16(REGCTL3 + XY2OFF(xcur_,ycur_,MADAM.wmod),pixel);
    }
}

static
INLINE
uint16_t
readPIX(int32_t x_,
        int32_t y_)
{
  uint32_t src = REGCTL2;

  if(HIRESMODE)
    {
      src += XY2OFF(x_ >> 1,y_ >> 1,MADAM.rmod);
      src += ((((y_ & 1) << 1) + (x_ & 1)) * VRAM_SIZE);
    }
  else
    {
      src += XY2OFF(x_,y_,MADAM.rmod);
    }

  return opera_mem_read16(src);
}

static
INLINE
void
writePIX(int32_t  x_,
         int32_t  y_,
         uint16_t p_)
{
  uint32_t src = REGCTL3;

  if(HIRESMODE)
    {
      src += XY2OFF(x_ >> 1,y_ >> 1,MADAM.wmod);
      src += ((((y_ & 1) << 1) + (x_ & 1)) * VRAM_SIZE);
    }
  else
    {
      src += XY2OFF(x_,y_,MADAM.wmod);
    }

  opera_mem_write16_base(src,p_);
}

static
int32_t
TexelDrawScale(uint16_t CURPIX_,
               uint16_t LAMV_,
               int32_t  xcur_,
               int32_t  ycur_,
               int32_t  deltax_,
               int32_t  deltay_)
{
  int32_t x;
  int32_t y;
  uint32_t pixel;
  uint32_t framePixel;

  if(flag_is_set(FIXMODE,FIX_BIT_TIMING_3))
    {
      deltay_ *= 5;
      ycur_   *= 5;
    }

  if((HDX1616 < 0) && (deltax_ < 0) && (xcur_ < 0))
    return -1;
  else if((HDY1616 < 0) && (deltay_ < 0) && (ycur_ < 0))
    return -1;
  else if((HDX1616 > 0) && (deltax_ > MADAM.clipx) && (xcur_ > MADAM.clipx))
    return -1;
  else if((HDY1616 > 0) && (deltay_ > MADAM.clipy) && (ycur_ > MADAM.clipy))
    return -1;

  if(xcur_ == deltax_)
    return 0;

  for(y = ycur_; y != deltay_; y += TEXEL_INCY)
    {
      for(x = xcur_; x != deltax_; x += TEXEL_INCX)
        {
          if(clipping(x,y))
            continue;

          process_pixel(x,y,CURPIX_,LAMV_);
        }
    }

  return 0;
}

static
int32_t
TexelDrawArbitrary(uint16_t CURPIX_,
                   uint16_t LAMV_,
                   int32_t  xA_,
                   int32_t  yA_,
                   int32_t  xB_,
                   int32_t  yB_,
                   int32_t  xC_,
                   int32_t  yC_,
                   int32_t  xD_,
                   int32_t  yD_)
{
  int32_t x;
  int32_t y;
  int32_t miny;
  int32_t maxy;
  int32_t maxx;
  int32_t maxxt;
  int32_t maxyt;
  int32_t tmp;
  uint32_t curr;
  uint32_t next;
  uint32_t pixel;
  int32_t xpoints[4];
  int32_t updowns[4];

  curr = -1;
  xA_ >>= (16 - HIRESMODE);
  xB_ >>= (16 - HIRESMODE);
  xC_ >>= (16 - HIRESMODE);
  xD_ >>= (16 - HIRESMODE);
  yA_ >>= (16 - HIRESMODE);
  yB_ >>= (16 - HIRESMODE);
  yC_ >>= (16 - HIRESMODE);
  yD_ >>= (16 - HIRESMODE);

  if((xA_ == xB_) && (xB_ == xC_) && (xC_ == xD_))
    return 0;

  maxxt = ((MADAM.clipx + 1) << HIRESMODE);
  maxyt = ((MADAM.clipy + 1) << HIRESMODE);

  if((HDX1616 < 0) && (HDDX1616 < 0))
    {
      if((xA_ < 0) && (xB_ < 0) && (xC_ < 0) && (xD_ < 0))
        return -1;
    }

  if((HDX1616 > 0) && (HDDX1616 > 0))
    {
      if((xA_ >= maxxt) && (xB_ >= maxxt) && (xC_ >= maxxt) && (xD_ >= maxxt))
        return -1;
    }

  if((HDY1616 < 0) && (HDDY1616 < 0))
    {
      if((yA_ < 0) && (yB_ < 0) && (yC_ < 0) && (yD_ < 0))
        return -1;
    }

  if((HDY1616 > 0) && (HDDY1616 > 0))
    {
      if((yA_ >= maxyt) && (yB_ >= maxyt) && (yC_ >= maxyt) && (yD_ >= maxyt))
        return -1;
    }

  miny = maxy = yA_;

  if(miny > yB_)
    miny = yB_;
  if(miny > yC_)
    miny = yC_;
  if(miny > yD_)
    miny = yD_;
  if(maxy < yB_)
    maxy = yB_;
  if(maxy < yC_)
    maxy = yC_;
  if(maxy < yD_)
    maxy = yD_;

  y = miny;
  if(y < 0)
    y = 0;
  if(maxy < maxyt)
    maxyt = maxy;

  for(; y < maxyt; y++)
    {
      int cnt_cross = 0;
      if((y < yB_) && (y >= yA_))
        {
          xpoints[cnt_cross] = (int32_t)((((xB_-xA_)*(y-yA_))/(yB_-yA_))+xA_);
          updowns[cnt_cross++] = 1;
        }
      else if((y >= yB_) && (y < yA_))
        {
          xpoints[cnt_cross] = (int32_t)((((xA_-xB_)*(y-yB_))/(yA_-yB_))+xB_);
          updowns[cnt_cross++] = 0;
        }

      if((y < yC_) && (y >= yB_))
        {
          xpoints[cnt_cross] = (int32_t)((((xC_-xB_)*(y-yB_))/(yC_-yB_))+xB_);
          updowns[cnt_cross++] = 1;
        }
      else if((y >= yC_) && (y < yB_))
        {
          xpoints[cnt_cross] = (int32_t)((((xB_-xC_)*(y-yC_))/(yB_-yC_))+xC_);
          updowns[cnt_cross++] = 0;
        }

      if((y < yD_) && (y >= yC_))
        {
          xpoints[cnt_cross] = (int32_t)((((xD_-xC_)*(y-yC_))/(yD_-yC_))+xC_);
          updowns[cnt_cross++] = 1;
        }
      else if((y >= yD_) && (y < yC_))
        {
          xpoints[cnt_cross] = (int32_t)((((xC_-xD_)*(y-yD_))/(yC_-yD_))+xD_);
          updowns[cnt_cross++] = 0;
        }

      if(cnt_cross & 1)
        {
          if((y < yA_) && (y >= yD_))
            {
              xpoints[cnt_cross] = (int32_t)((((xA_-xD_)*(y-yD_))/(yA_-yD_))+xD_);
              updowns[cnt_cross] = 1;
            }
          else if((y >= yA_) && (y < yD_))
            {
              xpoints[cnt_cross] = (int32_t)((((xD_-xA_)*(y-yA_))/(yD_-yA_))+xA_);
              updowns[cnt_cross] = 0;
            }
        }

      if(cnt_cross != 0)
        {
          if(xpoints[0] > xpoints[1])
            {
              xpoints[1] += xpoints[0];
              xpoints[0]  = xpoints[1] - xpoints[0];
              xpoints[1]  = xpoints[1] - xpoints[0];

              tmp        = updowns[0];
              updowns[0] = updowns[1];
              updowns[1] = tmp;
            }

          if(cnt_cross > 2)
            {
              if((flag_is_set(CCBFLAGS,CCB_ACW)  && (updowns[2] == 0)) ||
                 (flag_is_set(CCBFLAGS,CCB_ACCW) && (updowns[2] == 1)))
                {
                  x = xpoints[2];
                  if(x < 0)
                    x = 0;

                  maxx = xpoints[3];
                  if(maxx > maxxt)
                    maxx = maxxt;

                  for(; x < maxx; x++)
                    {
                      next = readPIX(x,y);
                      if(next != curr)
                        {
                          curr  = next;
                          pixel = PPROC(CURPIX_,next,LAMV_);
                          pixel = PPROJ_OUTPUT(CURPIX_,pixel,next);
                        }
                      writePIX(x,y,pixel);
                    }
                }
            }

          if((flag_is_set(CCBFLAGS,CCB_ACW)  && (updowns[0] == 0)) ||
             (flag_is_set(CCBFLAGS,CCB_ACCW) && (updowns[0] == 1)))
            {
              x = xpoints[0];
              if(x < 0)
                x = 0;

              maxx = xpoints[1];
              if(maxx > maxxt)
                maxx = maxxt;

              for(; x < maxx; x++)
                {
                  next = readPIX(x,y);
                  if(next != curr)
                    {
                      curr  = next;
                      pixel = PPROC(CURPIX_,next,LAMV_);
                      pixel = PPROJ_OUTPUT(CURPIX_,pixel,next);
                    }
                  writePIX(x,y,pixel);
                }
            }
        }
    }

  return 0;
}
