#ifndef LIBRETRO_RETRO_CDIMAGE_H_INCLUDED
#define LIBRETRO_RETRO_CDIMAGE_H_INCLUDED

#include "cuefile.h"

#include <boolean.h>
#include <streams/interface_stream.h>
#include <stddef.h>
#include <stdint.h>

#define CDIMAGE_MAX_TRACKS 100
#define CDIMAGE_MAX_FILES  CDIMAGE_MAX_TRACKS

typedef enum cdimage_track_type_e
{
  CDIMAGE_TRACK_DATA  = 0,
  CDIMAGE_TRACK_AUDIO = 1
} cdimage_track_type_t;

struct cdimage_file_s
{
  intfstream_t *fp;
  size_t        size;
};

typedef struct cdimage_file_s cdimage_file_t;

struct cdimage_track_s
{
  uint8_t  track_num;
  cdimage_track_type_t type;
  uint8_t  file_index;
  uint32_t start_lba;
  uint32_t frames;
  size_t   file_offset;
  uint16_t mode;        /* sector size: 2048, 2352, 2448 */
  uint8_t  offset;      /* data offset within sector */
};

typedef struct cdimage_track_s cdimage_track_t;

struct cdimage_s
{
  intfstream_t *fp;
  int           sector_size;
  int           sector_offset;
  size_t        logical_blocks;
  int           num_files;
  cdimage_file_t files[CDIMAGE_MAX_FILES];
  int           num_tracks;
  bool          swap_audio;
  cdimage_track_t tracks[CDIMAGE_MAX_TRACKS];
};

typedef struct cdimage_s cdimage_t;

int
retro_cdimage_open_chd(const char *path_,
                       cdimage_t  *cdimage_);
int
retro_cdimage_open_iso(const char *path_,
                       cdimage_t  *cdimage_);
int
retro_cdimage_open_bin(const char *path_,
                       cdimage_t  *cdimage_);
int
retro_cdimage_open_cue(const char *path_,
                       cdimage_t  *cdimage_);
int
retro_cdimage_open(const char *path_,
                   cdimage_t  *cdimage_);

int
retro_cdimage_close(cdimage_t *cdimage_);

int
retro_cdimage_get_track_for_sector(cdimage_t *cdimage_,
                                    size_t     sector_);

ssize_t
retro_cdimage_read(cdimage_t *cdimage_,
                    size_t     sector_,
                    void      *buf_,
                    size_t     bufsize_);

ssize_t
retro_cdimage_read_sector(cdimage_t *cdimage_,
                          size_t     sector_,
                          void      *buf_,
                          size_t     bufsize_);

ssize_t
retro_cdimage_get_number_of_logical_blocks(cdimage_t *cdimage_);

void
retro_cdimage_get_toc(cdimage_t      *cdimage_,
                      uint8_t        *track_first_,
                      uint8_t        *track_last_,
                      uint8_t        *disc_id_,
                      void           *disc_toc_,
                      uint32_t        disc_toc_size_);

#endif
