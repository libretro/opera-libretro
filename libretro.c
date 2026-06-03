#include "file/file_path.h"
#include "libretro.h"
#include "libretro_core_options.h"
#include "retro_miscellaneous.h"
#include "streams/file_stream.h"
#include "compat/posix_string.h"
#include "compat/strl.h"
#include "string/stdstring.h"

#include "lr_input.h"
#include "lr_input_crosshair.h"
#include "lr_input_descs.h"
#include "opera_lr_callbacks.h"
#include "opera_lr_dsp.h"
#include "opera_lr_nvram.h"
#include "opera_lr_opts.h"
#include "retro_cdimage.h"

#include "libopera/hack_flags.h"
#include "libopera/opera_3do.h"
#include "libopera/opera_arm.h"
#include "libopera/opera_bios.h"
#include "libopera/opera_cdrom.h"
#include "libopera/opera_clock.h"
#include "libopera/opera_core.h"
#include "libopera/opera_log.h"
#include "libopera/opera_madam.h"
#include "libopera/opera_mem.h"
#include "libopera/opera_nvram.h"
#include "libopera/opera_pbus.h"
#include "libopera/opera_region.h"
#include "libopera/opera_vdlp.h"
#include "libopera/opera_xbus_cdrom_plugin.h"
#include "libopera/prng16.h"
#include "libopera/prng32.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CDIMAGE_SECTOR_SIZE 2048

typedef enum retro_reset_flags_t
  {
    RETRO_RESET_FLAG_NONE       = 0,
    RETRO_RESET_FLAG_SAVE_NVRAM = (1 << 0)
  } retro_reset_flags_t;

typedef struct disk_image_s
{
  char *path;
  char *label;
} disk_image_t;

static cdimage_t  CDIMAGE;
static uint32_t   CDIMAGE_SECTOR;
static char      *g_GAME_INFO_PATH = NULL;
static disk_image_t *g_DISK_IMAGES = NULL;
static unsigned      g_DISK_IMAGE_CAPACITY = 0;
static unsigned      g_DISK_IMAGE_COUNT = 0;
static unsigned      g_DISK_IMAGE_INDEX = 0;
static unsigned      g_DISK_INITIAL_INDEX = 0;
static char         *g_DISK_INITIAL_PATH = NULL;

static
void
retro_reset_core(retro_reset_flags_t flags_);

static
bool
ode_reset_if_requested(void);

static
void
retro_environment_set_support_no_game(void)
{
  bool support_no_game = true;

  retro_environment_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME,&support_no_game);
}

static
void
retro_environment_set_controller_info(void)
{
  static const struct retro_controller_description port[] =
    {
     { "3DO Joypad",        RETRO_DEVICE_JOYPAD },
     { "3DO Flightstick",   RETRO_DEVICE_FLIGHTSTICK },
     { "3DO Mouse",         RETRO_DEVICE_MOUSE  },
     { "3DO Lightgun",      RETRO_DEVICE_LIGHTGUN },
     { "Arcade Lightgun",   RETRO_DEVICE_ARCADE_LIGHTGUN },
     { "Orbatak Trackball", RETRO_DEVICE_ORBATAK_TRACKBALL },
    };

  static const struct retro_controller_info ports[LR_INPUT_MAX_DEVICES+1] =
    {
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {NULL, 0}
    };

  retro_environment_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,(void*)ports);
}

static
void
retro_vfs_initialize(void)
{
  struct retro_vfs_interface_info vfs_info;

  vfs_info.required_interface_version = 1;
  vfs_info.iface                      = NULL;

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE,&vfs_info))
    filestream_vfs_init(&vfs_info);
}

void
retro_set_environment(retro_environment_t cb_)
{
  opera_lr_callbacks_set_environment(cb_);

  retro_vfs_initialize();
  retro_environment_set_controller_info();
  libretro_init_core_options();
  libretro_set_core_options();
  retro_environment_set_support_no_game();
}

void
retro_set_video_refresh(retro_video_refresh_t cb_)
{
  opera_lr_callbacks_set_video_refresh(cb_);
}

