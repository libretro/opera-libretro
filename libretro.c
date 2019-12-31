#include "libfreedo/freedo_3do.h"
#include "libfreedo/freedo_arm.h"
#include "libfreedo/freedo_bios.h"
#include "libfreedo/freedo_cdrom.h"
#include "libfreedo/freedo_clock.h"
#include "libfreedo/freedo_core.h"
#include "libfreedo/freedo_madam.h"
#include "libfreedo/freedo_pbus.h"
#include "libfreedo/freedo_region.h"
#include "libfreedo/freedo_vdlp.h"
#include "libfreedo/hack_flags.h"

#include "lr_dsp.h"
#include "lr_input.h"
#include "lr_input_crosshair.h"
#include "lr_input_descs.h"
#include "nvram.h"
#include "retro_callbacks.h"
#include "retro_cdimage.h"

#include <boolean.h>
#include <file/file_path.h>
#include <libretro.h>
#include <libretro_core_options.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CDIMAGE_SECTOR_SIZE 2048

static cdimage_t            CDIMAGE;
static uint32_t             CDIMAGE_SECTOR;
static uint32_t            *g_VIDEO_BUFFER;
static uint32_t             g_VIDEO_WIDTH;
static uint32_t             g_VIDEO_HEIGHT;
static uint32_t             g_VIDEO_PITCH_SHIFT;
static uint32_t             ACTIVE_DEVICES;
static int                  g_PIXEL_FORMAT_SET  = false;
static vdlp_pixel_format_e  g_VDLP_PIXEL_FORMAT = VDLP_PIXEL_FORMAT_XRGB8888;
static uint32_t             g_VDLP_FLAGS        = VDLP_FLAG_NONE;
static const freedo_bios_t *BIOS = NULL;
static const freedo_bios_t *FONT = NULL;

