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

// DSP.cpp: implementation of the CDSP class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include "dsp.h"
#include "clio.h"
#include "freedocore.h"

#if 0 //20 bit ALU
#define ALUSIZEMASK 0xFFFFf000
#else //32 bit ALU
#define ALUSIZEMASK 0xFFFFFFFF
#endif

//////////////////////////////////////////////////////////////////////
// Flag logic
//////////////////////////////////////////////////////////////////////

#define TOPBIT 0x80000000
#define ADDCFLAG(A,B,rd) (((((A)&(B))&TOPBIT) || (((A)&~rd)&TOPBIT) || (((B)&~rd)&TOPBIT)) ? 1 : 0 )
#define SUBCFLAG(A,B,rd) (((((A)&~(B))&TOPBIT) || (((A)&~rd)&TOPBIT) || ((~(B)&~rd)&TOPBIT)) ? 1 : 0 )
#define ADDVFLAG(A,B,rd) ( ((((A)&(B)&~rd)&TOPBIT) || ((~(A)&~(B)&rd)&TOPBIT)) ? 1 : 0 )
#define SUBVFLAG(A,B,rd) ( ((((A)&~(B)&~rd)&TOPBIT) || ((~(A)&(B)&rd)&TOPBIT)) ? 1 : 0 )

#define WAVELET (11025)

uint16_t RegBase(unsigned int reg);
uint16_t ireadh(unsigned int addr);
void iwriteh(unsigned int addr, uint16_t val);
void OperandLoader(int Requests);
int  OperandLoaderNWB(void);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
#pragma pack(push,1)

struct CIFTAG{
  unsigned int	BCH_ADDR	:10;
  unsigned int	FLAG_MASK	:2;
  unsigned int	FLGSEL		:1;
  unsigned int	MODE		:2;
  unsigned int	PAD			:1;
};

struct BRNTAG
{
  unsigned int BCH_ADDR:10;
  unsigned int FLAGM0	:1;
  unsigned int FLAGM1	:1;
  unsigned int FLAGSEL:1;
  unsigned int MODE0	:1;
  unsigned int MODE1  :1;
  unsigned int AC		:1;
};
struct BRNBITS {
  unsigned int BCH_ADDR:10;
  unsigned int bits	:5;
  unsigned int AC		:1;
};

struct AIFTAG{
  unsigned int	BS			:4;
  unsigned int	ALU			:4;
  unsigned int	MUXB		:2;
  unsigned int	MUXA		:2;
  signed int	M2SEL		:1;
  unsigned int	NUMOPS		:2;
  signed int	PAD			:1;
};
struct IOFTAG{
  signed int	IMMEDIATE	:13;
  signed int	JUSTIFY		:1;
  unsigned int	TYPE		:2;
};
struct NROFTAG{
  unsigned int	OP_ADDR		:10;
  signed int	DI			:1;
  unsigned int	WB1			:1;
  unsigned int	PAD			:1;
  unsigned int	TYPE		:3;
};
struct R2OFTAG{
  unsigned int	R1			:4;
  signed int	R1_DI		:1;
  unsigned int	R2			:4;
  signed int	R2_DI		:1;
  unsigned int	NUMREGS		:1;
  unsigned int	WB1			:1;
  unsigned int	WB2			:1;
  unsigned int	TYPE		:3;
};
struct R3OFTAG{
  unsigned int	R1			:4;
  signed int	R1_DI		:1;
  unsigned int	R2			:4;
  signed int	R2_DI		:1;
  unsigned int	R3			:4;
  signed int	R3_DI		:1;
  unsigned int	TYPE		:1;
};

union ITAG{
  unsigned int raw;
  struct AIFTAG	aif;
  struct CIFTAG	cif;
  struct IOFTAG	iof;
  struct NROFTAG	nrof;
  struct R2OFTAG	r2of;
  struct R3OFTAG	r3of;
  struct BRNTAG	branch;
  struct BRNBITS br;
};

struct RQFTAG{

  unsigned int BS:1;
  unsigned int ALU2:1;
  unsigned int ALU1:1;
  unsigned int MULT2:1;
  unsigned int MULT1:1;

};
union _requnion
{
  unsigned char raw;
  struct RQFTAG	rq;
};

struct __INSTTRAS
{
  union _requnion req;
  char BS;		// 4+1 bits
}; // only for ALU command

