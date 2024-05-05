#include "discdata.h"
#include "linkedmemblock.h"

#include "opera_mem.h"

#include "endianness.h"

#include "boolean.h"

#include <stdint.h>
#include <string.h>

#define NVRAM_BLOCKSIZE 1
#define NVRAM_BLOCKCOUNT (32 * 1024)

bool
opera_nvram_initialized(void      *buf_,
                        const int  bufsize_)
{
  int i;
  DiscLabel *dl;

  dl = (DiscLabel*)buf_;

  if(dl->dl_RecordType != DISC_LABEL_RECORD_TYPE)
    return false;
  if(dl->dl_VolumeStructureVersion != VOLUME_STRUCTURE_LINKED_MEM)
    return false;
  for(i = 0; i < VOLUME_SYNC_BYTE_LEN; i++)
    {
      if(dl->dl_VolumeSyncBytes[i] != VOLUME_SYNC_BYTE)
        return false;
    }

  return true;
}

// The below code mimics the official 3DO formatting tool "format"
// https://github.com/trapexit/portfolio_os/blob/master/src/filesystem/format.c
// https://github.com/trapexit/portfolio_os/blob/master/src/filesystem/lmadm.c
void
opera_nvram_init(void      *buf_,
                 const int  bufsize_)
{
  DiscLabel *disc_label;
  LinkedMemBlock *anchor_block;
  LinkedMemBlock *free_block;

  disc_label   = (DiscLabel*)buf_;
  anchor_block = (LinkedMemBlock*)&disc_label[1];
  free_block   = &anchor_block[1];

  memset(buf_,0,bufsize_);

  disc_label->dl_RecordType = DISC_LABEL_RECORD_TYPE;
  memset(disc_label->dl_VolumeSyncBytes,VOLUME_SYNC_BYTE,VOLUME_SYNC_BYTE_LEN);
  disc_label->dl_VolumeStructureVersion = VOLUME_STRUCTURE_LINKED_MEM;
  disc_label->dl_VolumeFlags = 0;
  strncpy((char*)disc_label->dl_VolumeCommentary,"opera formatted", VOLUME_COM_LEN);
  strncpy((char*)disc_label->dl_VolumeIdentifier,"nvram", VOLUME_ID_LEN);
  disc_label->dl_VolumeUniqueIdentifier = swap32_if_le(NVRAM_VOLUME_UNIQUE_ID); // ???
  disc_label->dl_VolumeBlockSize = swap32_if_le(NVRAM_BLOCKSIZE);
  disc_label->dl_VolumeBlockCount = swap32_if_le(bufsize_);
  disc_label->dl_RootUniqueIdentifier = swap32_if_le(NVRAM_ROOT_UNIQUE_ID);
  disc_label->dl_RootDirectoryBlockCount = 0;
  disc_label->dl_RootDirectoryBlockSize = swap32_if_le(NVRAM_BLOCKSIZE);
  disc_label->dl_RootDirectoryLastAvatarIndex = 0;
  disc_label->dl_RootDirectoryAvatarList[0] = swap32_if_le(sizeof(DiscLabel));

  anchor_block->fingerprint = swap32_if_le(FINGERPRINT_ANCHORBLOCK);
  anchor_block->flinkoffset = swap32_if_le(sizeof(DiscLabel) + sizeof(LinkedMemBlock));
  anchor_block->blinkoffset = swap32_if_le(sizeof(DiscLabel) + sizeof(LinkedMemBlock));
  anchor_block->blockcount  = swap32_if_le(sizeof(LinkedMemBlock));
  anchor_block->headerblockcount = swap32_if_le(sizeof(LinkedMemBlock));

  free_block->fingerprint = swap32_if_le(FINGERPRINT_FREEBLOCK);
  free_block->flinkoffset = swap32_if_le(sizeof(DiscLabel));
  free_block->blinkoffset = swap32_if_le(sizeof(DiscLabel));
  free_block->blockcount  = swap32_if_le(bufsize_ - sizeof(DiscLabel) - sizeof(LinkedMemBlock));
  free_block->headerblockcount = swap32_if_le(sizeof(LinkedMemBlock));
}
