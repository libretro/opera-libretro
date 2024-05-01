#include "file/file_path.h"
#include "libretro.h"
#include "libretro_core_options.h"
#include "retro_miscellaneous.h"
#include "streams/file_stream.h"

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
#include "libopera/prng16.h"
#include "libopera/prng32.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CDIMAGE_SECTOR_SIZE 2048

static char slash = path_default_slash_c();

static cdimage_t  CDIMAGE;
static uint32_t   CDIMAGE_SECTOR;
static uint32_t  *g_VIDEO_BUFFER;
static char g_GAME_NAME[PATH_MAX_LENGTH];
static char g_ROMS_DIR[PATH_MAX_LENGTH];

// disk swapping
#define M3U_MAX_FILE 4

static char g_DISK_PATHS[M3U_MAX_FILE][PATH_MAX_LENGTH];
static char g_DISK_LABELS[M3U_MAX_FILE][PATH_MAX_LENGTH];

static unsigned g_DISK_INITIAL_INDEX = 0;
static char g_DISK_INITIAL_PATH[PATH_MAX];
static unsigned g_DISK_INDEX = 0;
static unsigned g_DISK_TOTAL = 0;
static bool g_DISK_TRAY_OPEN = false;

static struct retro_disk_control_ext_callback g_DISK_CONTROL;

static
bool
read_m3u(const char *file)
{
  char line[PATH_MAX];
  char name[PATH_MAX];
  RFILE *f = filestream_open(file, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

  g_DISK_TOTAL = 0;

  if (!f)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR, "Could not read file\n");
      return false;
    }

  while (filestream_gets(f, line, sizeof(line)) && g_DISK_TOTAL <= M3U_MAX_FILE)
    {
      if (line[0] == '#')
        continue;

      char *carriage_return = strchr(line, '\r');
      if (carriage_return)
        *carriage_return = '\0';

      char *newline = strchr(line, '\n');
      if (newline)
        *newline = '\0';

      if (line[0] == '"')
        memmove(line, line + 1, strlen(line));

      if (line[strlen(line) - 1] == '"')
        line[strlen(line) - 1]  = '\0';

      if (strlen(g_ROMS_DIR) + strlen(line) + 1 >= PATH_MAX)
        {
          retro_log_printf_cb(RETRO_LOG_ERROR, "Path too long\n");
          filestream_close(f);
          return false;
        }

      if (line[0] != '\0')
        {
          snprintf(g_DISK_PATHS[g_DISK_TOTAL], sizeof(g_DISK_PATHS[g_DISK_TOTAL]), "%s%c%s", g_ROMS_DIR, slash, line);
          fill_pathname(g_DISK_LABELS[g_DISK_TOTAL], path_basename(g_DISK_PATHS[g_DISK_TOTAL]), "", sizeof(g_DISK_LABELS[g_DISK_TOTAL]));
          g_DISK_TOTAL++;
        }
    }

  filestream_close(f);
  return (g_DISK_TOTAL != 0);
}

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
  return retro_cdimage_get_number_of_logical_blocks(&CDIMAGE);
}

static
void
cdimage_set_sector(const uint32_t sector_)
{
  CDIMAGE_SECTOR = sector_;
}

static
void
cdimage_read_sector(void *buf_)
{
  retro_cdimage_read(&CDIMAGE,CDIMAGE_SECTOR,buf_,CDIMAGE_SECTOR_SIZE);
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

  size = opera_3do_state_save(data_,size_);

  return (size == size_);
}

