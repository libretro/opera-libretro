#include "opera_arm.h"

#include "retro_endianness.h"

#include <stdint.h>
#include <string.h>

#pragma pack(push,1)

typedef struct nvram_header_t nvram_header_t;
struct nvram_header_t
{
  uint8_t  record_type;
  uint8_t  sync_bytes[5];
  uint8_t  record_version;
  uint8_t  flags;
  uint8_t  comment[32];
  uint8_t  label[32];
  uint32_t id;
  uint32_t block_size;
  uint32_t block_count;
  uint32_t root_dir_id;
  uint32_t root_dir_blocks;
  uint32_t root_dir_block_size;
  uint32_t last_root_dir_copy;
  uint32_t root_dir_copies[8];

  uint32_t unknown_value0;
  uint32_t unknown_value1;
  uint32_t unknown_value2;
  uint32_t unknown_value3;
  uint32_t unknown_value4;
  uint32_t unknown_value5;
  uint32_t unknown_value6;
  uint32_t unknown_value7;
  uint32_t blocks_remaining;
  uint32_t unknown_value8;
};

#pragma pack(pop)

void
opera_nvram_init(void)
{
  nvram_header_t *nvram_hdr = (nvram_header_t*)opera_arm_nvram_get();

  memset(nvram_hdr,0,sizeof(nvram_header_t));

  nvram_hdr->record_type         = 0x01;
  nvram_hdr->sync_bytes[0]       = 'Z';
  nvram_hdr->sync_bytes[1]       = 'Z';
  nvram_hdr->sync_bytes[2]       = 'Z';
  nvram_hdr->sync_bytes[3]       = 'Z';
  nvram_hdr->sync_bytes[4]       = 'Z';
  nvram_hdr->record_version      = 0x02;
  nvram_hdr->label[0]            = 'N';
  nvram_hdr->label[1]            = 'V';
  nvram_hdr->label[2]            = 'R';
  nvram_hdr->label[3]            = 'A';
  nvram_hdr->label[4]            = 'M';
  nvram_hdr->id                  = swap_if_little32(0xFFFFFFFF);
  nvram_hdr->block_size          = swap_if_little32(0x00000001);
  nvram_hdr->block_count         = swap_if_little32(0x00008000);
  nvram_hdr->root_dir_id         = swap_if_little32(0xFFFFFFFE);
  nvram_hdr->root_dir_blocks     = swap_if_little32(0x00000000);
  nvram_hdr->root_dir_block_size = swap_if_little32(0x00000001);
  nvram_hdr->last_root_dir_copy  = swap_if_little32(0x00000000);
  nvram_hdr->root_dir_copies[0]  = swap_if_little32(0x00000084);
  nvram_hdr->unknown_value0      = swap_if_little32(0x855A02B6);
  nvram_hdr->unknown_value1      = swap_if_little32(0x00000098);
  nvram_hdr->unknown_value2      = swap_if_little32(0x00000098);
  nvram_hdr->unknown_value3      = swap_if_little32(0x00000014);
  nvram_hdr->unknown_value4      = swap_if_little32(0x00000014);
  nvram_hdr->unknown_value5      = swap_if_little32(0x7AA565BD);
  nvram_hdr->unknown_value6      = swap_if_little32(0x00000084);
  nvram_hdr->unknown_value7      = swap_if_little32(0x00000084);
  nvram_hdr->blocks_remaining    = swap_if_little32(0x00007F68);
  nvram_hdr->unknown_value8      = swap_if_little32(0x00000014);
}