void
retro_set_audio_sample(retro_audio_sample_t cb_)
{
  opera_lr_callbacks_set_audio_sample(cb_);
}

void
retro_set_audio_sample_batch(retro_audio_sample_batch_t cb_)
{
  opera_lr_callbacks_set_audio_sample_batch(cb_);
}

void
retro_set_input_poll(retro_input_poll_t cb_)
{
  opera_lr_callbacks_set_input_poll(cb_);
}

void
retro_set_input_state(retro_input_state_t cb_)
{
  opera_lr_callbacks_set_input_state(cb_);
}

static
uint32_t
cdimage_get_size(void)
{
  ssize_t blocks;

  blocks = retro_cdimage_get_number_of_logical_blocks(&CDIMAGE);
  if(blocks <= 0)
    return 0;
  if(blocks > UINT32_MAX)
    return UINT32_MAX;

  return (uint32_t)blocks;
}

static
void
cdimage_set_sector(const uint32_t sector_)
{
  CDIMAGE_SECTOR = sector_;
}

static
void
cdimage_read_sector(void  *buf_,
                    size_t len_)
{
  retro_cdimage_read_sector(&CDIMAGE,CDIMAGE_SECTOR,buf_,len_);
}

static
void
cdimage_get_toc(uint8_t  *track_first_,
                uint8_t  *track_last_,
                uint8_t  *disc_id_,
                void     *disc_toc_,
                uint32_t  disc_toc_size_)
{
  retro_cdimage_get_toc(&CDIMAGE,
                        track_first_,
                        track_last_,
                        disc_id_,
                        disc_toc_,
                        disc_toc_size_);
}

static
void
content_runtime_reset(void)
{
  cdimage_set_sector(0);
  opera_cdrom_ode_set_root(NULL);
}

static
void*
libopera_callback(int   cmd_,
                  void *data_)
{
  switch(cmd_)
    {
    case EXT_DSP_TRIGGER:
      opera_lr_dsp_process();
      break;
    default:
      break;
    }

  return NULL;
}

#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
void
retro_get_system_info(struct retro_system_info *info_)
{
  memset(info_,0,sizeof(*info_));

  info_->library_name     = "Opera";
  info_->library_version  = "1.0.0" GIT_VERSION;
  info_->need_fullpath    = true;
  info_->valid_extensions = "iso|bin|chd|cue|m3u";
}

size_t
retro_serialize_size(void)
{
  return opera_3do_state_size();
}

bool
retro_serialize(void   *data_,
                size_t  size_)
{
  uint32_t size;

  if(size_ == 0)
    return false;

  size = opera_3do_state_save(data_,size_);

  return (size == size_);
}

bool
retro_unserialize(void const *data_,
                  size_t      size_)
{
  uint32_t size;
  uint32_t backup_size;
  uint32_t restore_size;
  void *backup_state;

  backup_state = malloc(retro_serialize_size());
  if(backup_state == NULL)
    return false;
  backup_size = retro_serialize_size();
  size = retro_serialize(backup_state,backup_size);
  if(size)
    {
      size = opera_3do_state_load(data_,size_);
      if(size != size_)
        {
          restore_size = opera_3do_state_load(backup_state,backup_size);
          if(restore_size != backup_size)
            opera_log_printf(OPERA_LOG_ERROR,
                             "[Opera]: failed to restore previous state after unsuccessful state load\n");
          size = 0;
        }
    }

  free(backup_state);

  return (size == size_);
}

void
retro_cheat_reset(void)
{
}

void
retro_cheat_set(unsigned    index_,
                bool        enabled_,
                const char *code_)
{
}

void
retro_set_controller_port_device(unsigned port_,
                                 unsigned device_)
{
  lr_input_device_set_with_descs(port_,device_);
}