struct REGSTAG{
  unsigned int PC;//0x0ee
  unsigned short NOISE;//0x0ea
  unsigned short AudioOutStatus;//audlock,lftfull,rgtfull -- 0x0eb//0x3eb
  unsigned short Sema4Status;//0x0ec//0x3ec
  unsigned short Sema4Data;//0x0ed//0x3ed
  short DSPPCNT;//0x0ef
  short DSPPRLD;//0x3ef
  short AUDCNT;
  unsigned short INT;//0x3ee
};
struct INTAG{

  signed short MULT1;
  signed short MULT2;

  signed short ALU1;
  signed short ALU2;

  int BS;

  unsigned short RMAP;
  unsigned short nOP_MASK;

  unsigned short WRITEBACK;

  union _requnion	req;

  bool Running;
  bool GenFIQ;

};

struct DSPDatum
{
  unsigned int RBASEx4;
  struct __INSTTRAS INSTTRAS[0x8000];
  unsigned short REGCONV[8][16];
  bool BRCONDTAB[32][32];
  unsigned short NMem[2048];
  unsigned short IMem[1024];
  int REGi;
  struct REGSTAG dregs;
  struct INTAG flags;
  unsigned int g_seed;
  bool CPUSupply[16];
};

#pragma pack(pop)

static struct DSPDatum dsp;

unsigned int _dsp_SaveSize(void)
{
  return sizeof(struct DSPDatum);
}

void _dsp_Save(void *buff)
{
  memcpy(buff,&dsp,sizeof(struct DSPDatum));
}

void _dsp_Load(void *buff)
{
  memcpy(&dsp,buff,sizeof(struct DSPDatum));
}

#define RBASEx4 dsp.RBASEx4
#define INSTTRAS dsp.INSTTRAS
#define REGCONV dsp.REGCONV
#define BRCONDTAB dsp.BRCONDTAB
#define NMem dsp.NMem
#define IMem dsp.IMem
#define REGi dsp.REGi
#define dregs dsp.dregs
#define flags dsp.flags
#define g_seed dsp.g_seed
#define CPUSupply dsp.CPUSupply

int fastrand(void)
{
  g_seed = 69069 * g_seed + 1;
  return g_seed & 0xFFFF;
}

void _dsp_Init(void)
{
  int a,c;
  union ITAG inst;
  unsigned int i;

  g_seed=0xa5a5a5a5;
  //FRAMES=0;
  for(a=0;a<16;a++)
    {
      for(c=0;c<8;c++)
        {
          flags.RMAP=c;
          REGCONV[c][a]=RegBase(a);
        }
    }

  for(inst.raw=0;inst.raw<0x8000;inst.raw++)
    {
      flags.req.raw=0;

      if(inst.aif.BS==0x8)
        flags.req.rq.BS=1;

      switch(inst.aif.MUXA)
        {
        case 3:
          flags.req.rq.MULT1=1;
          flags.req.rq.MULT2=inst.aif.M2SEL;
          break;
        case 1:
          flags.req.rq.ALU1=1;
          break;
        case 2:
          flags.req.rq.ALU2=1;
          break;
        }

      switch(inst.aif.MUXB)
        {
        case 1:
          flags.req.rq.ALU1=1;
          break;
        case 2:
          flags.req.rq.ALU2=1;
          break;
        case 3:
          flags.req.rq.MULT1=1;
          flags.req.rq.MULT2=inst.aif.M2SEL;
          break;
        }

      INSTTRAS[inst.raw].req.raw=flags.req.raw;
      INSTTRAS[inst.raw].BS=inst.aif.BS | ((inst.aif.ALU&8)<<(4-3));

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

    //		int Flags.Zero;
    //		int Flags.Nega;
    //		int Flags.Carry;//not borrow
    //		int Flags.Over;
    int fExact;

    union {
      struct {
        int8_t Zero;
        int8_t Nega;
        int8_t Carry;//not borrow
        int8_t Over;
      };
      unsigned int raw;
    } Flags;

    for(inst.raw=0xA000; inst.raw<=0xFFFF; inst.raw+=1024)
      for(Flags.Zero=0;Flags.Zero<2;Flags.Zero++)
        for(Flags.Nega=0;Flags.Nega<2;Flags.Nega++)
          for(Flags.Carry=0;Flags.Carry<2;Flags.Carry++)
            for(Flags.Over=0;Flags.Over<2;Flags.Over++)
              for(fExact=0;fExact<2;fExact++)
                {

                  MD1=!inst.branch.MODE1 && inst.branch.MODE0;
                  MD2=inst.branch.MODE1 && !inst.branch.MODE0;
                  MD3=inst.branch.MODE1 && inst.branch.MODE0;

                  STAT0=(inst.branch.FLAGSEL && Flags.Carry) || (!inst.branch.FLAGSEL && Flags.Nega);
                  STAT1=(inst.branch.FLAGSEL && Flags.Zero) || (!inst.branch.FLAGSEL && Flags.Over);
                  NSTAT0=STAT0!=MD2;
                  NSTAT1=STAT1!=MD2;

                  TDCARE1=!inst.branch.FLAGM1 || (inst.branch.FLAGM1 && NSTAT0);
                  TDCARE0=!inst.branch.FLAGM0 || (inst.branch.FLAGM0 && NSTAT1);

                  RDCARE=!inst.branch.FLAGM1 && !inst.branch.FLAGM0;

                  MD12S=TDCARE1 && TDCARE0 && (inst.branch.MODE1!=inst.branch.MODE0) && !RDCARE;

                  SUPER0= MD1 && !inst.branch.FLAGSEL && RDCARE;
                  SUPER1= MD1 && inst.branch.FLAGSEL && RDCARE;

                  ALLZERO=SUPER0 && Flags.Zero && fExact;
                  NALLZERO=SUPER1 && !(Flags.Zero && fExact);

                  SDS=ALLZERO || NALLZERO;

                  NVTest=( ((Flags.Nega!=Flags.Over) || (Flags.Zero && inst.branch.FLAGM0)) != inst.branch.FLAGM1) && !inst.branch.FLAGSEL;
                  TMPCS=Flags.Carry && ! Flags.Zero;
                  CZTest=(TMPCS != inst.branch.FLAGM0) && inst.branch.FLAGSEL && !inst.branch.FLAGM1;
                  XactTest=(fExact != inst.branch.FLAGM0) && inst.branch.FLAGSEL && inst.branch.FLAGM1;

                  MD3S=(NVTest || CZTest || XactTest)&&MD3;

                  BRCONDTAB[inst.br.bits][fExact+((Flags.raw*0x10080402)>>24)] = (MD12S || MD3S || SDS)? true : false;
                }
  }

  flags.Running=0;
  flags.GenFIQ=false;
  dregs.DSPPRLD=567;
  dregs.AUDCNT=567;

  _dsp_Reset();

  dregs.Sema4Status = 0; //?? 8-CPU last, 4-DSP last, 2-CPU ACK, 1 DSP ACK ??

  for( i=0; i < sizeof(NMem)/sizeof(NMem[0]); i++)
    NMem[i]=0x8380; //SLEEP

  for(i=0;i<16;i++)
    CPUSupply[i]=0;
}

