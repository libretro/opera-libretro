#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_endianness.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

#include "libopera/opera_arm.h"
#include "libopera/opera_nvram.h"

#include "opera_lr_callbacks.h"
#include "opera_lr_nvram.h"
#include "opera_lr_opts.h"

static const char OLD_NVRAM_FILENAME[] = "3DO.nvram";

static
int
nvram_save(const void   *nvram_buf_,
           const size_t  nvram_size_,
           const char   *filepath_)
{
  int rv;
  char tmppath[PATH_MAX_LENGTH];

  fill_pathname_basedir(tmppath,filepath_,sizeof(tmppath));
  path_mkdir(tmppath);

  strlcpy(tmppath,filepath_,sizeof(tmppath));
  strlcat(tmppath,".tmp",sizeof(tmppath));

  rv = filestream_write_file(tmppath,nvram_buf_,nvram_size_);
  if(rv == 0)
    return -1;

  rv = filestream_rename(tmppath,filepath_);
  if(rv != 0)
    {
      filestream_delete(filepath_);
      rv = filestream_rename(tmppath,filepath_);
    }

  return ((rv == 0) ? 0 : -1);
}

static
int
nvram_load(void         *nvram_buf_,
           const size_t  nvram_size_,
           const char   *filepath_)
{
  RFILE *f;
  int64_t rv;

  f = filestream_open(filepath_,
                      RETRO_VFS_FILE_ACCESS_READ,
                      RETRO_VFS_FILE_ACCESS_HINT_NONE);
  if(f == NULL)
    return -1;

  rv = filestream_read(f,nvram_buf_,nvram_size_);

  filestream_close(f);

  return ((rv == nvram_size_) ? 0 : -1);
}

static
int
get_save_path(char path_[PATH_MAX_LENGTH])
{
  int rv;
  const char *basepath;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY,&basepath);
  if((rv == 0) || (basepath == NULL))
    return -1;

  strlcpy(path_,basepath,PATH_MAX_LENGTH);

  return 0;
}

static
int
get_system_path(char path_[PATH_MAX_LENGTH])
{
  int rv;
  const char *basepath;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&basepath);
  if((rv == 0) || (basepath == NULL))
    return -1;

  strlcpy(path_,basepath,PATH_MAX_LENGTH);

  return 0;
}

static
int
get_save_or_system_path(char path_[PATH_MAX_LENGTH])
{
  int rv;

  rv = get_save_path(path_);
  if(rv < 0)
    rv = get_system_path(path_);

  return rv;
}

static
int
get_opera_path(char path_[PATH_MAX_LENGTH])
{
  int rv;

  rv = get_save_or_system_path(path_);
  if(rv < 0)
    return -1;

  fill_pathname_join(path_,path_,"opera",PATH_MAX_LENGTH);

  return 0;
}

static
int
opera_lr_nvram_save_pergame(const uint8_t *nvram_buf_,
                            const size_t   nvram_size_,
                            const char    *name_,
                            const uint8_t  version_)
{
  int rv;
  char filename[PATH_MAX_LENGTH];
  char filepath[PATH_MAX_LENGTH];

  rv = get_opera_path(filepath);
  if(rv < 0)
    return -1;

  fill_pathname_join(filepath,filepath,"per_game",sizeof(filepath));
  snprintf(filename,PATH_MAX_LENGTH,"%s.%d.srm",name_,version_);
  fill_pathname_join(filepath,filepath,filename,sizeof(filepath));

  rv = nvram_save(nvram_buf_,nvram_size_,filepath);

  return rv;
}

static
int
opera_lr_nvram_save_shared(const uint8_t *nvram_buf_,
                           const size_t   nvram_size_,
                           const uint8_t  version_)
{
  int rv;
  char filename[PATH_MAX_LENGTH];
  char filepath[PATH_MAX_LENGTH];

  rv = get_opera_path(filepath);
  if(rv < 0)
    return -1;

  fill_pathname_join(filepath,filepath,"shared",sizeof(filepath));
  snprintf(filename,PATH_MAX_LENGTH,"nvram.%d.srm",version_);
  fill_pathname_join(filepath,filepath,filename,sizeof(filepath));

  rv = nvram_save(nvram_buf_,nvram_size_,filepath);

  return rv;
}

static
int
opera_lr_nvram_load_pergame_savedir(uint8_t      *nvram_buf_,
                                    const size_t  nvram_size_,
                                    const char   *name_)
{
  int rv;
  char filepath[PATH_MAX_LENGTH];

  rv = get_save_path(filepath);
  if(rv < 0)
    return -1;

  fill_pathname_join(filepath,filepath,name_,sizeof(filepath));
  strlcat(filepath,".srm",sizeof(filepath));

  rv = nvram_load(nvram_buf_,nvram_size_,filepath);

  return rv;
}

static
int
opera_lr_nvram_load_pergame_systemdir(uint8_t      *nvram_buf_,
                                      const size_t  nvram_size_,
                                      const char   *name_)
{
  int rv;
  char filepath[PATH_MAX_LENGTH];

  rv = get_system_path(filepath);
  if(rv < 0)
    return -1;

  fill_pathname_join(filepath,filepath,name_,sizeof(filepath));
  strlcat(filepath,".srm",sizeof(filepath));

  rv = nvram_load(nvram_buf_,nvram_size_,filepath);

  return rv;
}

