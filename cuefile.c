#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <libretro.h>
#include <streams/file_stream.h>

#include "cuefile.h"
#include "opera_lr_callbacks.h"

#define STRING_MAX 4096

static
RFILE*
cue_get_file_for_image(const char *path)
{
  int i;
  char cue_path_base[STRING_MAX];
  char cue_path[8192];
  char *exts[]   = {".cue", ".CUE"};
  char *last_dot = NULL;

  strcpy(cue_path_base, path);

  last_dot = strrchr(cue_path_base, '.');
  if(last_dot == NULL)
    return NULL;

  /* cut original extension */
  *(last_dot) = '\0';

  for(i=0; i<2; i++)
    {
      RFILE *cue_file = NULL;

      strcpy(cue_path, cue_path_base);
      strcat(cue_path, exts[i]);

      cue_file = filestream_open(cue_path, RETRO_VFS_FILE_ACCESS_READ, 0);
      if(cue_file)
        return cue_file;
    }

  return NULL;
}

static
void
str_to_upper(char *s)
{
  for( ; *s; ++s)
    *s = toupper((unsigned char)*s);
}

static
char*
extract_file_name(const char *path,
                  char       *line)
{
  char file[STRING_MAX];
  char base_path[STRING_MAX];
  char cd_image[STRING_MAX];
  char *last_separator  = NULL;
#ifdef _WIN32
  char slash            = '\\';
#else
  char slash            = '/';
#endif
  char *file_name_end   = NULL;
  char *file_name_start = strstr(line, "\"");
  if(!file_name_start)
    {
      if(retro_log_printf_cb)
        retro_log_printf_cb(RETRO_LOG_INFO, "[Opera]: Missing quotes in : %s\n", line);
      return NULL;
    }

  strcpy(file, ++file_name_start);
  file_name_end = strstr(file, "\"");

  if(!file_name_end)
    {
      if(retro_log_printf_cb)
        retro_log_printf_cb(RETRO_LOG_INFO, "[Opera]: Missing end quote in : %s\n", line);
      return NULL;
    }

  *file_name_end = '\0';

  strcpy(base_path, path);

  last_separator  = strrchr(base_path, slash);
  if(last_separator)
    {
      *last_separator = '\0';
      sprintf(cd_image, "%s%c%s", base_path, slash, file);
    }
  else
    strcpy(cd_image, file);

  return strdup(cd_image);
}

static
uint32_t
cue_msf_to_lba(const char *msf_)
{
  uint32_t min;
  uint32_t sec;
  uint32_t frame;

  if(sscanf(msf_,"%u:%u:%u",&min,&sec,&frame) != 3)
    return 0;

  return ((min * 60 * 75) + (sec * 75) + frame);
}

cueFile*
cue_get(const char *path)
{
  char line[STRING_MAX];
  char line_upper[STRING_MAX];
  int current_file = -1;
  int current_track = -1;
  cueFile *cue   = NULL;
  RFILE *cue_file =
    cue_is_cue_path(path) ?
    filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0) :
    cue_get_file_for_image(path);

  if(!cue_file)
    return NULL;

  cue = (cueFile *)calloc(1,sizeof(cueFile));
  cue->cd_format = CUE_MODE_UNKNOWN;

  while ((filestream_gets(cue_file, line, STRING_MAX)))
    {
      char *track_ptr;
      char *index_ptr;

      strcpy(line_upper,line);
      str_to_upper(line_upper);

      if(strstr(line_upper, "FILE "))
        {
          char *cd_image = extract_file_name(path, line);
          if(cd_image)
            {
              if(cue->num_files < CUE_MAX_FILES)
                {
                  cue->files[cue->num_files].path = cd_image;
                  current_file = cue->num_files;
                  if(cue->cd_image == NULL)
                    cue->cd_image = cd_image;
                  cue->num_files++;
                }
              else
                free(cd_image);
            }
        }

      track_ptr = strstr(line_upper, "TRACK ");
      if(track_ptr)
        {
          uint32_t track_num;
          char *mode_ptr;

          track_num = (uint32_t)strtoul(track_ptr + 6, NULL, 10);
          if(track_num > 0 && track_num <= CUE_MAX_TRACKS && cue->num_tracks < CUE_MAX_TRACKS)
            {
              current_track = cue->num_tracks;
              memset(&cue->tracks[current_track],0,sizeof(cue->tracks[current_track]));
              cue->tracks[current_track].track_num = (uint8_t)track_num;
              cue->tracks[current_track].file_index = (current_file >= 0) ?
                (uint8_t)current_file : 0;
              cue->tracks[current_track].format = CUE_MODE_UNKNOWN;

              mode_ptr = track_ptr + 8;
              while(*mode_ptr && isspace((unsigned char)*mode_ptr))
                mode_ptr++;

              if(strstr(mode_ptr,"MODE1/2048"))
                cue->tracks[current_track].format = MODE1_2048;
              else if(strstr(mode_ptr,"MODE1/2352"))
                cue->tracks[current_track].format = MODE1_2352;
              else if(strstr(mode_ptr,"MODE2/2352"))
                cue->tracks[current_track].format = MODE2_2352;
              else if(strstr(mode_ptr,"AUDIO"))
                cue->tracks[current_track].format = CUE_MODE_AUDIO;
            }
        }

      index_ptr = strstr(line_upper, "INDEX 01");
      if(index_ptr && current_track == cue->num_tracks && cue->num_tracks < CUE_MAX_TRACKS)
        {
          char *msf_ptr = index_ptr + 8;
          while(*msf_ptr && !isdigit((unsigned char)*msf_ptr))
            msf_ptr++;
          cue->tracks[cue->num_tracks].index_lba = cue_msf_to_lba(msf_ptr);
          cue->num_tracks++;
          current_track = -1;
        }
    }
  filestream_close(cue_file);

  if(cue->num_tracks > 0 && cue->cd_format == CUE_MODE_UNKNOWN)
    cue->cd_format = cue->tracks[0].format;

  if(retro_log_printf_cb)
    {
      retro_log_printf_cb(RETRO_LOG_INFO, "[Opera]: CD image files in CUE: %d first=%s\n",
                          cue->num_files,
                          cue->cd_image ? cue->cd_image : "Not found");
    }

  if((cue->cd_format != CUE_MODE_UNKNOWN || cue->num_tracks > 0) &&
     cue->num_files > 0)
    return cue;

  cue_free(cue);
  return NULL;
}

void
cue_free(cueFile *cue)
{
  int i;

  if(!cue)
    return;

  for(i = 0; i < cue->num_files; i++)
    free(cue->files[i].path);

  free(cue);
}

const
char*
cue_get_cd_format_name(CD_format cd_format)
{
  switch (cd_format)
    {
    case MODE1_2048:
      return "MODE1/2048";
    case MODE1_2352:
      return "MODE1/2352";
    case MODE2_2352:
      return "MODE2/2352";
    case CUE_MODE_AUDIO:
      return "AUDIO";
    default:
      break;
    }

  return "UNKNOWN";
}

int
cue_is_cue_path(const char *path)
{
  const char *dot = strrchr(path, '.');
  return (dot && (!strcmp(dot, ".cue") ||
                  !strcmp(dot, ".CUE")));
}
