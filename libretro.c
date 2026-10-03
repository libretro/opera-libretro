#include "file/file_path.h"
#include "libretro.h"
#include "libretro_core_options.h"
#include "retro_dirent.h"
#include "retro_miscellaneous.h"
#include "streams/file_stream.h"
#include "string/stdstring.h"
#include "lists/string_list.h"

#include "lr_input.h"
#include "lr_input_crosshair.h"
#include "lr_input_descs.h"
#include "opera_lr_callbacks.h"
#include "opera_lr_dsp.h"
#include "opera_lr_nvram.h"
#include "opera_lr_opts.h"
#include "retro_cdimage.h"

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
#include "libopera/prng16.h"
#include "libopera/prng32.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CDIMAGE_SECTOR_SIZE 2048
#define DISK_MAX_IMAGES     4
#define DISK_PATH_MAX       PATH_MAX_LENGTH

typedef enum retro_reset_flags_t
  {
    RETRO_RESET_FLAG_NONE       = 0,
    RETRO_RESET_FLAG_SAVE_NVRAM = (1 << 0)
  } retro_reset_flags_t;

/* Disk swapping state */
typedef struct disk_control_state_t
  {
    char     paths[DISK_MAX_IMAGES][DISK_PATH_MAX];
    char     labels[DISK_MAX_IMAGES][DISK_PATH_MAX];
    char     initial_path[DISK_PATH_MAX];
    unsigned initial_index;
    unsigned current_index;
    unsigned num_images;
    bool     tray_open;
  } disk_control_state_t;

static cdimage_t             cdimage;
static uint32_t              cdimage_sector;
static char                  game_name[PATH_MAX_LENGTH];
static char                  roms_dir[PATH_MAX_LENGTH];
static disk_control_state_t  disk_state;
static struct retro_disk_control_callback disk_control_cb;

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

  vfs_info.required_interface_version = 3;
  vfs_info.iface                      = NULL;

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE,&vfs_info) &&
     (vfs_info.iface != NULL))
    {
      filestream_vfs_init(&vfs_info);
      path_vfs_init(&vfs_info);
      dirent_vfs_init(&vfs_info);
      return;
    }

  vfs_info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
  vfs_info.iface                      = NULL;

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE,&vfs_info) &&
     (vfs_info.iface != NULL))
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
  return retro_cdimage_get_number_of_logical_blocks(&cdimage);
}

static
void
cdimage_set_sector(const uint32_t sector_)
{
  cdimage_sector = sector_;
}

static
void
cdimage_read_sector(void  *buf_,
                    size_t len_)
{
  retro_cdimage_read_sector(&cdimage,cdimage_sector,buf_,len_);
}