void _dsp_Reset(void)
{
  dregs.DSPPCNT=dregs.DSPPRLD;
  dregs.PC=0;
  RBASEx4=0;
  REGi=0;
  flags.nOP_MASK=~0;
}

uint32_t _dsp_Loop(void)
{
  unsigned int BOP;	//1st & 2nd operand
  unsigned int Y;			//accumulator

  union {
    struct {
      bool Zero;
      bool Nega;
      bool Carry;//not borrow
      bool Over;
    };
    unsigned int raw;
  } Flags;

  if(flags.Running&1)
    {
      unsigned AOP  = 0; /* 1st operand */
      unsigned RBSR = 0;	/* return address */
      bool fExact   = 0;
      bool Work     = true;
      _dsp_Reset();

      Flags.raw=0;

      BOP=0;
      Y=0;

      do
        {
          union ITAG inst;

          inst.raw=NMem[dregs.PC++];
          //DSPCYCLES++;

          if(inst.aif.PAD)
            {//Control instruction
              switch((inst.raw>>7)&255) //special
                {
                case 0://NOP TODO
                  break;
                case 1://branch accum
                  dregs.PC=(Y>>16)&0x3ff;
                  break;
                case 2://set rbase
                  RBASEx4=(inst.cif.BCH_ADDR&0x3f)<<2;
                  break;
                case 3://set rmap
                  REGi=inst.cif.BCH_ADDR&7;
                  break;
                case 4://RTS
                  dregs.PC=RBSR;
                  break;
                case 5://set op_mask
                  flags.nOP_MASK=~(inst.cif.BCH_ADDR&0x1f);
                  break;
                case 6:// -not used2- ins
                  break;
                case 7://SLEEP
                  Work=false;
                  break;
                case 8:  case 9:  case 10: case 11:
                case 12: case 13: case 14: case 15:
                  //jump //branch only if not branched
                  dregs.PC=inst.cif.BCH_ADDR;
                  break;
                case 16: case 17: case 18: case 19:
                case 20: case 21: case 22: case 23:
                  //jsr
                  RBSR=dregs.PC;
                  dregs.PC=inst.cif.BCH_ADDR;
                  break;
                case 24: case 25: case 26: case 27:
                case 28: case 29: case 30: case 31:
                  // branch only if was branched
                  dregs.PC=inst.cif.BCH_ADDR;
                  break;
                case 32: case 33: case 34: case 35:
                case 36: case 37: case 38: case 39:
                case 40: case 41: case 42: case 43: // ??? -not used- instr's
                case 44: case 45: case 46: case 47: // ??? -not used- instr's
                  // MOVEREG
                  {
                    int Operand=OperandLoaderNWB();
                    if(inst.r2of.R1_DI)
                      iwriteh(ireadh(REGCONV[REGi][inst.r2of.R1]^RBASEx4),Operand);
                    else
                      iwriteh(REGCONV[REGi][inst.r2of.R1]^RBASEx4,Operand);
                  }
                  break;
                case 48: case 49: case 50: case 51:
                case 52: case 53: case 54: case 55:
                case 56: case 57: case 58: case 59:
                case 60: case 61: case 62: case 63:
                  // MOVE
                  {
                    int Operand=OperandLoaderNWB();
                    if(inst.nrof.DI)
                      iwriteh(ireadh(inst.cif.BCH_ADDR),Operand);
                    else
                      iwriteh(inst.cif.BCH_ADDR,Operand);
                  }
                  break;
                default: // Coundition branch
                  if(1&BRCONDTAB[inst.br.bits][fExact+((Flags.raw*0x10080402)>>24)]) dregs.PC=inst.cif.BCH_ADDR;
                  break;
                }//switch((inst.raw>>7)&255) //special
            }
          else
            {
              //ALU instruction
              flags.req.raw=INSTTRAS[inst.raw].req.raw;
              flags.BS     =INSTTRAS[inst.raw].BS;

              OperandLoader(inst.aif.NUMOPS);

              switch(inst.aif.MUXA)
                {
                case 3:
                  if(inst.aif.M2SEL==0)
                    {
                      if((inst.aif.ALU==3)||(inst.aif.ALU==5)) // ACSBU signal
                        AOP=Flags.Carry? ((int)flags.MULT1<<16)&ALUSIZEMASK : 0;
                      else
                        AOP=( ((int)flags.MULT1*(((signed int)Y>>15)&~1))&ALUSIZEMASK );
                    }
                  else
                    AOP=( ((int)flags.MULT1*(int)flags.MULT2*2)&ALUSIZEMASK );
                  break;
                case 1:
                  AOP=flags.ALU1<<16;
                  break;
                case 0:
                  AOP=Y;
                  break;
                case 2:
                  AOP=flags.ALU2<<16;
                  break;
                }

              if((inst.aif.ALU==3)||(inst.aif.ALU==5)) // ACSBU signal
                BOP=Flags.Carry<<16;
              else
                {
                  switch(inst.aif.MUXB)
                    {
                    case 0:
                      BOP=Y;
                      break;
                    case 1:
                      BOP=flags.ALU1<<16;
                      break;
                    case 2:
                      BOP=flags.ALU2<<16;
                      break;
                    case 3:
                      if(inst.aif.M2SEL==0) // ACSBU==0 here always
                        BOP=( ((int)flags.MULT1*(((signed int)Y>>15))&~1)&ALUSIZEMASK );
                      else
                        BOP=( ((int)flags.MULT1*(int)flags.MULT2*2)&ALUSIZEMASK );
                      break;
                    }
                }
              //ok now ALU itself.
              //unsigned char ctt1,ctt2;
              Flags.Over=Flags.Carry=0; // Any ALU op. change Over and possible Carry
              switch(inst.aif.ALU)
                {
                case 0:
                  Y=AOP;
                  break;
                  //*
                case 1:
                  Y=0-BOP;
                  Flags.Carry=SUBCFLAG(0,BOP,Y);
                  Flags.Over=SUBVFLAG(0,BOP,Y);
                  break;
                case 2:
                case 3:
                  Y=AOP+BOP;
                  Flags.Carry=ADDCFLAG(AOP,BOP,Y);
                  Flags.Over=ADDVFLAG(AOP,BOP,Y);
                  break;
                case 4:
                case 5:
                  Y=AOP-BOP;
                  Flags.Carry=SUBCFLAG(AOP,BOP,Y);
                  Flags.Over=SUBVFLAG(AOP,BOP,Y);
                  break;
                case 6:
                  Y=AOP+0x1000;
                  Flags.Carry=ADDCFLAG(AOP,0x1000,Y);
                  Flags.Over=ADDVFLAG(AOP,0x1000,Y);
                  break;
                case 7:
                  Y=AOP-0x1000;
                  Flags.Carry=SUBCFLAG(AOP,0x1000,Y);
                  Flags.Over=SUBVFLAG(AOP,0x1000,Y);
                  break;

                case 8:		// A
                  Y=AOP;
                  break;
                case 9:		// NOT A
                  Y=AOP^ALUSIZEMASK;
                  break;
                case 10:	// A AND B
                  Y=AOP&BOP;
                  break;
                case 11:	// A NAND B
                  Y=(AOP&BOP)^ALUSIZEMASK;
                  break;
                case 12:	// A OR B
                  Y=AOP|BOP;
                  break;
                case 13:	// A NOR B
                  Y=(AOP|BOP)^ALUSIZEMASK;
                  break;
                case 14:	// A XOR B
                  Y=AOP^BOP;
                  break;
                case 15:	// A XNOR B
                  Y=AOP^BOP^ALUSIZEMASK;
                  break;
                }
              Flags.Zero=(Y&0xFFFF0000)?0:1;
              Flags.Nega=(Y>>31)?1:0;
              fExact=(Y&0x0000F000)?0:1;

              //and BarrelShifter
              switch(flags.BS)
                {
                case 1:
                case 17:
                  Y=Y<<1;
                  break;
                case 2:
                case 18:
                  Y=Y<<2;
                  break;
                case 3:
                case 19:
                  Y=Y<<3;
                  break;
                case 4:
                case 20:
                  Y=Y<<4;
                  break;
                case 5:
                case 21:
                  Y=Y<<5;
                  break;
                case 6:
                case 22:
                  Y=Y<<8;
                  break;

                  //arithmetic shifts
                case 9:
                  Y=(signed int)Y>>16;
                  Y&=ALUSIZEMASK;
                  break;
                case 10:
                  Y=(signed int)Y>>8;
                  Y&=ALUSIZEMASK;
                  break;
                case 11:
                  Y=(signed int)Y>>5;
                  Y&=ALUSIZEMASK;
                  break;
                case 12:
                  Y=(signed int)Y>>4;
                  Y&=ALUSIZEMASK;
                  break;
                case 13:
                  Y=(signed int)Y>>3;
                  Y&=ALUSIZEMASK;
                  break;
                case 14:
                  Y=(signed int)Y>>2;
                  Y&=ALUSIZEMASK;
                  break;
                case 15:
                  Y=(signed int)Y>>1;
                  Y&=ALUSIZEMASK;
                  break;

                  // logocal shift
                case 7: // CLIP ari
                case 23:// CLIP log
                  if(1&Flags.Over)
                    {
                      if(1&Flags.Nega)	Y=0x7FFFf000;
                      else				Y=0x80000000;
                    }
                  break;
                case 8: // Load operand load sameself again (ari)
                case 24:// same, but logicalshift
                  {
                    //int temp=Flags.Carry;
                    Flags.Carry=(signed)Y<0;	// shift out bit to Carry
                    //Y=Y<<1;
                    //Y|=temp<<16;
                    Y=((Y<<1)&0xfffe0000)|(Flags.Carry?1<<16:0)|(Y&0xf000);
                  }
                  break;
                case 25:
                  Y=(unsigned int)Y>>16;
                  Y&=ALUSIZEMASK;
                  break;
                case 26:
                  Y=(unsigned int)Y>>8;
                  Y&=ALUSIZEMASK;
                  break;
                case 27:
                  Y=(unsigned int)Y>>5;
                  Y&=ALUSIZEMASK;
                  break;
                case 28:
                  Y=(unsigned int)Y>>4;
                  Y&=ALUSIZEMASK;
                  break;
                case 29:
                  Y=(unsigned int)Y>>3;
                  Y&=ALUSIZEMASK;
                  break;
                case 30:
                  Y=(unsigned int)Y>>2;
                  Y&=ALUSIZEMASK;
                  break;
                case 31:
                  Y=(unsigned int)Y>>1;
                  Y&=ALUSIZEMASK;
                  break;
                }

              //now write back. (assuming in WRITEBACK there is an address where to write
              if(flags.WRITEBACK)
                iwriteh(flags.WRITEBACK,((signed int)Y)>>16);
            }

        }while(Work);//big while!!!


      if(1&flags.GenFIQ)
        {
          flags.GenFIQ=false;
          freedo_clio_fiq_generate(0x800,0);//AudioFIQ
          //printf("#!!! AudioFIQ Generated 0x%4.4X\n!!!",val);
        }

      dregs.DSPPCNT-=567;
      if(dregs.DSPPCNT<=0)
        dregs.DSPPCNT+=dregs.DSPPRLD;
    }
  return ((IMem[0x3ff]<<16)|IMem[0x3fe]);
}

