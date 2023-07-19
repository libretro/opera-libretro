#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <libretro.h>
#include <libretro_core_options.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

#include "libopera/hack_flags.h"
#include "libopera/opera_3do.h"
#include "libopera/opera_arm.h"
#include "libopera/opera_bios.h"
#include "libopera/opera_cdrom.h"
#include "libopera/opera_clock.h"
#include "libopera/opera_core.h"
#include "libopera/opera_madam.h"
#include "libopera/opera_pbus.h"
#include "libopera/opera_region.h"
#include "libopera/opera_vdlp.h"
#include "libopera/opera_nvram.h"

#include "opera_lr_dsp.h"
#include "lr_input.h"
#include "lr_input_crosshair.h"
#include "lr_input_descs.h"
#include "opera_lr_nvram.h"
#include "opera_lr_callbacks.h"
#include "opera_lr_opts.h"
#include "retro_cdimage.h"

#define CDIMAGE_SECTOR_SIZE 2048

static char slash = path_default_slash_c();

static cdimage_t  CDIMAGE;
static uint32_t   CDIMAGE_SECTOR;
static uint32_t  *g_VIDEO_BUFFER;
static char g_GAME_NAME[PATH_MAX_LENGTH];
static char g_ROMS_DIR[PATH_MAX_LENGTH];

// disk swapping
#define M3U_MAX_FILE 4

static char disk_paths[M3U_MAX_FILE][PATH_MAX_LENGTH];
static char disk_labels[M3U_MAX_FILE][PATH_MAX_LENGTH];

static unsigned disk_initial_index = 0;
static char disk_initial_path[PATH_MAX];
static unsigned disk_index = 0;
static unsigned disk_total = 0;
static bool disk_tray_open = false;

static struct retro_disk_control_callback retro_disk_control_cb;
static struct retro_disk_control_ext_callback retro_disk_control_ext_cb;

static bool read_m3u(const char *file)
{
   char line[PATH_MAX];
   char name[PATH_MAX];
   FILE *f = fopen(file, "r");

   disk_total = 0;

   if (!f)
   {
      retro_log_printf_cb(RETRO_LOG_ERROR, "Could not read file\n");
      return false;
   }

   while (fgets(line, sizeof(line), f) && disk_total <= M3U_MAX_FILE)
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

      if (line[0] != '\0')
      {
         snprintf(disk_paths[disk_total], sizeof(disk_paths[disk_total]), "%s%c%s", g_ROMS_DIR, slash, line);
         fill_pathname(disk_labels[disk_total], path_basename(disk_paths[disk_total]), "", sizeof(disk_labels[disk_total]));
         disk_total++;
      }
   }

   fclose(f);
   return (disk_total != 0);
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
void
video_init(void)
{
  /* The 4x multiplication is for hires mode */
  uint32_t size = (opera_region_max_width() * opera_region_max_height() * 4);
  if(!g_VIDEO_BUFFER)
    g_VIDEO_BUFFER = (uint32_t*)calloc(size,sizeof(uint32_t));
}

static
void
video_destroy(void)
{
  if(g_VIDEO_BUFFER)
    free(g_VIDEO_BUFFER);
  g_VIDEO_BUFFER = NULL;
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
  if(size_ != opera_3do_state_size())
    return false;

  opera_3do_state_save(data_);

  return true;
}

bool
retro_unserialize(const void *data_,
                  size_t      size_)
{
  if(size_ != opera_3do_state_size())
    return false;

  opera_3do_state_load(data_);

  return true;
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

static
void
process_opts(void)
{
  opera_lr_opts_process();

  opera_vdlp_configure(g_VIDEO_BUFFER,g_OPT_VDLP_PIXEL_FORMAT,g_OPT_VDLP_FLAGS);
}

void
retro_set_controller_port_device(unsigned port_,
                                 unsigned device_)
{
  lr_input_device_set_with_descs(port_,device_);
}

static
int64_t
read_file_from_system_directory(const char *filename_,
                                uint8_t    *data_,
                                int64_t     size_)
{
  int64_t rv;
  RFILE *file;
  const char *system_path;
  char fullpath[PATH_MAX_LENGTH];

  system_path = NULL;
  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&system_path);
  if((rv == 0) || (system_path == NULL))
    return -1;

  fill_pathname_join(fullpath,system_path,filename_,PATH_MAX_LENGTH);

  file = filestream_open(fullpath,RETRO_VFS_FILE_ACCESS_READ,RETRO_VFS_FILE_ACCESS_HINT_NONE);
  if(file == NULL)
    return -1;

  rv = filestream_read(file,data_,size_);

  filestream_close(file);

  return rv;
}

static
int
load_rom1(void)
{
  uint8_t *rom;
  int64_t  size;
  int64_t  rv;

  if(g_OPT_BIOS == NULL)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,"[Opera]: no BIOS ROM found\n");
      return -1;
    }

  rom  = opera_arm_rom1_get();
  size = opera_arm_rom1_size();
  if((rv = read_file_from_system_directory(g_OPT_BIOS->filename,rom,size)) < 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: unable to find or load BIOS ROM - %s\n",
                          g_OPT_BIOS->filename);
      return -1;
    }

  opera_arm_rom1_byteswap_if_necessary();

  return 0;
}

