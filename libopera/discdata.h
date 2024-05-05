#include "extern_c.h"

#include <stdint.h>

EXTERN_C_BEGIN

/*
  Define the position of the primary label on each Opera disc, the
  block offset between avatars, and the index of the last avatar
  (i.e. the avatar count minus one).  The latter figure *must* match
  the ROOT_HIGHEST_AVATAR figure from "filesystem.h", as the same
  File structure is use to read the label at boot time, and to provide
  access to the root directory.
*/

#define DISC_LABEL_RECORD_TYPE    1
#define DISC_BLOCK_SIZE           2048
#define DISC_LABEL_OFFSET         225
#define DISC_LABEL_AVATAR_DELTA   32786
#define DISC_LABEL_HIGHEST_AVATAR 7
#define DISC_TOTAL_BLOCKS         330000

#define ROOT_HIGHEST_AVATAR       7
#define FILESYSTEM_MAX_NAME_LEN   32

#define VOLUME_STRUCTURE_OPERA_READONLY    1
#define VOLUME_STRUCTURE_LINKED_MEM        2

#define NVRAM_VOLUME_UNIQUE_ID    -1
#define NVRAM_ROOT_UNIQUE_ID      -2
#define VOLUME_SYNC_BYTE          'Z'
#define VOLUME_SYNC_BYTE_LEN      5
#define VOLUME_COM_LEN      	  32
#define VOLUME_ID_LEN      	  32

/*
// This disc won't necessarily cause a reboot when inserted.  This flag is
// advisory ONLY. Only by checking with cdromdipir can you be really sure.
// Place in dl_VolumeFlags.  Note: the first volume gets this flag also.
*/
#define	VOLUME_FLAGS_DATADISC	0x01

/*
  Data structures written on CD disc (Compact Disc disc?)
*/
typedef struct DiscLabel DiscLabel;
struct DiscLabel
{
  uint8_t  dl_RecordType;       /* Should contain 1 */
  uint8_t  dl_VolumeSyncBytes[VOLUME_SYNC_BYTE_LEN]; /* Synchronization byte */
  uint8_t  dl_VolumeStructureVersion; /* Should contain 1 */
  uint8_t  dl_VolumeFlags;      /* Should contain 0 */
  uint8_t  dl_VolumeCommentary[VOLUME_COM_LEN]; /* Random comments about volume */
  uint8_t  dl_VolumeIdentifier[VOLUME_ID_LEN]; /* Should contain disc name */
  uint32_t dl_VolumeUniqueIdentifier; /* Roll a billion-sided die */
  uint32_t dl_VolumeBlockSize;  /* Usually contains 2048 */
  uint32_t dl_VolumeBlockCount; /* # of blocks on disc */
  uint32_t dl_RootUniqueIdentifier; /* Roll a billion-sided die */
  uint32_t dl_RootDirectoryBlockCount; /* # of blocks in root */
  uint32_t dl_RootDirectoryBlockSize; /* usually same as vol blk size */
  uint32_t dl_RootDirectoryLastAvatarIndex; /* should contain 7 */
  uint32_t dl_RootDirectoryAvatarList[ROOT_HIGHEST_AVATAR+1];
};

typedef struct DirectoryHeader DirectoryHeader;
struct DirectoryHeader
{
  int32_t  dh_NextBlock;
  int32_t  dh_PrevBlock;
  uint32_t dh_Flags;
  uint32_t dh_FirstFreeByte;
  uint32_t dh_FirstEntryOffset;
};

#define DIRECTORYRECORD(AVATARCOUNT)                    \
  uint32_t dir_Flags;                                   \
  uint32_t dir_UniqueIdentifier;                        \
  uint32_t dir_Type;                                    \
  uint32_t dir_BlockSize;                               \
  uint32_t dir_ByteCount;                               \
  uint32_t dir_BlockCount;                              \
  uint32_t dir_Burst;                                   \
  uint32_t dir_Gap;                                     \
  char     dir_FileName[FILESYSTEM_MAX_NAME_LEN];       \
  uint32_t dir_LastAvatarIndex;                         \
  uint32_t dir_AvatarList[AVATARCOUNT];

typedef struct DirectoryRecord {
  DIRECTORYRECORD(1)
} DirectoryRecord;

#define DIRECTORY_LAST_IN_DIR        0x80000000
#define DIRECTORY_LAST_IN_BLOCK      0x40000000

EXTERN_C_END