static
enum retro_pixel_format
vdlp_pixel_format_to_libretro(vdlp_pixel_format_e pf_)
{
  switch (pf_)
    {
    case VDLP_PIXEL_FORMAT_0RGB1555:
      return RETRO_PIXEL_FORMAT_0RGB1555;
    case VDLP_PIXEL_FORMAT_RGB565:
      return RETRO_PIXEL_FORMAT_RGB565;
    case VDLP_PIXEL_FORMAT_XRGB8888:
      return RETRO_PIXEL_FORMAT_XRGB8888;
    }

  return RETRO_PIXEL_FORMAT_XRGB8888;
}

static
int
set_pixel_format(void)
{
  int rv;
  enum retro_pixel_format fmt;

  fmt = vdlp_pixel_format_to_libretro(g_OPTS.vdlp_pixel_format);
  rv  = retro_environment_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&fmt);
  if(rv == 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: pixel format is not supported.\n");
      return -1;
    }

  return 0;
}

static
int
print_cdimage_open_fail(const char *path_)
{
  retro_log_printf_cb(RETRO_LOG_ERROR,
                      "[Opera]: failure opening image - %s\n",
                      path_);
  return -1;
}

static
int
open_cdimage(const char *path_)
{
  int rv;

  if(path_ == NULL)
    return 0;

  rv = retro_cdimage_open(path_,&CDIMAGE);
  if(rv == -1)
    return print_cdimage_open_fail(path_);

  return 0;
}

static
void
game_info_path_free(void)
{
  if(g_GAME_INFO_PATH == NULL)
    return;

  free(g_GAME_INFO_PATH);
  g_GAME_INFO_PATH = NULL;
}

static
void
game_info_path_save(const struct retro_game_info *info_)
{
  game_info_path_free();

  if((info_ == NULL) || (info_->path == NULL))
    return;

  g_GAME_INFO_PATH = strdup(info_->path);
}

static
void
game_info_path_save_path(const char *path_)
{
  game_info_path_free();

  if(path_ == NULL)
    return;

  g_GAME_INFO_PATH = strdup(path_);
}

static
const
char*
game_info_path_get(void)
{
  return g_GAME_INFO_PATH;
}

static
void
disk_image_free(disk_image_t *image_)
{
  if(image_ == NULL)
    return;

  free(image_->path);
  free(image_->label);
  image_->path  = NULL;
  image_->label = NULL;
}

static
void
disk_images_clear(void)
{
  unsigned i;

  for(i = 0; i < g_DISK_IMAGE_COUNT; i++)
    disk_image_free(&g_DISK_IMAGES[i]);

  free(g_DISK_IMAGES);
  g_DISK_IMAGES         = NULL;
  g_DISK_IMAGE_CAPACITY = 0;
  g_DISK_IMAGE_COUNT    = 0;
  g_DISK_IMAGE_INDEX    = 0;
}

static
void
disk_initial_image_clear(void)
{
  free(g_DISK_INITIAL_PATH);
  g_DISK_INITIAL_PATH  = NULL;
  g_DISK_INITIAL_INDEX = 0;
}

static
bool
disk_images_ensure_capacity(unsigned capacity_)
{
  disk_image_t *images;
  unsigned old_capacity;
  unsigned new_capacity;

  if(capacity_ <= g_DISK_IMAGE_CAPACITY)
    return true;

  old_capacity = g_DISK_IMAGE_CAPACITY;
  new_capacity = (g_DISK_IMAGE_CAPACITY > 0) ? (g_DISK_IMAGE_CAPACITY * 2) : 4;
  while(new_capacity < capacity_)
    new_capacity *= 2;

  images = (disk_image_t*)realloc(g_DISK_IMAGES,new_capacity * sizeof(*g_DISK_IMAGES));
  if(images == NULL)
    return false;

  g_DISK_IMAGES = images;
  memset(&g_DISK_IMAGES[old_capacity],0,(new_capacity - old_capacity) * sizeof(*g_DISK_IMAGES));
  g_DISK_IMAGE_CAPACITY = new_capacity;

  return true;
}

