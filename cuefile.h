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
    char *    cd_image;
} cueFile;

cueFile    *cue_get(const char *path);
const char *cue_get_cd_format_name(CD_format cd_format);
int         cue_is_cue_path(const char *path);

#endif
