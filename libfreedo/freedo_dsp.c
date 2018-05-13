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

#include "freedo_clio.h"
#include "freedo_core.h"
#include "freedo_dsp.h"
#include "inline.h"

#include <string.h>

#if 0 //20 bit ALU
#define ALUSIZEMASK 0xFFFFF000
#else //32 bit ALU
#define ALUSIZEMASK 0xFFFFFFFF
#endif

#define TOPBIT       0x80000000
#define SYSTEM_TICKS 568        /* ceil(((25000000 / 44100) + 1)) */

#pragma pack(push,1)

struct CIFTAG_s
{
  uint32_t BCH_ADDR:10;
  uint32_t FLAG_MASK:2;
  uint32_t FLGSEL:1;
  uint32_t MODE:2;
  uint32_t PAD:1;
};

typedef struct CIFTAG_s CIFTAG_t;

struct BRNTAG_s
{
  uint32_t BCH_ADDR:10;
  uint32_t FLAGM0:1;
  uint32_t FLAGM1:1;
  uint32_t FLAGSEL:1;
  uint32_t MODE0:1;
  uint32_t MODE1:1;
  uint32_t AC:1;
};

typedef struct BRNTAG_s BRNTAG_t;

struct BRNBITS_s
{
  uint32_t BCH_ADDR:10;
  uint32_t bits:5;
  uint32_t AC:1;
};

typedef struct BRNBITS_s BRNBITS_t;

struct AIFTAG_s
{
  uint32_t BS:4;
  uint32_t ALU:4;
  uint32_t MUXB:2;
  uint32_t MUXA:2;
  int32_t  M2SEL:1;
  uint32_t NUMOPS:2;
  int32_t  PAD:1;
};

typedef struct AIFTAG_s AIFTAG_t;

struct IOFTAG_s
{
  int32_t  IMMEDIATE:13;
  int32_t  JUSTIFY:1;
  uint32_t TYPE:2;
};

typedef struct IOFTAG_s IOFTAG_t;

struct NROFTAG_s
{
  uint32_t OP_ADDR:10;
  int32_t  DI:1;
  uint32_t WB1:1;
  uint32_t PAD:1;
  uint32_t TYPE:3;
};

typedef struct NROFTAG_s NROFTAG_t;

struct R2OFTAG_s
{
  uint32_t R1:4;
  int32_t  R1_DI:1;
  uint32_t R2:4;
  int32_t  R2_DI:1;
  uint32_t NUMREGS:1;
  uint32_t WB1:1;
  uint32_t WB2:1;
  uint32_t TYPE:3;
};

typedef struct R2OFTAG_s R2OFTAG_t;

struct R3OFTAG_s
{
  uint32_t R1:4;
  int32_t  R1_DI:1;
  uint32_t R2:4;
  int32_t  R2_DI:1;
  uint32_t R3:4;
  int32_t  R3_DI:1;
  uint32_t TYPE:1;
};

typedef struct R3OFTAG_s R3OFTAG_t;

union ITAG_u
{
  uint32_t  raw;
  AIFTAG_t  aif;
  CIFTAG_t  cif;
  IOFTAG_t  iof;
  NROFTAG_t nrof;
  R2OFTAG_t r2of;
  R3OFTAG_t r3of;
  BRNTAG_t  branch;
  BRNBITS_t br;
};

typedef union ITAG_u ITAG_t;

struct RQFTAG_s
{
  uint32_t BS:1;
  uint32_t ALU2:1;
  uint32_t ALU1:1;
  uint32_t MULT2:1;
  uint32_t MULT1:1;
};

typedef struct RQFTAG_s RQFTAG_t;

union REQ_u
{
  uint8_t  raw;
  RQFTAG_t rq;
};

typedef union REQ_u REQ_t;

/* only for ALU command */
struct INSTTRAS_s
{
  REQ_t req;
  char  BS;                     // 4+1 bits
};

typedef struct INSTTRAS_s INSTTRAS_t;

struct REGSTAG_s
{
  uint32_t PC;                  // 0x0ee
  uint16_t NOISE;               // 0x0ea
  uint16_t AudioOutStatus;      // audlock,lftfull,rgtfull -- 0x0eb//0x3eb
  uint16_t Sema4Status;         // 0x0ec // 0x3ec
  uint16_t Sema4Data;           // 0x0ed // 0x3ed
  int16_t  DSPPCNT;             // 0x0ef
  int16_t  DSPPRLD;             // 0x3ef
  int16_t  AUDCNT;
  uint16_t INT;                 // 0x3ee
};

typedef struct REGSTAG_s REGSTAG_t;

