/*
  documentation:
  * https://3dodev.com/documentation/hardware/opera/memory_configurations
  * About Memory: https://3dodev.com/documentation/development/opera/pf25/ppgfldr/pgsfldr/spg/05spg001
  * Manageing Memory: https://3dodev.com/documentation/development/opera/pf25/ppgfldr/pgsfldr/spg/01spg003
  */

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

static opera_mem_cfg_t g_MEM_CFG = DRAM_VRAM_UNSET;

typedef struct opera_mem_state_t opera_mem_state_t;
struct opera_mem_state_t
{
  uint8_t mem_cfg;
};

opera_mem_cfg_t
opera_mem_cfg()
{
  return g_MEM_CFG;
}

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

#define MADAM_RAM_MASK     0x0000007F
#define DRAMSIZE_SHIFT     3    /* RED ONLY */
#define VRAMSIZE_SHIFT     0    /* RED ONLY */
#define VRAMSIZE_1MB       0x00000001 /* RED ONLY */
#define VRAMSIZE_2MB       0x00000002 /* RED ONLY */
#define DRAMSIZE_SETMASK   3    /* RED ONLY */
#define DRAMSIZE_1MB       0x000000001 /* RED ONLY */
#define DRAMSIZE_4MB       0x000000002 /* RED ONLY */
#define DRAMSIZE_16MB      0x000000003 /* RED ONLY */
#define DRAMSIZE_SET1SHIFT 3    /* RED ONLY */
#define DRAMSIZE_SET0SHIFT (DRAMSIZE_SET1SHIFT+2) /* RED ONLY */

uint32_t
opera_mem_madam_red_sysbits(uint32_t const v_)
{
  uint32_t v = v_;

  v &= ~MADAM_RAM_MASK;

  switch(g_MEM_CFG)
    {
    default:
    case DRAM_2MB_VRAM_1MB:
      v |= ((VRAMSIZE_1MB << VRAMSIZE_SHIFT)     |
            (DRAMSIZE_1MB << DRAMSIZE_SET0SHIFT) |
            (DRAMSIZE_1MB << DRAMSIZE_SET1SHIFT));
      break;
    case DRAM_2MB_VRAM_2MB:
      v |= ((VRAMSIZE_2MB << VRAMSIZE_SHIFT)     |
            (DRAMSIZE_1MB << DRAMSIZE_SET0SHIFT) |
            (DRAMSIZE_1MB << DRAMSIZE_SET1SHIFT));
      break;
    case DRAM_4MB_VRAM_1MB:
      v |= ((VRAMSIZE_1MB << VRAMSIZE_SHIFT)     |
            (DRAMSIZE_4MB << DRAMSIZE_SET0SHIFT));
      break;
    case DRAM_4MB_VRAM_2MB:
      v |= ((VRAMSIZE_2MB << VRAMSIZE_SHIFT)     |
            (DRAMSIZE_4MB << DRAMSIZE_SET0SHIFT));
      break;
    case DRAM_8MB_VRAM_1MB:
      v |= ((VRAMSIZE_1MB << VRAMSIZE_SHIFT)     |
            (DRAMSIZE_4MB << DRAMSIZE_SET0SHIFT) |
            (DRAMSIZE_4MB << DRAMSIZE_SET1SHIFT));
      break;
    case DRAM_8MB_VRAM_2MB:
      v |= ((VRAMSIZE_2MB << VRAMSIZE_SHIFT)     |
            (DRAMSIZE_4MB << DRAMSIZE_SET0SHIFT) |
            (DRAMSIZE_4MB << DRAMSIZE_SET1SHIFT));
      break;
    case DRAM_14MB_VRAM_2MB:
      v |= ((VRAMSIZE_2MB  << VRAMSIZE_SHIFT)     |
            (DRAMSIZE_16MB << DRAMSIZE_SET1SHIFT));
      break;
    case DRAM_15MB_VRAM_1MB:
      v |= ((VRAMSIZE_1MB  << VRAMSIZE_SHIFT)     |
            (DRAMSIZE_16MB << DRAMSIZE_SET1SHIFT));
      break;
    }

  return v;
}