static
bool
disk_image_extension_supported(const char *path_)
{
  const char *ext;

  if(string_is_empty(path_))
    return false;

  ext = path_get_extension(path_);
  return (!strcasecmp(ext,"iso") ||
          !strcasecmp(ext,"bin") ||
          !strcasecmp(ext,"chd") ||
          !strcasecmp(ext,"cue"));
}

static
bool
disk_image_normalize_path(char       *path_out_,
                          const char *path_,
                          size_t      path_out_size_)
{
  if((path_out_ == NULL) || (path_out_size_ == 0))
    return false;

  path_out_[0] = 0;
  if(string_is_empty(path_))
    return false;

  if(strlcpy(path_out_,path_,path_out_size_) >= path_out_size_)
    return false;
  path_resolve_realpath(path_out_,path_out_size_);

  return (path_out_[0] != 0);
}

static
char*
disk_image_label_alloc(const char *path_)
{
  const char *base;
  char label[PATH_MAX_LENGTH];

  base = path_basename(path_);
  if(string_is_empty(base))
    base = path_;

  fill_pathname(label,base,"",sizeof(label));
  if(string_is_empty(label))
    strlcpy(label,base,sizeof(label));

  return strdup(label);
}

static
bool
disk_images_set_path(unsigned    index_,
                     const char *path_)
{
  char path[PATH_MAX_LENGTH];
  char *path_dup;
  char *label_dup;

  if(index_ >= g_DISK_IMAGE_COUNT)
    return false;
  if(!disk_image_normalize_path(path,path_,sizeof(path)))
    return false;
  if(!disk_image_extension_supported(path))
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: unsupported disk image - %s\n",
                          path);
      return false;
    }

  path_dup = strdup(path);
  label_dup = disk_image_label_alloc(path);
  if((path_dup == NULL) || (label_dup == NULL))
    {
      free(path_dup);
      free(label_dup);
      return false;
    }

  disk_image_free(&g_DISK_IMAGES[index_]);
  g_DISK_IMAGES[index_].path  = path_dup;
  g_DISK_IMAGES[index_].label = label_dup;

  return true;
}

static
bool
disk_images_append_empty(void)
{
  if(!disk_images_ensure_capacity(g_DISK_IMAGE_COUNT + 1))
    return false;

  g_DISK_IMAGES[g_DISK_IMAGE_COUNT].path  = NULL;
  g_DISK_IMAGES[g_DISK_IMAGE_COUNT].label = NULL;
  g_DISK_IMAGE_COUNT++;

  return true;
}

static
bool
disk_images_append_path(const char *path_)
{
  if(!disk_images_append_empty())
    return false;

  if(!disk_images_set_path(g_DISK_IMAGE_COUNT - 1,path_))
    {
      g_DISK_IMAGE_COUNT--;
      return false;
    }

  return true;
}

static
bool
disk_images_remove(unsigned index_)
{
  unsigned i;

  if(index_ >= g_DISK_IMAGE_COUNT)
    return false;

  disk_image_free(&g_DISK_IMAGES[index_]);
  for(i = index_; (i + 1) < g_DISK_IMAGE_COUNT; i++)
    g_DISK_IMAGES[i] = g_DISK_IMAGES[i + 1];

  g_DISK_IMAGE_COUNT--;
  memset(&g_DISK_IMAGES[g_DISK_IMAGE_COUNT],0,sizeof(g_DISK_IMAGES[g_DISK_IMAGE_COUNT]));

  if(g_DISK_IMAGE_INDEX > index_)
    g_DISK_IMAGE_INDEX--;
  else if((g_DISK_IMAGE_INDEX == index_) &&
          (g_DISK_IMAGE_INDEX >= g_DISK_IMAGE_COUNT))
    g_DISK_IMAGE_INDEX = g_DISK_IMAGE_COUNT;

  return true;
}

static
void
m3u_strip_bom(char *line_)
{
  if((line_ == NULL) || (strlen(line_) < 3))
    return;

  if(((uint8_t)line_[0] == 0xEF) &&
     ((uint8_t)line_[1] == 0xBB) &&
     ((uint8_t)line_[2] == 0xBF))
    memmove(line_,line_ + 3,strlen(line_ + 3) + 1);
}