struct INTAG_s
{
  int16_t  MULT1;
  int16_t  MULT2;
  int16_t  ALU1;
  int16_t  ALU2;
  int32_t  BS;
  uint16_t RMAP;
  uint16_t nOP_MASK;
  uint16_t WRITEBACK;
  REQ_t    req;
  bool     Running;
  bool     GenFIQ;
};

typedef struct INTAG_s INTAG_t;

struct dsp_s
{
  uint32_t   RBASEx4;
  INSTTRAS_t INSTTRAS[0x8000];
  uint16_t   REGCONV[8][16];
  bool       BRCONDTAB[32][32];
  uint16_t   NMem[2048];
  uint16_t   IMem[1024];
  int        REGi;
  REGSTAG_t  dregs;
  INTAG_t    flags;
  uint32_t   g_seed;
  bool       CPUSupply[16];
};

typedef struct dsp_s dsp_t;

union dsp_alu_flags_u
{
  uint32_t raw;

  struct
  {
    uint8_t zero;
    uint8_t negative;
    uint8_t carry;
    uint8_t overflow;
  };
};

typedef union dsp_alu_flags_u dsp_alu_flags_t;

#pragma pack(pop)

static dsp_t DSP;

int
fastrand(void)
{
  DSP.g_seed = 69069 * DSP.g_seed + 1;
  return (DSP.g_seed & 0xFFFF);
}

static
FORCEINLINE
int
ADD_CFLAG(const uint32_t a_,
          const uint32_t b_,
          const uint32_t y_)
{
  return ((a_ &  b_ & TOPBIT) ||
          (a_ & ~y_ & TOPBIT) ||
          (b_ & ~y_ & TOPBIT));
}

static
FORCEINLINE
int
SUB_CFLAG(const uint32_t a_,
          const uint32_t b_,
          const uint32_t y_)
{
  return (( a_ & ~b_ & TOPBIT) ||
          ( a_ & ~y_ & TOPBIT) ||
          (~b_ & ~y_ & TOPBIT));
}

static
FORCEINLINE
int
ADD_VFLAG(const uint32_t a_,
          const uint32_t b_,
          const uint32_t y_)
{
  return (( a_ &  b_ & ~y_ & TOPBIT) ||
          (~a_ & ~b_ &  y_ & TOPBIT));
}

static
FORCEINLINE
int
SUB_VFLAG(const uint32_t a_,
          const uint32_t b_,
          const uint32_t y_)
{
  return (( a_ & ~b_ & ~y_ & TOPBIT) ||
          (~a_ &  b_ &  y_ & TOPBIT));
}

/* DSP IREAD (includes EI, I) */
static
uint16_t
dsp_read(uint32_t addr_)
{
  uint16_t val;

  /* addr &= 0x3FF; */
  switch(addr_)
    {
    case 0xEA:
      DSP.dregs.NOISE = fastrand();
      return DSP.dregs.NOISE;
    case 0xEB:
      return DSP.dregs.AudioOutStatus;
    case 0xEC:
      return DSP.dregs.Sema4Status;
    case 0xED:
      return DSP.dregs.Sema4Data;
    case 0xEE:
      return DSP.dregs.PC;
    case 0xEF:
      return DSP.dregs.DSPPCNT;
    case 0xF0:
    case 0xF1:
    case 0xF2:
    case 0xF3:
    case 0xF4:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
      /*
        printf("#DSP read from CPU!!! chan=0x%x\n",addr&0x0f);
        val=IMem[addr-0x80];
      */
      if(DSP.CPUSupply[addr_ - 0xF0])
        return (DSP.CPUSupply[addr_ - 0xF0] = 0, fastrand());
      return freedo_clio_fifo_ei(addr_ & 0x0F);
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77:
    case 0x78:
    case 0x79:
    case 0x7A:
    case 0x7B:
    case 0x7C:
      //printf("#DSP read from CPU!!! chan=0x%x\n",addr&0x0f);
      if(DSP.CPUSupply[addr_ - 0x70])
        return (DSP.CPUSupply[addr_ - 0x70] = 0, DSP.IMem[addr_]);
      return freedo_clio_fifo_ei_read(addr_ & 0x0F);
    case 0xD0:
    case 0xD1:
    case 0xD2:
    case 0xD3:
    case 0xD4:
    case 0xD5:
    case 0xD6:
    case 0xD7:
    case 0xD8:
    case 0xD9:
    case 0xDA:
    case 0xDB:
    case 0xDC:
    case 0xDD:
    case 0xDE:
      /*
        what is last two case's?
        if(CPUSupply[addr&0x0f])
        {
        CPUSupply[addr&0x0f]=0;
        printf("#DSP read from CPU!!! chan=0x%x\n",addr&0x0f);
        return IMem[0x70+addr&0x0f];
        }
        else
        printf("#DSP read EIFifo status 0x%4.4X\n",addr&0x0f);
      */
      if(DSP.CPUSupply[addr_ & 0x0F])
        return 2;
      return freedo_clio_fifo_ei_status(addr_ & 0x0F);
    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
      return freedo_clio_fifo_eo_status(addr_ & 0x0F);
    default:
      //printf("#EIRead 0x%3.3X>=0x%4.4X\n",addr, IMem[addr_ & 0x7F]);
      addr_ -= 0x100;
      if(addr_ < 0x200)
        return DSP.IMem[addr_ | 0x100];
    }

  return DSP.IMem[addr_ & 0x7F];
}