int
opera_mem_init(opera_mem_cfg_t const cfg_)
{
  opera_mem_cfg_t cfg = DRAM_2MB_VRAM_1MB;

  if(g_MEM_CFG != DRAM_VRAM_UNSET)
    return -1;

  cfg = cfg_;
  if(cfg == DRAM_VRAM_UNSET)
    cfg = DRAM_VRAM_STOCK;

  /*
    Allocate max possible RAM to make things easier
    Since it is zeroed out it will compress well
  */
  DRAM  = calloc(MAX_HIRES_RAM_SIZE,1);
  ROM1  = calloc(ROM1_SIZE,1);
  ROM2  = calloc(ROM2_SIZE,1);
  NVRAM = calloc(NVRAM_SIZE,1);

  if(!DRAM || !ROM1 || !ROM2 || !NVRAM)
    {
      opera_mem_destroy();
      return -1;
    }

  _setup_dram_vram(cfg);

  opera_mem_rom_select(ROM1);

  g_MEM_CFG = cfg;

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

  g_MEM_CFG = DRAM_VRAM_UNSET;
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
  if((rom_ != ROM1) && (rom_ != ROM2))
    rom_ = ROM1;

  ROM = rom_;
}

uint32_t
opera_mem_state_size_v1()
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
opera_mem_state_size()
{
  return opera_state_chunk_size(sizeof(uint8_t) +
                                MAX_HIRES_RAM_SIZE +
                                ROM1_SIZE +
                                ROM2_SIZE +
                                NVRAM_SIZE);
}

uint32_t
opera_mem_state_save(void *data_)
{
  opera_mem_state_t memstate;
  opera_state_writer_t writer;

  memstate.mem_cfg = opera_mem_cfg();

  opera_state_writer_init(&writer,data_,opera_mem_state_size());
  opera_state_write_chunk_header(&writer,
                                 "MEM",
                                 sizeof(memstate.mem_cfg) +
                                 MAX_HIRES_RAM_SIZE +
                                 ROM1_SIZE +
                                 ROM2_SIZE +
                                 NVRAM_SIZE);
  opera_state_write_u8(&writer,memstate.mem_cfg);
  opera_state_write_bytes(&writer,DRAM,MAX_HIRES_RAM_SIZE);
  opera_state_write_bytes(&writer,ROM1,ROM1_SIZE);
  opera_state_write_bytes(&writer,ROM2,ROM2_SIZE);
  opera_state_write_bytes(&writer,NVRAM,NVRAM_SIZE);

  return opera_state_writer_ok(&writer) ? opera_state_writer_used(&writer) : 0;
}

uint32_t
opera_mem_state_load_v1(void const     *data_,
                        uint32_t const  size_)
{
  uint8_t const *start = (uint8_t const*)data_;
  uint8_t const *data  = (uint8_t const*)data_;
  uint8_t const *end   = (uint8_t const*)data_ + size_;
  opera_mem_state_t memstate;
  uint32_t rv;

  rv = opera_state_load_sized(&memstate,"MCFG",data,(uint32_t)(end - data),sizeof(memstate));
  if(rv == 0)
    return 0;
  data += rv;
  _setup_dram_vram(memstate.mem_cfg);
  g_MEM_CFG = memstate.mem_cfg;
  rv = opera_state_load_sized(DRAM,"RAM",data,(uint32_t)(end - data),MAX_HIRES_RAM_SIZE);
  if(rv == 0)
    return 0;
  data += rv;
  rv = opera_state_load_sized(ROM1,"ROM1",data,(uint32_t)(end - data),ROM1_SIZE);
  if(rv == 0)
    return 0;
  data += rv;
  rv = opera_state_load_sized(ROM2,"ROM2",data,(uint32_t)(end - data),ROM2_SIZE);
  if(rv == 0)
    return 0;
  data += rv;
  rv = opera_state_load_sized(NVRAM,"NVRM",data,(uint32_t)(end - data),NVRAM_SIZE);
  if(rv == 0)
    return 0;
  data += rv;

  return (data - start);
}

uint32_t
opera_mem_state_load(void const     *data_,
                     uint32_t const  size_)
{
  uint8_t mem_cfg;
  opera_state_reader_t reader;
  opera_state_reader_t payload;

  opera_state_reader_init(&reader,data_,size_);

  if(!opera_state_read_chunk(&reader,"MEM",&payload) ||
     !opera_state_read_u8(&payload,&mem_cfg) ||
     (opera_state_reader_remaining(&payload) != (MAX_HIRES_RAM_SIZE +
                                                 ROM1_SIZE +
                                                 ROM2_SIZE +
                                                 NVRAM_SIZE)))
    return 0;
  _setup_dram_vram(mem_cfg);
  g_MEM_CFG = (opera_mem_cfg_t)mem_cfg;

  if(!opera_state_read_bytes(&payload,DRAM,MAX_HIRES_RAM_SIZE) ||
     !opera_state_read_bytes(&payload,ROM1,ROM1_SIZE) ||
     !opera_state_read_bytes(&payload,ROM2,ROM2_SIZE) ||
     !opera_state_read_bytes(&payload,NVRAM,NVRAM_SIZE) ||
     !opera_state_reader_finished(&payload))
    return 0;

  return opera_state_reader_used(&reader);
}