static
void
retro_environment_set_support_no_game(void)
{
  bool support_no_game;

  support_no_game = true;
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

void
retro_set_environment(retro_environment_t cb_)
{
  retro_set_environment_cb(cb_);

  retro_environment_set_controller_info();
  libretro_init_core_options();
  libretro_set_core_options();
  retro_environment_set_support_no_game();
}

void
retro_set_video_refresh(retro_video_refresh_t cb_)
{
  retro_set_video_refresh_cb(cb_);
}

void
retro_set_audio_sample(retro_audio_sample_t cb_)
{
  retro_set_audio_sample_cb(cb_);
}

void
retro_set_audio_sample_batch(retro_audio_sample_batch_t cb_)
{
  retro_set_audio_sample_batch_cb(cb_);
}

void
retro_set_input_poll(retro_input_poll_t cb_)
{
  retro_set_input_poll_cb(cb_);
}

void
retro_set_input_state(retro_input_state_t cb_)
{
  retro_set_input_state_cb(cb_);
}

static
void
video_init(void)
{
  uint32_t size;

  /* The 4x multiplication is for hires mode */
  size = (freedo_region_max_width() * freedo_region_max_height() * 4);
  if(g_VIDEO_BUFFER == NULL)
    g_VIDEO_BUFFER = (uint32_t*)calloc(size,sizeof(uint32_t));
}

static
void
video_destroy(void)
{
  if(g_VIDEO_BUFFER != NULL)
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
libfreedo_callback(int   cmd_,
                   void *data_)
{
  switch(cmd_)
    {
    case EXT_DSP_TRIGGER:
      lr_dsp_process();
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

  info_->library_name     = "4DO";
  info_->library_version  = "1.3.2.4" GIT_VERSION;
  info_->need_fullpath    = true;
  info_->valid_extensions = "iso|bin|chd|cue";
}

size_t
retro_serialize_size(void)
{
  return freedo_3do_state_size();
}

bool
retro_serialize(void   *data_,
                size_t  size_)
{
  if(size_ != freedo_3do_state_size())
    return false;

  freedo_3do_state_save(data_);

  return true;
}

bool
retro_unserialize(const void *data_,
                  size_t      size_)
{
  if(size_ != freedo_3do_state_size())
    return false;

  freedo_3do_state_load(data_);

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
  (void)index_;
  (void)enabled_;
  (void)code_;
}

static
bool
option_enabled(const char *key_)
{
  int rv;
  struct retro_variable var;

  var.key   = key_;
  var.value = NULL;
  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    return (strcmp(var.value,"enabled") == 0);

  return false;
}

static
void
chkopt_set_reset_bits(const char *key_,
                            uint32_t   *input_,
                            uint32_t    bitmask_)
{
  *input_ = (option_enabled(key_) ?
             (*input_ | bitmask_) :
             (*input_ & ~bitmask_));
}

static
void
chkopt_4do_bios(void)
{
  int rv;
  const freedo_bios_t *bios;
  struct retro_variable var;

  var.key   = "4do_bios";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if((rv == 0) || (var.value == NULL))
    {
      BIOS = freedo_bios_begin();
      return;
    }

  for(bios = freedo_bios_begin(); bios != freedo_bios_end(); bios++)
    {
      if(strcmp(bios->name,var.value))
        continue;

      BIOS = bios;
      return;
    }

  BIOS = freedo_bios_end();
}

static
void
chkopt_4do_font(void)
{
  int rv;
  const freedo_bios_t *font;
  struct retro_variable var;

  var.key   = "4do_font";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if((rv == 0) || (var.value == NULL))
    {
      FONT = freedo_bios_font_end();
      return;
    }

  for(font = freedo_bios_font_begin(); font != freedo_bios_font_end(); font++)
    {
      if(strcmp(font->name,var.value))
        continue;

      FONT = font;
      return;
    }

  FONT = freedo_bios_font_end();
}

static
void
chkopt_4do_region(void)
{
  int rv;
  struct retro_variable var;

  var.key   = "4do_region";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    {
      if(!strcmp(var.value,"ntsc"))
        freedo_region_set_NTSC();
      else if(!strcmp(var.value,"pal1"))
        freedo_region_set_PAL1();
      else if(!strcmp(var.value,"pal2"))
        freedo_region_set_PAL2();
    }
}

static
void
chkopt_4do_high_resolution(void)
{
  if(option_enabled("4do_high_resolution"))
    {
      HIRESMODE       = 1;
      g_VIDEO_WIDTH   = (freedo_region_width()  << 1);
      g_VIDEO_HEIGHT  = (freedo_region_height() << 1);
      g_VDLP_FLAGS   |= VDLP_FLAG_HIRES_CEL;
    }
  else
    {
      HIRESMODE       = 0;
      g_VIDEO_WIDTH   = freedo_region_width();
      g_VIDEO_HEIGHT  = freedo_region_height();
      g_VDLP_FLAGS   &= ~VDLP_FLAG_HIRES_CEL;
    }
}

static
void
chkopt_4do_cpu_overclock(void)
{
  int rv;
  struct retro_variable var;

  var.key   = "4do_cpu_overclock";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    {
      float mul;

      mul = atof(var.value);

      freedo_clock_cpu_set_freq_mul(mul);
    }
}

static
void
chkopt_4do_vdlp_pixel_format(void)
{
  int rv;
  struct retro_variable var;

  if(g_PIXEL_FORMAT_SET)
    return;

  var.key   = "4do_vdlp_pixel_format";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    {
      if(!strcmp(var.value,"XRGB8888"))
        g_VDLP_PIXEL_FORMAT = VDLP_PIXEL_FORMAT_XRGB8888;
      else if(!strcmp(var.value,"RGB565"))
        g_VDLP_PIXEL_FORMAT = VDLP_PIXEL_FORMAT_RGB565;
      else if(!strcmp(var.value,"0RGB1555"))
        g_VDLP_PIXEL_FORMAT = VDLP_PIXEL_FORMAT_0RGB1555;
    }

  g_PIXEL_FORMAT_SET = true;
}

static
void
chkopt_4do_vdlp_bypass_clut(void)
{
  chkopt_set_reset_bits("4do_vdlp_bypass_clut",
                        &g_VDLP_FLAGS,
                        VDLP_FLAG_CLUT_BYPASS);
}

static
bool
chkopt_nvram_per_game(void)
{
  int rv;
  struct retro_variable var;

  var.key   = "4do_nvram_storage";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    {
      if(strcmp(var.value,"per game"))
        return false;
    }

  return true;
}

static
bool
chkopt_nvram_shared(void)
{
  return !chkopt_nvram_per_game();
}

static
void
chkopt_4do_active_devices(void)
{
  int rv;
  struct retro_variable var;

  ACTIVE_DEVICES = 0;

  var.key = "4do_active_devices";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    ACTIVE_DEVICES = atoi(var.value);

  if(ACTIVE_DEVICES > LR_INPUT_MAX_DEVICES)
    ACTIVE_DEVICES = 1;
}

static
void
chkopt_4do_madam_matrix_engine(void)
{
  int rv;
  struct retro_variable var;

  var.key   = "4do_madam_matrix_engine";
  var.value = NULL;

  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,&var);
  if(rv && var.value)
    {
      if(!strcmp(var.value,"software"))
        freedo_madam_me_mode_software();
      else
        freedo_madam_me_mode_hardware();
    }
}

static
void
chkopt_4do_kprint(void)
{
  int rv;

  rv = option_enabled("4do_kprint");

  if(rv)
    freedo_madam_kprint_enable();
  else
    freedo_madam_kprint_disable();
}

static
void
chkopt_4do_dsp_threaded(void)
{
  bool rv;

  rv = option_enabled("4do_dsp_threaded");

  lr_dsp_init(rv);
}

static
void
chkopt_4do_swi_hle(void)
{
  bool rv;

  rv = option_enabled("4do_swi_hle");

  freedo_arm_swi_hle_set(rv);
}

static
void
chkopts(void)
{
  chkopt_4do_bios();
  chkopt_4do_font();
  chkopt_4do_region();
  chkopt_4do_vdlp_pixel_format();
  chkopt_4do_vdlp_bypass_clut();
  chkopt_4do_high_resolution();
  chkopt_4do_cpu_overclock();
  chkopt_4do_dsp_threaded();
  chkopt_4do_active_devices();
  chkopt_set_reset_bits("4do_hack_timing_1",&FIXMODE,FIX_BIT_TIMING_1);
  chkopt_set_reset_bits("4do_hack_timing_3",&FIXMODE,FIX_BIT_TIMING_3);
  chkopt_set_reset_bits("4do_hack_timing_5",&FIXMODE,FIX_BIT_TIMING_5);
  chkopt_set_reset_bits("4do_hack_timing_6",&FIXMODE,FIX_BIT_TIMING_6);
  chkopt_set_reset_bits("4do_hack_graphics_step_y",&FIXMODE,FIX_BIT_GRAPHICS_STEP_Y);
  chkopt_4do_kprint();
  chkopt_4do_madam_matrix_engine();
  chkopt_4do_swi_hle();

  freedo_vdlp_configure(g_VIDEO_BUFFER,g_VDLP_PIXEL_FORMAT,g_VDLP_FLAGS);
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
  char fullpath[PATH_MAX_LENGTH];
  const char *system_path;

  system_path = NULL;
  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&system_path);
  if((rv == 0) || (system_path == NULL))
    return -1;

  fill_pathname_join(fullpath,system_path,filename_,PATH_MAX_LENGTH);

  file = filestream_open(fullpath,
                         RETRO_VFS_FILE_ACCESS_READ,
                         RETRO_VFS_FILE_ACCESS_HINT_NONE);
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

  if((BIOS == NULL) || (BIOS == freedo_bios_end()))
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,"[4DO]: no BIOS ROM found\n");
      return -1;
    }

  rom  = freedo_arm_rom1_get();
  size = freedo_arm_rom1_size();

  rv = read_file_from_system_directory(BIOS->filename,rom,size);
  if(rv < 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[4DO]: unable to find or load BIOS ROM - %s\n",
                          BIOS->filename);
      return -1;
    }

  freedo_arm_rom1_byteswap_if_necessary();

  return 0;
}