static
int
load_rom2(void)
{
  int64_t  rv;
  uint8_t *rom;
  int64_t  size;

  rom  = opera_arm_rom2_get();
  size = opera_arm_rom2_size();
  if(g_OPT_FONT == NULL)
    {
      memset(rom,0,size);
      return 0;
    }

  if((rv = read_file_from_system_directory(g_OPT_FONT->filename,rom,size)) < 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: unable to find or load FONT ROM - %s\n",
                          g_OPT_FONT->filename);
      return -1;
    }

  opera_arm_rom2_byteswap_if_necessary();

  return 0;
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

  fmt = vdlp_pixel_format_to_libretro(g_OPT_VDLP_PIXEL_FORMAT);
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

static void extract_basename(char *buf, const char *path, size_t size) {
  strncpy(buf, path_basename(path), size - 1);
  buf[size - 1] = '\0';

  char *ext = strrchr(buf, '.');
  if (ext)
    *ext = '\0';
}

static void extract_directory(char *buf, const char *path, size_t size) {
  strncpy(buf, path, size - 1);
  buf[size - 1] = '\0';

  char *base = strrchr(buf, slash);

  if (base)
    *base = '\0';
  else {
    buf[0] = '.';
    buf[1] = '\0';
  }
}

static bool disk_set_eject_state(bool ejected) {
   disk_tray_open = ejected;
   if (ejected) {
     return true;
   } else {
     int rv;
     // unload
     opera_lr_nvram_save(g_GAME_NAME);

     opera_lr_dsp_destroy();
     opera_3do_destroy();

     retro_cdimage_close(&CDIMAGE);

     video_destroy();

     // load 
     rv = open_cdimage(disk_paths[disk_index]);
     if (rv == -1)
       return false;

     cdimage_set_sector(0);
     opera_3do_init(libopera_callback);
     video_init();
     process_opts();
     load_rom1();
     load_rom2();

     rv = set_pixel_format();
     if (rv < 0)
       return false;

     opera_nvram_init();
     opera_lr_nvram_load(g_GAME_NAME);

     return true;
   }
}

static bool disk_get_eject_state() { return disk_tray_open; }

static bool disk_set_image_index(unsigned index) {
   if (disk_tray_open == true) {
     if (index < disk_total) {
       disk_index = index;
       return true;
     }
   }

   return false;
}

static unsigned disk_get_image_index() { return disk_index; }

static unsigned disk_get_num_images(void) { return disk_total; }

static bool disk_add_image_index(void) {
   if (disk_total >= M3U_MAX_FILE)
     return false;
   disk_total++;
   disk_paths[disk_total - 1][0] = '\0';
   disk_labels[disk_total - 1][0] = '\0';
   return true;
}

static bool disk_replace_image_index(unsigned index,
                                     const struct retro_game_info *info) {
   if ((index >= disk_total))
     return false;

   if (!info) {
     disk_paths[index][0] = '\0';
     disk_labels[index][0] = '\0';
     disk_total--;

     if ((disk_index >= index) && (disk_index > 0))
       disk_index--;
   } else {
     snprintf(disk_paths[index], sizeof(disk_paths[index]), "%s", info->path);
     fill_pathname(disk_labels[index], path_basename(disk_paths[index]), "",
                   sizeof(disk_labels[index]));
   }

   return true;
}

static bool disk_set_initial_image(unsigned index, const char *path) {
   if (!path || (*path == '\0'))
     return false;

   disk_initial_index = index;
   snprintf(disk_initial_path, sizeof(disk_initial_path), "%s", path);

   return true;
}

static bool disk_get_image_path(unsigned index, char *path, size_t len) {
   if (len < 1)
     return false;

   if (index >= disk_total)
     return false;

   if (disk_paths[index] == NULL || disk_paths[index][0] == '\0')
     return false;

   strncpy(path, disk_paths[index], len - 1);
   path[len - 1] = '\0';

   return true;
}

static bool disk_get_image_label(unsigned index, char *label, size_t len) {
   if (len < 1)
     return false;

   if (index >= disk_total)
     return false;

   if (disk_labels[index] == NULL || disk_labels[index][0] == '\0')
     return false;

   strncpy(label, disk_labels[index], len - 1);
   label[len - 1] = '\0';

   return true;
}

