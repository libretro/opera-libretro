#ifndef CUEFILE_H__
#define CUEFILE_H__

#include <stdint.h>

#define SECTOR_SIZE_2048 2048
#define SECTOR_SIZE_2352 2352

#define SECTOR_OFFSET_MODE1_2048 0
#define SECTOR_OFFSET_MODE1_2352 16
#define SECTOR_OFFSET_MODE2_2352 24

#define CUE_MAX_TRACKS 100
#define CUE_MAX_FILES  CUE_MAX_TRACKS

typedef enum {MODE1_2048, MODE1_2352, MODE2_2352, CUE_MODE_AUDIO, CUE_MODE_UNKNOWN} CD_format;

typedef struct {
  uint8_t   track_num;
  uint8_t   file_index;
  CD_format format;
  uint32_t  index_lba;
  uint32_t  frames;
} cue_track_t;

typedef struct {
  char *path;
} cue_file_t;

typedef struct {
  CD_format cd_format;
  char *    cd_image;
  int       num_files;
  cue_file_t files[CUE_MAX_FILES];
  int       num_tracks;
  cue_track_t tracks[CUE_MAX_TRACKS];
} cueFile;

cueFile    *cue_get(const char *path);
void        cue_free(cueFile *cue);
const char *cue_get_cd_format_name(CD_format cd_format);
int         cue_is_cue_path(const char *path);

#endif