static
void
cdimage_get_toc(uint8_t  *track_first_,
                uint8_t  *track_last_,
                uint8_t  *disc_id_,
                void     *disc_toc_,
                uint32_t  disc_toc_size_)
{
  retro_cdimage_get_toc(&cdimage,
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
open_cdimage_if_needed(const struct retro_game_info *info_)
{
  int rv;

  if(!info_)
    return 0;

  rv = retro_cdimage_open(info_->path,&cdimage);
  if(rv == -1)
    return print_cdimage_open_fail(info_->path);

  return 0;
}

/* Extract basename without extension from path */
static
void
extract_basename(char *buf, const char *path, size_t size)
{
  const char *base = path_basename(path);
  char *ext;

  if(!base)
    {
      buf[0] = '\0';
      return;
    }

  strncpy(buf, base, size - 1);
  buf[size - 1] = '\0';

  ext = strrchr(buf, '.');
  if(ext)
    *ext = '\0';
}

/* Extract directory from path */
static
void
extract_directory(char *buf, const char *path, size_t size)
{
  const char *base;

  if(!path || !buf || size == 0)
    return;

  strncpy(buf, path, size - 1);
  buf[size - 1] = '\0';

  base = strrchr(buf, path_default_slash_c());
  if(base)
    *(char *)base = '\0';
  else
    {
      buf[0] = '.';
      buf[1] = '\0';
    }
}

/* Parse M3U playlist file */
static
bool
disk_read_m3u(const char *path)
{
  RFILE *file;
  char line[PATH_MAX_LENGTH];

  if(!path)
    return false;

  file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0);
  if(!file)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: failed to open M3U file: %s\n", path);
      return false;
    }

  disk_state.num_images = 0;

  while(filestream_gets(file, line, sizeof(line)) &&
        disk_state.num_images < DISK_MAX_IMAGES)
    {
      char *cr, *nl;
      size_t len;

      /* Skip comments */
      if(line[0] == '#')
        continue;

      /* Remove carriage return and newline */
      cr = strchr(line, '\r');
      if(cr)
        *cr = '\0';

      nl = strchr(line, '\n');
      if(nl)
        *nl = '\0';

      /* Remove leading/trailing quotes */
      len = strlen(line);
      if(len == 0)
        continue;

      if(line[0] == '"')
        memmove(line, line + 1, len);

      len = strlen(line);
      if(len > 0 && line[len - 1] == '"')
        line[len - 1] = '\0';

      /* Skip empty lines */
      if(string_is_empty(line))
        continue;

      /* Build full path */
      fill_pathname_join(disk_state.paths[disk_state.num_images],
                         roms_dir, line,
                         sizeof(disk_state.paths[disk_state.num_images]));

      /* Generate label from filename */
      fill_pathname(disk_state.labels[disk_state.num_images],
                    path_basename(disk_state.paths[disk_state.num_images]),
                    "",
                    sizeof(disk_state.labels[disk_state.num_images]));

      disk_state.num_images++;
    }

  filestream_close(file);

  return (disk_state.num_images > 0);
}

/* Disk control callbacks */
static
bool
disk_set_eject_state(bool ejected)
{
  disk_state.tray_open = ejected;

  if(!ejected)
    {
      int rv;

      /* Tray closing - swap disc */
      opera_lr_nvram_save(game_name,
                          g_OPTS.nvram_shared,
                          g_OPTS.nvram_version);

      opera_lr_dsp_destroy();
      opera_3do_destroy();
      retro_cdimage_close(&cdimage);

      rv = retro_cdimage_open(disk_state.paths[disk_state.current_index],
                              &cdimage);
      if(rv == -1)
        {
          retro_log_printf_cb(RETRO_LOG_ERROR,
                              "[Opera]: failed to open disc image: %s\n",
                              disk_state.paths[disk_state.current_index]);
          return false;
        }

      cdimage_set_sector(0);
      opera_3do_init(libopera_callback);

      rv = set_pixel_format();
      if(rv < 0)
        return false;

      opera_lr_nvram_load(game_name,
                          g_OPTS.nvram_shared,
                          g_OPTS.nvram_version);
    }

  return true;
}

static
bool
disk_get_eject_state(void)
{
  return disk_state.tray_open;
}

static
bool
disk_set_image_index(unsigned index)
{
  if(!disk_state.tray_open)
    return false;

  if(index >= disk_state.num_images)
    return false;

  disk_state.current_index = index;
  return true;
}

static
unsigned
disk_get_image_index(void)
{
  return disk_state.current_index;
}

static
unsigned
disk_get_num_images(void)
{
  return disk_state.num_images;
}

static
bool
disk_add_image_index(void)
{
  if(disk_state.num_images >= DISK_MAX_IMAGES)
    return false;

  disk_state.paths[disk_state.num_images][0] = '\0';
  disk_state.labels[disk_state.num_images][0] = '\0';
  disk_state.num_images++;

  return true;
}