/* DSP IWRITE (includes EO,I) */
static
void
dsp_write(uint32_t addr_,
          uint16_t val_)
{
  addr_ &= 0x3FF;
  switch(addr_)
    {
    case 0x3EB:
      DSP.dregs.AudioOutStatus = val_;
      break;
    case 0x3EC:
      /* DSP write to Sema4ACK */
      DSP.dregs.Sema4Status |= 0x01;
      break;
    case 0x3ED:
      DSP.dregs.Sema4Data   = val_;
      DSP.dregs.Sema4Status = 0x4;  /* DSP write to Sema4Data */
      break;
    case 0x3EE:
      DSP.dregs.INT    = val_;
      DSP.flags.GenFIQ = true;
      break;
    case 0x3EF:
      DSP.dregs.DSPPRLD = val_;
      break;
    case 0x3F0:
    case 0x3F1:
    case 0x3F2:
    case 0x3F3:
      freedo_clio_fifo_eo(addr_ & 0x0F,val_);
      break;
    case 0x3FD:
      /* FLUSH EOFIFO */
      break;
    case 0x3FE: /* DAC Left channel */
    case 0x3FF: /* DAC Right channel */
      DSP.IMem[addr_] = val_;
      break;
    default:
      if(addr_ < 0x100)
        return;

      addr_ -= 0x100;
      if(addr_ < 0x200)
        DSP.IMem[addr_ | 0x100] = val_;
      else
        DSP.IMem[addr_ + 0x100] = val_;
      break;
    }
}

uint32_t
freedo_dsp_state_size(void)
{
  return sizeof(dsp_t);
}

void
freedo_dsp_state_save(void *buf_)
{
  memcpy(buf_,&DSP,sizeof(dsp_t));
}

void
freedo_dsp_state_load(const void *buf_)
{
  memcpy(&DSP,buf_,sizeof(dsp_t));
}

static
uint16_t
dsp_operand_load1(void)
{
  uint16_t op;
  ITAG_t operand;

  operand.raw = DSP.NMem[DSP.dregs.PC++];
  switch(operand.nrof.TYPE)
    {
    case 0:
    case 1:
    case 2:
    case 3:
      op = dsp_read(DSP.REGCONV[DSP.REGi][operand.r3of.R3] ^ DSP.RBASEx4);
      if(operand.r3of.R3_DI) /* ??? */
        return dsp_read(op);
      return op;
    case 4:
      // non reg format
      // IT'S an address!!!
      op = dsp_read(operand.nrof.OP_ADDR);
      if(operand.nrof.DI)
        return dsp_read(op);
      return op;
    case 5:
      // if(operand.r2of.NUMREGS) ignore... It's right?
      op = dsp_read(DSP.REGCONV[DSP.REGi][operand.r2of.R1] ^ DSP.RBASEx4);
      if(operand.r2of.R1_DI)
        return dsp_read(op);
      return op;
    case 6:
    case 7:
      return (operand.iof.IMMEDIATE << (operand.iof.JUSTIFY & 3));
    default:
      break;
    }

  return 0;
}

