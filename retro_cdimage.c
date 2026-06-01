#include "retro_cdimage.h"

#include "libopera/opera_cdrom.h"
#include "opera_lr_callbacks.h"
#include "retro_miscellaneous.h"

#include "file/file_path.h"
#include "streams/interface_stream.h"

#ifdef HAVE_CHD
#include "streams/chd_stream.h"

#include "deps/libchdr/include/libchdr/chd.h"
#endif

#include "endianness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CHD
static
bool
cdimage_chd_pregap_in_track(const chdstream_cdrom_metadata_t *metadata_)
{
  return metadata_->pgtype[0] == 'V';
}

static
uint32_t
cdimage_chd_data_frames(const chdstream_cdrom_metadata_t *metadata_)
{
  if(!cdimage_chd_pregap_in_track(metadata_))
    return metadata_->frames;

  if(metadata_->frames <= metadata_->pregap)
    return 0;

  return metadata_->frames - metadata_->pregap;
}
#endif

static
void
cdimage_set_size_and_offset(cdimage_t *cd_,
                            const int  size_,
                            const int  offset_)
{
  cd_->sector_size   = size_;
  cd_->sector_offset = offset_;
  cd_->logical_blocks = 0;
  cd_->num_tracks    = 0;
  cd_->swap_audio    = false;
  memset(cd_->tracks,0,sizeof(cd_->tracks));
}

static
ssize_t
cdimage_get_stream_size(intfstream_t *fp_)
{
  int rv;
  size_t size;
  size_t current_pos;

  if(fp_ == NULL)
    return -1;

  current_pos = intfstream_tell(fp_);
  rv = intfstream_seek(fp_,0,RETRO_VFS_SEEK_POSITION_END);
  if(rv == -1)
    return -1;

  size = intfstream_tell(fp_);
  rv = intfstream_seek(fp_,current_pos,RETRO_VFS_SEEK_POSITION_START);
  if(rv == -1)
    return -1;

  return size;
}

static
ssize_t
cdimage_get_file_size(cdimage_t *cdimage_)
{
  return cdimage_get_stream_size(cdimage_->fp);
}

static
uint16_t
cdimage_mode_for_cue_format(CD_format format_)
{
  switch(format_)
    {
    case MODE1_2048:
      return 2048;
    case MODE1_2352:
    case MODE2_2352:
    case CUE_MODE_AUDIO:
    default:
      return 2352;
    }
}

static
uint8_t
cdimage_offset_for_cue_format(CD_format format_)
{
  switch(format_)
    {
    case MODE1_2048:
      return 0;
    case MODE1_2352:
      return 16;
    case MODE2_2352:
      return 24;
    default:
      return 0;
    }
}