void  _dsp_WriteMemory(uint16_t addr, uint16_t val) //CPU writes NMEM of DSP
{
  //mwriteh(addr,val);
  //printf("#NWRITE 0x%3.3X<=0x%4.4X\n",addr,val);
  NMem[addr&0x3ff]=val;
}

uint16_t  RegBase(unsigned int reg)
{
  uint8_t twi,x,y;

  reg &= 0xf;
  x    = (reg >> 2) & 1;
  y    = (reg >> 3) & 1;

  switch(flags.RMAP)
    {
    case 0:
    case 1:
    case 2:
    case 3:
      twi=x;
      break;
    case 4:
      twi=y;
      break;
    case 5:
      twi=!y;
      break;
    case 6:
      twi=x&y;
      break;
    case 7:
      twi=x|y;
      break;
    }

  return ((reg & 7) | (twi << 8) | (reg >> 3) << 9);
}

uint16_t  ireadh(unsigned int addr) //DSP IREAD (includes EI, I)
{
  uint16_t val;

  //	addr&=0x3ff;
  switch(addr)
    {
    case 0xea:
      dregs.NOISE=fastrand();
      return dregs.NOISE;
    case 0xeb:
      //printf("#DSP read AudioOutStatus (0x%4.4X)\n",dregs.AudioOutStatus);
      return dregs.AudioOutStatus;
    case 0xec:
      //printf("#DSP read Sema4Status: 0x%4.4X\n",dregs.Sema4Status);
      return dregs.Sema4Status;
    case 0xed:
      //printf("#DSP read Sema4Data: 0x%4.4X\n",dregs.Sema4Data);
      return dregs.Sema4Data;
    case 0xee:
      //printf("#DSP read PC (0x%4.4X)\n",dregs.PC);
      return dregs.PC;
    case 0xef:
      //printf("#DSP read DSPPCNT (0x%4.4X)\n",dregs.DSPPCNT); //?? 0x4000 always
      return dregs.DSPPCNT;
    case 0xf0:	case 0xf1:	case 0xf2:	case 0xf3:
    case 0xf4:	case 0xf5:	case 0xf6:	case 0xf7:
    case 0xf8:	case 0xf9:	case 0xfa:	case 0xfb:
    case 0xfc:
      if(CPUSupply[addr-0xf0])
        {
          CPUSupply[addr-0xf0]=0;
          //printf("#DSP read from CPU!!! chan=0x%x\n",addr&0x0f);
          //val=IMem[addr-0x80];
          val=(fastrand()<<16)|fastrand();
        }
      else
        val=freedo_clio_fifo_ei(addr&0x0f);
      return val;
    case 0x70:	case 0x71:	case 0x72:	case 0x73:
    case 0x74:	case 0x75:	case 0x76:	case 0x77:
    case 0x78:	case 0x79:	case 0x7a:	case 0x7b:
    case 0x7c:
      if(CPUSupply[addr-0x70])
        {
          CPUSupply[addr-0x70]=0;
          //printf("#DSP read from CPU!!! chan=0x%x\n",addr&0x0f);
          return IMem[addr];
        }
      return freedo_clio_fifo_ei_read(addr&0x0f);
    case 0xd0:	case 0xd1:	case 0xd2:	case 0xd3:
    case 0xd4:	case 0xd5:	case 0xd6:	case 0xd7:
    case 0xd8:	case 0xd9:	case 0xda:	case 0xdb:
    case 0xdc:	case 0xdd:	case 0xde: // what is last two case's?
      //if(CPUSupply[addr&0x0f])
      //{
      //CPUSupply[addr&0x0f]=0;
      //printf("#DSP read from CPU!!! chan=0x%x\n",addr&0x0f);
      //return IMem[0x70+addr&0x0f];
      //}
      //else
      //printf("#DSP read EIFifo status 0x%4.4X\n",addr&0x0f);
      if(CPUSupply[addr&0xf])
        return 2;
      return freedo_clio_fifo_ei_status(addr&0xf);
    case 0xe0:
    case 0xe1:
    case 0xe2:
    case 0xe3:
      //printf("#DSP read EOFifo status 0x%4.4X\n",addr&0x0f);
      return freedo_clio_fifo_eo_status(addr&0x0f);
    default:
      //printf("#EIRead 0x%3.3X>=0x%4.4X\n",addr, IMem[addr&0x7f]);
      addr-=0x100;
      if((addr<0x200))
        return IMem[addr|0x100];
    }

  return IMem[addr&0x7f];
}