static
void
m3u_strip_quotes(char *line_)
{
  size_t len;

  if(line_ == NULL)
    return;

  len = strlen(line_);
  if((len >= 2) && (line_[0] == '"') && (line_[len - 1] == '"'))
    {
      memmove(line_,line_ + 1,len - 2);
      line_[len - 2] = 0;
    }
}

static
bool
disk_images_load_m3u(const char *path_)
{
  RFILE *file;
  char line[PATH_MAX_LENGTH];
  char disk_path[PATH_MAX_LENGTH];
  unsigned line_number;
  bool rv;

  file = filestream_open(path_,
                         RETRO_VFS_FILE_ACCESS_READ,
                         RETRO_VFS_FILE_ACCESS_HINT_NONE);
  if(file == NULL)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: failed to open M3U playlist - %s\n",
                          path_);
      return false;
    }

  line_number = 0;
  rv = true;
  while(filestream_gets(file,line,sizeof(line)) != NULL)
    {
      line_number++;
      m3u_strip_bom(line);
      string_trim_whitespace(line);
      m3u_strip_quotes(line);
      string_trim_whitespace(line);

      if(string_is_empty(line) || (line[0] == '#'))
        continue;

      fill_pathname_resolve_relative(disk_path,path_,line,sizeof(disk_path));
      if(!disk_images_append_path(disk_path))
        {
          retro_log_printf_cb(RETRO_LOG_ERROR,
                              "[Opera]: failed to add M3U entry %u - %s\n",
                              line_number,
                              line);
          rv = false;
          break;
        }
    }

  filestream_close(file);

  if(!rv)
    {
      disk_images_clear();
      return false;
    }
  if(g_DISK_IMAGE_COUNT == 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: M3U playlist contains no disk images - %s\n",
                          path_);
      return false;
    }

  return true;
}

static
bool
disk_images_load_content(const struct retro_game_info *info_)
{
  const char *ext;

  if((info_ == NULL) || (info_->path == NULL))
    return true;

  ext = path_get_extension(info_->path);
  if(!strcasecmp(ext,"m3u"))
    return disk_images_load_m3u(info_->path);

  return disk_images_append_path(info_->path);
}

static
void
disk_image_select_initial(void)
{
  disk_image_t *image;

  g_DISK_IMAGE_INDEX = 0;
  if((g_DISK_INITIAL_PATH == NULL) ||
     (g_DISK_INITIAL_INDEX >= g_DISK_IMAGE_COUNT))
    return;

  image = &g_DISK_IMAGES[g_DISK_INITIAL_INDEX];
  if((image->path != NULL) && !strcmp(image->path,g_DISK_INITIAL_PATH))
    g_DISK_IMAGE_INDEX = g_DISK_INITIAL_INDEX;
}

static
bool
disk_set_eject_state(bool ejected_)
{
  const char *path;

  if(xbus_cdrom_media_ejected() == ejected_)
    return true;

  if(ejected_)
    {
      if(!opera_cdrom_ode_request_launch(NULL,0))
        return false;
      xbus_cdrom_media_eject();
      if(!ode_reset_if_requested())
        return false;

      return true;
    }

  path = NULL;
  if(g_DISK_IMAGE_INDEX < g_DISK_IMAGE_COUNT)
    {
      path = g_DISK_IMAGES[g_DISK_IMAGE_INDEX].path;
      if(path == NULL)
        return false;
    }

  if(!opera_cdrom_ode_request_launch(path,0))
    return false;
  if(!ode_reset_if_requested())
    return false;

  return true;
}

static
bool
disk_get_eject_state(void)
{
  return xbus_cdrom_media_ejected();
}

static
unsigned
disk_get_image_index(void)
{
  return g_DISK_IMAGE_INDEX;
}