static
bool
cdimage_raw_2352_sector_is_data(const uint8_t *sector_)
{
  static const uint8_t sync[12] =
    { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
  static const uint8_t opera_header[8] =
    { 0x01, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x01, 0x00 };

  if(!memcmp(sector_,sync,sizeof(sync)) &&
     ((sector_[15] == 1) || (sector_[15] == 2)))
    return true;

  return memcmp(sector_ + 16,opera_header,sizeof(opera_header)) == 0;
}

static
bool
cdimage_raw_2352_stream_is_data(intfstream_t *fp_,
                                size_t        frames_)
{
  uint8_t sector[2352];
  size_t current_pos;
  size_t scan_frames;
  size_t i;
  bool rv;

  if(fp_ == NULL)
    return false;

  current_pos = intfstream_tell(fp_);
  scan_frames = MIN(frames_,300);
  rv = false;

  for(i = 0; i < scan_frames; i++)
    {
      if(intfstream_seek(fp_,i * sizeof(sector),RETRO_VFS_SEEK_POSITION_START) == -1)
        break;
      if(intfstream_read(fp_,sector,sizeof(sector)) != sizeof(sector))
        break;
      if(cdimage_raw_2352_sector_is_data(sector))
        {
          rv = true;
          break;
        }
    }

  intfstream_seek(fp_,current_pos,RETRO_VFS_SEEK_POSITION_START);
  return rv;
}

static
int
cdimage_file_extension_supported(const char *path_)
{
  const char *ext = path_get_extension(path_);

  if(ext == NULL)
    return 0;

  return (!strcasecmp(ext,"iso") ||
          !strcasecmp(ext,"bin") ||
          !strcasecmp(ext,"img"));
}

static
ssize_t
cdimage_open_cue_file(cdimage_t  *cdimage_,
                      int         index_,
                      const char *path_)
{
  intfstream_t *fp;
  ssize_t size;

  if((index_ < 0) || (index_ >= CDIMAGE_MAX_FILES))
    return -1;

  if(!cdimage_file_extension_supported(path_))
    return -1;

  fp = intfstream_open_file(path_,
                            RETRO_VFS_FILE_ACCESS_READ,
                            RETRO_VFS_FILE_ACCESS_HINT_NONE);
  if(fp == NULL)
    return -1;

  size = cdimage_get_stream_size(fp);
  if(size <= 0)
    {
      intfstream_close(fp);
      return -1;
    }

  cdimage_->files[index_].fp   = fp;
  cdimage_->files[index_].size = (size_t)size;
  if(index_ == 0)
    cdimage_->fp = fp;

  return size;
}

int
retro_cdimage_open_chd(const char *path_,
                       cdimage_t  *cdimage_)
{
#ifdef HAVE_CHD
  uint8_t buf[8];
  uint8_t pattern[8] = { 0x01, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x01, 0x00 };
  chd_file *chd;
  chd_error err;
  int i;
  uint32_t lba;
  size_t file_offset;

  cdimage_->fp = intfstream_open_chd_track(path_,
                                           RETRO_VFS_FILE_ACCESS_READ,
                                           RETRO_VFS_FILE_ACCESS_HINT_NONE,
                                           CHDSTREAM_TRACK_FULL_DISC);
  if(cdimage_->fp == NULL)
    return -1;

  cdimage_->num_files = 0;
  memset(cdimage_->files,0,sizeof(cdimage_->files));

  intfstream_seek(cdimage_->fp,0,RETRO_VFS_SEEK_POSITION_START);
  intfstream_read(cdimage_->fp,buf,8);
  intfstream_seek(cdimage_->fp,0,RETRO_VFS_SEEK_POSITION_START);

  /*
   * Heuristic: check the first sector header for a 3DO MODE1 disc.
   * 3DO MODE1 discs use 2448-byte sectors (2352 + 96 subcode).
   * Discs whose first sector does not start with this pattern
   * are treated as MODE1_RAW (2352-byte sectors + 16-byte data offset).
   * This is not a generic CD-ROM MODE1 detection; it matches a specific
   * pattern found in 3DO full-disc CHD dumps.
   */
  if(!memcmp(buf,pattern,sizeof(pattern)))
    cdimage_set_size_and_offset(cdimage_,2448,0);
  else
    cdimage_set_size_and_offset(cdimage_,2352,16);
  cdimage_->swap_audio = false;

  /* Enumerate tracks from CHD metadata */
  err = chd_open(path_, CHD_OPEN_READ, NULL, &chd);
  if(err == CHDERR_NONE)
    {
      const chd_header *hd = chd_get_header(chd);
      lba = 0;
      file_offset = 0;
      for(i = 0; i < CDIMAGE_MAX_TRACKS; i++)
        {
          chdstream_cdrom_metadata_t metadata;
          uint32_t data_frames;

          if(!chdstream_get_cdrom_metadata(chd,i,&metadata))
            break;

          data_frames = cdimage_chd_data_frames(&metadata);

          cdimage_->tracks[i].track_num  = (uint8_t)metadata.track;
          cdimage_->tracks[i].type       = (!strcmp(metadata.type, "AUDIO") ?
                                            CDIMAGE_TRACK_AUDIO : CDIMAGE_TRACK_DATA);
          cdimage_->tracks[i].frames     = data_frames;
          cdimage_->tracks[i].file_index = 0;
          if(!strcmp(metadata.type, "AUDIO"))
            {
              cdimage_->tracks[i].mode   = 2352;
              cdimage_->tracks[i].offset = 0;
            }
          else if(!strcmp(metadata.type, "MODE1_RAW"))
            {
              cdimage_->tracks[i].mode   = 2352;
              cdimage_->tracks[i].offset = 16;
            }
          else if(!strcmp(metadata.type, "MODE2_RAW"))
            {
              cdimage_->tracks[i].mode   = 2352;
              cdimage_->tracks[i].offset = 24;
            }
          else
            {
              cdimage_->tracks[i].mode   = hd ? hd->unitbytes : cdimage_->sector_size;
              cdimage_->tracks[i].offset = 0;
            }

          cdimage_->tracks[i].start_lba = lba + metadata.pregap;
          cdimage_->tracks[i].file_offset = file_offset +
            ((size_t)metadata.pregap * cdimage_->tracks[i].mode);

          lba += metadata.pregap + data_frames + metadata.postgap;
          file_offset += ((size_t)metadata.pregap +
                          data_frames +
                          metadata.postgap) * cdimage_->tracks[i].mode;

          cdimage_->num_tracks++;
        }
      if(cdimage_->num_tracks > 0)
        cdimage_->logical_blocks = lba;
      chd_close(chd);
    }

  return 0;
#else
  (void)path_;
  (void)cdimage_;
  return -1;
#endif
}

int
retro_cdimage_open_iso(const char *path_,
                       cdimage_t  *cdimage_)
{
  const char *ext;
  int64_t size;
  bool raw_audio;

  cdimage_->fp = intfstream_open_file(path_,
                                      RETRO_VFS_FILE_ACCESS_READ,
                                      RETRO_VFS_FILE_ACCESS_HINT_NONE);
  if(cdimage_->fp == NULL)
    return -1;

  cdimage_->num_files = 0;
  memset(cdimage_->files,0,sizeof(cdimage_->files));

  raw_audio = false;
  ext = path_get_extension(path_);
  size = intfstream_get_size(cdimage_->fp);
  if((size % 2048) == 0)
    cdimage_set_size_and_offset(cdimage_,2048,0);
  else if((size % 2352) == 0)
    {
      size_t frames = (size_t)(size / 2352);
      raw_audio = (ext &&
                   (!strcasecmp(ext,"bin") || !strcasecmp(ext,"img")) &&
                   !cdimage_raw_2352_stream_is_data(cdimage_->fp,frames));
      cdimage_set_size_and_offset(cdimage_,2352,raw_audio ? 0 : 16);
      cdimage_->swap_audio = raw_audio;
    }
  else
    cdimage_set_size_and_offset(cdimage_,2048,0);

  /* Without CUE metadata, a 2352-byte BIN may be a single-track audio disc. */
  cdimage_->num_tracks    = 1;
  cdimage_->tracks[0].track_num = 1;
  cdimage_->tracks[0].file_index = 0;
  cdimage_->tracks[0].start_lba = 0;
  cdimage_->tracks[0].type      = raw_audio ? CDIMAGE_TRACK_AUDIO : CDIMAGE_TRACK_DATA;
  cdimage_->tracks[0].file_offset = 0;
  cdimage_->tracks[0].mode      = cdimage_->sector_size;
  cdimage_->tracks[0].offset    = cdimage_->sector_offset;
  cdimage_->tracks[0].frames    = size / cdimage_->sector_size;
  cdimage_->logical_blocks      = cdimage_->tracks[0].frames;

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
  cueFile *cue_file;
  int i;
  int last_track_for_file[CDIMAGE_MAX_FILES];
  uint32_t file_base_lba[CDIMAGE_MAX_FILES];
  uint32_t file_frames[CDIMAGE_MAX_FILES];

  cue_file = cue_get(path_);
  if(cue_file == NULL)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: Failed to parse CUE file: %s\n",
                          path_);
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
    case CUE_MODE_AUDIO:
      cdimage_set_size_and_offset(cdimage_,2352,0);
      break;
    default:
    case CUE_MODE_UNKNOWN:
      cdimage_set_size_and_offset(cdimage_,2352,0);
      break;
    }

  cdimage_->fp = NULL;
  cdimage_->num_files = 0;
  memset(cdimage_->files,0,sizeof(cdimage_->files));
  memset(file_base_lba,0,sizeof(file_base_lba));
  memset(file_frames,0,sizeof(file_frames));
  for(i = 0; i < CDIMAGE_MAX_FILES; i++)
    last_track_for_file[i] = -1;

  for(i = 0; i < cue_file->num_files; i++)
    {
      if(cdimage_open_cue_file(cdimage_,i,cue_file->files[i].path) < 0)
        {
          retro_log_printf_cb(RETRO_LOG_ERROR,
                              "[Opera]: Failed to open CUE image file: %s\n",
                              cue_file->files[i].path ? cue_file->files[i].path : "(null)");
          cue_free(cue_file);
          retro_cdimage_close(cdimage_);
          return -1;
        }
      cdimage_->num_files++;
    }

  /* Populate track list from CUE */
  cdimage_->num_tracks = cue_file->num_tracks;
  for(i = 0; i < cue_file->num_tracks; i++)
    {
      int last_track;
      uint8_t file_index;

      file_index = cue_file->tracks[i].file_index;
      if(file_index >= cdimage_->num_files)
        {
          retro_log_printf_cb(RETRO_LOG_ERROR,
                              "[Opera]: CUE track %u references missing file %u\n",
                              cue_file->tracks[i].track_num,
                              file_index);
          cue_free(cue_file);
          retro_cdimage_close(cdimage_);
          return -1;
        }

      if(cue_file->tracks[i].format == CUE_MODE_UNKNOWN)
        {
          retro_log_printf_cb(RETRO_LOG_ERROR,
                              "[Opera]: CUE track %u has unsupported format\n",
                              cue_file->tracks[i].track_num);
          cue_free(cue_file);
          retro_cdimage_close(cdimage_);
          return -1;
        }

      cdimage_->tracks[i].track_num = cue_file->tracks[i].track_num;
      cdimage_->tracks[i].file_index = file_index;
      cdimage_->tracks[i].type      = (cue_file->tracks[i].format == CUE_MODE_AUDIO) ?
        CDIMAGE_TRACK_AUDIO : CDIMAGE_TRACK_DATA;
      cdimage_->tracks[i].mode      = cdimage_mode_for_cue_format(cue_file->tracks[i].format);
      cdimage_->tracks[i].offset    = cdimage_offset_for_cue_format(cue_file->tracks[i].format);
      if(cdimage_->tracks[i].type == CDIMAGE_TRACK_AUDIO)
        cdimage_->swap_audio = true;

      last_track = last_track_for_file[file_index];
      if((last_track >= 0) &&
         (cue_file->tracks[i].index_lba >= cue_file->tracks[last_track].index_lba))
        {
          cdimage_->tracks[i].file_offset =
            cdimage_->tracks[last_track].file_offset +
            ((cue_file->tracks[i].index_lba -
              cue_file->tracks[last_track].index_lba) *
             cdimage_->tracks[last_track].mode);
        }
      else
        {
          cdimage_->tracks[i].file_offset =
            cue_file->tracks[i].index_lba * cdimage_->tracks[i].mode;
        }
      last_track_for_file[file_index] = i;
    }

  for(i = 0; i < cdimage_->num_files; i++)
    {
      int track;

      track = last_track_for_file[i];
      if(track >= 0)
        {
          if(cdimage_->files[i].size > cdimage_->tracks[track].file_offset)
            file_frames[i] =
              cue_file->tracks[track].index_lba +
              (uint32_t)((cdimage_->files[i].size -
                          cdimage_->tracks[track].file_offset) /
                         cdimage_->tracks[track].mode);
        }
      else if(cdimage_->sector_size > 0)
        file_frames[i] = (uint32_t)(cdimage_->files[i].size / cdimage_->sector_size);
    }

  for(i = 1; i < cdimage_->num_files; i++)
    file_base_lba[i] = (file_base_lba[i - 1] + file_frames[i - 1]);

  cdimage_->logical_blocks = 0;
  for(i = 0; i < cdimage_->num_files; i++)
    cdimage_->logical_blocks += file_frames[i];

  for(i = 0; i < cdimage_->num_tracks; i++)
    {
      uint8_t file_index = cdimage_->tracks[i].file_index;
      cdimage_->tracks[i].start_lba =
        file_base_lba[file_index] + cue_file->tracks[i].index_lba;
    }

  for(i = 0; i < cdimage_->num_tracks; i++)
    {
      uint32_t next_lba;

      next_lba = (((i + 1) < cdimage_->num_tracks) ?
                  cdimage_->tracks[i + 1].start_lba :
                  (uint32_t)cdimage_->logical_blocks);
      if(next_lba > cdimage_->tracks[i].start_lba)
        cdimage_->tracks[i].frames = next_lba - cdimage_->tracks[i].start_lba;
    }

  cue_free(cue_file);

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
  int i;

  rv = 0;
  if(cdimage_->num_files > 0)
    {
      for(i = 0; i < cdimage_->num_files; i++)
        {
          if(cdimage_->files[i].fp)
            {
              int close_rv = intfstream_close(cdimage_->files[i].fp);
              if(close_rv != 0)
                rv = close_rv;
            }
        }
    }
  else if(cdimage_->fp)
    {
      rv = intfstream_close(cdimage_->fp);
    }

  cdimage_->fp             = NULL;
  cdimage_->sector_size    = 0;
  cdimage_->sector_offset  = 0;
  cdimage_->logical_blocks = 0;
  cdimage_->num_files      = 0;
  cdimage_->num_tracks     = 0;
  cdimage_->swap_audio     = false;
  memset(cdimage_->files,0,sizeof(cdimage_->files));
  memset(cdimage_->tracks,0,sizeof(cdimage_->tracks));

  return rv;
}