void  iwriteh(unsigned int addr, uint16_t val) //DSP IWRITE (includes EO,I)
{
  //uint16_t imem;
  addr&=0x3ff;
  switch(addr)
    {
    case 0x3eb:
      dregs.AudioOutStatus=val;
      break;
    case 0x3ec:
      /* DSP write to Sema4ACK */
      dregs.Sema4Status|=0x1;
      break;
    case 0x3ed:
      dregs.Sema4Data=val;
      dregs.Sema4Status=0x4;  // DSP write to Sema4Data
      break;
    case 0x3ee:
      dregs.INT=val;
      flags.GenFIQ=true;
      break;
    case 0x3ef:
      dregs.DSPPRLD=val;
      break;
    case 0x3f0:
    case 0x3f1:
    case 0x3f2:
    case 0x3f3:
      freedo_clio_fifo_eo(addr&0x0f,val);
      break;
    case 0x3fd:
      //FLUSH EOFIFO
      break;
    case 0x3fe: // DAC Left channel
    case 0x3ff: // DAC Right channel
      IMem[addr]=val;
      break;
    default:
      if(addr<0x100)
        return;

      addr-=0x100;
      if(addr<0x200)
        IMem[addr | 0x100]=val;
      else
        IMem[addr + 0x100]=val;
      break;
    }
}

