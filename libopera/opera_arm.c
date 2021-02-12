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

#include "bool.h"
#include "endianness.h"
#include "hack_flags.h"
#include "inline.h"
#include "opera_arm.h"
#include "opera_arm_core.h"
#include "opera_clio.h"
#include "opera_core.h"
#include "opera_diag_port.h"
#include "opera_fixedpoint_math.h"
#include "opera_madam.h"
#include "opera_sport.h"
#include "opera_swi_hle_0x5XXXX.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARM_INITIAL_PC  0x03000000

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

#define ARM_MODE_USER   0
#define ARM_MODE_FIQ    1
#define ARM_MODE_IRQ    2
#define ARM_MODE_SVC    3
#define ARM_MODE_ABT    4
#define ARM_MODE_UND    5
#define ARM_MODE_UNK    0xff

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

#define NCYCLE 4
#define SCYCLE 1
#define ICYCLE 1

//--------------------------Conditions-------------------------------------------
// flags - N Z C V  -  31...28
const static uint16_t cond_flags_cross[]=
  {
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

#define DRAM_SIZE  ( 2 * 1024 * 1024)
#define VRAM_SIZE  ( 1 * 1024 * 1024)
#define RAM_SIZE   ( 3 * 1024 * 1024)
#define ROM1_SIZE  ( 1 * 1024 * 1024)
#define ROM2_SIZE  ( 1 * 1024 * 1024)
#define NVRAM_SIZE (32 * 1024)

static int        g_SWI_HLE;
static arm_core_t CPU;
static int        CYCLES;	//cycle counter

static uint32_t readusr(uint32_t rn);
static void     loadusr(uint32_t rn, uint32_t val);
static uint32_t mreadb(uint32_t addr);
static void     mwriteb(uint32_t addr, uint8_t val);
static uint32_t mreadw(uint32_t addr);
static void     mwritew(uint32_t addr,uint32_t val);

uint8_t*
opera_arm_nvram_get(void)
{
  return CPU.nvram;
}

uint64_t
opera_arm_nvram_size(void)
{
  return NVRAM_SIZE;
}

uint8_t*
opera_arm_rom1_get(void)
{
  return CPU.rom1;
}

uint64_t
opera_arm_rom1_size(void)
{
  return ROM1_SIZE;
}

void
opera_arm_rom1_byteswap_if_necessary(void)
{
  uint8_t *rom;
  int64_t  size;

  rom  = opera_arm_rom1_get();
  size = opera_arm_rom1_size();

  swap32_array_if_little_endian((uint32_t*)rom,(size / sizeof(uint32_t)));
}

uint8_t*
opera_arm_rom2_get(void)
{
  return CPU.rom2;
}

uint64_t
opera_arm_rom2_size(void)
{
  return ROM2_SIZE;
}

void
opera_arm_rom2_byteswap_if_necessary(void)
{
  uint8_t *rom;
  int64_t  size;

  rom  = opera_arm_rom2_get();
  size = opera_arm_rom2_size();

  swap32_array_if_little_endian((uint32_t*)rom,(size / sizeof(uint32_t)));
}

uint8_t*
opera_arm_ram_get(void)
{
  return CPU.ram;
}

uint64_t
opera_arm_ram_size(void)
{
  return DRAM_SIZE;
}

uint8_t*
opera_arm_vram_get(void)
{
  return (CPU.ram + DRAM_SIZE);
}

uint64_t
opera_arm_vram_size(void)
{
  return VRAM_SIZE;
}

uint32_t
opera_arm_state_size(void)
{
  return (sizeof(arm_core_t) + RAM_SIZE + ROM1_SIZE + NVRAM_SIZE);
}

void
opera_arm_state_save(void *buf_)
{
  memcpy(buf_,&CPU,sizeof(arm_core_t));
  memcpy(((uint8_t*)buf_)+sizeof(arm_core_t),CPU.ram,RAM_SIZE);
  memcpy(((uint8_t*)buf_)+sizeof(arm_core_t)+RAM_SIZE,CPU.rom1,ROM1_SIZE);
  memcpy(((uint8_t*)buf_)+sizeof(arm_core_t)+RAM_SIZE+ROM1_SIZE,CPU.nvram,NVRAM_SIZE);
}

void
opera_arm_state_load(const void *buf_)
{
  uint8_t i;
  uint8_t *ram   = CPU.ram;
  uint8_t *rom1  = CPU.rom1;
  uint8_t *rom2  = CPU.rom2;
  uint8_t *nvram = CPU.nvram;

  memcpy(&CPU,buf_,sizeof(arm_core_t));
  memcpy(ram,((uint8_t*)buf_)+sizeof(arm_core_t),RAM_SIZE);
  memcpy(rom1,((uint8_t*)buf_)+sizeof(arm_core_t)+RAM_SIZE,ROM1_SIZE);
  memcpy(nvram,((uint8_t*)buf_)+sizeof(arm_core_t)+RAM_SIZE+ROM1_SIZE,NVRAM_SIZE);

  for(i = 3; i < 18; i++)
    memcpy(ram + (i * 1024 * 1024),
           ram + (2 * 1024 * 1024),
           1024 * 1024);

  CPU.ram   = ram;
  CPU.rom1  = rom1;
  CPU.rom2  = rom2;
  CPU.nvram = nvram;
}

static
void
ARM_RestUserRONS(void)
{
  switch(arm_mode_table[CPU.CPSR & 0x1F])
    {
    case ARM_MODE_USER:
      break;
    case ARM_MODE_FIQ:
      memcpy(CPU.FIQ,&CPU.USER[8],7<<2);
      memcpy(&CPU.USER[8],CPU.CASH,7<<2);
      break;
    case ARM_MODE_IRQ:
      CPU.IRQ[0]   = CPU.USER[13];
      CPU.IRQ[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.CASH[5];
      CPU.USER[14] = CPU.CASH[6];
      break;
    case ARM_MODE_SVC:
      CPU.SVC[0]   = CPU.USER[13];
      CPU.SVC[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.CASH[5];
      CPU.USER[14] = CPU.CASH[6];
      break;
    case ARM_MODE_ABT:
      CPU.ABT[0]   = CPU.USER[13];
      CPU.ABT[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.CASH[5];
      CPU.USER[14] = CPU.CASH[6];
      break;
    case ARM_MODE_UND:
      CPU.UND[0]   = CPU.USER[13];
      CPU.UND[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.CASH[5];
      CPU.USER[14] = CPU.CASH[6];
      break;
    }
}

static
void
ARM_RestFiqRONS(void)
{
  switch(arm_mode_table[CPU.CPSR & 0x1F])
    {
    case ARM_MODE_USER:
      memcpy(CPU.CASH,&CPU.USER[8],7<<2);
      memcpy(&CPU.USER[8],CPU.FIQ,7<<2);
      break;
    case ARM_MODE_FIQ:
      break;
    case ARM_MODE_IRQ:
      memcpy(CPU.CASH,&CPU.USER[8],5<<2);
      CPU.IRQ[0] = CPU.USER[13];
      CPU.IRQ[1] = CPU.USER[14];
      memcpy(&CPU.USER[8],CPU.FIQ,7<<2);
      break;
    case ARM_MODE_SVC:
      memcpy(CPU.CASH,&CPU.USER[8],5<<2);
      CPU.SVC[0] = CPU.USER[13];
      CPU.SVC[1] = CPU.USER[14];
      memcpy(&CPU.USER[8],CPU.FIQ,7<<2);
      break;
    case ARM_MODE_ABT:
      memcpy(CPU.CASH,&CPU.USER[8],5<<2);
      CPU.ABT[0] = CPU.USER[13];
      CPU.ABT[1] = CPU.USER[14];
      memcpy(&CPU.USER[8],CPU.FIQ,7<<2);
      break;
    case ARM_MODE_UND:
      memcpy(CPU.CASH,&CPU.USER[8],5<<2);
      CPU.UND[0] = CPU.USER[13];
      CPU.UND[1] = CPU.USER[14];
      memcpy(&CPU.USER[8],CPU.FIQ,7<<2);
      break;
    }
}

static
void
ARM_RestIrqRONS(void)
{
  switch(arm_mode_table[CPU.CPSR & 0x1F])
    {
    case ARM_MODE_USER:
      CPU.CASH[5]  = CPU.USER[13];
      CPU.CASH[6]  = CPU.USER[14];
      CPU.USER[13] = CPU.IRQ[0];
      CPU.USER[14] = CPU.IRQ[1];
      break;
    case ARM_MODE_FIQ:
      memcpy(CPU.FIQ,&CPU.USER[8],7<<2);
      memcpy(&CPU.USER[8],CPU.CASH,5<<2);
      CPU.USER[13] = CPU.IRQ[0];
      CPU.USER[14] = CPU.IRQ[1];
      break;
    case ARM_MODE_IRQ:
      break;
    case ARM_MODE_SVC:
      CPU.SVC[0]   = CPU.USER[13];
      CPU.SVC[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.IRQ[0];
      CPU.USER[14] = CPU.IRQ[1];
      break;
    case ARM_MODE_ABT:
      CPU.ABT[0]   = CPU.USER[13];
      CPU.ABT[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.IRQ[0];
      CPU.USER[14] = CPU.IRQ[1];
      break;
    case ARM_MODE_UND:
      CPU.UND[0]   = CPU.USER[13];
      CPU.UND[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.IRQ[0];
      CPU.USER[14] = CPU.IRQ[1];
      break;
    }
}

static
void
ARM_RestSvcRONS(void)
{
  switch(arm_mode_table[CPU.CPSR & 0x1F])
    {
    case ARM_MODE_USER:
      CPU.CASH[5]  = CPU.USER[13];
      CPU.CASH[6]  = CPU.USER[14];
      CPU.USER[13] = CPU.SVC[0];
      CPU.USER[14] = CPU.SVC[1];
      break;
    case ARM_MODE_FIQ:
      memcpy(CPU.FIQ,&CPU.USER[8],7<<2);
      memcpy(&CPU.USER[8],CPU.CASH,5<<2);
      CPU.USER[13] = CPU.SVC[0];
      CPU.USER[14] = CPU.SVC[1];
      break;
    case ARM_MODE_IRQ:
      CPU.IRQ[0]   = CPU.USER[13];
      CPU.IRQ[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.SVC[0];
      CPU.USER[14] = CPU.SVC[1];
      break;
    case ARM_MODE_SVC:
      break;
    case ARM_MODE_ABT:
      CPU.ABT[0]   = CPU.USER[13];
      CPU.ABT[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.SVC[0];
      CPU.USER[14] = CPU.SVC[1];
      break;
    case ARM_MODE_UND:
      CPU.UND[0]   = CPU.USER[13];
      CPU.UND[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.SVC[0];
      CPU.USER[14] = CPU.SVC[1];
      break;
    }
}

static
void
ARM_RestAbtRONS(void)
{
  switch(arm_mode_table[CPU.CPSR & 0x1F])
    {
    case ARM_MODE_USER:
      CPU.CASH[5]  = CPU.USER[13];
      CPU.CASH[6]  = CPU.USER[14];
      CPU.USER[13] = CPU.ABT[0];
      CPU.USER[14] = CPU.ABT[1];
      break;
    case ARM_MODE_FIQ:
      memcpy(CPU.FIQ,&CPU.USER[8],7<<2);
      memcpy(&CPU.USER[8],CPU.CASH,5<<2);
      CPU.USER[13] = CPU.ABT[0];
      CPU.USER[14] = CPU.ABT[1];
      break;
    case ARM_MODE_IRQ:
      CPU.IRQ[0]   = CPU.USER[13];
      CPU.IRQ[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.ABT[0];
      CPU.USER[14] = CPU.ABT[1];
      break;
    case ARM_MODE_SVC:
      CPU.SVC[0]   = CPU.USER[13];
      CPU.SVC[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.ABT[0];
      CPU.USER[14] = CPU.ABT[1];
      break;
    case ARM_MODE_ABT:
      break;
    case ARM_MODE_UND:
      CPU.UND[0]   = CPU.USER[13];
      CPU.UND[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.ABT[0];
      CPU.USER[14] = CPU.ABT[1];
      break;
    }
}

static
void
ARM_RestUndRONS(void)
{
  switch(arm_mode_table[CPU.CPSR & 0x1F])
    {
    case ARM_MODE_USER:
      CPU.CASH[5]  = CPU.USER[13];
      CPU.CASH[6]  = CPU.USER[14];
      CPU.USER[13] = CPU.UND[0];
      CPU.USER[14] = CPU.UND[1];
      break;
    case ARM_MODE_FIQ:
      memcpy(CPU.FIQ,&CPU.USER[8],7<<2);
      memcpy(&CPU.USER[8],CPU.CASH,5<<2);
      CPU.USER[13] = CPU.UND[0];
      CPU.USER[14] = CPU.UND[1];
      break;
    case ARM_MODE_IRQ:
      CPU.IRQ[0]   = CPU.USER[13];
      CPU.IRQ[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.UND[0];
      CPU.USER[14] = CPU.UND[1];
      break;
    case ARM_MODE_SVC:
      CPU.SVC[0]   = CPU.USER[13];
      CPU.SVC[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.UND[0];
      CPU.USER[14] = CPU.UND[1];
      break;
    case ARM_MODE_ABT:
      CPU.ABT[0]   = CPU.USER[13];
      CPU.ABT[1]   = CPU.USER[14];
      CPU.USER[13] = CPU.UND[0];
      CPU.USER[14] = CPU.UND[1];
      break;
    case ARM_MODE_UND:
      break;
    }
}

static
void
ARM_Change_ModeSafe(uint32_t mode_)
{
  switch(arm_mode_table[mode_ & 0x1F])
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

void
opera_arm_rom_select(int n_)
{
  CPU.rom = ((n_ == 0) ? CPU.rom1 : CPU.rom2);
}

static
INLINE
void
arm_cpsr_set(uint32_t a_)
{
  a_ |= 0x10;
  ARM_Change_ModeSafe(a_);
  CPU.CPSR = (a_ & 0xf00000df);
}


static
INLINE
void
SETM(uint32_t a_)
{
  a_ |= 0x10;
  ARM_Change_ModeSafe(a_);
  CPU.CPSR = ((CPU.CPSR & 0xffffffe0) | (a_ & 0x1F));
}

static
INLINE
void
SETN(int a_)
{
  CPU.CPSR = (CPU.CPSR & 0x7fffffff) | ((a_ ? 1 << 31 : 0));
}

static
INLINE
void
SETZ(int a_)
{
  CPU.CPSR = (CPU.CPSR & 0xbfffffff) | ((a_ ? 1 << 30 : 0));
}

static
INLINE
void
SETC(int a_)
{
  CPU.CPSR = (CPU.CPSR & 0xdfffffff) | ((a_ ? 1 << 29 : 0));
}

static
INLINE
void
SETV(int a_)
{
  CPU.CPSR = (CPU.CPSR & 0xefffffff) | ((a_ ? 1 << 28 : 0));
}

static
INLINE
void
SETI(int a_)
{
  CPU.CPSR = (CPU.CPSR & 0xffffff7f) | ((a_ ? 1 << 7 : 0));
}

static
INLINE
void
SETF(int a_)
{
  CPU.CPSR = (CPU.CPSR & 0xffffffbf) | ((a_ ? 1 << 6 : 0));
}

#define MODE ((CPU.CPSR & 0x1F))
#define ISN  ((CPU.CPSR >> 31) & 1)
#define ISZ  ((CPU.CPSR >> 30) & 1)
#define ISC  ((CPU.CPSR >> 29) & 1)
#define ISV  ((CPU.CPSR >> 28) & 1)
#define ISI  ((CPU.CPSR >>  7) & 1)
#define ISF  ((CPU.CPSR >>  6) & 1)

static
INLINE
uint32_t
ROTR(const uint32_t val_,
     const uint32_t shift_)
{
  return (shift_ ?
          ((val_ >> shift_) | (val_ << (32 - shift_))) :
          val_);
}

void
opera_arm_init(void)
{
  int i;

  g_SWI_HLE = 0;

  CYCLES = 0;
  for(i = 0; i < 16; i++)
    CPU.USER[i] = 0;

  for(i = 0; i < 2; i++)
    {
      CPU.SVC[i] = 0;
      CPU.ABT[i] = 0;
      CPU.IRQ[i] = 0;
      CPU.UND[i] = 0;
    }

  for(i = 0;i < 7; i++)
    CPU.CASH[i] = CPU.FIQ[i] = 0;

  CPU.ram   = calloc(RAM_SIZE + 1024*1024*16,1);
  CPU.rom1  = calloc(ROM1_SIZE,1);
  CPU.rom2  = calloc(ROM2_SIZE,1);
  CPU.rom   = CPU.rom1;
  CPU.nvram = calloc(NVRAM_SIZE,1);

  CPU.nFIQ = FALSE;
  CPU.MAS_Access_Exept = FALSE;

  CPU.USER[15] = ARM_INITIAL_PC;
  arm_cpsr_set(0x13);
}

void
opera_arm_destroy(void)
{
  if(CPU.nvram)
    free(CPU.nvram);
  CPU.nvram = NULL;

  if(CPU.rom1)
    free(CPU.rom1);
  CPU.rom1 = NULL;

  if(CPU.rom2)
    free(CPU.rom2);
  CPU.rom2 = NULL;

  if(CPU.ram)
    free(CPU.ram);
  CPU.ram = NULL;
}

void
opera_arm_reset(void)
{
  int i;

  CYCLES = 0;
  CPU.rom = CPU.rom1;

  for(i = 0; i < 16; i++)
    CPU.USER[i] = 0;

  for(i = 0; i < 2; i++)
    {
      CPU.SVC[i] = 0;
      CPU.ABT[i] = 0;
      CPU.IRQ[i] = 0;
      CPU.UND[i] = 0;
    }

  for(i = 0; i < 7; i++)
    CPU.CASH[i] = CPU.FIQ[i] = 0;

  CPU.MAS_Access_Exept = FALSE;

  CPU.nFIQ = FALSE;

  CPU.USER[15] = ARM_INITIAL_PC;
  arm_cpsr_set(0x13);

  opera_clio_reset();
  opera_madam_reset();
}

void
opera_arm_swi_hle_set(const int hle_)
{
  g_SWI_HLE = !!hle_;
}

int
opera_arm_swi_hle_get(void)
{
  return g_SWI_HLE;
}

static int32_t addrr = 0;
static int32_t vall  = 0;
static int32_t inuse = 0;

static
void
ldm_accur(uint32_t opc_,
          uint32_t base_,
          uint32_t rn_ind_)
{
  uint16_t x;
  uint16_t list;
  uint32_t i;
  uint32_t tmp;
  uint32_t base_comp;

  i = 0;
  x    = opc_ & 0xFFFF;
  list = opc_ & 0xFFFF;
  x = ((x & 0x5555) + ((x >> 1) & 0x5555));
  x = ((x & 0x3333) + ((x >> 2) & 0x3333));
  x = ((x & 0x00ff) + (x >> 8));
  x = ((x & 0x000f) + (x >> 4));

  switch((opc_>>23)&3)
    {
    case 0:
      base_     -= (x << 2);
      base_comp  = (base_ + 4);
      break;
    case 1:
      base_comp  = base_;
      base_     += (x << 2);
      break;
    case 2:
      base_comp = base_ = (base_ - (x << 2));
      break;
    case 3:
      base_comp  = (base_ + 4);
      base_     += (x<<2);
      break;
    }

  //base_comp&=~3;

  //if(opc_&(1<<21))CPU.USER[rn_ind_]=base_;

  if((opc_ & (1 << 22)) && !(opc_ & 0x8000))
    {
      if(opc_ & (1 << 21))
        loadusr(rn_ind_,base_);

      while(list)
        {
          if(list & 1)
            {
              tmp = mreadw(base_comp);
              /*
                if(MAS_Access_Exept)
                {
                if(opc_&(1<<21))
                CPU.USER[rn_ind_]=base_;
                break;
                }
              */
              loadusr(i,tmp);
              base_comp += 4;
            }

          i++;
          list >>= 1;
        }
    }
  else
    {
      if(opc_ & (1 << 21))
        CPU.USER[rn_ind_] = base_;

      while(list)
        {
          if(list & 1)
            {
              tmp = mreadw(base_comp);
              if((tmp == 0xF1000)         &&
                 (i == 0x1)               &&
                 (CPU.USER[2] != 0xF0000) &&
                 (CNBFIX == 0)            &&
                 (FIXMODE & FIX_BIT_TIMING_1))
                {
                  tmp+=0x1000;
                }

              if((inuse == 1) && (base_comp & 0x1FFFFF))
                {
                  if(base_comp == addrr)
                    inuse = 0;

                  if(tmp != vall)
                    {
                      if((tmp == 0xEFE54) &&
                         (i == 0x4)       &&
                         (CNBFIX == 0)    &&
                         (FIXMODE & FIX_BIT_TIMING_1))
                        tmp -= 0xF;
                    }
                }

              CPU.USER[i]  = tmp;
              base_comp   += 4;
            }

          i++;
          list >>= 1;
        }

      if((opc_ & (1 << 22)) && arm_mode_table[MODE] /*&& !MAS_Access_Exept*/)
        arm_cpsr_set(CPU.SPSR[arm_mode_table[MODE]]);
    }

  CYCLES -= ((x-1) * SCYCLE + NCYCLE + ICYCLE);
}

static
void
stm_accur(uint32_t opc_,
          uint32_t base_,
          uint32_t rn_ind_)
{
  uint16_t x;
  uint16_t list;
  uint32_t i;
  uint32_t base_comp;

  i = 0;
  x    = opc_ & 0xFFFF;
  list = opc_ & 0x7FFF;
  x = ((x & 0x5555) + ((x >> 1) & 0x5555));
  x = ((x & 0x3333) + ((x >> 2) & 0x3333));
  x = ((x & 0x00ff) + (x >> 8));
  x = ((x & 0x000f) + (x >> 4));

  switch((opc_ >> 23) & 3)
    {
    case 0:
      base_     -= (x << 2);
      base_comp  = (base_ + 4);
      break;
    case 1:
      base_comp  = base_;
      base_     += (x << 2);
      break;
    case 2:
      base_comp = base_ = (base_ - (x << 2));
      break;
    case 3:
      base_comp  = (base_ + 4);
      base_     += (x << 2);
      break;
    }

  if((opc_ & (1 << 22)))
    {
      if((opc_ & (1 << 21)) && (opc_ & ((1 << rn_ind_) - 1)))
        loadusr(rn_ind_,base_);

      while(list)
        {
          if(list & 1)
            {
              mwritew(base_comp,readusr(i));
              //if(MAS_Access_Exept)break;
              base_comp += 4;
            }

          i++;
          list >>= 1;
        }

      if(opc_ & (1 << 21))
        loadusr(rn_ind_,base_);
    }
  else
    {
      if((opc_ & (1 << 21)) && (opc_ & ((1 << rn_ind_) - 1)))
        CPU.USER[rn_ind_] = base_;

      while(list)
        {
          if(list&1)
            {
              mwritew(base_comp,CPU.USER[i]);
              if(base_comp & 0x1FFFFF)
                {
                  addrr = base_comp;
                  vall  = CPU.USER[i];
                  inuse = 1;
                }
              base_comp += 4;
            }

          i++;
          list >>= 1;
        }

      if(opc_ & (1 << 21))
        CPU.USER[rn_ind_] = base_;
    }

  if((opc_ & 0x8000) /*&& !MAS_Access_Exept*/)
    mwritew(base_comp,CPU.USER[15]+8);

  CYCLES -= ((x - 2) * SCYCLE + NCYCLE + NCYCLE);
}

static
void
bdt_core(uint32_t opc_)
{
  uint32_t base;
  uint32_t rn_ind = ((opc_ >> 16) & 0xF);

  if(rn_ind == 0xF)
    base = CPU.USER[rn_ind] + 8;
  else
    base = CPU.USER[rn_ind];

  if(opc_ & (1 << 20)) /* memory or register? */
    {
      if(opc_ & 0x8000)
        CYCLES -= (SCYCLE + NCYCLE);
      ldm_accur(opc_,base,rn_ind);
    }
  else
    {
      stm_accur(opc_,base,rn_ind);
    }
}

typedef struct TagArg
{
  uint32_t Type;
  uint32_t Arg;
} TagItem;


static
void
decode_swi_lle(void)
{
  CPU.SPSR[arm_mode_table[0x13]] = CPU.CPSR;

  SETI(1);
  SETM(0x13);

  CPU.USER[14] = CPU.USER[15];
  CPU.USER[15] = 0x00000008;
}

static void decode_swi_hle(const uint32_t op_)
{
  switch(op_ & 0x000FFFFF)
    {
    case 0x50000:
      opera_swi_hle_0x50000(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2]);
      return;
    case 0x50001:
      opera_swi_hle_0x50001(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2]);
      return;
    case 0x50002:
      opera_swi_hle_0x50002(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2],CPU.USER[3]);
      return;
    case 0x50003:
      break;
    case 0x50004:
      break;
    case 0x50005:
      opera_swi_hle_0x50005(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2],CPU.USER[3]);
      return;
    case 0x50006:
      opera_swi_hle_0x50006(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2],CPU.USER[3]);
      return;
    case 0x50007:
      opera_swi_hle_0x50007(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2]);
      return;
    case 0x50008:
      opera_swi_hle_0x50008(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2]);
      return;
    case 0x50009:
      opera_swi_hle_0x50009(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2],CPU.USER[3]);
      return;
    case 0x5000A:
      break;
    case 0x5000B:
      break;
    case 0x5000C:
      CPU.USER[0] = opera_swi_hle_0x5000C(CPU.ram,CPU.USER[0],CPU.USER[1]);
      return;
    case 0x5000E:
      opera_swi_hle_0x5000E(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2]);
      return;
    case 0x5000F:
      CPU.USER[0] = opera_swi_hle_0x5000F(CPU.ram,CPU.USER[0]);
      return;
    case 0x50010:
      CPU.USER[0] = opera_swi_hle_0x50010(CPU.ram,CPU.USER[0]);
      return;
    case 0x50011:
      opera_swi_hle_0x50011(CPU.ram,CPU.USER[0],CPU.USER[1],CPU.USER[2],CPU.USER[3]);
      return;
    case 0x50012:
      opera_swi_hle_0x50012(CPU.ram,CPU.USER[0]);
      return;
    }

  decode_swi_lle();
}

static void decode_swi(const uint32_t op_)
{
  CYCLES -= (SCYCLE + NCYCLE);  // +2S+1N

  if(g_SWI_HLE)
  {
    decode_swi_hle(op_);
    return;
  }

  decode_swi_lle();
}


static uint32_t carry_out = 0;

static
INLINE
void
ARM_SET_C(const uint32_t x_)
{
  CPU.CPSR = ((CPU.CPSR & 0xdfffffff) | ((x_ & 1) << 29));
}

static
INLINE
void
ARM_SET_Z(const uint32_t x_)
{
  CPU.CPSR = ((CPU.CPSR & 0xbfffffff) | (x_ == 0 ? 0x40000000 : 0));
}

static
INLINE
void
ARM_SET_N(const uint32_t x_)
{
  CPU.CPSR = ((CPU.CPSR & 0x7fffffff) | (x_ & 0x80000000));
}

static
INLINE
uint32_t
ARM_GET_C(void)
{
  return ((CPU.CPSR >> 29) & 1);
}

static
INLINE
void
ARM_SET_ZN(const uint32_t val_)
{
  if(val_)
    CPU.CPSR = ((CPU.CPSR & 0x3fffffff) | (val_ & 0x80000000));
  else
    CPU.CPSR = ((CPU.CPSR & 0x3fffffff) | 0x40000000);
}

static
INLINE
void
ARM_SET_CV(const uint32_t rd_,
           const uint32_t op1_,
           const uint32_t op2_)
{
  CPU.CPSR = ((CPU.CPSR & 0xcfffffff) |
              ((((op1_ & op2_) | ((~rd_) & (op1_ | op2_))) & 0x80000000) >> 2) |
              (((((op1_ & (op2_ & (~rd_))) | ((~op1_) & (~op2_) & rd_))) & 0x80000000) >> 3));
}

static
INLINE
void
ARM_SET_CV_sub(uint32_t rd_,
               uint32_t op1_,
               uint32_t op2_)
{
  CPU.CPSR = ((CPU.CPSR & 0xcfffffff) |
              ((((op1_ & (~op2_)) | ((~rd_) & (op1_ | (~op2_)))) & 0x80000000) >> 2) |
              (((((op1_ & ((~op2_) & (~rd_))) | ((~op1_) & op2_ & rd_))) & 0x80000000) >> 3));
}

static
INLINE
int
ARM_ALU_Exec(uint32_t  inst_,
             uint8_t   opc_,
             uint32_t  op1_,
             uint32_t  op2_,
             uint32_t *rd_)
{
  switch(opc_)
    {
    case 0:
      *rd_ = op1_ & op2_;
      break;
    case 2:
      *rd_ = op1_ ^ op2_;
      break;
    case 4:
      *rd_ = op1_ - op2_;
      break;
    case 6:
      *rd_ = op2_ - op1_;
      break;
    case 8:
      *rd_ = op1_ + op2_;
      break;
    case 10:
      *rd_ = op1_ + op2_ + ARM_GET_C();
      break;
    case 12:
      *rd_ = op1_ - op2_ - (ARM_GET_C() ^ 1);
      break;
    case 14:
      *rd_ = op2_ - op1_ - (ARM_GET_C() ^ 1);
      break;
    case 16:
    case 20:
      if((inst_ >> 22) & 1)
        CPU.USER[(inst_ >> 12) & 0xF] = CPU.SPSR[arm_mode_table[CPU.CPSR & 0x1F]];
      else
        CPU.USER[(inst_ >> 12) & 0xF] = CPU.CPSR;
      return TRUE;
    case 18:
    case 22:
      if(!((inst_ >> 16) & 0x1) || !(arm_mode_table[MODE]))
        {
          if((inst_ >> 22) & 1)
            CPU.SPSR[arm_mode_table[MODE]] = (CPU.SPSR[arm_mode_table[MODE]] & 0x0fffffff) | (op2_ & 0xf0000000);
          else
            CPU.CPSR = (CPU.CPSR & 0x0fffffff) | (op2_ & 0xf0000000);
        }
      else
        {
          if((inst_ >> 22) & 1)
            CPU.SPSR[arm_mode_table[MODE]] = op2_ & 0xf00000df;
          else
            arm_cpsr_set(op2_);
        }
      return TRUE;
    case 24:
      *rd_ = op1_ | op2_;
      break;
    case 26:
      *rd_ = op2_;
      break;
    case 28:
      *rd_ = op1_ & ~op2_;
      break;
    case 30:
      *rd_ = ~op2_;
      break;
    case 1:
      *rd_ = op1_ & op2_;
      ARM_SET_ZN(*rd_);
      break;
    case 3:
      *rd_ = op1_ ^ op2_;
      ARM_SET_ZN(*rd_);
      break;
    case 5:
      *rd_ = op1_ - op2_;
      ARM_SET_ZN(*rd_);
      ARM_SET_CV_sub(*rd_,op1_,op2_);
      break;
    case 7:
      *rd_ = op2_ - op1_;
      ARM_SET_ZN(*rd_);
      ARM_SET_CV_sub(*rd_,op2_,op1_);
      break;
    case 9:
      *rd_ = op1_ + op2_;
      ARM_SET_ZN(*rd_);
      ARM_SET_CV(*rd_,op1_,op2_);
      break;

    case 11:
      *rd_ = op1_ + op2_ + ARM_GET_C();
      ARM_SET_ZN(*rd_);
      ARM_SET_CV(*rd_,op1_,op2_);
      break;
    case 13:
      *rd_ = op1_ - op2_ - (ARM_GET_C()^1);
      ARM_SET_ZN(*rd_);
      ARM_SET_CV_sub(*rd_,op1_,op2_);
      break;
    case 15:
      *rd_ = op2_ - op1_ - (ARM_GET_C()^1);
      ARM_SET_ZN(*rd_);
      ARM_SET_CV_sub(*rd_,op2_,op1_);
      break;//*/
    case 17:
      op1_ &= op2_;
      ARM_SET_ZN(op1_);
      return TRUE;
    case 19:
      op1_ ^= op2_;
      ARM_SET_ZN(op1_);
      return TRUE;
    case 21:
      ARM_SET_CV_sub(op1_ - op2_,op1_,op2_);
      ARM_SET_ZN(op1_ - op2_);
      return TRUE;
    case 23:
      ARM_SET_CV(op1_ + op2_,op1_,op2_);
      ARM_SET_ZN(op1_ + op2_);
      return TRUE;
    case 25:
      *rd_ = op1_ | op2_;
      ARM_SET_ZN(*rd_);
      break;
    case 27:
      *rd_ = op2_;
      ARM_SET_ZN(*rd_);
      break;
    case 29:
      *rd_ = op1_ & ~op2_;
      ARM_SET_ZN(*rd_);
      break;
    case 31:
      *rd_ = ~op2_;
      ARM_SET_ZN(*rd_);
      break;
    };
  return FALSE;
}

static
uint32_t
ARM_SHIFT_NSC(uint32_t value_,
              uint8_t  shift_,
              uint8_t  type_)
{
  switch(type_)
    {
    case 0:
      if(shift_)
        {
          if(shift_ > 32)
            carry_out = 0;
          else
            carry_out = (((value_ << (shift_ - 1)) & 0x80000000) >> 31);
        }
      else
        {
          carry_out = ARM_GET_C();
        }

      if(shift_ == 0)
        return value_;
      if(shift_ > 31)
        return 0;
      return (value_ << shift_);
    case 1:
      if(shift_)
        {
          if(shift_ > 32)
            carry_out = 0;
          else
            carry_out = ((value_ >> (shift_ - 1)) & 1);
        }
      else
        {
          carry_out = ARM_GET_C();
        }

      if(shift_ == 0)
        return value_;
      if(shift_ > 31)
        return 0;
      return (value_ >> shift_);
    case 2:
      if(shift_)
        {
          if(shift_ > 32)
            carry_out = ((((int32_t)value_) >> 31) & 1);
          else
            carry_out = ((((int32_t)value_) >> (shift_ - 1)) & 1);
        }
      else
        {
          carry_out = ARM_GET_C();
        }

      if(shift_ == 0)
        return value_;
      if(shift_ > 31)
        return (((int32_t)value_) >> 31);
      return (((int32_t)value_) >> shift_);
    case 3:
      if(shift_)
        {
          if(shift_&31)
            carry_out = ((value_ >> (shift_ - 1)) & 1);
          else
            carry_out = ((value_ >> 31) & 1);
        }
      else
        {
          carry_out = ARM_GET_C();
        }

      shift_ &= 31;
      if(shift_ == 0)
        return value_;
      return ROTR(value_,shift_);
    case 4:
      carry_out = value_ & 1;
      return ((value_ >> 1) | (ARM_GET_C() << 31));
    }

  return 0;
}

static
uint32_t
ARM_SHIFT_SC(uint32_t value_,
             uint8_t  shift_,
             uint8_t  type_)
{
  uint32_t tmp;

  switch(type_)
    {
    case 0:
      if(shift_)
        {
          if(shift_ > 32)
            ARM_SET_C(0);
          else
            ARM_SET_C(((value_ << (shift_ - 1)) & 0x80000000) >> 31);
        }
      else
        {
          return value_;
        }

      if(shift_ > 31)
        return 0;
      return (value_ << shift_);
    case 1:
      if(shift_)
        {
          if(shift_ > 32)
            ARM_SET_C(0);
          else
            ARM_SET_C((value_ >> (shift_ - 1)) & 1);
        }
      else
        {
          return value_;
        }

      if(shift_ > 31)
        return 0;
      return (value_ >> shift_);
    case 2:
      if(shift_)
        {
          if(shift_ > 32)
            ARM_SET_C((((int32_t)value_) >> 31) & 1);
          else
            ARM_SET_C((((int32_t)value_) >> (shift_ - 1)) & 1);
        }
      else
        {
          return value_;
        }

      if(shift_ > 31)
        return (((int32_t)value_) >> 31);
      return ((int32_t)value_) >> shift_;
    case 3:
      if(shift_)
        {
          shift_ = (shift_ & 31);
          if(shift_)
            ARM_SET_C((value_ >> (shift_ - 1)) & 1);
          else
            ARM_SET_C((value_ >> 31) & 1);
        }
      else
        {
          return value_;
        }

      return ROTR(value_,shift_);
    case 4:
      tmp = ARM_GET_C() << 31;
      ARM_SET_C(value_ & 1);
      return ((value_ >> 1) | (tmp));
    }

  return 0;
}

static
void
ARM_SWAP(uint32_t cmd_)
{
  uint32_t tmp;
  uint32_t addr;

  CPU.USER[15] += 4;
  addr          = CPU.USER[(cmd_ >> 16) & 0xF];
  CPU.USER[15] += 4;

  if(cmd_ & (1 << 22))
    {
      tmp = mreadb(addr);
      //	if(MAS_Access_Exept)return TRUE;
      mwriteb(addr,CPU.USER[cmd_ & 0xF]);
      CPU.USER[15] -= 8;
      //	if(MAS_Access_Exept)return TRUE;
      CPU.USER[(cmd_ >> 12) & 0xF] = tmp;
    }
  else
    {
      tmp = mreadw(addr);
      //if(MAS_Access_Exept)return TRUE;
      mwritew(addr,CPU.USER[cmd_ & 0xF]);
      CPU.USER[15] -= 8;
      //if(MAS_Access_Exept)return TRUE;
      if(addr & 3)
        tmp = ((tmp >> ((addr & 3) << 3)) | (tmp << (32 - ((addr & 3) << 3))));
      CPU.USER[(cmd_ >> 12) & 0xF] = tmp;
    }
}

static
INLINE
uint32_t
calcbits(uint32_t num_)
{
  uint32_t rv;

  if(!num_)
    return 1;

  if(num_ >> 16)
    {
      num_ >>= 16;
      rv     = 16;
    }
  else
    {
      rv = 0;
    }

  if(num_ >> 8)
    {
      num_ >>= 8;
      rv    += 8;
    }

  if(num_ >> 4)
    {
      num_ >>= 4;
      rv    += 4;
    }

  if(num_ >> 2)
    {
      num_ >>= 2;
      rv    += 2;
    }

  if(num_ >> 1)
    {
      num_ >>= 1;
      rv    += 2;
    }
  else if(num_)
    {
      rv++;
    }

  return rv;
}

static const int is_logic[] =
  {
    TRUE,TRUE,FALSE,FALSE,
    FALSE,FALSE,FALSE,FALSE,
    TRUE,TRUE,FALSE,FALSE,
    TRUE,TRUE,TRUE,TRUE
  };

int32_t
opera_arm_execute(void)
{
  uint32_t op1;
  uint32_t op2;
  uint8_t shift;
  uint8_t shtype;
  uint32_t cmd;
  uint32_t pc_tmp;
  int isexeption;

  isexeption = FALSE;
  if((CPU.USER[15] == 0x94D60) &&
     (CPU.USER[0] == 0x113000) &&
     (CPU.USER[1] == 0x113000) &&
     (CNBFIX == 0)             &&
     (FIXMODE & FIX_BIT_TIMING_1))
    {
      CPU.USER[15] = 0x9E9CC;
      CNBFIX = 1;
    }

  cmd = mreadw(CPU.USER[15]);
  CPU.USER[15] += 4;

  CYCLES = -SCYCLE;
  if((cmd == 0xE5101810) && (CPU.CPSR == 0x80000093))
    isexeption = TRUE;

  if(((cond_flags_cross[cmd >> 28] >> (CPU.CPSR >> 28)) & 1) &&
     (isexeption == FALSE))
    {
      switch((cmd >> 24) & 0xF)
        {
        case 0x0:               //Multiply
          if((cmd & ARM_MUL_MASK) == ARM_MUL_SIGN)
            {
              uint32_t res = ((calcbits(CPU.USER[(cmd>>8)&0xf])+5)>>1)-1;
              if(res > 16)
                CYCLES -= 16;
              else
                CYCLES -= res;

              if(((cmd >> 16) & 0xF) == (cmd & 0xF))
                {
                  if(cmd & (1 << 21))
                    {
                      CPU.USER[15] += 8;
                      res=CPU.USER[(cmd >> 12) & 0xF];
                      CPU.USER[15] -= 8;
                    }
                  else
                    {
                      res = 0;
                    }
                }
              else
                {
                  if(cmd & (1 << 21))
                    {
                      res = CPU.USER[cmd & 0xF] * CPU.USER[(cmd >> 8) & 0xF];
                      CPU.USER[15] += 8;
                      res += CPU.USER[(cmd >> 12) & 0xF];
                      CPU.USER[15] -= 8;
                    }
                  else
                    {
                      res = CPU.USER[cmd & 0xF] * CPU.USER[(cmd >> 8) & 0xF];
                    }
                }

              if(cmd & (1 << 20))
                ARM_SET_ZN(res);

              CPU.USER[(cmd >> 16) & 0xF] = res;
              break;
            }
        case 0x1:               //Single Data Swap
          if((cmd & ARM_SDS_MASK) == ARM_SDS_SIGN)
            {
              ARM_SWAP(cmd);
              //if(MAS_Access_Exept)
              CYCLES -= (2 * NCYCLE + ICYCLE);
              break;
            }
        case 0x2:               //ALU
        case 0x3:
          {
            if((cmd & 0x2000090) != 0x90)
              {
                /* SHIFT */
                pc_tmp = CPU.USER[15];
                CPU.USER[15] += 4;
                if(cmd & (1 << 25))
                  {
                    op2 = cmd & 0xFF;
                    if(((cmd >> 7) & 0x1E))
                      {
                        op2 = ROTR(op2,((cmd >> 7) & 0x1E));
                        //if((cmd&(1<<20))) SETC(((cmd&0xff)>>(((cmd>>7)&0x1e)-1))&1);
                      }
                    op1 = CPU.USER[(cmd >> 16) & 0xF];
                  }
                else
                  {
                    shtype = ((cmd >> 5) & 0x3);
                    if(cmd & (1 << 4))
                      {
                        shift = ((cmd >> 8) & 0xF);
                        shift = (CPU.USER[shift] & 0xFF);
                        CPU.USER[15] += 4;
                        op2 = CPU.USER[cmd & 0xF];
                        op1 = CPU.USER[(cmd >> 16) & 0xF];
                        CYCLES -= ICYCLE;
                      }
                    else
                      {
                        shift = ((cmd >> 7) & 0x1F);

                        if(!shift)
                          {
                            if(shtype)
                              {
                                if(shtype == 3)
                                  shtype++;
                                else
                                  shift=32;
                              }
                          }

                        op2 = CPU.USER[cmd & 0xF];
                        op1 = CPU.USER[(cmd >> 16) & 0xF];
                      }

                    //if((cmd&(1<<20)) && is_logic[((cmd>>21)&0xf)] ) op2=ARM_SHIFT_SC(op2, shift, shtype);
                    //else
                    op2 = ARM_SHIFT_NSC(op2,shift,shtype);
                  }

                CPU.USER[15] = pc_tmp;

                if((cmd & (1 << 20)) && is_logic[((cmd >> 21) & 0xF)])
                  ARM_SET_C(carry_out);

                if(ARM_ALU_Exec(cmd,((cmd >> 20) & 0x1F),op1,op2,&CPU.USER[(cmd >> 12) & 0xF]))
                  break;

                if(((cmd >> 12) & 0xF) == 0xF) //destination = pc, take care of cpsr
                  {
                    if(cmd & (1 << 20))
                      arm_cpsr_set(CPU.SPSR[arm_mode_table[MODE]]);

                    CYCLES -= (ICYCLE + NCYCLE);
                  }
                break;
              }
          }
        case 0x6:               //Undefined
        case 0x7:
        Undefine:
          if((cmd & ARM_UND_MASK) == ARM_UND_SIGN)
            {
              CPU.SPSR[arm_mode_table[0x1b]] = CPU.CPSR;
              SETI(1);
              SETM(0x1b);
              CPU.USER[14] = CPU.USER[15];
              CPU.USER[15] = 0x00000004;
              CYCLES -= (SCYCLE + NCYCLE); // +2S+1N
              break;
            }
        case 0x4:               //Single Data Transfer
        case 0x5:
          if((cmd & 0x2000090) != 0x2000090)
            {
              uint32_t base;
              uint32_t tbas;
              uint32_t oper2;
              uint32_t val;
              uint32_t rora;

              pc_tmp = CPU.USER[15];
              CPU.USER[15] += 4;
              if(cmd & (1 << 25))
                {
                  shtype = ((cmd >> 5) & 0x3);
                  if(cmd & (1 << 4))
                    {
                      shift = ((cmd >> 8) & 0xF);
                      shift = (CPU.USER[shift] & 0xFF);
                      CPU.USER[15] += 4;
                    }
                  else
                    {
                      shift = ((cmd >> 7) & 0x1F);
                      if(!shift)
                        {
                          if(shtype)
                            {
                              if(shtype == 3)
                                shtype++;
                              else
                                shift = 32;
                            }
                        }
                    }

                  oper2 = ARM_SHIFT_NSC(CPU.USER[cmd & 0xF],shift,shtype);
                }
              else
                {
                  oper2 = (cmd & 0x0FFF);
                }

              tbas = base = CPU.USER[((cmd >> 16) & 0xF)];

              if(!(cmd & (1 << 23)))
                oper2 = (0 - oper2);

              if(cmd & (1 << 24))
                tbas = base = (base + oper2);
              else
                base = (base + oper2);

              if(cmd & (1 << 20)) //load
                {
                  if(cmd & (1 << 22)) //bytes
                    {
                      val = mreadb(tbas);
                    }
                  else //words/halfwords
                    {
                      uint32_t rora;

                      rora = tbas & 3;
                      val = mreadw(tbas);

                      if(rora)
                        val = ROTR(val,rora*8);
                    }

                  if(((cmd >> 12) & 0xF) == 0xF)
                    CYCLES -= (SCYCLE + NCYCLE);   // +1S+1N ifR15 load

                  CYCLES -= (NCYCLE + ICYCLE);  // +1N+1I
                  CPU.USER[15] = pc_tmp;

                  if((cmd & (1 << 21)) || (!(cmd & (1 << 24))))
                    CPU.USER[(cmd >> 16) & 0xF] = base;

                  if((cmd & (1 << 21)) && !(cmd & (1 << 24)))
                    loadusr((cmd >> 12) & 0xF,val);
                  else
                    CPU.USER[(cmd >> 12) & 0xF] = val;
                }
              else // store
                {
                  if((cmd & (1 << 21)) && !(cmd & (1 << 24)))
                    val = readusr((cmd >> 12) & 0xF);
                  else
                    val = CPU.USER[(cmd >> 12) & 0xF];

                  CPU.USER[15] = pc_tmp;
                  CYCLES -= (-SCYCLE + 2 * NCYCLE);  // 2N

                  if(cmd & (1 << 22)) //bytes/words
                    mwriteb(tbas,val);
                  else //words/halfwords
                    mwritew(tbas,val);

                  if((cmd & (1 << 21)) || !(cmd & (1 << 24)))
                    CPU.USER[(cmd >> 16) & 0xF] = base;
                }

              //if(MAS_Access_Exept)
              break;
            }
          else
            {
              goto Undefine;
            }

        case 0x8:               //Block Data Transfer
        case 0x9:
          bdt_core(cmd);
          /*if(MAS_Access_Exept)
            {
            //sprintf(str,"*PC: 0x%8.8X DataAbort!!!\n",CPU.USER[15]);
            //CDebug::DPrint(str);
            //!!Exeption!!

            CPU.SPSR[arm_mode_table[0x17]]=CPU.CPSR;
            SETI(1);
            SETM(0x17);
            CPU.USER[14] = (CPU.USER[15] + 4);
            CPU.USER[15] = 0x00000010;
            CYCLES-=SCYCLE+NCYCLE;
            MAS_Access_Exept=FALSE;
            break;
            } */
          break;

        case 0xa:               //BRANCH
        case 0xb:
          if(cmd & (1 << 24))
            CPU.USER[14] = CPU.USER[15];
          CPU.USER[15] += ((((cmd & 0x00FFFFFF) | ((cmd & 0x00800000) ? 0xFF000000 : 0)) << 2) + 4);

          CYCLES -= (SCYCLE + NCYCLE); //2S+1N
          break;

        case 0xf:               //SWI
          decode_swi(cmd);
          break;

        default:                //coprocessor
          CPU.SPSR[arm_mode_table[0x1b]] = CPU.CPSR;
          SETI(1);
          SETM(0x1b);
          CPU.USER[14] = CPU.USER[15];
          CPU.USER[15] = 0x00000004;
          CYCLES -= (SCYCLE + NCYCLE);
          break;
        }
    }

  if(!ISF && opera_clio_fiq_needed()/*CPU.nFIQ*/)
    {
      //Set_madam_FSM(FSM_SUSPENDED);
      CPU.nFIQ = FALSE;
      CPU.SPSR[arm_mode_table[0x11]] = CPU.CPSR;
      SETF(1);
      SETI(1);
      SETM(0x11);
      CPU.USER[14] = (CPU.USER[15] + 4);
      CPU.USER[15] = 0x0000001C;
    }

  return -CYCLES;
}

void
opera_mem_write8(uint32_t addr_,
                 uint8_t  val_)
{
  CPU.ram[addr_] = val_;
  if(!HIRESMODE || (addr_ < 0x200000))
    return;
  CPU.ram[addr_ + 1*1024*1024] = val_;
  CPU.ram[addr_ + 2*1024*1024] = val_;
  CPU.ram[addr_ + 3*1024*1024] = val_;
}

void
opera_mem_write16(uint32_t addr_,
                  uint16_t val_)
{
  *((uint16_t*)&CPU.ram[addr_]) = val_;
  if(!HIRESMODE || (addr_ < 0x200000))
    return;
  *((uint16_t*)&CPU.ram[addr_ + 1*1024*1024]) = val_;
  *((uint16_t*)&CPU.ram[addr_ + 2*1024*1024]) = val_;
  *((uint16_t*)&CPU.ram[addr_ + 3*1024*1024]) = val_;
}

void
opera_mem_write32(uint32_t addr_,
                  uint32_t val_)
{
  *((uint32_t*)&CPU.ram[addr_]) = val_;
  if(!HIRESMODE || (addr_ < 0x200000))
    return;
  *((uint32_t*)&CPU.ram[addr_ + 1*1024*1024]) = val_;
  *((uint32_t*)&CPU.ram[addr_ + 2*1024*1024]) = val_;
  *((uint32_t*)&CPU.ram[addr_ + 3*1024*1024]) = val_;
}

uint16_t
opera_mem_read16(uint32_t addr_)
{
  return *((uint16_t*)&CPU.ram[addr_]);
}

uint32_t
opera_mem_read32(uint32_t addr_)
{
  return *((uint32_t*)&CPU.ram[addr_]);
}

uint8_t
opera_mem_read8(uint32_t addr_)
{
  return CPU.ram[addr_];
}

static void mwritew(uint32_t addr_, uint32_t val_)
{
   uint32_t index;

   addr_ &= ~3;

   if(addr_ < 0x00300000)
   {
      opera_mem_write32(addr_,val_);
      return;
   }

   index = (addr_ ^ 0x03300000);
   if(!(index & ~0x7FF))
   {
      opera_madam_poke(index,val_);
      return;
   }

   index = (addr_ ^ 0x03400000);
   if(!(index & ~0xFFFF))
   {
      if(opera_clio_poke(index,val_))
         CPU.USER[15] += 4;  /* ??? */
      return;
   }

   index = (addr_ ^ 0x03200000);
   if(!(index & ~0xFFFFF))
   {
      opera_sport_write_access(index,val_);
      return;
   }

   index = (addr_ ^ 0x03100000);
   if(!(index & ~0xFFFFF))
   {
      if(index & 0x80000)
         opera_diag_port_send(val_);
      else if(index & 0x40000)
         CPU.nvram[(index >> 2) & 0x7FFF] = (uint8_t)val_;
   }
}

static
uint32_t
mreadw(uint32_t addr_)
{
  int32_t index;

  addr_ &= ~3;

  if(addr_ < 0x00300000)
    return opera_mem_read32(addr_);

  index = (addr_ ^ 0x03300000);
  if(!(index & ~0xFFFFF))
    return opera_madam_peek(index);

  index = (addr_ ^ 0x03400000);
  if(!(index & ~0xFFFFF))
    return opera_clio_peek(index);

  index = (addr_ ^ 0x03200000);
  if(!(index & ~0xFFFFF))
    {
      if(!(index & ~0x1FFF))
        return (opera_sport_set_source(index),0);
      return 0xBADACCE5;
    }

  /* Standard ROM */
  index = (addr_ ^ 0x03000000);
  if(!(index & ~0xFFFFF))
    return *(uint32_t*)&CPU.rom[index];

  /* ANVIL ROM */
  index = (addr_ ^ 0x06000000);
  if(!(index & ~0xFFFFF))
    return *(uint32_t*)&CPU.rom[index];

  index = (addr_ ^ 0x03100000);
  if(!(index & ~0xFFFFF))
    {
      if(index & 0x80000)
        return opera_diag_port_get();
      else if(index & 0x40000)
        return CPU.nvram[(index >> 2) & 0x7FFF];
    }

  /* MAS_Access_Exept = TRUE; */

  return 0xBADACCE5;
}

static void mwriteb(uint32_t addr_, uint8_t  val_)
{
  int32_t index;

  if(addr_ < 0x00300000)
  {
    opera_mem_write8(addr_ ^ 3,val_);
    return;
  }

  index = (addr_ ^ 0x03100003);
  if(!(index & ~0xFFFFF))
  {
     if((index & 0x40000) == 0x40000)
     {
        CPU.nvram[(index >> 2) & 0x7FFF] = val_;
        return;
     }
  }
}

static
uint32_t
mreadb(uint32_t addr_)
{
  int32_t index;

  if(addr_ < 0x00300000)
    return opera_mem_read8(addr_ ^ 3);

  /* Standard ROM */
  index = (addr_ ^ 0x03000003);
  if(!(index & ~0xFFFFF))
    return CPU.rom[index];

  /* ANVIL ROM */
  index = (addr_ ^ 0x06000003);
  if(!(index & ~0xFFFFF))
    return CPU.rom[index];

  index = (addr_ ^ 0x03100003);
  if(!(index & ~0xFFFFF))
    {
      if((index & 0x40000) == 0x40000)
        return CPU.nvram[(index >> 2) & 0x7FFF];
    }

  /* MAS_Access_Exept = TRUE; */

  return 0xBADACCE5;
}

static
void
loadusr(uint32_t n_,
        uint32_t val_)
{
  if(n_ == 15)
    {
      CPU.USER[15] = val_;
      return;
    }

  switch(arm_mode_table[(CPU.CPSR & 0x1F) | 0x10])
    {
    case ARM_MODE_USER:
      CPU.USER[n_] = val_;
      break;
    case ARM_MODE_FIQ:
      if(n_ > 7)
        CPU.CASH[n_ - 8] = val_;
      else
        CPU.USER[n_] = val_;
      break;
    case ARM_MODE_IRQ:
    case ARM_MODE_ABT:
    case ARM_MODE_UND:
    case ARM_MODE_SVC:
      if(n_ > 12)
        CPU.CASH[n_ - 8] = val_;
      else
        CPU.USER[n_] = val_;
      break;
    }
}

static
uint32_t
readusr(uint32_t n_)
{
  if(n_ == 15)
    return CPU.USER[15];

  switch(arm_mode_table[CPU.CPSR & 0x1F])
    {
    case ARM_MODE_USER:
      return CPU.USER[n_];
    case ARM_MODE_FIQ:
      if(n_ > 7)
        return CPU.CASH[n_ - 8];
      return CPU.USER[n_];
    case ARM_MODE_IRQ:
    case ARM_MODE_ABT:
    case ARM_MODE_UND:
    case ARM_MODE_SVC:
      if(n_ > 12)
        return CPU.CASH[n_ - 8];
      return CPU.USER[n_];
    }

  return 0;
}

uint32_t opera_io_read(const uint32_t addr_)
{
   return mreadw(addr_);
}

void opera_io_write(const uint32_t addr_, const uint32_t val_)
{
  mwritew(addr_,val_);
}
