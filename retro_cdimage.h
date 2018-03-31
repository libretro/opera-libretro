#ifndef LIBRETRO_4DO_RETRO_CDIMAGE_H_INCLUDED
#define LIBRETRO_4DO_RETRO_CDIMAGE_H_INCLUDED

#include "cuefile.h"

#include <streams/interface_stream.h>

struct cdimage_t
{
  intfstream_t *fp;
  int           sector_size;
  int           sector_offset;
};

int
retro_cdimage_open_chd(const char       *path_,
                       struct cdimage_t *cdimage_);
int
retro_cdimage_open_iso(const char       *path_,
                       struct cdimage_t *cdimage_);
int
retro_cdimage_open_bin(const char       *path_,
                       struct cdimage_t *cdimage_);
int
retro_cdimage_open_cue(const char       *path_,
                       struct cdimage_t *cdimage_);
int
retro_cdimage_open(const char       *path_,
                   struct cdimage_t *cdimage_);

int
retro_cdimage_close(struct cdimage_t *cdimage_);

ssize_t
retro_cdimage_read(struct cdimage_t *cdimage_,
                   size_t            sector_,
                   void             *buf_,
                   size_t            bufsize_);

ssize_t
retro_cdimage_get_number_of_logical_blocks(struct cdimage_t *cdimage_);

#endif /* LIBRETRO_4DO_RETRO_CDIMAGE_H_INCLUDED */