void
retro_cdimage_get_toc(cdimage_t *cdimage_,
                      uint8_t   *track_first_,
                      uint8_t   *track_last_,
                      uint8_t   *disc_id_,
                      void      *disc_toc_,
                      uint32_t   disc_toc_size_)
{
  int i;
  uint8_t first_track;
  uint8_t last_track;
  uint32_t max_entries;

  max_entries = (disc_toc_size_ / 8);
  first_track = 0;
  last_track  = 0;

  if(track_first_)
    *track_first_ = 1;
  if(track_last_)
    *track_last_ = 1;
  if(disc_id_)
    *disc_id_ = MEI_DISC_DA_OR_CDROM;

  if((cdimage_->num_tracks == 0) || (max_entries == 0))
    return;

  for(i = 0; i < cdimage_->num_tracks; i++)
    {
      uint8_t *toc_entry;
      uint8_t track_num;
      uint32_t minutes;
      uint32_t seconds;
      uint32_t frames;
      uint32_t mod;
      uint32_t lba;

      track_num = cdimage_->tracks[i].track_num;
      if((track_num == 0) || (track_num >= max_entries))
        continue;

      lba       = cdimage_->tracks[i].start_lba;
      toc_entry = ((uint8_t*)disc_toc_) + (track_num * 8);

      mod     = ((lba + 150) % (75 * 60));
      minutes = ((lba + 150) / (75 * 60));
      seconds = (mod / 75);
      frames  = (mod % 75);

      if(first_track == 0 || track_num < first_track)
        first_track = track_num;
      if(track_num > last_track)
        last_track = track_num;
      if((cdimage_->tracks[i].type == CDIMAGE_TRACK_DATA) &&
         (cdimage_->tracks[i].mode == 2352) &&
         (cdimage_->tracks[i].offset == 24) &&
         disc_id_)
        *disc_id_ = MEI_DISC_CDROM_XA;

      toc_entry[0] = 0; /* res0 */
      toc_entry[1] = ((cdimage_->tracks[i].type == CDIMAGE_TRACK_DATA) ?
                      (CD_CTL_DATA_TRACK | CD_CTL_Q_POSITION) :
                      (CD_CTL_Q_POSITION));
      toc_entry[2] = track_num;
      toc_entry[3] = 0; /* res1 */
      toc_entry[4] = (uint8_t)minutes;
      toc_entry[5] = (uint8_t)seconds;
      toc_entry[6] = (uint8_t)frames;
      toc_entry[7] = 0; /* res2 */
    }

  if(first_track != 0)
    {
      if(track_first_)
        *track_first_ = first_track;
      if(track_last_)
        *track_last_  = last_track;
    }
}

