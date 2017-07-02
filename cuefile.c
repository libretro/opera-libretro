#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "libretro.h"
#include "cuefile.h"

#define STRING_MAX 4096

retro_log_printf_t cue_log_cb;

static FILE *cue_get_file_for_image(const char *path)
{
   int i;
   char cue_path_base[STRING_MAX];
   char cue_path[8192];
   char *exts[]   = {".cue", ".CUE"};
   char *last_dot = NULL;

   strncpy(cue_path_base, path, STRING_MAX);

   last_dot = strrchr(cue_path_base, '.');
   if (last_dot == NULL)
      return NULL;

   // cut original extension
   *(last_dot) = '\0';

   for(i=0; i<2; i++)
   {
      FILE *cue_file = NULL;

      strcpy(cue_path, cue_path_base);
      strcat(cue_path, exts[i]);

      cue_file = fopen(cue_path, "r");
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
   char *fule_name_end   = NULL;
   char *file_name_start = strstr(line, "\"");
   if (!file_name_start)
   {
      if (cue_log_cb)
         cue_log_cb(RETRO_LOG_INFO, "[4DO]: Missing quotes in : %s", line);
      return NULL;
   }

   strncpy(file, ++file_name_start, STRING_MAX);
   file_name_end = strstr(file, "\"");

   if (!file_name_end)
   {
      if (cue_log_cb)
         cue_log_cb(RETRO_LOG_INFO, "[4DO]: Missing end quote in : %s", line);
      return NULL;
   }

   *file_name_end = '\0';

   strncpy(base_path, path, STRING_MAX);

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
   FILE *cue_file =
      cue_is_cue_path(path) ?
      fopen(path, "r") :
      cue_get_file_for_image(path);

   if (!cue_file)
      return NULL;

   cue = (cueFile *)malloc(sizeof(cueFile));
   cue->cd_format = CUE_MODE_UNKNOWN;

   while ((fgets(line, STRING_MAX, cue_file)))
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
            if (cue_log_cb)
               cue_log_cb(RETRO_LOG_INFO, "[4DO]: Unknown file format in CUE file: %s -> %s", line);
         }
         break;
      }
   }
   fclose(cue_file);

   if (cue_log_cb)
   {
      cue_log_cb(RETRO_LOG_INFO, "[4DO]: CD image file in CUE: %s",
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