static
void
dsp_operand_load(int requests_)
{
  int idx;
  int op_cnt;
  uint16_t ops[6];
  uint16_t GWRITEBACK;
  ITAG_t operand;

  DSP.flags.WRITEBACK = 0;

  if(requests_ == 0)
    {
      if(DSP.flags.req.raw)
        requests_ = 4;
      else
        return;
    }

  op_cnt     = 0;
  GWRITEBACK = 0;

  do
    {
      operand.raw = DSP.NMem[DSP.dregs.PC++];
      switch(operand.nrof.TYPE)
        {
        case 0:
        case 1:
        case 2:
        case 3:
          ops[op_cnt] = dsp_read(DSP.REGCONV[DSP.REGi][operand.r3of.R3] ^ DSP.RBASEx4);
          if(operand.r3of.R3_DI)
            ops[op_cnt] = dsp_read(ops[op_cnt]);
          op_cnt++;

          ops[op_cnt] = dsp_read(DSP.REGCONV[DSP.REGi][operand.r3of.R2] ^ DSP.RBASEx4);
          if(operand.r3of.R2_DI)
            ops[op_cnt] = dsp_read(ops[op_cnt]);
          op_cnt++;

          /* only R1 can be WRITEBACK */
          DSP.flags.WRITEBACK = (DSP.REGCONV[DSP.REGi][operand.r3of.R1] ^ DSP.RBASEx4);
          ops[op_cnt] = dsp_read(DSP.flags.WRITEBACK);
          if(operand.r3of.R1_DI)
            ops[op_cnt] = dsp_read(ops[op_cnt]);
          op_cnt++;
          break;
        case 4:
          //non reg format ///IT'S an address!!!
          DSP.flags.WRITEBACK = operand.nrof.OP_ADDR;
          ops[op_cnt] = dsp_read(DSP.flags.WRITEBACK);
          if(operand.nrof.DI)
            ops[op_cnt] = dsp_read(ops[op_cnt]);
          op_cnt++;

          if(operand.nrof.WB1)
            GWRITEBACK = DSP.flags.WRITEBACK;
          break;
        case 5:
          //regged 1/2 format
          if(operand.r2of.NUMREGS)
            {
              DSP.flags.WRITEBACK = DSP.REGCONV[DSP.REGi][operand.r2of.R2] ^ DSP.RBASEx4;
              if(operand.r2of.R2_DI)
                DSP.flags.WRITEBACK = dsp_read(DSP.flags.WRITEBACK);
              ops[op_cnt] = dsp_read(DSP.flags.WRITEBACK);
              op_cnt++;

              if(operand.r2of.WB2)
                GWRITEBACK = DSP.flags.WRITEBACK;
            }

          DSP.flags.WRITEBACK = DSP.REGCONV[DSP.REGi][operand.r2of.R1] ^ DSP.RBASEx4;
          if(operand.r2of.R1_DI)
            DSP.flags.WRITEBACK = dsp_read(DSP.flags.WRITEBACK);
          ops[op_cnt] = dsp_read(DSP.flags.WRITEBACK);
          op_cnt++;

          if(operand.r2of.WB1)
            GWRITEBACK = DSP.flags.WRITEBACK;
          break;
        case 6:
        case 7:
          ops[op_cnt] = (operand.iof.IMMEDIATE << (operand.iof.JUSTIFY & 3));
          DSP.flags.WRITEBACK = ops[op_cnt++];
          break;
        default:
          break;
        }
    } while(op_cnt < requests_);


  /* ok let's clean out requests_ (using op_mask) */
  DSP.flags.req.raw &= DSP.flags.nOP_MASK;

  idx = 0;
  if(DSP.flags.req.rq.MULT1)
    DSP.flags.MULT1 = ops[idx++];
  if(DSP.flags.req.rq.MULT2)
    DSP.flags.MULT2 = ops[idx++];

  if(DSP.flags.req.rq.ALU1)
    DSP.flags.ALU1 = ops[idx++];
  if(DSP.flags.req.rq.ALU2)
    DSP.flags.ALU2 = ops[idx++];

  if(DSP.flags.req.rq.BS)
    DSP.flags.BS = ops[idx++];

  if(op_cnt != idx)
    {
      if(GWRITEBACK)
        DSP.flags.WRITEBACK = GWRITEBACK;
      /* else
         DSP.flags.WRITEBACK = 0; */
    }
  else
    {
      DSP.flags.WRITEBACK = GWRITEBACK;
    }
}

static
INLINE
uint16_t
dsp_register_base(uint32_t reg_)
{
  uint8_t x;
  uint8_t y;
  uint8_t twi;

  reg_ &= 0x0000000F;
  x     = ((reg_ >> 2) & 1);
  y     = ((reg_ >> 3) & 1);

  switch(DSP.flags.RMAP)
    {
    case 0:
    case 1:
    case 2:
    case 3:
      twi = x;
      break;
    case 4:
      twi = y;
      break;
    case 5:
      twi = !y;
      break;
    case 6:
      twi = x & y;
      break;
    case 7:
      twi = x | y;
      break;
    }

  return ((reg_ & 7) | (twi << 8) | (reg_ >> 3) << 9);
}