static
bool
disk_set_image_index(unsigned index_)
{
  if(!xbus_cdrom_media_ejected())
    return false;

  if(index_ < g_DISK_IMAGE_COUNT)
    {
      if(g_DISK_IMAGES[index_].path == NULL)
        return false;

      g_DISK_IMAGE_INDEX = index_;
      return true;
    }

  g_DISK_IMAGE_INDEX = g_DISK_IMAGE_COUNT;
  return true;
}

static
unsigned
disk_get_num_images(void)
{
  return g_DISK_IMAGE_COUNT;
}

static
bool
disk_replace_image_index(unsigned                      index_,
                         const struct retro_game_info *info_)
{
  if(!xbus_cdrom_media_ejected())
    return false;
  if(index_ >= g_DISK_IMAGE_COUNT)
    return false;

  if(info_ == NULL)
    return disk_images_remove(index_);

  return disk_images_set_path(index_,info_->path);
}

static
bool
disk_add_image_index(void)
{
  return disk_images_append_empty();
}

static
bool
disk_set_initial_image(unsigned    index_,
                       const char *path_)
{
  char path[PATH_MAX_LENGTH];
  char *path_dup;

  if(!disk_image_normalize_path(path,path_,sizeof(path)))
    return false;

  path_dup = strdup(path);
  if(path_dup == NULL)
    return false;

  disk_initial_image_clear();
  g_DISK_INITIAL_INDEX = index_;
  g_DISK_INITIAL_PATH  = path_dup;

  return true;
}

static
bool
disk_get_image_path(unsigned  index_,
                    char     *path_,
                    size_t    path_size_)
{
  if((path_ == NULL) || (path_size_ == 0))
    return false;
  if(index_ >= g_DISK_IMAGE_COUNT)
    return false;
  if(g_DISK_IMAGES[index_].path == NULL)
    return false;

  return (strlcpy(path_,g_DISK_IMAGES[index_].path,path_size_) < path_size_);
}

static
bool
disk_get_image_label(unsigned  index_,
                     char     *label_,
                     size_t    label_size_)
{
  if((label_ == NULL) || (label_size_ == 0))
    return false;
  if(index_ >= g_DISK_IMAGE_COUNT)
    return false;
  if(g_DISK_IMAGES[index_].label == NULL)
    return false;

  return (strlcpy(label_,g_DISK_IMAGES[index_].label,label_size_) < label_size_);
}

static const struct retro_disk_control_callback DISK_CONTROL =
{
  disk_set_eject_state,
  disk_get_eject_state,
  disk_get_image_index,
  disk_set_image_index,
  disk_get_num_images,
  disk_replace_image_index,
  disk_add_image_index
};

static const struct retro_disk_control_ext_callback DISK_CONTROL_EXT =
{
  disk_set_eject_state,
  disk_get_eject_state,
  disk_get_image_index,
  disk_set_image_index,
  disk_get_num_images,
  disk_replace_image_index,
  disk_add_image_index,
  disk_set_initial_image,
  disk_get_image_path,
  disk_get_image_label
};

static
void
disk_control_set_interface(void)
{
  unsigned dci_version;

  dci_version = 0;
  retro_environment_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION,
                       &dci_version);

  if(dci_version >= 1)
    retro_environment_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE,
                         (void*)&DISK_CONTROL_EXT);
  else
    retro_environment_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE,
                         (void*)&DISK_CONTROL);
}

static
int
cdimage_ode_launch(const char *path_,
                   uint32_t    flags_)
{
  cdimage_t next;
  int rv;

  memset(&next,0,sizeof(next));
  if(path_ != NULL)
    {
      rv = retro_cdimage_open(path_,&next);
      if(rv == -1)
        {
          retro_log_printf_cb(RETRO_LOG_ERROR,
                              "[Opera]: ODE launch failed opening image - %s\n",
                              path_);
          return -1;
        }
    }

  opera_lr_nvram_save(game_info_path_get(),
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);

  retro_cdimage_close(&CDIMAGE);
  CDIMAGE = next;
  cdimage_set_sector(0);
  if((path_ != NULL) &&
     (flags_ & OPERA_CDROM_ODE_LAUNCH_UPDATE_CONTENT_PATH))
    game_info_path_save_path(path_);

  if(path_ != NULL)
    retro_log_printf_cb(RETRO_LOG_INFO,
                        "[Opera]: ODE launched image - %s\n",
                        path_);
  else
    retro_log_printf_cb(RETRO_LOG_INFO,
                        "[Opera]: ODE ejected image\n");
  return 0;
}