static
int
opera_lr_nvram_load_pergame_operadir(uint8_t       *nvram_buf_,
                                     const size_t   nvram_size_,
                                     const char    *name_,
                                     const uint8_t  version_)
{
  int rv;
  char filename[PATH_MAX_LENGTH];
  char filepath[PATH_MAX_LENGTH];

  rv = get_opera_path(filepath);
  if(rv < 0)
    return -1;

  snprintf(filename,sizeof(filename),"%s.%d.srm",name_,version_);
  fill_pathname_join(filepath,filepath,"per_game",sizeof(filepath));
  fill_pathname_join(filepath,filepath,filename,sizeof(filepath));

  rv = nvram_load(nvram_buf_,nvram_size_,filepath);

  return rv;
}

static
int
opera_lr_nvram_load_pergame(uint8_t       *nvram_buf_,
                            const size_t   nvram_size_,
                            const char    *name_,
                            const uint8_t  version_)
{
  int rv;

  rv = opera_lr_nvram_load_pergame_operadir(nvram_buf_,nvram_size_,name_,version_);
  if(rv == 0)
    return 0;

  rv = opera_lr_nvram_load_pergame_savedir(nvram_buf_,nvram_size_,name_);
  if(rv == 0)
    return 0;

  rv = opera_lr_nvram_load_pergame_systemdir(nvram_buf_,nvram_size_,name_);
  if(rv == 0)
    return 0;

  return -1;
}

static
int
opera_lr_nvram_load_shared_savedir(uint8_t      *nvram_buf_,
                                   const size_t  nvram_size_)
{
  int rv;
  char filepath[PATH_MAX_LENGTH];

  rv = get_save_path(filepath);
  if(rv < 0)
    return -1;

  fill_pathname_join(filepath,filepath,OLD_NVRAM_FILENAME,sizeof(filepath));

  rv = nvram_load(nvram_buf_,nvram_size_,filepath);

  return rv;
}

static
int
opera_lr_nvram_load_shared_systemdir(uint8_t      *nvram_buf_,
                                     const size_t  nvram_size_)
{
  int rv;
  char filepath[PATH_MAX_LENGTH];

  rv = get_system_path(filepath);
  if(rv < 0)
    return -1;

  fill_pathname_join(filepath,filepath,OLD_NVRAM_FILENAME,sizeof(filepath));

  rv = nvram_load(nvram_buf_,nvram_size_,filepath);

  return rv;
}

static
int
opera_lr_nvram_load_shared_operadir(uint8_t       *nvram_buf_,
                                    const size_t   nvram_size_,
                                    const uint8_t  version_)
{
  int rv;
  char filename[PATH_MAX_LENGTH];
  char filepath[PATH_MAX_LENGTH];

  rv = get_opera_path(filepath);
  if(rv < 0)
    return -1;

  fill_pathname_join(filepath,filepath,"shared",sizeof(filepath));
  snprintf(filename,PATH_MAX_LENGTH,"nvram.%d.srm",version_);
  fill_pathname_join(filepath,filepath,filename,sizeof(filepath));

  rv = nvram_load(nvram_buf_,nvram_size_,filepath);

  return rv;
}

static
int
opera_lr_nvram_load_shared(uint8_t       *nvram_buf_,
                           const size_t   nvram_size_,
                           const uint8_t  version_)
{
  int rv;

  rv = opera_lr_nvram_load_shared_operadir(nvram_buf_,nvram_size_,version_);
  if(rv == 0)
    return 0;

  rv = opera_lr_nvram_load_shared_savedir(nvram_buf_,nvram_size_);
  if(rv == 0)
    return 0;

  rv = opera_lr_nvram_load_shared_systemdir(nvram_buf_,nvram_size_);
  if(rv == 0)
    return 0;

  return -1;
}

/*
  PUBLIC FUNCTIONS
*/

void
opera_lr_nvram_save(const char *gamepath_)
{
  uint8_t version;
  size_t nvram_size;
  const uint8_t *nvram_buf;

  nvram_buf  = opera_arm_nvram_get();
  nvram_size = opera_arm_nvram_size();
  version    = opera_lr_opts_nvram_version();
  if(opera_lr_opts_is_nvram_shared())
    {
      opera_lr_nvram_save_shared(nvram_buf,nvram_size,version);
    }
  else
    {
      char filename[256];

      fill_pathname_base_noext(filename,gamepath_,sizeof(filename));

      opera_lr_nvram_save_pergame(nvram_buf,nvram_size,filename,version);
    }
}

void
opera_lr_nvram_load(const char *gamepath_)
{
  uint8_t   version;
  uint8_t  *nvram_buf;
  uint32_t  nvram_size;

  nvram_buf  = opera_arm_nvram_get();
  nvram_size = opera_arm_nvram_size();
  version    = opera_lr_opts_nvram_version();
  if(opera_lr_opts_is_nvram_shared())
    {
      opera_lr_nvram_load_shared(nvram_buf,nvram_size,version);
    }
  else
    {
      char filename[256];

      fill_pathname_base_noext(filename,gamepath_,sizeof(filename));

      opera_lr_nvram_load_pergame(nvram_buf,nvram_size,filename,version);
    }
}