static
bool
disk_replace_image_index(unsigned index,
                         const struct retro_game_info *info)
{
  if(index >= disk_state.num_images)
    return false;

  if(!info)
    {
      /* Remove this image */
      unsigned i;
      for(i = index; i < disk_state.num_images - 1; i++)
        {
          memcpy(disk_state.paths[i], disk_state.paths[i + 1], DISK_PATH_MAX);
          memcpy(disk_state.labels[i], disk_state.labels[i + 1], DISK_PATH_MAX);
        }
      disk_state.num_images--;

      if(disk_state.current_index >= index && disk_state.current_index > 0)
        disk_state.current_index--;
    }
  else
    {
      strncpy(disk_state.paths[index], info->path, DISK_PATH_MAX - 1);
      disk_state.paths[index][DISK_PATH_MAX - 1] = '\0';
      fill_pathname(disk_state.labels[index],
                    path_basename(info->path),
                    "",
                    DISK_PATH_MAX);
    }

  return true;
}

static
bool
disk_set_initial_image(unsigned index, const char *path)
{
  if(!path || string_is_empty(path))
    return false;

  disk_state.initial_index = index;
  strncpy(disk_state.initial_path, path, DISK_PATH_MAX - 1);
  disk_state.initial_path[DISK_PATH_MAX - 1] = '\0';

  return true;
}

static
bool
disk_get_image_path(unsigned index, char *path, size_t len)
{
  if(len < 1 || index >= disk_state.num_images)
    return false;

  if(string_is_empty(disk_state.paths[index]))
    return false;

  strncpy(path, disk_state.paths[index], len - 1);
  path[len - 1] = '\0';

  return true;
}

static
bool
disk_get_image_label(unsigned index, char *label, size_t len)
{
  if(len < 1 || index >= disk_state.num_images)
    return false;

  if(string_is_empty(disk_state.labels[index]))
    return false;

  strncpy(label, disk_state.labels[index], len - 1);
  label[len - 1] = '\0';

  return true;
}

static
void
disk_control_interface_init(void)
{
  unsigned dci_version = 0;

  memset(&disk_control_cb, 0, sizeof(disk_control_cb));
  memset(&disk_state, 0, sizeof(disk_state));

  disk_control_cb.set_eject_state     = disk_set_eject_state;
  disk_control_cb.get_eject_state     = disk_get_eject_state;
  disk_control_cb.set_image_index     = disk_set_image_index;
  disk_control_cb.get_image_index     = disk_get_image_index;
  disk_control_cb.get_num_images      = disk_get_num_images;
  disk_control_cb.add_image_index     = disk_add_image_index;
  disk_control_cb.replace_image_index = disk_replace_image_index;

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION,
                          &dci_version) && dci_version >= 1)
    {
      struct retro_disk_control_ext_callback disk_control_ext_cb;

      memset(&disk_control_ext_cb, 0, sizeof(disk_control_ext_cb));
      disk_control_ext_cb.set_eject_state     = disk_set_eject_state;
      disk_control_ext_cb.get_eject_state     = disk_get_eject_state;
      disk_control_ext_cb.set_image_index     = disk_set_image_index;
      disk_control_ext_cb.get_image_index     = disk_get_image_index;
      disk_control_ext_cb.get_num_images      = disk_get_num_images;
      disk_control_ext_cb.add_image_index     = disk_add_image_index;
      disk_control_ext_cb.replace_image_index = disk_replace_image_index;
      disk_control_ext_cb.set_initial_image   = disk_set_initial_image;
      disk_control_ext_cb.get_image_path      = disk_get_image_path;
      disk_control_ext_cb.get_image_label     = disk_get_image_label;

      retro_environment_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE,
                           &disk_control_ext_cb);
    }
  else
    {
      retro_environment_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE,
                           &disk_control_cb);
    }
}