void  _dsp_SetRunning(bool val)
{
  flags.Running= val;
}

void  _dsp_WriteIMem(uint16_t addr, uint16_t val)//CPU writes to EI,I of DSP
{
  if (addr >= 0x70 && addr <= 0x7c)
    {
      CPUSupply[addr-0x70]=1;
      //printf("# Coeff ARM write=0x%3.3X, val=0x%4.4X\n",addr,val);
      IMem[addr&0x7f]=val;
#if 0
      printf(">>>ARM TO DSP FIFO DIRECT!!!\n");
#endif
    }
  else
    {
      if(!(addr&0x80))
        IMem[addr&0x7f]=val;
      else
        {
#if 0
          printf(">>>ARM TO DSP HZ!!!\n");
#endif
        }
    }
}

void  _dsp_ARMwrite2sema4(unsigned int val)
{
  // How about Sema4ACK? Now don't think about it
  dregs.Sema4Data=val&0xffff;	// ARM write to Sema4Data low 16 bits
  dregs.Sema4Status=0x8;		// ARM be last
  //printf("#Arm write Sema4Data=0x%4.4X\n",val);
  //printf("Sema4Status set to 0x%4.4X\n",dregs.Sema4Status);
}


uint16_t  _dsp_ReadIMem(uint16_t addr) //CPU reads from EO,I of DSP
{

  switch(addr)
    {
    case 0x3eb:
      return dregs.AudioOutStatus;
    case 0x3ec:
      return dregs.Sema4Status;
    case 0x3ed:
      return dregs.Sema4Data;
    case 0x3ee:
      return dregs.INT;
    case 0x3ef:
      return dregs.DSPPRLD;
    default:
      break;
    }

  //	printf("#Arm read IMem[0x%3.3X]=0x%4.4X\n",addr, IMem[addr]);
  return IMem[addr];

}

