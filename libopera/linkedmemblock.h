#pragma once

#include <stdint.h>

/*
  LinkedMemDisk defines a high-level "disk" which consists of a
  doubly-linked list of storage blocks in memory (RAM, ROM, NVRAM,
  or out on a gamesaver cartridge.  LinkedMemDisks have a standard
  Opera label at offset 0, with a type code of 2.  The linked list
  normally begins immediately after the label;  its offset is given
  by the zero'th avatar of the root directory.  LinkedMemDisks are,
  by definition, flat file systems and cannot contain directories.
*/

#define FINGERPRINT_FILEBLOCK   0xBE4F32A6
#define FINGERPRINT_FREEBLOCK   0x7AA565BD
#define FINGERPRINT_ANCHORBLOCK 0x855A02B6

typedef struct LinkedMemBlock LinkedMemBlock;
struct LinkedMemBlock
{
  uint32_t fingerprint;
  uint32_t flinkoffset;
  uint32_t blinkoffset;
  uint32_t blockcount;
  uint32_t headerblockcount;
};