void
freedo_dsp_init(void)
{
  uint32_t i;
  int32_t a,c;
  ITAG_t inst;

  DSP.g_seed = 0xa5a5a5a5;
  for(a = 0; a < 16; a++)
    {
      for(c = 0; c < 8; c++)
        {
          DSP.flags.RMAP    = c;
          DSP.REGCONV[c][a] = dsp_register_base(a);
        }
    }

  for(inst.raw = 0; inst.raw < 0x8000; inst.raw++)
    {
      DSP.flags.req.raw = 0;

      if(inst.aif.BS == 0x8)
        DSP.flags.req.rq.BS = 1;

      switch(inst.aif.MUXA)
        {
        case 3:
          DSP.flags.req.rq.MULT1 = 1;
          DSP.flags.req.rq.MULT2 = inst.aif.M2SEL;
          break;
        case 1:
          DSP.flags.req.rq.ALU1 = 1;
          break;
        case 2:
          DSP.flags.req.rq.ALU2 = 1;
          break;
        }

      switch(inst.aif.MUXB)
        {
        case 1:
          DSP.flags.req.rq.ALU1 = 1;
          break;
        case 2:
          DSP.flags.req.rq.ALU2 = 1;
          break;
        case 3:
          DSP.flags.req.rq.MULT1 = 1;
          DSP.flags.req.rq.MULT2 = inst.aif.M2SEL;
          break;
        }

      DSP.INSTTRAS[inst.raw].req.raw = DSP.flags.req.raw;
      DSP.INSTTRAS[inst.raw].BS      = (inst.aif.BS | ((inst.aif.ALU & 8) << (4 - 3)));
    }

  {
    bool MD1, MD2, MD3;
    bool STAT0, STAT1;
    bool NSTAT0, NSTAT1;
    bool TDCARE0, TDCARE1;
    bool RDCARE;
    bool MD12S;
    bool SUPER0, SUPER1;
    bool ALLZERO, NALLZERO;
    bool SDS;
    bool NVTest;
    bool TMPCS;
    bool CZTest, XactTest;
    bool MD3S;
    int fExact;

    dsp_alu_flags_t flags;

    for(inst.raw = 0xA000; inst.raw <= 0xFFFF; inst.raw += 1024)
      for(flags.zero = 0; flags.zero < 2; flags.zero++)
        for(flags.negative = 0; flags.negative < 2; flags.negative++)
          for(flags.carry = 0; flags.carry < 2; flags.carry++)
            for(flags.overflow = 0; flags.overflow < 2; flags.overflow++)
              for(fExact = 0; fExact < 2; fExact++)
                {
                  MD1 = !inst.branch.MODE1 && inst.branch.MODE0;
                  MD2 = inst.branch.MODE1 && !inst.branch.MODE0;
                  MD3 = inst.branch.MODE1 && inst.branch.MODE0;

                  STAT0  = (inst.branch.FLAGSEL && flags.carry) || (!inst.branch.FLAGSEL && flags.negative);
                  STAT1  = (inst.branch.FLAGSEL && flags.zero) || (!inst.branch.FLAGSEL && flags.overflow);
                  NSTAT0 = STAT0 != MD2;
                  NSTAT1 = STAT1 != MD2;

                  TDCARE1 = !inst.branch.FLAGM1 || (inst.branch.FLAGM1 && NSTAT0);
                  TDCARE0 = !inst.branch.FLAGM0 || (inst.branch.FLAGM0 && NSTAT1);

                  RDCARE = !inst.branch.FLAGM1 && !inst.branch.FLAGM0;

                  MD12S = TDCARE1 && TDCARE0 && (inst.branch.MODE1!=inst.branch.MODE0) && !RDCARE;

                  SUPER0 = MD1 && !inst.branch.FLAGSEL && RDCARE;
                  SUPER1 = MD1 && inst.branch.FLAGSEL &&  RDCARE;

                  ALLZERO  = SUPER0 && flags.zero && fExact;
                  NALLZERO = SUPER1 && !(flags.zero && fExact);

                  SDS = ALLZERO || NALLZERO;

                  NVTest   = ((((flags.negative != flags.overflow) ||
                                (flags.zero && inst.branch.FLAGM0)) != inst.branch.FLAGM1) &&
                              !inst.branch.FLAGSEL);
                  TMPCS    = flags.carry && !flags.zero;
                  CZTest   = (TMPCS != inst.branch.FLAGM0) && inst.branch.FLAGSEL && !inst.branch.FLAGM1;
                  XactTest = (fExact != inst.branch.FLAGM0) && inst.branch.FLAGSEL && inst.branch.FLAGM1;

                  MD3S = ((NVTest || CZTest || XactTest) && MD3);

                  DSP.BRCONDTAB[inst.br.bits][fExact+((flags.raw*0x10080402)>>24)] = (MD12S || MD3S || SDS);
                }
  }

  DSP.flags.Running = 0;
  DSP.flags.GenFIQ  = false;
  DSP.dregs.DSPPRLD = SYSTEM_TICKS;
  DSP.dregs.AUDCNT  = SYSTEM_TICKS;

  freedo_dsp_reset();

  /* ?? 8-CPU last, 4-DSP last, 2-CPU ACK, 1 DSP ACK ?? */
  DSP.dregs.Sema4Status = 0;

  for(i = 0; i < sizeof(DSP.NMem)/sizeof(DSP.NMem[0]); i++)
    DSP.NMem[i] = 0x8380; /* sleep */

  for(i = 0; i < 16; i++)
    DSP.CPUSupply[i] = 0;
}