int
retro_cdimage_get_track_for_sector(cdimage_t *cdimage_,
                                   size_t     sector_)
{
  int i;

  for(i = 0; i < cdimage_->num_tracks; i++)
    {
      if(sector_ < cdimage_->tracks[i].start_lba)
        continue;
      if(((i + 1) < cdimage_->num_tracks) &&
         (sector_ >= cdimage_->tracks[i + 1].start_lba))
        continue;
      return i;
    }

  return -1;
}

static
intfstream_t*
cdimage_stream_for_track(cdimage_t             *cdimage_,
                         const cdimage_track_t *track_)
{
  if(cdimage_->num_files <= 0)
    return cdimage_->fp;

  if(track_->file_index >= cdimage_->num_files)
    return NULL;

  return cdimage_->files[track_->file_index].fp;
}

static
void
cdimage_swap_audio_byte_pairs(void    *buf_,
                              ssize_t  len_)
{
  ssize_t i;
  uint8_t *buf = (uint8_t*)buf_;

  for(i = 0; (i + 1) < len_; i += 2)
    {
      uint8_t tmp = buf[i];
      buf[i]      = buf[i + 1];
      buf[i + 1]  = tmp;
    }
}

static
bool
cdimage_needs_audio_swap(const cdimage_t       *cdimage_,
                         const cdimage_track_t *track_)
{
  return (IS_LITTLE_ENDIAN &&
          cdimage_->swap_audio &&
          (track_->type == CDIMAGE_TRACK_AUDIO));
}