bool
retro_unserialize(void const *data_,
                  size_t      size_)
{
  uint32_t size;
  void *backup_state;

  backup_state = malloc(retro_serialize_size());
  if(backup_state == NULL)
    return false;
  size = retro_serialize(backup_state,retro_serialize_size());
  if(size)
    {
      size = opera_3do_state_load(data_,size_);
      if(size != size_)
        opera_3do_state_load(backup_state,retro_serialize_size());
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
  rv = retro_cdimage_open(path_,&CDIMAGE);
  if(rv == -1)
    return print_cdimage_open_fail(path_);

  return 0;
}

static
void
extract_basename(char *buf, const char *path, size_t size)
{
  strncpy(buf, path_basename(path), size - 1);
  buf[size - 1] = '\0';

  char *ext = strrchr(buf, '.');
  if (ext)
    *ext = '\0';
}

static
void
extract_directory(char *buf, const char *path, size_t size)
{
  strncpy(buf, path, size - 1);
  buf[size - 1] = '\0';

  char *base = strrchr(buf, slash);

  if (base)
    *base = '\0';
  else
    {
      buf[0] = '.';
      buf[1] = '\0';
    }
}

static
bool
disk_set_eject_state(bool ejected)
{
  g_DISK_TRAY_OPEN = ejected;
  if (ejected)
    return true;

  int rv;
  // unload
  opera_lr_nvram_save(g_GAME_NAME, g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);

  opera_3do_destroy();

  retro_cdimage_close(&CDIMAGE);

  opera_lr_opts_reset();

  // load
  rv = open_cdimage(g_DISK_PATHS[g_DISK_INDEX]);
  if (rv == -1)
    return false;

  opera_lr_opts_process();
  opera_3do_init(libopera_callback);

  cdimage_set_sector(0);

  rv = set_pixel_format();
  if (rv < 0)
    return false;

  opera_lr_nvram_load(g_GAME_NAME, g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);

  return true;
}

static
bool
disk_get_eject_state()
{
  return g_DISK_TRAY_OPEN;
}

static
bool
disk_set_image_index(unsigned index)
{
  if (g_DISK_TRAY_OPEN && index < g_DISK_TOTAL)
    {
      g_DISK_INDEX = index;
      return true;
    }
  return false;
}

static
unsigned
disk_get_image_index()
{
  return g_DISK_INDEX;
}

static
unsigned
disk_get_num_images(void)
{
  return g_DISK_TOTAL;
}

static
bool
disk_add_image_index(void)
{
  if (g_DISK_TOTAL >= M3U_MAX_FILE)
    return false;
  g_DISK_TOTAL++;
  g_DISK_PATHS[g_DISK_TOTAL - 1][0] = '\0';
  g_DISK_LABELS[g_DISK_TOTAL - 1][0] = '\0';
  return true;
}

static
bool
disk_replace_image_index(unsigned index,
                         const struct retro_game_info *info)
{
  if ((index >= g_DISK_TOTAL))
    return false;

  if (!info)
    {
      g_DISK_PATHS[index][0] = '\0';
      g_DISK_LABELS[index][0] = '\0';
      g_DISK_TOTAL--;

      if ((g_DISK_INDEX >= index) && (g_DISK_INDEX > 0))
        g_DISK_INDEX--;
    }
  else
    {
      snprintf(g_DISK_PATHS[index], sizeof(g_DISK_PATHS[index]), "%s", info->path);
      fill_pathname(g_DISK_LABELS[index], path_basename(g_DISK_PATHS[index]), "",
                    sizeof(g_DISK_LABELS[index]));
    }
  return true;
}

static bool disk_set_initial_image(unsigned index, const char *path) {
  if (!path || (*path == '\0'))
    return false;

  g_DISK_INITIAL_INDEX = index;
  snprintf(g_DISK_INITIAL_PATH, sizeof(g_DISK_INITIAL_PATH), "%s", path);

  return true;
}

static
bool
disk_get_image_path(unsigned index, char *path, size_t len)
{
  if (len < 1)
    return false;

  if (index >= g_DISK_TOTAL)
    return false;

  if (g_DISK_PATHS[index][0] == '\0')
    return false;

  strncpy(path, g_DISK_PATHS[index], len - 1);
  path[len - 1] = '\0';

  return true;
}

static
bool
disk_get_image_label(unsigned index, char *label, size_t len)
{
  if (len < 1)
    return false;

  if (index >= g_DISK_TOTAL)
    return false;

  if (g_DISK_LABELS[index][0] == '\0')
    return false;

  strncpy(label, g_DISK_LABELS[index], len - 1);
  label[len - 1] = '\0';

  return true;
}

static
void
init_disk_control_interface(void)
{
  unsigned dci_version = 0;

  g_DISK_CONTROL.set_eject_state     = disk_set_eject_state;
  g_DISK_CONTROL.get_eject_state     = disk_get_eject_state;
  g_DISK_CONTROL.set_image_index     = disk_set_image_index;
  g_DISK_CONTROL.get_image_index     = disk_get_image_index;
  g_DISK_CONTROL.get_num_images      = disk_get_num_images;
  g_DISK_CONTROL.add_image_index     = disk_add_image_index;
  g_DISK_CONTROL.replace_image_index = disk_replace_image_index;
  g_DISK_CONTROL.set_initial_image   = disk_set_initial_image;
  g_DISK_CONTROL.get_image_path      = disk_get_image_path;
  g_DISK_CONTROL.get_image_label     = disk_get_image_label;

  g_DISK_INITIAL_INDEX = 0;
  g_DISK_INITIAL_PATH[0] = '\0';

  retro_environment_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION,
                       &dci_version);

  retro_environment_cb(dci_version >= 1 ? RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE
                                        : RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE,
                       &g_DISK_CONTROL);
}