void
freedo_dsp_reset(void)
{
  DSP.dregs.DSPPCNT  = DSP.dregs.DSPPRLD;
  DSP.dregs.PC       = 0;
  DSP.RBASEx4        = 0;
  DSP.REGi           = 0;
  DSP.flags.nOP_MASK = ~0;
}

uint32_t
freedo_dsp_loop(void)
{
  uint32_t Y;	/* accumulator */
  uint32_t BOP; /* 1st & 2nd operand */
  dsp_alu_flags_t flags;

  if(DSP.flags.Running & 1)
    {
      uint32_t AOP  = 0;        /* 1st operand */
      uint32_t RBSR = 0;	/* return address */
      bool     fExact = 0;
      bool     work   = true;

      freedo_dsp_reset();

      Y   = 0;
      BOP = 0;
      flags.raw = 0;

      do
        {
          ITAG_t inst;

          inst.raw = DSP.NMem[DSP.dregs.PC++];
          if(inst.aif.PAD)
            { // Control instruction
              switch((inst.raw >> 7) & 0xFF)
                {
                case 0:         /* NOP TODO */
                  break;
                case 1:         /* branch accum */
                  DSP.dregs.PC = ((Y >> 16) & 0x3FF);
                  break;
                case 2:         /* set rbase */
                  DSP.RBASEx4 = ((inst.cif.BCH_ADDR & 0x3F) << 2);
                  break;
                case 3:         /* set rmap */
                  DSP.REGi = (inst.cif.BCH_ADDR & 7);
                  break;
                case 4:         /* RTS */
                  DSP.dregs.PC = RBSR;
                  break;
                case 5:         /* set op_mask */
                  DSP.flags.nOP_MASK = ~(inst.cif.BCH_ADDR & 0x1F);
                  break;
                case 6:         /* -not used2- ins */
                  break;
                case 7:         /* sleep */
                  work = false;
                  break;
                case 8:
                case 9:
                case 10:
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                  /* jump */
                  DSP.dregs.PC = inst.cif.BCH_ADDR;
                  break;
                case 16:
                case 17:
                case 18:
                case 19:
                case 20:
                case 21:
                case 22:
                case 23:
                  /* jsr */
                  RBSR         = DSP.dregs.PC;
                  DSP.dregs.PC = inst.cif.BCH_ADDR;
                  break;
                case 24:
                case 25:
                case 26:
                case 27:
                case 28:
                case 29:
                case 30:
                case 31:
                  /* branch only if was branched */
                  DSP.dregs.PC = inst.cif.BCH_ADDR;
                  break;
                case 32:
                case 33:
                case 34:
                case 35:
                case 36:
                case 37:
                case 38:
                case 39:
                case 40:
                case 41:
                case 42:
                case 43:
                case 44:
                case 45:
                case 46:
                case 47: /* MOVEREG */
                  {
                    uint16_t op;
                    uint16_t addr;

                    op   = dsp_operand_load1();
                    addr = DSP.REGCONV[DSP.REGi][inst.r2of.R1] ^ DSP.RBASEx4;
                    if(inst.r2of.R1_DI)
                      addr = dsp_read(addr);
                    dsp_write(addr,op);
                  }
                  break;
                case 48:
                case 49:
                case 50:
                case 51:
                case 52:
                case 53:
                case 54:
                case 55:
                case 56:
                case 57:
                case 58:
                case 59:
                case 60:
                case 61:
                case 62:
                case 63: /* move */
                  {
                    uint16_t op;
                    uint16_t addr;

                    op   = dsp_operand_load1();
                    addr = inst.cif.BCH_ADDR;
                    if(inst.nrof.DI)
                      addr = dsp_read(addr);
                    dsp_write(addr,op);
                  }
                  break;
                default: /* condition branch */
                  if(1 & DSP.BRCONDTAB[inst.br.bits][fExact+((flags.raw*0x10080402)>>24)])
                    DSP.dregs.PC = inst.cif.BCH_ADDR;
                  break;
                }
            }
          else
            {
              /* ALU instruction */
              DSP.flags.req.raw = DSP.INSTTRAS[inst.raw].req.raw;
              DSP.flags.BS      = DSP.INSTTRAS[inst.raw].BS;

              dsp_operand_load(inst.aif.NUMOPS);

              switch(inst.aif.MUXA)
                {
                case 3:
                  if(inst.aif.M2SEL == 0)
                    {
                      if((inst.aif.ALU == 3) || (inst.aif.ALU == 5))  // ACSBU signal
                        AOP = (flags.carry ? ((int)DSP.flags.MULT1<<16) & ALUSIZEMASK : 0);
                      else
                        AOP = (((int)DSP.flags.MULT1 * (((int32_t)Y >> 15) & ~1)) & ALUSIZEMASK);
                    }
                  else
                    {
                      AOP = (((int)DSP.flags.MULT1 * (int)DSP.flags.MULT2 * 2) & ALUSIZEMASK);
                    }
                  break;
                case 1:
                  AOP = (DSP.flags.ALU1 << 16);
                  break;
                case 0:
                  AOP = Y;
                  break;
                case 2:
                  AOP = (DSP.flags.ALU2 << 16);
                  break;
                }

              /* ACSBU signal */
              if((inst.aif.ALU == 3) || (inst.aif.ALU == 5))
                {
                  BOP = (flags.carry << 16);
                }
              else
                {
                  switch(inst.aif.MUXB)
                    {
                    case 0:
                      BOP = Y;
                      break;
                    case 1:
                      BOP = (DSP.flags.ALU1 << 16);
                      break;
                    case 2:
                      BOP = (DSP.flags.ALU2 << 16);
                      break;
                    case 3:
                      if(inst.aif.M2SEL == 0) // ACSBU == 0 here always
                        BOP = (((int)DSP.flags.MULT1 * (((int32_t)Y >> 15)) & ~1) & ALUSIZEMASK);
                      else
                        BOP = (((int)DSP.flags.MULT1 * (int)DSP.flags.MULT2 * 2) & ALUSIZEMASK);
                      break;
                    }
                }

              /* Any ALU op. change overflow and possible carry */
              flags.carry    = 0;
              flags.overflow = 0;
              switch(inst.aif.ALU)
                {
                case 0:
                  Y = AOP;
                  break;
                  //*
                case 1:
                  Y = (0 - BOP);
                  flags.carry    = SUB_CFLAG(0,BOP,Y);
                  flags.overflow = SUB_VFLAG(0,BOP,Y);
                  break;
                case 2:
                case 3:
                  Y = (AOP + BOP);
                  flags.carry    = ADD_CFLAG(AOP,BOP,Y);
                  flags.overflow = ADD_VFLAG(AOP,BOP,Y);
                  break;
                case 4:
                case 5:
                  Y = (AOP - BOP);
                  flags.carry    = SUB_CFLAG(AOP,BOP,Y);
                  flags.overflow = SUB_VFLAG(AOP,BOP,Y);
                  break;
                case 6:
                  Y = (AOP + 0x1000);
                  flags.carry    = ADD_CFLAG(AOP,0x1000,Y);
                  flags.overflow = ADD_VFLAG(AOP,0x1000,Y);
                  break;
                case 7:
                  Y = (AOP - 0x1000);
                  flags.carry    = SUB_CFLAG(AOP,0x1000,Y);
                  flags.overflow = SUB_VFLAG(AOP,0x1000,Y);
                  break;
                case 8:		// A
                  Y = AOP;
                  break;
                case 9:		// NOT A
                  Y = (AOP ^ ALUSIZEMASK);
                  break;
                case 10:	// A AND B
                  Y = (AOP & BOP);
                  break;
                case 11:	// A NAND B
                  Y = ((AOP & BOP) ^ ALUSIZEMASK);
                  break;
                case 12:	// A OR B
                  Y= (AOP | BOP);
                  break;
                case 13:	// A NOR B
                  Y = ((AOP | BOP) ^ ALUSIZEMASK);
                  break;
                case 14:	// A XOR B
                  Y = (AOP ^ BOP);
                  break;
                case 15:	// A XNOR B
                  Y = (AOP ^ BOP ^ ALUSIZEMASK);
                  break;
                }

              flags.zero     = ((Y & 0xFFFF0000) ? 0 : 1);
              flags.negative = ((Y >> 31) ? 1 : 0);
              fExact         = ((Y & 0x0000F000) ? 0 : 1);

              //and BarrelShifter
              switch(DSP.flags.BS)
                {
                case 1:
                case 17:
                  Y = Y << 1;
                  break;
                case 2:
                case 18:
                  Y = Y << 2;
                  break;
                case 3:
                case 19:
                  Y = Y << 3;
                  break;
                case 4:
                case 20:
                  Y = Y << 4;
                  break;
                case 5:
                case 21:
                  Y = Y << 5;
                  break;
                case 6:
                case 22:
                  Y = Y << 8;
                  break;

                  //arithmetic shifts
                case 9:
                  Y  = ((int32_t)Y >> 16);
                  Y &= ALUSIZEMASK;
                  break;
                case 10:
                  Y  = ((int32_t)Y >> 8);
                  Y &= ALUSIZEMASK;
                  break;
                case 11:
                  Y  = ((int32_t)Y >> 5);
                  Y &= ALUSIZEMASK;
                  break;
                case 12:
                  Y  = ((int32_t)Y >> 4);
                  Y &= ALUSIZEMASK;
                  break;
                case 13:
                  Y  = ((int32_t)Y >> 3);
                  Y &= ALUSIZEMASK;
                  break;
                case 14:
                  Y  = ((int32_t)Y >> 2);
                  Y &= ALUSIZEMASK;
                  break;
                case 15:
                  Y  = ((int32_t)Y >> 1);
                  Y &= ALUSIZEMASK;
                  break;

                  // logocal shift
                case 7:         // CLIP ari
                case 23:        // CLIP log
                  if(1 & flags.overflow)
                    {
                      if(1 & flags.negative)
                        Y = 0x7FFFF000;
                      else
                        Y = 0x80000000;
                    }
                  break;
                case 8:         // Load operand load sameself again (ari)
                case 24:        // same, but logicalshift
                  {
                    //int temp=flags.carry;
                    flags.carry = ((signed)Y < 0); // shift out bit to Carry
                    //Y=Y<<1;
                    //Y|=temp<<16;
                    Y = (((Y << 1) & 0xFFFE0000)   |
                         (flags.carry ? 1<<16 : 0) |
                         (Y & 0xF000));
                  }
                  break;
                case 25:
                  Y  = ((uint32_t)Y >> 16);
                  Y &= ALUSIZEMASK;
                  break;
                case 26:
                  Y  = ((uint32_t)Y >> 8);
                  Y &= ALUSIZEMASK;
                  break;
                case 27:
                  Y  = ((uint32_t)Y >> 5);
                  Y &= ALUSIZEMASK;
                  break;
                case 28:
                  Y  = ((uint32_t)Y >> 4);
                  Y &= ALUSIZEMASK;
                  break;
                case 29:
                  Y  = ((uint32_t)Y >> 3);
                  Y &= ALUSIZEMASK;
                  break;
                case 30:
                  Y  = ((uint32_t)Y >> 2);
                  Y &= ALUSIZEMASK;
                  break;
                case 31:
                  Y  = ((uint32_t)Y >> 1);
                  Y &= ALUSIZEMASK;
                  break;
                }

              if(DSP.flags.WRITEBACK)
                dsp_write(DSP.flags.WRITEBACK,((int32_t)Y) >> 16);
            }

        } while(work);


      if(1 & DSP.flags.GenFIQ)
        {
          DSP.flags.GenFIQ = false;
          freedo_clio_fiq_generate(0x800,0); /* AudioFIQ */
        }

      DSP.dregs.DSPPCNT -= SYSTEM_TICKS;
      if(DSP.dregs.DSPPCNT <= 0)
        DSP.dregs.DSPPCNT += DSP.dregs.DSPPRLD;
    }

  return ((DSP.IMem[0x3FF] << 16) | DSP.IMem[0x3FE]);
}