static
int
load_rom2(void)
{
  uint8_t *rom;
  int64_t  size;
  int64_t  rv;

  rom  = freedo_arm_rom2_get();
  size = freedo_arm_rom2_size();

  if((FONT == NULL) || (FONT == freedo_bios_font_end()))
    {
      memset(rom,0,size);
      return 0;
    }

  rv = read_file_from_system_directory(FONT->filename,rom,size);
  if(rv < 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[4DO]: unable to find or load FONT ROM - %s\n",
                          BIOS->filename);
      return -1;
    }

  freedo_arm_rom2_byteswap_if_necessary();

  return 0;
}

enum retro_pixel_format
vdlp_pixel_format_to_libretro(vdlp_pixel_format_e pf_)
{
  switch(pf_)
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

  fmt = vdlp_pixel_format_to_libretro(g_VDLP_PIXEL_FORMAT);
  rv = retro_environment_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&fmt);
  if(rv == 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[4DO]: pixel format is not supported.\n");
      return -1;
    }

  switch(fmt)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888:
      g_VIDEO_PITCH_SHIFT = 2;
      break;
    default:
    case RETRO_PIXEL_FORMAT_RGB565:
    case RETRO_PIXEL_FORMAT_0RGB1555:
      g_VIDEO_PITCH_SHIFT = 1;
      break;
    }

  return 0;
}

static
int
print_cdimage_open_fail(const char *path_)
{
  retro_log_printf_cb(RETRO_LOG_ERROR,
                      "[4DO]: failure opening image - %s\n",
                      path_);
  return -1;
}

static
int
open_cdimage_if_needed(const struct retro_game_info *info_)
{
  int rv;

  if(info_ == NULL)
    return 0;

  rv = retro_cdimage_open(info_->path,&CDIMAGE);
  if(rv == -1)
    return print_cdimage_open_fail(info_->path);

  return 0;
}

