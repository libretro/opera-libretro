#ifndef CUEFILE_H__
#define CUEFILE_H__

#define SECTOR_SIZE_2048 2048
#define SECTOR_SIZE_2352 2352

#define SECTOR_OFFSET_MODE1_2048 0
#define SECTOR_OFFSET_MODE1_2352 16
#define SECTOR_OFFSET_MODE2_2352 24

typedef enum {MODE1_2048, MODE1_2352, MODE2_2352, CUE_MODE_UNKNOWN} CD_format;

typedef struct {
    CD_format cd_format;
} cueFile;

cueFile *cue_get(const char *path, retro_log_printf_t log_cb);
const char *cue_get_cd_format_name(CD_format cd_format);

#endif
