#include "retro_cdimage.h"

#include "file/file_path.h"
#include "retro_endianness.h"
#include "streams/chd_stream.h"
#include "streams/interface_stream.h"

#include "libretro.h"

#include <stdlib.h>
#include <strings.h>

#define min(a,b) \
  ({ __typeof__ (a) _a = (a);   \
     __typeof__ (b) _b = (b);   \
     _a < _b ? _a : _b; })

int
retro_cdimage_open_chd(const char       *path_,
                       struct cdimage_t *cdimage_)
{
  cdimage_->fp = intfstream_open_chd_track(path_,
                                           RETRO_VFS_FILE_ACCESS_READ,
                                           RETRO_VFS_FILE_ACCESS_HINT_NONE,
                                           CHDSTREAM_TRACK_PRIMARY);
  if(cdimage_->fp == NULL)
    return -1;

  cdimage_->sector_size   = 2448;
  cdimage_->sector_offset = 0;

  return 0;
}

int
retro_cdimage_open_iso(const char       *path_,
                       struct cdimage_t *cdimage_)
{
  int size;

  cdimage_->fp = intfstream_open_file(path_,
                                      RETRO_VFS_FILE_ACCESS_READ,
                                      RETRO_VFS_FILE_ACCESS_HINT_NONE);
  if(cdimage_->fp == NULL)
    return -1;

  size = intfstream_get_size(cdimage_->fp);
  if((size % 2048) == 0)
    {
      cdimage_->sector_size   = 2048;
      cdimage_->sector_offset = 0;
    }
  else if((size % 2352) == 0)
    {
      cdimage_->sector_size   = 2352;
      cdimage_->sector_offset = 16;
    }
  else
    {
      cdimage_->sector_size   = 2048;
      cdimage_->sector_offset = 0;
    }

  return 0;
}

int
retro_cdimage_open_bin(const char       *path_,
                       struct cdimage_t *cdimage_)
{
  return retro_cdimage_open_iso(path_,cdimage_);
}

int
retro_cdimage_open_cue(const char       *path_,
                       struct cdimage_t *cdimage_)
{
  int rv;
  const char *ext;
  cueFile *cue_file;
  intfstream_t *stream;

  cue_file = cue_get(path_);
  if(cue_file == NULL)
    return -1;

  ext = path_get_extension(path_);
  if(!strcasecmp(ext,"iso"))
    rv = retro_cdimage_open_iso(path_,cdimage_);
  else if(!strcasecmp(ext,"bin"))
    rv = retro_cdimage_open_bin(path_,cdimage_);
  else
    rv = -1;

  if(rv == -1)
    {
      free(cue_file);
      return -1;
    }

  switch(cue_file->cd_format)
    {
    case MODE1_2048:
      cdimage_->sector_size   = 2048;
      cdimage_->sector_offset = 0;
      break;
    case MODE1_2352:
      cdimage_->sector_size   = 2352;
      cdimage_->sector_offset = 16;
      break;
    case MODE2_2352:
      cdimage_->sector_size   = 2352;
      cdimage_->sector_offset = 24;
      break;
    default:
    case CUE_MODE_UNKNOWN:
      cdimage_->sector_size   = 2048;
      cdimage_->sector_offset = 0;
      break;
    }

  free(cue_file);

  return 0;
}

int
retro_cdimage_open(const char       *path_,
                   struct cdimage_t *cdimage_)
{
  const char *ext;

  ext = path_get_extension(path_);
  if(ext == NULL)
    return -1;

  if(!strcasecmp(ext,"chd"))
    return retro_cdimage_open_chd(path_,cdimage_);
  if(!strcasecmp(ext,"cue"))
    return retro_cdimage_open_cue(path_,cdimage_);
  if(!strcasecmp(ext,"iso"))
    return retro_cdimage_open_iso(path_,cdimage_);
  if(!strcasecmp(ext,"bin"))
    return retro_cdimage_open_bin(path_,cdimage_);

  return -1;
}

int
retro_cdimage_close(struct cdimage_t *cdimage_)
{
  int rv;

  rv = 0;
  if(cdimage_->fp)
    rv = intfstream_close(cdimage_->fp);

  cdimage_->fp            = NULL;
  cdimage_->sector_size   = 0;
  cdimage_->sector_offset = 0;

  return rv;
}

ssize_t
retro_cdimage_read(struct cdimage_t *cdimage_,
                   size_t            sector_,
                   void             *buf_,
                   size_t            bufsize_)
{
  int rv;
  size_t pos;

  bufsize_ = min(bufsize_,cdimage_->sector_size);
  pos = ((sector_ * cdimage_->sector_size) + cdimage_->sector_offset);

  rv = intfstream_seek(cdimage_->fp,pos,RETRO_VFS_SEEK_POSITION_START);
  if(rv == -1)
    return -1;

  return intfstream_read(cdimage_->fp,buf_,bufsize_);
}

ssize_t
retro_cdimage_get_number_of_logical_blocks(struct cdimage_t *cdimage_)
{
  int rv;
  size_t pos;
  uint32_t blocks;

  pos = (cdimage_->sector_offset + 80);
  rv = intfstream_seek(cdimage_->fp,pos,RETRO_VFS_SEEK_POSITION_START);
  if(rv == -1)
    return -1;

  rv = intfstream_read(cdimage_->fp,&blocks,sizeof(blocks));
  if(rv == -1)
    return -1;

  return swap_if_little32(blocks);
}
