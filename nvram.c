#include "nvram.h"
#include "retro_callbacks.h"

#include <file/file_path.h>
#include <retro_endianness.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

static char NVRAM_FILENAME[] = "3DO.nvram";

/*
  At some point it'd be good to put the init code into a separate
  module as part of the core emulator.
 */
void
nvram_init(void *nvram_)
{
  struct NVRAM_Header *nvram_hdr = (struct NVRAM_Header*)nvram_;

  memset(nvram_,0,sizeof(struct NVRAM_Header));

  nvram_hdr->record_type         = 0x01;
  nvram_hdr->sync_bytes[0]       = 'Z';
  nvram_hdr->sync_bytes[1]       = 'Z';
  nvram_hdr->sync_bytes[2]       = 'Z';
  nvram_hdr->sync_bytes[3]       = 'Z';
  nvram_hdr->sync_bytes[4]       = 'Z';
  nvram_hdr->record_version      = 0x02;
  nvram_hdr->label[0]            = 'N';
  nvram_hdr->label[1]            = 'V';
  nvram_hdr->label[2]            = 'R';
  nvram_hdr->label[3]            = 'A';
  nvram_hdr->label[4]            = 'M';
  nvram_hdr->id                  = swap_if_little32(0xFFFFFFFF);
  nvram_hdr->block_size          = swap_if_little32(0x00000001);
  nvram_hdr->block_count         = swap_if_little32(0x00008000);
  nvram_hdr->root_dir_id         = swap_if_little32(0xFFFFFFFE);
  nvram_hdr->root_dir_blocks     = swap_if_little32(0x00000000);
  nvram_hdr->root_dir_block_size = swap_if_little32(0x00000001);
  nvram_hdr->last_root_dir_copy  = swap_if_little32(0x00000000);
  nvram_hdr->root_dir_copies[0]  = swap_if_little32(0x00000084);
  nvram_hdr->unknown_value0      = swap_if_little32(0x855A02B6);
  nvram_hdr->unknown_value1      = swap_if_little32(0x00000098);
  nvram_hdr->unknown_value2      = swap_if_little32(0x00000098);
  nvram_hdr->unknown_value3      = swap_if_little32(0x00000014);
  nvram_hdr->unknown_value4      = swap_if_little32(0x00000014);
  nvram_hdr->unknown_value5      = swap_if_little32(0x7AA565BD);
  nvram_hdr->unknown_value6      = swap_if_little32(0x00000084);
  nvram_hdr->unknown_value7      = swap_if_little32(0x00000084);
  nvram_hdr->blocks_remaining    = swap_if_little32(0x00007F68);
  nvram_hdr->unknown_value8      = swap_if_little32(0x00000014);
}

int
nvram_save(const void   *nvram_,
           const size_t  size_,
           const char   *basepath_,
           const char   *filename_)
{
  int rv;
  char fullpath[PATH_MAX_LENGTH];
  char fullpath_tmp[PATH_MAX_LENGTH];

  fill_pathname_join(fullpath,basepath_,filename_,sizeof(fullpath));
  strncpy(fullpath_tmp,fullpath,sizeof(fullpath_tmp));
  strncat(fullpath_tmp,".tmp",sizeof(fullpath_tmp) - strlen(fullpath_tmp) - 1);

  rv = filestream_write_file(fullpath_tmp,nvram_,size_);
  if(rv == 0)
    return -1;

  return filestream_rename(fullpath_tmp,fullpath);
}

int
nvram_load(void         *nvram_,
           const size_t  size_,
           const char   *basepath_,
           const char   *filename_)
{
  RFILE *f;
  int64_t rv;
  char fullpath[PATH_MAX_LENGTH];

  fill_pathname_join(fullpath,basepath_,filename_,sizeof(fullpath));

  f = filestream_open(fullpath,
                      RETRO_VFS_FILE_ACCESS_READ,
                      RETRO_VFS_FILE_ACCESS_HINT_NONE);
  if(f == NULL)
    return -1;

  rv = filestream_read(f,nvram_,size_);

  filestream_close(f);

  return ((rv == size_) ? 0 : -1);
}

/*
  Ideally there would be a core specific directory available
  regardless content is loaded but for now we'll save to the system
  directory.
 */
void
retro_nvram_save(const uint8_t *nvram_)
{
  int rv;
  const char *basepath;
  struct retro_variable var;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&basepath);
  if((rv == 0) || (basepath == NULL))
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[4DO]: unable to save %s - system directory unavailable",
                          NVRAM_FILENAME);
      return;
    }

  rv = nvram_save(nvram_,NVRAM_SIZE,basepath,NVRAM_FILENAME);
  if(rv)
    retro_log_printf_cb(RETRO_LOG_ERROR,
                        "[4DO]: unknown error saving %s\n",
                        NVRAM_FILENAME);
}

void
retro_nvram_load(uint8_t *nvram_)
{
  int rv;
  const char *basepath;
  struct retro_variable var;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&basepath);
  if((rv == 0) || (basepath == NULL))
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[4DO]: unable to load %s - system directory unavailable",
                          NVRAM_FILENAME);
      return;
    }

  rv = nvram_load(nvram_,NVRAM_SIZE,basepath,NVRAM_FILENAME);
  if(rv)
    retro_log_printf_cb(RETRO_LOG_ERROR,
                        "[4DO]: unknown error loading %s\n",
                        NVRAM_FILENAME);
}
