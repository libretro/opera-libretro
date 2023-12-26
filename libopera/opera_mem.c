#include "opera_mem.h"
#include "endianness.h"
#include "opera_state.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

uint32_t  RAM_SIZE             = (DEFAULT_RAM_SIZE);
uint32_t  HIRES_RAM_SIZE       = (DEFAULT_HIRES_RAM_SIZE);
bool      HIRESMODE            = false;
uint8_t  *DRAM                 = NULL;
uint32_t  DRAM_SIZE            = (DEFAULT_DRAM_SIZE);
uint8_t  *VRAM                 = NULL;
uint32_t  VRAM_SIZE            = (DEFAULT_VRAM_SIZE);
uint32_t  VRAM_SIZE_MASK       = (DEFAULT_VRAM_SIZE - 1);
uint32_t  HIRES_VRAM_SIZE      = (DEFAULT_HIRES_VRAM_SIZE);
uint32_t  HIRES_VRAM_SIZE_MASK = (DEFAULT_HIRES_VRAM_SIZE - 1);
uint8_t  *NVRAM                = NULL;
uint8_t  *ROM                  = NULL;
uint8_t  *ROM1                 = NULL;
uint8_t  *ROM2                 = NULL;

typedef struct opera_mem_state_t opera_mem_state_t;
struct opera_mem_state_t
{
  uint8_t mem_cfg;
};


uint32_t
opera_mem_dram_size(opera_mem_cfg_t cfg_)
{
  return (((cfg_ & 0xF0) >> 4) * ONE_MB);
}

uint32_t
opera_mem_vram_size(opera_mem_cfg_t cfg_)
{
  return (((cfg_ & 0x0F) >> 0) * ONE_MB);
}

static
void
_setup_dram_vram(opera_mem_cfg_t const cfg_)
{
  DRAM_SIZE            = opera_mem_dram_size(cfg_);
  VRAM_SIZE            = opera_mem_vram_size(cfg_);
  VRAM_SIZE_MASK       = (VRAM_SIZE - 1);
  HIRES_VRAM_SIZE      = (VRAM_SIZE * 4);
  HIRES_VRAM_SIZE_MASK = (HIRES_RAM_SIZE - 1);
  RAM_SIZE             = (DRAM_SIZE + VRAM_SIZE);
  HIRES_RAM_SIZE       = (DRAM_SIZE + HIRES_VRAM_SIZE);

  /* VRAM is always at the top of DRAM */
  VRAM = &DRAM[DRAM_SIZE];
}

int
opera_mem_init()
{
  opera_mem_cfg_t cfg = DRAM_2MB_VRAM_1MB;

  /*
    Allocate max possible RAM to make things easier
    Since it is zeroed out it will compress well
  */
  DRAM  = calloc(MAX_HIRES_RAM_SIZE,1);
  ROM1  = calloc(ROM1_SIZE,1);
  ROM2  = calloc(ROM2_SIZE,1);
  NVRAM = calloc(NVRAM_SIZE,1);

  _setup_dram_vram(cfg);

  opera_mem_rom_select(ROM1);

  return 0;
}

int
opera_mem_cfg(opera_mem_cfg_t const cfg_)
{
  _setup_dram_vram(cfg_);

  return 0;
}

void
opera_mem_destroy()
{
  if(DRAM)
    free(DRAM);
  DRAM = NULL;
  VRAM = NULL;

  if(ROM1)
    free(ROM1);
  ROM1 = NULL;

  if(ROM2)
    free(ROM2);
  ROM2 = NULL;

  if(NVRAM)
    free(NVRAM);
  NVRAM = NULL;
}

void
opera_mem_rom1_clear()
{
  memset(ROM1,0,ROM1_SIZE);
}

void
opera_mem_rom1_byteswap32_if_le()
{
  uint32_t *mem;
  uint64_t  size;

  mem  = (uint32_t*)ROM1;
  size = (ROM1_SIZE / sizeof(uint32_t));

  swap32_array_if_little_endian(mem,size);
}

void
opera_mem_rom2_clear()
{
  memset(ROM2,0,ROM2_SIZE);
}

void
opera_mem_rom2_byteswap32_if_le()
{
  uint32_t *mem;
  uint64_t  size;

  mem  = (uint32_t*)ROM2;
  size = (ROM2_SIZE / sizeof(uint32_t));

  swap32_array_if_little_endian(mem,size);
}

void
opera_mem_rom_select(void *rom_)
{
  if((rom_ != ROM1) || (rom_ != ROM2))
    rom_ = ROM1;

  ROM = rom_;
}

uint32_t
opera_mem_state_size()
{
  uint32_t size;

  size = 0;
  size += opera_state_save_size(sizeof(opera_mem_state_t));
  size += opera_state_save_size(MAX_HIRES_RAM_SIZE);
  size += opera_state_save_size(ROM1_SIZE);
  size += opera_state_save_size(ROM2_SIZE);
  size += opera_state_save_size(NVRAM_SIZE);

  return size;
}

uint32_t
opera_mem_state_save(void *data_)
{
  uint8_t *start = (uint8_t*)data_;
  uint8_t *data  = (uint8_t*)data_;
  opera_mem_state_t memstate;

  memstate.mem_cfg = DRAM_2MB_VRAM_1MB;

  data += opera_state_save(data,"MCFG",&memstate,sizeof(memstate));
  data += opera_state_save(data,"RAM",DRAM,MAX_HIRES_RAM_SIZE);
  data += opera_state_save(data,"ROM1",ROM1,ROM1_SIZE);
  data += opera_state_save(data,"ROM2",ROM2,ROM2_SIZE);
  data += opera_state_save(data,"NVRM",NVRAM,NVRAM_SIZE);

  return (data - start);
}

uint32_t
opera_mem_state_load(void const *data_)
{
  uint8_t const *start = (uint8_t const*)data_;
  uint8_t const *data  = (uint8_t const*)data_;
  opera_mem_state_t memstate;

  data += opera_state_load(&memstate,"MCFG",data,sizeof(memstate));
  data += opera_state_load(DRAM,"RAM",data,MAX_HIRES_RAM_SIZE);
  data += opera_state_load(ROM1,"ROM1",data,ROM1_SIZE);
  data += opera_state_load(ROM2,"ROM2",data,ROM2_SIZE);
  data += opera_state_load(NVRAM,"NVRM",data,NVRAM_SIZE);

  return (data - start);
}