bool
retro_load_game(const struct retro_game_info *info_)
{
  int rv;

  extract_basename(g_GAME_NAME, info_->path, sizeof(g_GAME_NAME));
  extract_directory(g_ROMS_DIR, info_->path, sizeof(g_ROMS_DIR));

  if (strcmp(path_get_extension(info_->path), "m3u") == 0)
    {
      if (!read_m3u(info_->path))
        {
          retro_log_printf_cb(RETRO_LOG_ERROR, "Aborting, this m3u file is invalid\n");
          return false;
        }
      else
        {
          g_DISK_INDEX = 0;

          if ((g_DISK_TOTAL > 1) && (g_DISK_INITIAL_INDEX > 0) && (g_DISK_INITIAL_INDEX < g_DISK_TOTAL))
            if (strcmp(g_DISK_PATHS[g_DISK_INITIAL_INDEX], g_DISK_INITIAL_PATH) == 0)
                g_DISK_INDEX = g_DISK_INITIAL_INDEX;
        }
    }
  else
    {
      snprintf(g_DISK_PATHS[g_DISK_TOTAL], sizeof(g_DISK_PATHS[g_DISK_TOTAL]), "%s", info_->path);
      fill_pathname(g_DISK_LABELS[g_DISK_TOTAL], path_basename(g_DISK_PATHS[g_DISK_TOTAL]), "", sizeof(g_DISK_LABELS[g_DISK_TOTAL]));
      g_DISK_TOTAL++;
    }

  rv = open_cdimage(g_DISK_PATHS[g_DISK_INDEX]);
  if(rv == -1)
    return false;

  opera_lr_opts_process();
  opera_3do_init(libopera_callback);
  cdimage_set_sector(0);

  rv = set_pixel_format();
  if(rv < 0)
    return false;

  opera_lr_nvram_load(g_GAME_NAME,
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
  opera_lr_nvram_save(g_GAME_NAME,
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);

  opera_3do_destroy();

  retro_cdimage_close(&CDIMAGE);

  opera_lr_opts_reset();
}

void
retro_get_system_av_info(struct retro_system_av_info *info_)
{
  memset(info_,0,sizeof(*info_));

  info_->timing.fps            = opera_region_field_rate();
  info_->timing.sample_rate    = 44100;
  info_->geometry.base_width   = opera_region_min_width();
  info_->geometry.base_height  = opera_region_min_height();
  info_->geometry.max_width    = (opera_region_max_width()  * 2);
  info_->geometry.max_height   = (opera_region_max_height() * 2);
  info_->geometry.aspect_ratio = 4.0 / 3.0;
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
                            cdimage_read_sector);

  init_disk_control_interface();

  srand(time(NULL));
  prng16_seed(time(NULL));
  prng32_seed(time(NULL));
}

void
retro_deinit(void)
{
}

void
retro_reset(void)
{
  opera_lr_nvram_save(g_GAME_NAME,
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);

  opera_3do_destroy();
  opera_lr_opts_reset();

  opera_lr_opts_process();
  opera_3do_init(libopera_callback);
  cdimage_set_sector(0);

  opera_lr_nvram_load(g_GAME_NAME,
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);
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
  if(!variable_updated())
    return;

  opera_lr_opts_process();
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
  process_opts_if_updated();

  lr_input_update(g_OPTS.active_devices);

  opera_3do_process_frame();

  draw_crosshairs_if_enabled();

  opera_lr_dsp_upload();

  retro_video_refresh_cb(g_OPTS.video_buffer,
                         g_OPTS.video_width,
                         g_OPTS.video_height,
                         g_OPTS.video_width << g_OPTS.video_pitch_shift);
}