static
void
ode_root_set_for_content(const struct retro_game_info *info_)
{
  char root[PATH_MAX_LENGTH];

  if((info_ == NULL) || (info_->path == NULL))
    {
      opera_cdrom_ode_set_root(NULL);
      return;
    }

  strncpy(root,info_->path,sizeof(root) - 1);
  root[sizeof(root) - 1] = 0;
  path_basedir(root);

  opera_cdrom_ode_set_root(root);
}

bool
retro_load_game(const struct retro_game_info *info_)
{
  int rv;

  disk_images_clear();
  content_runtime_reset();
  game_info_path_save(info_);

  if(!disk_images_load_content(info_))
    {
      disk_initial_image_clear();
      return false;
    }

  disk_image_select_initial();
  rv = 0;
  if(g_DISK_IMAGE_INDEX < g_DISK_IMAGE_COUNT)
    rv = open_cdimage(g_DISK_IMAGES[g_DISK_IMAGE_INDEX].path);
  disk_initial_image_clear();
  if(rv == -1)
    {
      disk_images_clear();
      return false;
    }

  ode_root_set_for_content(info_);
  opera_lr_opts_process();
  opera_3do_init(libopera_callback);
  cdimage_set_sector(0);

  rv = set_pixel_format();
  if(rv < 0)
    return false;

  opera_lr_nvram_load(game_info_path_get(),
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);

  return true;
}

bool
retro_load_game_special(unsigned                      game_type_,
                        const struct retro_game_info *info_,
                        size_t                        num_info_)
{
  return false;
}

void
retro_unload_game(void)
{
  opera_lr_nvram_save(game_info_path_get(),
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);
  game_info_path_free();
  disk_images_clear();
  disk_initial_image_clear();

  opera_3do_destroy();
  opera_mem_destroy();

  content_runtime_reset();
  retro_cdimage_close(&CDIMAGE);

  opera_lr_opts_reset();
}

static
void
get_system_geometry(struct retro_game_geometry *geometry_)
{
  geometry_->base_width   = (opera_region_width()  << g_OPTS.high_resolution);
  geometry_->base_height  = (opera_region_height() << g_OPTS.high_resolution);
  geometry_->max_width    = (opera_region_max_width()  * 2);
  geometry_->max_height   = (opera_region_max_height() * 2);
  geometry_->aspect_ratio = 4.0 / 3.0;
}

static
void
get_system_av_info(struct retro_system_av_info *info_)
{
  memset(info_,0,sizeof(*info_));

  info_->timing.fps         = opera_region_refresh_rate();
  info_->timing.sample_rate = 44100;
  get_system_geometry(&info_->geometry);
}

void
retro_get_system_av_info(struct retro_system_av_info *info_)
{
  get_system_av_info(info_);
}

static
void
set_system_av_info(void)
{
  struct retro_system_av_info info;

  get_system_av_info(&info);
  retro_environment_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO,&info);
}

static
void
set_system_geometry(void)
{
  struct retro_game_geometry geometry;

  get_system_geometry(&geometry);
  retro_environment_cb(RETRO_ENVIRONMENT_SET_GEOMETRY,&geometry);
}

unsigned
retro_get_region(void)
{
  switch(opera_region_get())
    {
    case OPERA_REGION_PAL1:
    case OPERA_REGION_PAL2:
      return RETRO_REGION_PAL;
    case OPERA_REGION_NTSC:
    default:
      break;
    }

  return RETRO_REGION_NTSC;
}

unsigned
retro_api_version(void)
{
  return RETRO_API_VERSION;
}

