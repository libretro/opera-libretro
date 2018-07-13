#include "libfreedo/freedo_3do.h"
#include "libfreedo/freedo_arm.h"
#include "libfreedo/freedo_bios.h"
#include "libfreedo/freedo_cdrom.h"
#include "libfreedo/freedo_core.h"
#include "libfreedo/freedo_frame.h"
#include "libfreedo/freedo_madam.h"
#include "libfreedo/freedo_pbus.h"
#include "libfreedo/freedo_quarz.h"
#include "libfreedo/freedo_vdlp.h"
#include "libfreedo/hack_flags.h"

#include "lr_input.h"
#include "lr_input_crosshair.h"
#include "lr_input_descs.h"
#include "nvram.h"
#include "retro_callbacks.h"
#include "retro_cdimage.h"

#include <boolean.h>
#include <file/file_path.h>
#include <libretro.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CDIMAGE_SECTOR_SIZE 2048
#define SAMPLE_BUFFER_SIZE 512

static vdlp_frame_t *FRAME = NULL;

static cdimage_t  CDIMAGE;
static uint32_t   CDIMAGE_SECTOR;
static uint32_t  *VIDEO_BUFFER = NULL;
static uint32_t   VIDEO_WIDTH;
static uint32_t   VIDEO_HEIGHT;
static uint32_t   SAMPLE_IDX;
static int32_t    SAMPLE_BUFFER[SAMPLE_BUFFER_SIZE];
static uint32_t   ACTIVE_DEVICES;

static const freedo_bios_t *BIOS = NULL;
static const freedo_bios_t *FONT = NULL;

static
bool
file_exists(const char *path_)
{
  RFILE *fp;

  fp = filestream_open(path_,
                       RETRO_VFS_FILE_ACCESS_READ,
                       RETRO_VFS_FILE_ACCESS_HINT_NONE);

  if(fp == NULL)
    return false;

  filestream_close(fp);

  return true;
}

static
bool
file_exists_in_system_directory(const char *filename_)
{
  int rv;
  char fullpath[PATH_MAX_LENGTH];
  const char *system_path;

  system_path = NULL;
  rv = retro_environment_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY,&system_path);
  if((rv == 0) || (system_path == NULL))
    return false;

  fill_pathname_join(fullpath,system_path,filename_,PATH_MAX_LENGTH);

  return file_exists(fullpath);
}

static
void
create_bios_option_list(char *buf_)
{
  int rv;
  const freedo_bios_t *bios;

  strcpy(buf_,"BIOS (rom1); ");
  for(bios = freedo_bios_begin(); bios != freedo_bios_end(); bios++)
    {
      rv = file_exists_in_system_directory(bios->filename);
      if(rv)
        {
          strcat(buf_,bios->name);
          strcat(buf_,"|");
        }
    }

  rv = (strlen(buf_) - 1);
  if(buf_[rv] == '|')
    buf_[rv] = '\0';
  else
    strcat(buf_,"None Found");
}

static
void
create_font_option_list(char *buf_)
{
  int rv;
  const freedo_bios_t *font;

  strcpy(buf_,"Font (rom2); disabled|");
  for(font = freedo_bios_font_begin(); font != freedo_bios_font_end(); font++)
    {
      rv = file_exists_in_system_directory(font->filename);
      if(rv)
        {
          strcat(buf_,font->name);
          strcat(buf_,"|");
        }
    }

  rv = (strlen(buf_) - 1);
  buf_[rv] = '\0';
}