ssize_t
retro_cdimage_read(cdimage_t *cdimage_,
                   size_t     sector_,
                   void      *buf_,
                   size_t     bufsize_)
{
  intfstream_t *fp;
  int rv;
  size_t pos;
  size_t offset;

  fp = cdimage_->fp;
  if((cdimage_->num_files > 0) && cdimage_->files[0].fp)
    fp = cdimage_->files[0].fp;
  if(fp == NULL)
    return -1;

  offset = (bufsize_ >= (size_t)cdimage_->sector_size) ?
    0 : (size_t)cdimage_->sector_offset;
  bufsize_ = MIN(bufsize_, cdimage_->sector_size);
  pos      = ((sector_ * cdimage_->sector_size) + offset);

  rv = intfstream_seek(fp,pos,RETRO_VFS_SEEK_POSITION_START);
  if(rv == -1)
    return -1;

  return intfstream_read(fp,buf_,bufsize_);
}

ssize_t
retro_cdimage_read_sector(cdimage_t *cdimage_,
                          size_t     sector_,
                          void      *buf_,
                          size_t     bufsize_)
{
  intfstream_t *fp;
  cdimage_track_t *track_info;
  int track;
  int rv;
  size_t pos;
  size_t offset;

  track = retro_cdimage_get_track_for_sector(cdimage_,sector_);
  if(track < 0)
    {
      if(cdimage_->num_files > 1)
        return -1;

      return retro_cdimage_read(cdimage_,sector_,buf_,bufsize_);
    }

  track_info = &cdimage_->tracks[track];
  fp = cdimage_stream_for_track(cdimage_,track_info);
  if(fp == NULL)
    return -1;

  offset   = ((bufsize_ >= track_info->mode) ? 0 : track_info->offset);
  bufsize_ = MIN(bufsize_, track_info->mode);
  pos      = (track_info->file_offset +
              ((sector_ - track_info->start_lba) * track_info->mode) +
              offset);

  rv = intfstream_seek(fp,pos,RETRO_VFS_SEEK_POSITION_START);
  if(rv == -1)
    return -1;

  rv = (int)intfstream_read(fp,buf_,bufsize_);

  /*
   * Raw BIN/CUE CD-DA sectors need native-order samples for the 3DO READ_DATA
   * audio path. CHD audio is already decoded by libchdr in that order, so CHD
   * images leave swap_audio clear.
   */
  if((rv > 0) && cdimage_needs_audio_swap(cdimage_,track_info))
    cdimage_swap_audio_byte_pairs(buf_,rv);

  return rv;
}

ssize_t
retro_cdimage_get_number_of_logical_blocks(cdimage_t *cdimage_)
{
  ssize_t size;

  if(cdimage_->logical_blocks > 0)
    return cdimage_->logical_blocks;

  size = cdimage_get_file_size(cdimage_);
  if(size == -1)
    return -1;

  if(size == 0)
    return -1;

  return (size / cdimage_->sector_size);
}
