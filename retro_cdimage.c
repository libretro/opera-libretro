#include "retro_cdimage.h"

#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <retro_endianness.h>
#include <streams/chd_stream.h>
#include <streams/interface_stream.h>
#include <retro_miscellaneous.h>

#include <libretro.h>

static
void
cdimage_set_size_and_offset(cdimage_t *cd_,
                            const int  size_,
                            const int  offset_)
{
  cd_->sector_size   = size_;
  cd_->sector_offset = offset_;
}

int
retro_cdimage_open_chd(const char *path_,
                       cdimage_t  *cdimage_)
{
  uint8_t buf[8];
  uint8_t pattern[8] = { 0x01, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x01, 0x00 };

  cdimage_->fp = intfstream_open_chd_track(path_,
                                           RETRO_VFS_FILE_ACCESS_READ,
                                           RETRO_VFS_FILE_ACCESS_HINT_NONE,
                                           CHDSTREAM_TRACK_PRIMARY);
  if(cdimage_->fp == NULL)
    return -1;

  intfstream_seek(cdimage_->fp,0,RETRO_VFS_SEEK_POSITION_START);
  intfstream_read(cdimage_->fp,buf,8);
  intfstream_seek(cdimage_->fp,0,RETRO_VFS_SEEK_POSITION_START);

  /* MODE1 */
  if(!memcmp(buf,pattern,sizeof(pattern)))
    cdimage_set_size_and_offset(cdimage_,2448,0);
  else /* MODE1_RAW */
    cdimage_set_size_and_offset(cdimage_,2352,16);

  return 0;
}

int
retro_cdimage_open_iso(const char *path_,
                       cdimage_t  *cdimage_)
{
  int size;

  cdimage_->fp = intfstream_open_file(path_,
                                      RETRO_VFS_FILE_ACCESS_READ,
                                      RETRO_VFS_FILE_ACCESS_HINT_NONE);
  if(cdimage_->fp == NULL)
    return -1;

  size = intfstream_get_size(cdimage_->fp);
  if((size % 2048) == 0)
    cdimage_set_size_and_offset(cdimage_,2048,0);
  else if((size % 2352) == 0)
    cdimage_set_size_and_offset(cdimage_,2352,16);
  else
    cdimage_set_size_and_offset(cdimage_,2048,0);

  return 0;
}

int
retro_cdimage_open_bin(const char *path_,
                       cdimage_t  *cdimage_)
{
  return retro_cdimage_open_iso(path_,cdimage_);
}

int
retro_cdimage_open_cue(const char *path_,
                       cdimage_t  *cdimage_)
{
  int rv;
  const char *ext;
  cueFile *cue_file;
  intfstream_t *stream;

  cue_file = cue_get(path_);
  if(cue_file == NULL)
    return -1;

  ext = path_get_extension(cue_file->cd_image);
  if(!strcasecmp(ext,"iso"))
    rv = retro_cdimage_open_iso(cue_file->cd_image,cdimage_);
  else if(!strcasecmp(ext,"bin"))
    rv = retro_cdimage_open_bin(cue_file->cd_image,cdimage_);
  else if(!strcasecmp(ext,"img"))
    rv = retro_cdimage_open_bin(cue_file->cd_image,cdimage_);
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
      cdimage_set_size_and_offset(cdimage_,2048,0);
      break;
    case MODE1_2352:
      cdimage_set_size_and_offset(cdimage_,2352,16);
      break;
    case MODE2_2352:
      cdimage_set_size_and_offset(cdimage_,2352,24);
      break;
    default:
    case CUE_MODE_UNKNOWN:
      cdimage_set_size_and_offset(cdimage_,2048,0);
      break;
    }

  free(cue_file);

  return 0;
}

int
retro_cdimage_open(const char *path_,
                   cdimage_t  *cdimage_)
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
retro_cdimage_close(cdimage_t *cdimage_)
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
retro_cdimage_read(cdimage_t *cdimage_,
                   size_t     sector_,
                   void      *buf_,
                   size_t     bufsize_)
{
  int rv;
  size_t pos;

  bufsize_ = MIN(bufsize_, cdimage_->sector_size);
  pos      = ((sector_ * cdimage_->sector_size) + cdimage_->sector_offset);

  rv = intfstream_seek(cdimage_->fp,pos,RETRO_VFS_SEEK_POSITION_START);
  if(rv == -1)
    return -1;

  return intfstream_read(cdimage_->fp,buf_,bufsize_);
}

ssize_t
retro_cdimage_get_number_of_logical_blocks(cdimage_t *cdimage_)
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