static
int
cdimage_ode_launch(const char *path_)
{
  cdimage_t next;
  int rv;

  memset(&next,0,sizeof(next));
  rv = retro_cdimage_open(path_,&next);
  if(rv == -1)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: ODE launch failed opening image - %s\n",
                          path_);
      return -1;
    }

  opera_lr_nvram_save(game_name,
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);

  retro_cdimage_close(&cdimage);
  cdimage = next;
  cdimage_set_sector(0);

  /* Update game_name for the new image */
  extract_basename(game_name, path_, sizeof(game_name));

  retro_log_printf_cb(RETRO_LOG_INFO,
                      "[Opera]: ODE launched image - %s\n",
                      path_);
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
  const char *ext;

  if(!info_ || !info_->path)
    return false;

  content_runtime_reset();

  /* Extract game name and ROM directory */
  extract_basename(game_name, info_->path, sizeof(game_name));
  extract_directory(roms_dir, info_->path, sizeof(roms_dir));

  /* Check file extension */
  ext = path_get_extension(info_->path);

  if(ext && string_is_equal_noncase(ext, "m3u"))
    {
      /* Parse M3U playlist */
      if(!disk_read_m3u(info_->path))
        {
          retro_log_printf_cb(RETRO_LOG_ERROR,
                              "[Opera]: failed to parse M3U file\n");
          return false;
        }

      /* Set initial disc index if specified */
      disk_state.current_index = 0;
      if(disk_state.num_images > 1 &&
         disk_state.initial_index > 0 &&
         disk_state.initial_index < disk_state.num_images)
        {
          if(string_is_equal(disk_state.paths[disk_state.initial_index],
                             disk_state.initial_path))
            disk_state.current_index = disk_state.initial_index;
        }
    }
  else
    {
      /* Single image - add to disk list */
      strncpy(disk_state.paths[0], info_->path, DISK_PATH_MAX - 1);
      disk_state.paths[0][DISK_PATH_MAX - 1] = '\0';
      fill_pathname(disk_state.labels[0],
                    path_basename(info_->path),
                    "",
                    DISK_PATH_MAX);
      disk_state.num_images = 1;
      disk_state.current_index = 0;
    }

  /* Open the current disc image */
  rv = retro_cdimage_open(disk_state.paths[disk_state.current_index], &cdimage);
  if(rv == -1)
    {
      print_cdimage_open_fail(disk_state.paths[disk_state.current_index]);
      return false;
    }

  ode_root_set_for_content(info_);
  opera_lr_opts_process();
  opera_3do_init(libopera_callback);
  cdimage_set_sector(0);

  rv = set_pixel_format();
  if(rv < 0)
    return false;

  opera_lr_nvram_load(game_name,
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
  opera_lr_nvram_save(game_name,
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);

  opera_3do_destroy();
  opera_mem_destroy();

  content_runtime_reset();
  retro_cdimage_close(&cdimage);

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
  bool fixed_random_seed;
  struct retro_log_callback log;
  uint32_t random_seed;
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

  fixed_random_seed = opera_lr_opts_get_random_seed(&random_seed);
  if(!fixed_random_seed)
    random_seed = (uint32_t)time(NULL);

  prng16_seed(random_seed);
  prng32_seed(random_seed);

  retro_log_printf_cb(RETRO_LOG_INFO,
                      "[Opera]: random seed 0x%08X (%s)\n",
                      (unsigned)random_seed,
                      fixed_random_seed ? "fixed" : "time-based");

  disk_control_interface_init();
}

void
retro_deinit(void)
{

}

static
void
retro_reset_core(retro_reset_flags_t flags_)
{
  if(flags_ & RETRO_RESET_FLAG_SAVE_NVRAM)
    opera_lr_nvram_save(game_name,
                        g_OPTS.nvram_shared,
                        g_OPTS.nvram_version);

  opera_3do_destroy();
  opera_lr_opts_reset();

  opera_lr_opts_process();
  opera_3do_init(libopera_callback);
  cdimage_set_sector(0);

  opera_lr_nvram_load(game_name,
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
                      "[Opera]: ODE media launch requested core reset\n");
  /* cdimage_ode_launch already saved NVRAM before switching game paths. */
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