bool
retro_load_game(const struct retro_game_info *info_)
{
  int rv;

  rv = open_cdimage_if_needed(info_);
  if(rv == -1)
    return false;

  cdimage_set_sector(0);
  freedo_3do_init(libfreedo_callback);
  video_init();
  chkopts();
  load_rom1();
  load_rom2();

  rv = set_pixel_format();
  if(rv == -1)
    return false;

  nvram_init(freedo_arm_nvram_get());
  if(chkopt_nvram_shared())
    retro_nvram_load(freedo_arm_nvram_get());

  return true;
}

bool
retro_load_game_special(unsigned                      game_type_,
                        const struct retro_game_info *info_,
                        size_t                        num_info_)
{
  (void)game_type_;
  (void)info_;
  (void)num_info_;

  return false;
}

void
retro_unload_game(void)
{
  if(chkopt_nvram_shared())
    retro_nvram_save(freedo_arm_nvram_get());

  lr_dsp_destroy();
  freedo_3do_destroy();

  retro_cdimage_close(&CDIMAGE);

  video_destroy();
}

void
retro_get_system_av_info(struct retro_system_av_info *info_)
{
  memset(info_,0,sizeof(*info_));

  info_->timing.fps            = freedo_region_field_rate();
  info_->timing.sample_rate    = 44100;
  info_->geometry.base_width   = g_VIDEO_WIDTH;
  info_->geometry.base_height  = g_VIDEO_HEIGHT;
  info_->geometry.max_width    = (freedo_region_max_width()  << 1);
  info_->geometry.max_height   = (freedo_region_max_height() << 1);
  info_->geometry.aspect_ratio = 4.0 / 3.0;
}

unsigned
retro_get_region(void)
{
  switch(freedo_region_get())
    {
    case FREEDO_REGION_PAL1:
    case FREEDO_REGION_PAL2:
      return RETRO_REGION_PAL;
    case FREEDO_REGION_NTSC:
    default:
      return RETRO_REGION_NTSC;
    }
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
      if(chkopt_nvram_shared())
        return NULL;
      return freedo_arm_nvram_get();
    case RETRO_MEMORY_SYSTEM_RAM:
      return freedo_arm_ram_get();
    case RETRO_MEMORY_VIDEO_RAM:
      return freedo_arm_vram_get();
    }

  return NULL;
}

size_t
retro_get_memory_size(unsigned id_)
{
  switch(id_)
    {
    case RETRO_MEMORY_SAVE_RAM:
      if(chkopt_nvram_shared())
        return 0;
      return freedo_arm_nvram_size();
    case RETRO_MEMORY_SYSTEM_RAM:
      return freedo_arm_ram_size();
    case RETRO_MEMORY_VIDEO_RAM:
      return freedo_arm_vram_size();
    }

  return 0;
}

void
retro_init(void)
{
  unsigned level;
  uint64_t serialization_quirks;
  struct retro_log_callback log;

  level = 5;
  serialization_quirks = RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION;

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE,&log))
    retro_set_log_printf_cb(log.log);

  retro_environment_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL,&level);
  retro_environment_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS,&serialization_quirks);

  freedo_cdrom_set_callbacks(cdimage_get_size,
                             cdimage_set_sector,
                             cdimage_read_sector);
}

void
retro_deinit(void)
{

}

void
retro_reset(void)
{
  if(chkopt_nvram_shared())
    retro_nvram_save(freedo_arm_nvram_get());

  lr_dsp_destroy();
  freedo_3do_destroy();

  freedo_3do_init(libfreedo_callback);
  video_init();
  chkopts();
  cdimage_set_sector(0);
  load_rom1();
  load_rom2();

  /* XXX: Is this really a frontend responsibility? */
  nvram_init(freedo_arm_nvram_get());
  if(chkopt_nvram_shared())
    retro_nvram_load(freedo_arm_nvram_get());
}

void
retro_run(void)
{
  bool updated = false;
  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,&updated) && updated)
    chkopts();

  lr_input_update(ACTIVE_DEVICES);

  freedo_3do_process_frame();

  lr_input_crosshairs_draw(g_VIDEO_BUFFER,g_VIDEO_WIDTH,g_VIDEO_HEIGHT);

  lr_dsp_upload();

  retro_video_refresh_cb(g_VIDEO_BUFFER,
                         g_VIDEO_WIDTH,
                         g_VIDEO_HEIGHT,
                         g_VIDEO_WIDTH << g_VIDEO_PITCH_SHIFT);
}
