#pragma once

#include <stdint.h>
#include <string.h>

#define NVRAM_SIZE (32 * 1024)

#pragma pack(push,1)

struct NVRAM_Header
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

void nvram_init(void *nvram);

int nvram_save(const void   *nvram,
               const size_t  size,
               const char   *basepath,
               const char   *filename);
int nvram_load(void         *nvram,
               const size_t  size,
               const char   *basepath,
               const char   *filename);

void retro_nvram_save(void);
void retro_nvram_load(void);