static
void
retro_environment_set_variables(void)
{
  char bios[1024];
  char font[1024];
  static struct retro_variable vars[] =
    {
      { "4do_bios", NULL },
      { "4do_font", NULL },
      { "4do_cpu_overclock",        "CPU overclock; "
                                    "1.0x (12.50Mhz)|"
                                    "1.1x (13.75Mhz)|"
                                    "1.2x (15.00Mhz)|"
                                    "1.5x (18.75Mhz)|"
                                    "1.6x (20.00Mhz)|"
                                    "1.8x (22.50Mhz)|"
                                    "2.0x (25.00Mhz)" },
      { "4do_high_resolution",      "High Resolution; disabled|enabled" },
      { "4do_nvram_storage",        "NVRAM Storage; per game|shared" },
      { "4do_active_devices",       "Active Devices; 1|2|3|4|5|6|7|8|0" },
      { "4do_hack_timing_1",        "Timing Hack 1 (Crash 'n Burn); disabled|enabled" },
      { "4do_hack_timing_3",        "Timing Hack 3 (Dinopark Tycoon); disabled|enabled" },
      { "4do_hack_timing_5",        "Timing Hack 5 (Microcosm); disabled|enabled" },
      { "4do_hack_timing_6",        "Timing Hack 6 (Alone in the Dark); disabled|enabled" },
      { "4do_hack_graphics_step_y", "Graphics Step Y Hack (Samurai Shodown); disabled|enabled" },
      { "4do_madam_matrix_engine",  "MADAM Matrix Engine; hardware|software" },
      { "4do_kprint",               "3DO debugging output (stderr); disabled|enabled" },
      { NULL, NULL },
    };

  const freedo_bios_t *b;

  vars[0].value = bios;
  vars[1].value = font;
  create_bios_option_list(bios);
  create_font_option_list(font);
  retro_environment_cb(RETRO_ENVIRONMENT_SET_VARIABLES,(void*)vars);
}

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
  retro_environment_set_variables();
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
  if(VIDEO_BUFFER == NULL)
    VIDEO_BUFFER = (uint32_t*)malloc(640 * 480 * sizeof(uint32_t));

  if(FRAME == NULL)
    FRAME = (vdlp_frame_t*)malloc(sizeof(vdlp_frame_t));

  memset(FRAME,0,sizeof(vdlp_frame_t));
  memset(VIDEO_BUFFER,0,(640 * 480 * sizeof(uint32_t)));
}

static
void
video_destroy(void)
{
  if(VIDEO_BUFFER != NULL)
    free(VIDEO_BUFFER);
  VIDEO_BUFFER = NULL;

  if(FRAME != NULL)
    free(FRAME);
  FRAME = NULL;
}

static
void
audio_reset_sample_buffer(void)
{
  SAMPLE_IDX = 0;
  memset(SAMPLE_BUFFER,0,(sizeof(int32_t) * SAMPLE_BUFFER_SIZE));
}

static
void
audio_push_sample(const int32_t sample_)
{
  SAMPLE_BUFFER[SAMPLE_IDX++] = sample_;
  if(SAMPLE_IDX >= SAMPLE_BUFFER_SIZE)
    {
      SAMPLE_IDX = 0;
      retro_audio_sample_batch_cb((int16_t*)SAMPLE_BUFFER,SAMPLE_BUFFER_SIZE);
    }
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
    case EXT_SWAPFRAME:
      freedo_frame_get_bitmap_xrgb_8888(FRAME,VIDEO_BUFFER,VIDEO_WIDTH,VIDEO_HEIGHT);
      return FRAME;
    case EXT_PUSH_SAMPLE:
      /* TODO: fix all this, not right */
      audio_push_sample((intptr_t)data_);
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

void
retro_get_system_av_info(struct retro_system_av_info *info_)
{
  memset(info_,0,sizeof(*info_));

  info_->timing.fps            = 60;
  info_->timing.sample_rate    = 44100;
  info_->geometry.base_width   = VIDEO_WIDTH;
  info_->geometry.base_height  = VIDEO_HEIGHT;
  info_->geometry.max_width    = 640;
  info_->geometry.max_height   = 480;
  info_->geometry.aspect_ratio = 4.0 / 3.0;
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
check_option_4do_bios(void)
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
check_option_4do_font(void)
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
check_option_4do_high_resolution(void)
{
  if(option_enabled("4do_high_resolution"))
    {
      HIRESMODE    = 1;
      VIDEO_WIDTH  = 640;
      VIDEO_HEIGHT = 480;
    }
  else
    {
      HIRESMODE    = 0;
      VIDEO_WIDTH  = 320;
      VIDEO_HEIGHT = 240;
    }
}

static
void
check_option_4do_cpu_overclock(void)
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

      freedo_quarz_cpu_set_freq_mul(mul);
    }
}