static void init_disk_control_interface(void)
{
   unsigned dci_version = 0;

   retro_disk_control_cb.set_eject_state     = disk_set_eject_state;
   retro_disk_control_cb.get_eject_state     = disk_get_eject_state;
   retro_disk_control_cb.set_image_index     = disk_set_image_index;
   retro_disk_control_cb.get_image_index     = disk_get_image_index;
   retro_disk_control_cb.get_num_images      = disk_get_num_images;
   retro_disk_control_cb.add_image_index     = disk_add_image_index;
   retro_disk_control_cb.replace_image_index = disk_replace_image_index;

   retro_disk_control_ext_cb.set_eject_state     = disk_set_eject_state;
   retro_disk_control_ext_cb.get_eject_state     = disk_get_eject_state;
   retro_disk_control_ext_cb.set_image_index     = disk_set_image_index;
   retro_disk_control_ext_cb.get_image_index     = disk_get_image_index;
   retro_disk_control_ext_cb.get_num_images      = disk_get_num_images;
   retro_disk_control_ext_cb.add_image_index     = disk_add_image_index;
   retro_disk_control_ext_cb.replace_image_index = disk_replace_image_index;
   retro_disk_control_ext_cb.set_initial_image   = disk_set_initial_image;
   retro_disk_control_ext_cb.get_image_path      = disk_get_image_path;
   retro_disk_control_ext_cb.get_image_label     = disk_get_image_label;

   disk_initial_index = 0;
   disk_initial_path[0] = '\0';
   if (retro_environment_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION, &dci_version) && (dci_version >= 1))
      retro_environment_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &retro_disk_control_ext_cb);
   else
      retro_environment_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &retro_disk_control_cb);

   /* retro_environment_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, */
   /*                  &retro_disk_control_cb); */
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
        disk_index = 0;

        if ((disk_total > 1) && (disk_initial_index > 0) && (disk_initial_index < disk_total))
          if (strcmp(disk_paths[disk_initial_index], disk_initial_path) == 0)
              disk_index = disk_initial_index;
    }
  }
  else
  {
    snprintf(disk_paths[disk_total], sizeof(disk_paths[disk_total]), "%s", info_->path);
    fill_pathname(disk_labels[disk_total], path_basename(disk_paths[disk_total]), "", sizeof(disk_labels[disk_total]));
    disk_total++;
  }

  rv = open_cdimage(disk_paths[disk_index]);
  if(rv == -1)
    return false;

  cdimage_set_sector(0);
  opera_3do_init(libopera_callback);
  video_init();
  process_opts();
  load_rom1();
  load_rom2();

  rv = set_pixel_format();
  if(rv < 0)
    return false;

  opera_nvram_init();
  opera_lr_nvram_load(g_GAME_NAME);

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
  opera_lr_nvram_save(g_GAME_NAME);

  opera_lr_dsp_destroy();
  opera_3do_destroy();

  retro_cdimage_close(&CDIMAGE);

  video_destroy();
}

void
retro_get_system_av_info(struct retro_system_av_info *info_)
{
  memset(info_,0,sizeof(*info_));

  info_->timing.fps            = opera_region_field_rate();
  info_->timing.sample_rate    = 44100;
  info_->geometry.base_width   = g_OPT_VIDEO_WIDTH;
  info_->geometry.base_height  = g_OPT_VIDEO_HEIGHT;
  info_->geometry.max_width    = (opera_region_max_width()  << 1);
  info_->geometry.max_height   = (opera_region_max_height() << 1);
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
      return opera_arm_ram_get();
    case RETRO_MEMORY_VIDEO_RAM:
      return opera_arm_vram_get();
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
      return opera_arm_ram_size();
    case RETRO_MEMORY_VIDEO_RAM:
      return opera_arm_vram_size();
    }

  return 0;
}

void
retro_init(void)
{
  struct retro_log_callback log;
  unsigned level                = 5;
  uint64_t serialization_quirks = RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION;

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE,&log))
    opera_lr_callbacks_set_log_printf(log.log);

  retro_environment_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL,&level);
  retro_environment_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS,&serialization_quirks);

  opera_cdrom_set_callbacks(cdimage_get_size,
                            cdimage_set_sector,
                            cdimage_read_sector);
  init_disk_control_interface();
}

void retro_deinit(void) {}

void
retro_reset(void)
{
  opera_lr_nvram_save(g_GAME_NAME);

  opera_lr_dsp_destroy();
  opera_3do_destroy();

  opera_3do_init(libopera_callback);
  video_init();
  process_opts();
  cdimage_set_sector(0);
  load_rom1();
  load_rom2();

  /* XXX: Is this really a frontend responsibility? */
  opera_nvram_init();
  opera_lr_nvram_load(g_GAME_NAME);
}

void
retro_run(void)
{
  bool updated = false;
  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,&updated) && updated)
    process_opts();

  lr_input_update(g_OPT_ACTIVE_DEVICES);

  opera_3do_process_frame();

  lr_input_crosshairs_draw(g_VIDEO_BUFFER,g_OPT_VIDEO_WIDTH,g_OPT_VIDEO_HEIGHT);

  opera_lr_dsp_upload();

  retro_video_refresh_cb(g_VIDEO_BUFFER,
                         g_OPT_VIDEO_WIDTH,
                         g_OPT_VIDEO_HEIGHT,
                         g_OPT_VIDEO_WIDTH << g_OPT_VIDEO_PITCH_SHIFT);
}