unsigned int _dsp_ARMread2sema4(void)
{
  //printf("#Arm read both Sema4Status & Sema4Data\n");
  return (dregs.Sema4Status<<16) | dregs.Sema4Data;
}

void  OperandLoader(int Requests)
{
  int Operands;//total of operands
  int Ptr;
  uint16_t OperandPool[6]; // c'mon -- 5 is real maximum
  uint16_t GWRITEBACK;

  union ITAG operand;

  flags.WRITEBACK=0;

  if(Requests==0)
    {
      if(flags.req.raw)
        Requests=4;
      else
        return;
    }

  //DSPCYCLES+=Requests;
  Operands=0;
  GWRITEBACK=0;

  do
    {
      operand.raw=NMem[dregs.PC++];
      if(operand.nrof.TYPE==4)
        {
          //non reg format ///IT'S an address!!!
          if(operand.nrof.DI)
            OperandPool[Operands]=ireadh(flags.WRITEBACK=ireadh(operand.nrof.OP_ADDR));
          else
            OperandPool[Operands]=ireadh(flags.WRITEBACK=       operand.nrof.OP_ADDR);
          Operands++;

          if(operand.nrof.WB1)
            GWRITEBACK=flags.WRITEBACK;

        }else if ((operand.nrof.TYPE&6)==6)
        {
          //case 6: and case 7:  immediate format
          OperandPool[Operands]=operand.iof.IMMEDIATE<<(operand.iof.JUSTIFY&3);
          flags.WRITEBACK=OperandPool[Operands++];

        }else if(!(operand.nrof.TYPE&4))  // case 0..3
        {
          if(operand.r3of.R3_DI)
            OperandPool[Operands]=ireadh(ireadh(REGCONV[REGi][operand.r3of.R3]^RBASEx4));
          else
            OperandPool[Operands]=ireadh(       REGCONV[REGi][operand.r3of.R3]^RBASEx4 );
          Operands++;

          if(operand.r3of.R2_DI)
            OperandPool[Operands]=ireadh(ireadh(REGCONV[REGi][operand.r3of.R2]^RBASEx4));
          else
            OperandPool[Operands]=ireadh(       REGCONV[REGi][operand.r3of.R2]^RBASEx4 );
          Operands++;

          // only R1 can be WRITEBACK
          if(operand.r3of.R1_DI)
            OperandPool[Operands]=ireadh(flags.WRITEBACK=ireadh(REGCONV[REGi][operand.r3of.R1]^RBASEx4));
          else
            OperandPool[Operands]=ireadh(flags.WRITEBACK=       REGCONV[REGi][operand.r3of.R1]^RBASEx4 );
          Operands++;

        }else //if(operand.nrof.TYPE==5)
        {
          //regged 1/2 format
          if(operand.r2of.NUMREGS)
            {
              if(operand.r2of.R2_DI)
                OperandPool[Operands]=ireadh(flags.WRITEBACK=ireadh(REGCONV[REGi][operand.r2of.R2]^RBASEx4));
              else
                OperandPool[Operands]=ireadh(flags.WRITEBACK=       REGCONV[REGi][operand.r2of.R2]^RBASEx4 );
              Operands++;

              if(operand.r2of.WB2)
                GWRITEBACK=flags.WRITEBACK;
            }

          if(operand.r2of.R1_DI)
            OperandPool[Operands]=ireadh(flags.WRITEBACK=ireadh(REGCONV[REGi][operand.r2of.R1]^RBASEx4));
          else
            OperandPool[Operands]=ireadh(flags.WRITEBACK=       REGCONV[REGi][operand.r2of.R1]^RBASEx4 );
          Operands++;

          if(operand.r2of.WB1)
            GWRITEBACK=flags.WRITEBACK;
        }//if
    }while(Operands<Requests);


  //ok let's clean out Requests (using op_mask)
  flags.req.raw&=flags.nOP_MASK;

  Ptr=0;

  if(flags.req.rq.MULT1)
    flags.MULT1=OperandPool[Ptr++];
  if(flags.req.rq.MULT2)
    flags.MULT2=OperandPool[Ptr++];

  if(flags.req.rq.ALU1)
    flags.ALU1=OperandPool[Ptr++];
  if(flags.req.rq.ALU2)
    flags.ALU2=OperandPool[Ptr++];

  if(flags.req.rq.BS)
    flags.BS=OperandPool[Ptr++];

  if(Operands!=Ptr)
    {
      if(GWRITEBACK)
        flags.WRITEBACK=GWRITEBACK;
      //else flags.WRITEBACK=0;
    }
  else
    flags.WRITEBACK=GWRITEBACK;
}