static
void
check_option_set_reset_bits(const char *key_,
                         int        *input_,
                         int         bitmask_)
{
  *input_ = (option_enabled(key_) ?
             (*input_ | bitmask_) :
             (*input_ & ~bitmask_));
}

static
bool
check_option_nvram_per_game(void)
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
check_option_nvram_shared(void)
{
  return !check_option_nvram_per_game();
}

static
void
check_option_4do_active_devices(void)
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
check_option_4do_madam_matrix_engine(void)
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
check_option_4do_kprint(void)
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
check_options(void)
{
  check_option_4do_bios();
  check_option_4do_font();
  check_option_4do_high_resolution();
  check_option_4do_cpu_overclock();
  check_option_4do_active_devices();
  check_option_set_reset_bits("4do_hack_timing_1",&FIXMODE,FIX_BIT_TIMING_1);
  check_option_set_reset_bits("4do_hack_timing_3",&FIXMODE,FIX_BIT_TIMING_3);
  check_option_set_reset_bits("4do_hack_timing_5",&FIXMODE,FIX_BIT_TIMING_5);
  check_option_set_reset_bits("4do_hack_timing_6",&FIXMODE,FIX_BIT_TIMING_6);
  check_option_set_reset_bits("4do_hack_graphics_step_y",&FIXMODE,FIX_BIT_GRAPHICS_STEP_Y);
  check_option_4do_kprint();
  check_option_4do_madam_matrix_engine();
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

bool
retro_load_game(const struct retro_game_info *info_)
{
  int rv;
  enum retro_pixel_format fmt;

  fmt = RETRO_PIXEL_FORMAT_XRGB8888;
  rv = retro_environment_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&fmt);
  if(rv == 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[4DO]: XRGB8888 is not supported.\n");
      return false;
    }

  cdimage_set_sector(0);
  audio_reset_sample_buffer();

  if(info_)
    {
      rv = retro_cdimage_open(info_->path,&CDIMAGE);
      if(rv == -1)
        {
          retro_log_printf_cb(RETRO_LOG_ERROR,
                              "[4DO]: failure opening image - %s\n",
                              info_->path);
          return false;
        }
    }

  check_options();
  video_init();
  freedo_3do_init(libfreedo_callback);
  load_rom1();
  load_rom2();

  /* XXX: Is this really a frontend responsibility? */
  nvram_init(freedo_arm_nvram_get());
  if(check_option_nvram_shared())
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
  if(check_option_nvram_shared())
    retro_nvram_save(freedo_arm_nvram_get());

  freedo_3do_destroy();

  retro_cdimage_close(&CDIMAGE);

  video_destroy();
}

unsigned
retro_get_region(void)
{
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
      if(check_option_nvram_shared())
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
      if(check_option_nvram_shared())
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
  if(check_option_nvram_shared())
    retro_nvram_save(freedo_arm_nvram_get());

  freedo_3do_destroy();

  check_options();
  video_init();
  cdimage_set_sector(0);
  audio_reset_sample_buffer();
  freedo_3do_init(libfreedo_callback);
  load_rom1();
  load_rom2();

  nvram_init(freedo_arm_nvram_get());
  if(check_option_nvram_shared())
    retro_nvram_load(freedo_arm_nvram_get());
}

void
retro_run(void)
{
  bool updated = false;
  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,&updated) && updated)
    check_options();

  lr_input_update(ACTIVE_DEVICES);

  freedo_3do_process_frame(FRAME);

  lr_input_crosshairs_draw(VIDEO_BUFFER,VIDEO_WIDTH,VIDEO_HEIGHT);

  retro_video_refresh_cb(VIDEO_BUFFER,VIDEO_WIDTH,VIDEO_HEIGHT,VIDEO_WIDTH << 2);
}
