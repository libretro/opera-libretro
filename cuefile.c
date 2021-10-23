#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <libretro.h>
#include <streams/file_stream.h>

#include "cuefile.h"
#include "opera_lr_callbacks.h"

#define STRING_MAX 4096

static RFILE *cue_get_file_for_image(const char *path)
{
   int i;
   char cue_path_base[STRING_MAX];
   char cue_path[8192];
   char *exts[]   = {".cue", ".CUE"};
   char *last_dot = NULL;

   strcpy(cue_path_base, path);

   last_dot = strrchr(cue_path_base, '.');
   if (last_dot == NULL)
      return NULL;

   /* cut original extension */
   *(last_dot) = '\0';

   for(i=0; i<2; i++)
   {
      RFILE *cue_file = NULL;

      strcpy(cue_path, cue_path_base);
      strcat(cue_path, exts[i]);

      cue_file = filestream_open(cue_path, RETRO_VFS_FILE_ACCESS_READ, 0);
      if (cue_file)
         return cue_file;
   }

   return NULL;
}

static void str_to_upper(char *s)
{
   for ( ; *s; ++s) *s = toupper(*s);
}

static char *extract_file_name(const char *path, char *line)
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
   if (!file_name_start)
   {
      if (retro_log_printf_cb)
         retro_log_printf_cb(RETRO_LOG_INFO, "[Opera]: Missing quotes in : %s\n", line);
      return NULL;
   }

   strcpy(file, ++file_name_start);
   file_name_end = strstr(file, "\"");

   if (!file_name_end)
   {
      if (retro_log_printf_cb)
         retro_log_printf_cb(RETRO_LOG_INFO, "[Opera]: Missing end quote in : %s\n", line);
      return NULL;
   }

   *file_name_end = '\0';

   strcpy(base_path, path);

   last_separator  = strrchr(base_path, slash);
   *last_separator = '\0';

   sprintf(cd_image, "%s%c%s", base_path, slash, file);

   return strdup(cd_image);
}

cueFile *cue_get(const char *path)
{
   char line[STRING_MAX];
   int files_found = 0;
   cueFile *cue   = NULL;
   RFILE *cue_file =
      cue_is_cue_path(path) ?
      filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0) :
      cue_get_file_for_image(path);

   if (!cue_file)
      return NULL;

   cue = (cueFile *)malloc(sizeof(cueFile));
   cue->cd_format = CUE_MODE_UNKNOWN;

   while ((filestream_gets(cue_file, line, STRING_MAX)))
   {
      if (strstr(line, "FILE") && files_found == 0)
      {
         char *cd_image = extract_file_name(path, line);
         if (cd_image)
         {
            files_found++;
            cue->cd_image = cd_image;
         }
      }

      str_to_upper(line);
      if (strstr(line, "TRACK 01"))
      {
         if (strstr(line,"TRACK 01 MODE1/2048"))
            cue->cd_format = MODE1_2048;
         else if (strstr(line, "TRACK 01 MODE1/2352"))
            cue->cd_format = MODE1_2352;
         else if (strstr(line, "TRACK 01 MODE2/2352"))
            cue->cd_format = MODE2_2352;
         else
         {
            if (retro_log_printf_cb)
              retro_log_printf_cb(RETRO_LOG_INFO, "[Opera]: Unknown file format in CUE file: %s -> %s", path, line);
         }
         break;
      }
   }
   filestream_close(cue_file);

   if (retro_log_printf_cb)
   {
      retro_log_printf_cb(RETRO_LOG_INFO, "[Opera]: CD image file in CUE: %s",
            cue->cd_image ? cue->cd_image : "Not found");
   }

   if (cue->cd_format != CUE_MODE_UNKNOWN)
      return cue;

   free(cue);
   return NULL;
}

const char *cue_get_cd_format_name(CD_format cd_format)
{
   switch (cd_format)
   {
      case MODE1_2048:
         return "MODE1/2048";
      case MODE1_2352:
         return "MODE1/2352";
      case MODE2_2352:
         return "MODE2/2352";
      default:
         break;
   }

   return "UNKNOWN";
}

int cue_is_cue_path(const char *path)
{
   char *dot = strrchr(path, '.');
   return (dot && (!strcmp(dot, ".cue") || !strcmp(dot, ".CUE")));
}