void*
retro_get_memory_data(unsigned id_)
{
  switch(id_)
    {
    case RETRO_MEMORY_SAVE_RAM:
      return NULL;
    case RETRO_MEMORY_SYSTEM_RAM:
      return DRAM;
    case RETRO_MEMORY_VIDEO_RAM:
      return VRAM;
    }

  return NULL;
}

size_t
retro_get_memory_size(unsigned id_)
{
  switch(id_)
    {
    case RETRO_MEMORY_SAVE_RAM:
      return 0;
    case RETRO_MEMORY_SYSTEM_RAM:
      return DRAM_SIZE;
    case RETRO_MEMORY_VIDEO_RAM:
      return VRAM_SIZE;
    }

  return 0;
}

void
retro_init(void)
{
  struct retro_log_callback log;
  unsigned level;
  uint64_t serialization_quirks;

  level = 5;
  serialization_quirks = (RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT |
                          RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT);

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE,&log))
    {
      opera_lr_callbacks_set_log_printf(log.log);
      opera_log_set_func(log.log);
    }

  retro_environment_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL,&level);
  retro_environment_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS,&serialization_quirks);

  opera_cdrom_set_callbacks(cdimage_get_size,
                            cdimage_set_sector,
                            cdimage_read_sector,
                            cdimage_get_toc);
  opera_cdrom_ode_set_launch_callback(cdimage_ode_launch);
  disk_control_set_interface();

  srand(time(NULL));
  prng16_seed(time(NULL));
  prng32_seed(time(NULL));
}

void
retro_deinit(void)
{
  disk_images_clear();
  disk_initial_image_clear();
}

static
void
retro_reset_core(retro_reset_flags_t flags_)
{
  if(flags_ & RETRO_RESET_FLAG_SAVE_NVRAM)
    opera_lr_nvram_save(game_info_path_get(),
                        g_OPTS.nvram_shared,
                        g_OPTS.nvram_version);

  opera_3do_destroy();
  opera_lr_opts_reset();

  opera_lr_opts_process();
  opera_3do_init(libopera_callback);
  cdimage_set_sector(0);

  opera_lr_nvram_load(game_info_path_get(),
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);
}

void
retro_reset(void)
{
  retro_reset_core(RETRO_RESET_FLAG_SAVE_NVRAM);
}

static
bool
ode_reset_if_requested(void)
{
  if(!opera_cdrom_ode_consume_restart_request())
    return false;

  retro_log_printf_cb(RETRO_LOG_INFO,
                      "[Opera]: CDROM media change requested core reset\n");
  /* cdimage_ode_launch already saved NVRAM before switching media. */
  retro_reset_core(RETRO_RESET_FLAG_NONE);
  return true;
}

static
bool
variable_updated()
{
  bool updated;

  updated = false;
  if(!retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,&updated))
    return false;
  return updated;
}

static
void
process_opts_if_updated()
{
  uint32_t changes;

  if(!variable_updated())
    return;

  changes = opera_lr_opts_process();

  if(changes & OPERA_LR_OPTS_CHANGE_TIMING)
    set_system_av_info();
  else if(changes & OPERA_LR_OPTS_CHANGE_GEOMETRY)
    set_system_geometry();
}

static
void
draw_crosshairs_if_enabled()
{
  if(g_OPTS.hide_lightgun_crosshairs)
    return;

  lr_input_crosshairs_draw(g_OPTS.video_buffer,
                           g_OPTS.video_width,
                           g_OPTS.video_height);
}

void
retro_run(void)
{
  if(ode_reset_if_requested())
    return;

  process_opts_if_updated();

  lr_input_update(g_OPTS.active_devices);

  opera_3do_process_frame();
  if(ode_reset_if_requested())
    return;

  draw_crosshairs_if_enabled();

  opera_lr_dsp_upload();

  retro_video_refresh_cb(g_OPTS.video_buffer,
                         g_OPTS.video_width,
                         g_OPTS.video_height,
                         g_OPTS.video_width << g_OPTS.video_pitch_shift);
}