/* CPU writes NMEM of DSP */
void
freedo_dsp_mem_write(uint16_t addr_,
                     uint16_t val_)
{
  //mwriteh(addr,val);
  DSP.NMem[addr_ & 0x3FF] = val_;
}

void
freedo_dsp_set_running(bool val_)
{
  DSP.flags.Running = val_;
}

/* CPU writes to EI,I of DSP */
void
freedo_dsp_imem_write(uint16_t addr_,
                      uint16_t val_)
{
  if((addr_ >= 0x70) && (addr_ <= 0x7C))
    {
      DSP.CPUSupply[addr_ - 0x70] = 1;
      DSP.IMem[addr_ & 0x7F]      = val_;
    }
  else if(!(addr_ & 0x80))
    {
      DSP.IMem[addr_ & 0x7F] = val_;
    }
}

void
freedo_dsp_arm_semaphore_write(uint32_t val_)
{
  // How about Sema4ACK? Now don't think about it
  // ARM write to Sema4Data low 16 bits
  // ARM be last
  DSP.dregs.Sema4Data   = (val_ & 0xFFFF);
  DSP.dregs.Sema4Status = 0x8;
}

/* CPU reads from EO,I of DSP */
uint16_t
freedo_dsp_imem_read(uint16_t addr_)
{
  switch(addr_)
    {
    case 0x3EB:
      return DSP.dregs.AudioOutStatus;
    case 0x3EC:
      return DSP.dregs.Sema4Status;
    case 0x3ED:
      return DSP.dregs.Sema4Data;
    case 0x3EE:
      return DSP.dregs.INT;
    case 0x3EF:
      return DSP.dregs.DSPPRLD;
    default:
      break;
    }

  return DSP.IMem[addr_];
}

uint32_t
freedo_dsp_arm_semaphore_read(void)
{
  return ((DSP.dregs.Sema4Status << 16) | DSP.dregs.Sema4Data);
}
