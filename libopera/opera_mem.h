#ifndef LIBOPERA_MEM_H_INCLUDED
#define LIBOPERA_MEM_H_INCLUDED

#include "boolean.h"
#include "inline.h"

#include <stdint.h>

#define ONE_KB (1024)
#define ONE_MB (ONE_KB * 1024)

#define DEFAULT_DRAM_SIZE       (ONE_MB * 2)
#define DEFAULT_VRAM_SIZE       (ONE_MB * 1)
#define DEFAULT_HIRES_VRAM_SIZE (DEFAULT_VRAM_SIZE * 4)
#define DEFAULT_RAM_SIZE        (DEFAULT_DRAM_SIZE + DEFAULT_VRAM_SIZE)
#define DEFAULT_HIRES_RAM_SIZE  (DEFAULT_DRAM_SIZE + DEFAULT_HIRES_VRAM_SIZE)
#define MAX_DRAM_SIZE           (ONE_MB * 14)
#define MAX_VRAM_SIZE           (ONE_MB * 2)
#define MAX_HIRES_VRAM_SIZE     (MAX_VRAM_SIZE * 4)
#define MAX_HIRES_RAM_SIZE      (MAX_DRAM_SIZE + MAX_HIRES_VRAM_SIZE)

extern uint32_t RAM_SIZE;
extern uint32_t HIRES_RAM_SIZE;
extern bool     HIRESMODE;

extern uint8_t *DRAM;
extern uint32_t DRAM_SIZE;

extern uint8_t *VRAM;
extern uint32_t VRAM_SIZE;
extern uint32_t VRAM_SIZE_MASK;
extern uint32_t HIRES_VRAM_SIZE;
extern uint32_t HIRES_VRAM_SIZE_MASK;

extern uint8_t *NVRAM;
#define NVRAM_SIZE      (ONE_KB * 32)
#define NVRAM_SIZE_MASK (NVRAM_SIZE - 1)

extern uint8_t *ROM;

extern uint8_t *ROM1;
#define ROM1_SIZE      (ONE_MB * 1)
#define ROM1_SIZE_MASK (ROM1_SIZE - 1)

extern uint8_t *ROM2;
#define ROM2_SIZE      (ONE_MB * 1)
#define ROM2_SIZE_MASK (ROM2_SIZE - 1)


enum opera_mem_cfg_t
  {
    DRAM_VRAM_UNSET    = 0x00,
    DRAM_VRAM_STOCK    = 0x21,
    DRAM_2MB_VRAM_1MB  = 0x21,
    DRAM_2MB_VRAM_2MB  = 0x22,
    DRAM_4MB_VRAM_1MB  = 0x41,
    DRAM_4MB_VRAM_2MB  = 0x42,
    DRAM_8MB_VRAM_1MB  = 0x81,
    DRAM_8MB_VRAM_2MB  = 0x82,
    DRAM_14MB_VRAM_2MB = 0xE2,
    DRAM_15MB_VRAM_1MB = 0xF1
  };
typedef enum opera_mem_cfg_t opera_mem_cfg_t;

int  opera_mem_init(opera_mem_cfg_t);
void opera_mem_destroy();

opera_mem_cfg_t opera_mem_cfg();

uint32_t opera_mem_madam_red_sysbits(uint32_t const);

void opera_mem_rom1_clear();
void opera_mem_rom1_byteswap32_if_le();
void opera_mem_rom2_clear();
void opera_mem_rom2_byteswap32_if_le();

void opera_mem_rom_select(void *rom);

uint32_t opera_mem_state_size();
uint32_t opera_mem_state_save(void *data);
uint32_t opera_mem_state_load(void const *data);

static
INLINE
uint8_t
opera_mem_read8(uint32_t const addr_)
{
#if IS_BIG_ENDIAN
  return DRAM[addr_];
#else
  return DRAM[addr_ ^ 3];
#endif
}

static
INLINE
uint16_t
opera_mem_read16(uint32_t const addr_)
{
#if IS_BIG_ENDIAN
  uint32_t const addr = addr_;
#else
  uint32_t const addr = (addr_ ^ 2);
#endif

  return *((uint16_t const *)&DRAM[addr]);
}

static
INLINE
uint32_t
opera_mem_read32(uint32_t const addr_)
{
  return *((uint32_t const *)&DRAM[addr_]);
}

static
INLINE
void
opera_mem_write8(uint32_t const addr_,
                 uint8_t  const val_)
{
#if IS_BIG_ENDIAN
  uint32_t const addr = addr_;
#else
  uint32_t const addr = (addr_ ^ 3);
#endif

  DRAM[addr] = val_;
  if(!HIRESMODE || (addr < DRAM_SIZE))
    return;
  DRAM[addr + 1*VRAM_SIZE] =
    DRAM[addr + 2*VRAM_SIZE] =
    DRAM[addr + 3*VRAM_SIZE] = val_;
}

static
INLINE
void
opera_mem_write16_base(uint32_t const addr_,
                       uint16_t const val_)
{
#if IS_BIG_ENDIAN
  uint32_t const addr = addr_;
#else
  uint32_t const addr = (addr_ ^ 2);
#endif

  *((uint16_t*)&DRAM[addr]) = val_;
}

static
INLINE
void
opera_mem_write16(uint32_t const addr_,
                  uint16_t const val_)
{
#if IS_BIG_ENDIAN
  uint32_t const addr = addr_;
#else
  uint32_t const addr = (addr_ ^ 2);
#endif

  *((uint16_t*)&DRAM[addr]) = val_;
  if(!HIRESMODE || (addr < DRAM_SIZE))
    return;
  *((uint16_t*)&DRAM[addr + 1*VRAM_SIZE]) =
    *((uint16_t*)&DRAM[addr + 2*VRAM_SIZE]) =
    *((uint16_t*)&DRAM[addr + 3*VRAM_SIZE]) = val_;
}

static
INLINE
void
opera_mem_write32(uint32_t const addr_,
                  uint32_t const val_)
{
  *((uint32_t*)&DRAM[addr_]) = val_;
  if(!HIRESMODE || (addr_ < DRAM_SIZE))
    return;
  *((uint32_t*)&DRAM[addr_ + 1*VRAM_SIZE]) =
    *((uint32_t*)&DRAM[addr_ + 2*VRAM_SIZE]) =
    *((uint32_t*)&DRAM[addr_ + 3*VRAM_SIZE]) = val_;
}

#endif