int  OperandLoaderNWB(void)
{
  union ITAG operand;
  int Operand = 0;

  operand.raw=NMem[dregs.PC++];
  if(operand.nrof.TYPE==4)
    {
      //non reg format ///IT'S an address!!!
      if(operand.nrof.DI)
        Operand=ireadh(ireadh(operand.nrof.OP_ADDR));
      else
        Operand=ireadh(       operand.nrof.OP_ADDR);
    }
  else if(!(operand.nrof.TYPE & 4))  // case 0..3
    {
      if(operand.r3of.R3_DI) // ???
        Operand=ireadh(ireadh(REGCONV[REGi][operand.r3of.R3]^RBASEx4));
      else
        Operand=ireadh(       REGCONV[REGi][operand.r3of.R3]^RBASEx4 );
    }
  else if ((operand.nrof.TYPE & 6) == 6)
    {
      //case 6: and case 7:  immediate format
      Operand=operand.iof.IMMEDIATE<<(operand.iof.JUSTIFY&3);
    }
  else if(operand.nrof.TYPE == 5)
    { //if(operand.r2of.NUMREGS) ignore... It's right?
      if(operand.r2of.R1_DI)
        Operand = ireadh(ireadh(REGCONV[REGi][operand.r2of.R1]^RBASEx4));
      else
        Operand = ireadh(       REGCONV[REGi][operand.r2of.R1]^RBASEx4 );
    }

  return Operand;
}
