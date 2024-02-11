/*
  ISC License

  Copyright (c) 2021, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "opera_lr_opts.h"

#include "opera_lr_callbacks.h"
#include "opera_lr_dsp.h"
#include "lr_input.h"

#include "libopera/hack_flags.h"
#include "libopera/opera_arm.h"
#include "libopera/opera_bios.h"
#include "libopera/opera_clock.h"
#include "libopera/opera_core.h"
#include "libopera/opera_madam.h"
#include "libopera/opera_mem.h"
#include "libopera/opera_region.h"
#include "libopera/opera_vdlp.h"

#include "file/file_path.h"
#include "retro_miscellaneous.h"
#include "streams/file_stream.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

opera_lr_opts_t g_OPTS = {0};

static
int
getvar(struct retro_variable *var_)
{
  return retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE,var_);
}

static
const
char*
getval(const char *key_)
{
  int rv;
  char key[64];
  struct retro_variable var;

  strncpy(key,"opera_",(sizeof(key)-1));
  strncat(key,key_,(sizeof(key)-1));
  var.key = key;
  var.value = NULL;
  rv = getvar(&var);
  if(rv && (var.value != NULL))
    return var.value;

  return NULL;
}

static
int
getval_as_int(const char *key_,
              const int   default_)
{
  const char *val;

  val = getval(key_);
  if(val == NULL)
    return default_;

  return atoi(val);
}

static
float
getval_as_float(const char  *key_,
                const float  default_)
{
  const char *val;

  val = getval(key_);
  if(val == NULL)
    return default_;

  return atof(val);
}

static
bool
getval_is_str(const char *key_,
              const char *str_,
              const bool  default_)
{
  const char *val;

  val = getval(key_);
  if(val == NULL)
    return default_;

  return (strcmp(val,str_) == 0);
}

static
bool
getval_is_enabled(const char *key_,
                  const bool  default_)
{
  return getval_is_str(key_,"enabled",default_);
}

static
void
opera_lr_opts_get_mem_cfg(opera_lr_opts_t *opts_)
{
  unsigned x;
  const char *val;

  opts_->mem_cfg = DRAM_VRAM_STOCK;

  val = getval("mem_capacity");
  if(val == NULL)
    return;

  x = 0;
  sscanf(val,"%x",&x);

  opts_->mem_cfg = (opera_mem_cfg_t)x;
}

static
void
opera_lr_opts_set_mem_cfg(opera_lr_opts_t const *opts_)
{
  if(g_OPTS.initialized_opera)
    return;

  opera_mem_init(opts_->mem_cfg);

  g_OPTS.mem_cfg = opts_->mem_cfg;
}

static
void
opera_lr_opts_get_video_buffer(opera_lr_opts_t *opts_)
{
  (void)opts_;
}

static
void
opera_lr_opts_set_video_buffer(opera_lr_opts_t const *opts_)
{
  uint32_t size;

  if(g_OPTS.initialized_opera)
    return;
  if(g_OPTS.video_buffer)
    return;

  /*
   * The 4x multiplication is for hires mode
   * 4 bytes per pixel will handle any format
   * Wastes some memory but simplifies things
   */
  size = (opera_region_max_width() * opera_region_max_height() * 4);

  g_OPTS.video_buffer = (uint32_t*)calloc(size,sizeof(uint32_t));

  opera_vdlp_set_video_buffer(g_OPTS.video_buffer);
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
void
opera_lr_opts_get_bios(opera_lr_opts_t *opts_)
{
  const char *val;
  const opera_bios_t *bios;

  opts_->bios = NULL;

  val = getval("bios");
  if(val == NULL)
    return;

  for(bios = opera_bios_begin(); bios != opera_bios_end(); bios++)
    {
      if(strcmp(bios->filename,val))
        continue;

      opts_->bios = bios;
      return;
    }
}

static
void
opera_lr_opts_set_bios(opera_lr_opts_t const *opts_)
{
  int64_t rv;

  if(g_OPTS.initialized_opera)
    return;

  opera_lr_opts_set_mem_cfg(opts_);

  if(opts_->bios == NULL)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,"[Opera]: no BIOS ROM found\n");
      return;
    }

  rv = read_file_from_system_directory(opts_->bios->filename,ROM1,ROM1_SIZE);
  if(rv < 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: unable to find or load BIOS ROM - %s\n",
                          opts_->bios->filename);
      return;
    }

  retro_log_printf_cb(RETRO_LOG_INFO,
                      "[Opera]: loaded BIOS ROM - %s\n",
                      opts_->bios->filename);

  opera_mem_rom1_byteswap32_if_le();

  g_OPTS.bios = opts_->bios;
}

static
void
opera_lr_opts_get_font(opera_lr_opts_t *opts_)
{
  const char *val;
  const opera_bios_t *font;

  opts_->font = NULL;

  val = getval("font");
  if(val == NULL)
    return;

  for(font = opera_bios_font_begin(); font != opera_bios_font_end(); font++)
    {
      if(strcmp(font->filename,val))
        continue;

      opts_->font = font;
      return;
    }
}

static
void
opera_lr_opts_set_font(opera_lr_opts_t const *opts_)
{
  int64_t rv;

  if(g_OPTS.initialized_opera)
    return;

  opera_lr_opts_set_mem_cfg(opts_);

  if(opts_->font == NULL)
    {
      opera_mem_rom2_clear();
      return;
    }

  rv = read_file_from_system_directory(opts_->font->filename,ROM2,ROM2_SIZE);
  if(rv < 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: unable to find or load FONT ROM - %s\n",
                          opts_->font->filename);
      return;
    }

  retro_log_printf_cb(RETRO_LOG_INFO,
                      "[Opera]: loaded FONT ROM - %s\n",
                      opts_->font->filename);

  opera_mem_rom2_byteswap32_if_le();

  g_OPTS.font = opts_->font;
}

static
void
opera_lr_opts_get_nvram_shared(opera_lr_opts_t *opts_)
{
  opts_->nvram_shared = getval_is_str("nvram_storage",
                                      "shared",
                                      true);
}

static
void
opera_lr_opts_set_nvram_shared(opera_lr_opts_t const *opts_)
{
  g_OPTS.nvram_shared = opts_->nvram_shared;
}

static
void
opera_lr_opts_get_nvram_version(opera_lr_opts_t *opts_)
{
  opts_->nvram_version = getval_as_int("nvram_version",0);
}

static
void
opera_lr_opts_set_nvram_version(opera_lr_opts_t const *opts_)
{
  g_OPTS.nvram_version = opts_->nvram_version;
}

static
void
opera_lr_opts_get_region(opera_lr_opts_t *opts_)
{
  const char *val;

  opts_->region = OPERA_REGION_NTSC;

  val = getval("region");
  if(val == NULL)
    return;

  if(!strcmp(val,"ntsc"))
    opts_->region = OPERA_REGION_NTSC;
  else if(!strcmp(val,"pal1"))
    opts_->region = OPERA_REGION_PAL1;
  else if(!strcmp(val,"pal2"))
    opts_->region = OPERA_REGION_PAL2;
}

static
void
opera_lr_opts_set_region(opera_lr_opts_t const *opts_)
{
  switch(opts_->region)
    {
    default:
    case OPERA_REGION_NTSC:
      opera_region_set_NTSC();
      break;
    case OPERA_REGION_PAL1:
      opera_region_set_PAL1();
      break;
    case OPERA_REGION_PAL2:
      opera_region_set_PAL2();
      break;
    }

  g_OPTS.region       = opts_->region;
  g_OPTS.video_width  = (opera_region_width()  << g_OPTS.high_resolution);
  g_OPTS.video_height = (opera_region_height() << g_OPTS.high_resolution);
}

static
void
opera_lr_opts_get_cpu_overclock(opera_lr_opts_t *opts_)
{
  opts_->cpu_overclock = getval_as_float("cpu_overclock",1.0);
}

static
void
opera_lr_opts_set_cpu_overclock(opera_lr_opts_t const *opts_)
{
  opera_clock_cpu_set_freq_mul(opts_->cpu_overclock);
  g_OPTS.cpu_overclock = opts_->cpu_overclock;
}

static
void
opera_lr_opts_get_vdlp_pixel_format(opera_lr_opts_t *opts_)
{
  const char *val;

  opts_->vdlp_pixel_format = VDLP_PIXEL_FORMAT_RGB565;
  opts_->video_pitch_shift = 1;

  val = getval("vdlp_pixel_format");
  if(val == NULL)
    return;

  if(!strcmp(val,"XRGB8888"))
    {
      opts_->vdlp_pixel_format = VDLP_PIXEL_FORMAT_XRGB8888;
      opts_->video_pitch_shift = 2;
    }
  else if(!strcmp(val,"RGB565"))
    {
      opts_->vdlp_pixel_format = VDLP_PIXEL_FORMAT_RGB565;
      opts_->video_pitch_shift = 1;
    }
  else if(!strcmp(val,"0RGB1555"))
    {
      opts_->vdlp_pixel_format = VDLP_PIXEL_FORMAT_0RGB1555;
      opts_->video_pitch_shift = 1;
    }
}

static
void
opera_lr_opts_set_vdlp_pixel_format(opera_lr_opts_t const *opts_)
{
  uint32_t flags;

  if(g_OPTS.initialized_libretro)
    return;

  opera_vdlp_set_pixel_format(opts_->vdlp_pixel_format);

  g_OPTS.vdlp_pixel_format = opts_->vdlp_pixel_format;
  g_OPTS.video_pitch_shift = opts_->video_pitch_shift;
}

static
void
opera_lr_opts_get_vdlp_bypass_clut(opera_lr_opts_t *opts_)
{
  opts_->vdlp_bypass_clut = getval_is_enabled("vdlp_bypass_clut",false);
}

static
void
opera_lr_opts_set_vdlp_bypass_clut(opera_lr_opts_t const *opts_)
{
  int rv;

  rv = opera_vdlp_set_bypass_clut(opts_->vdlp_bypass_clut);
  if(rv != 0)
    return;

  g_OPTS.vdlp_bypass_clut = opts_->vdlp_bypass_clut;
}

static
void
opera_lr_opts_get_high_resolution(opera_lr_opts_t *opts_)
{
  opts_->high_resolution = getval_is_enabled("high_resolution",false);
}

static
void
opera_lr_opts_set_high_resolution(opera_lr_opts_t const *opts_)
{
  opera_vdlp_set_hires(opts_->high_resolution);

  g_OPTS.high_resolution = opts_->high_resolution;

  HIRESMODE = g_OPTS.high_resolution;

  g_OPTS.video_width  = (opera_region_width()  << g_OPTS.high_resolution);
  g_OPTS.video_height = (opera_region_height() << g_OPTS.high_resolution);
}

static
void
opera_lr_opts_get_active_devices(opera_lr_opts_t *opts_)
{
  unsigned rv;

  opts_->active_devices = getval_as_int("active_devices",1);
  if(opts_->active_devices > LR_INPUT_MAX_DEVICES)
    opts_->active_devices = 1;
}

static
void
opera_lr_opts_set_active_devices(opera_lr_opts_t const *opts_)
{
  g_OPTS.active_devices = opts_->active_devices;
}

static
void
opera_lr_opts_get_hide_lightgun_crosshairs(opera_lr_opts_t *opts_)
{
  opts_->hide_lightgun_crosshairs = getval_is_enabled("hide_lightgun_crosshairs",false);
}

static
void
opera_lr_opts_set_hide_lightgun_crosshairs(opera_lr_opts_t const *opts_)
{
  g_OPTS.hide_lightgun_crosshairs = opts_->hide_lightgun_crosshairs;
}

static
void
opera_lr_opts_get_madam_matrix_engine(opera_lr_opts_t *opts_)
{
  const char *val;

  opts_->madam_matrix_engine = "hardware";

  val = getval("madam_matrix_engine");
  if(val == NULL)
    return;

  if(!strcmp(val,"software"))
    opts_->madam_matrix_engine = "software";
  else
    opts_->madam_matrix_engine = "hardware";
}

static
void
opera_lr_opts_set_madam_matrix_engine(opera_lr_opts_t const *opts_)
{
  if(g_OPTS.initialized_opera)
    return;

  if(!strcmp(opts_->madam_matrix_engine,"software"))
    opera_madam_me_mode_software();
  else
    opera_madam_me_mode_hardware();

  g_OPTS.madam_matrix_engine = opts_->madam_matrix_engine;
}

static
void
opera_lr_opts_get_kprint(opera_lr_opts_t *opts_)
{
  opts_->kprint = getval_is_enabled("kprint",false);
}

static
void
opera_lr_opts_set_kprint(opera_lr_opts_t const *opts_)
{
  if(opts_->kprint)
    opera_madam_kprint_enable();
  else
    opera_madam_kprint_disable();

  g_OPTS.kprint = opts_->kprint;
}

static
void
opera_lr_opts_get_dsp_threaded(opera_lr_opts_t *opts_)
{
  opts_->dsp_threaded = getval_is_enabled("dsp_threaded",false);
}

static
void
opera_lr_opts_set_dsp_threaded(opera_lr_opts_t const *opts_)
{
  opera_lr_dsp_init(opts_->dsp_threaded);
  g_OPTS.dsp_threaded = opts_->dsp_threaded;
}

static
void
opera_lr_opts_get_swi_hle(opera_lr_opts_t *opts_)
{
  opts_->swi_hle = getval_is_enabled("swi_hle",false);
}

static
void
opera_lr_opts_set_swi_hle(opera_lr_opts_t const *opts_)
{
  opera_arm_swi_hle_set(opts_->swi_hle);
  g_OPTS.swi_hle = opts_->swi_hle;
}

static
uint32_t
set_reset_bits(const char *key_,
               uint32_t    input_,
               uint32_t    bitmask_)
{
  return (getval_is_enabled(key_,false) ?
          (input_ |  bitmask_) :
          (input_ & ~bitmask_));
}

static
void
opera_lr_opts_get_hacks(opera_lr_opts_t *opts_)
{
  uint32_t rv;

  rv = 0;
  rv = set_reset_bits("hack_timing_1",rv,FIX_BIT_TIMING_1);
  rv = set_reset_bits("hack_timing_3",rv,FIX_BIT_TIMING_3);
  rv = set_reset_bits("hack_timing_5",rv,FIX_BIT_TIMING_5);
  rv = set_reset_bits("hack_timing_6",rv,FIX_BIT_TIMING_6);
  rv = set_reset_bits("hack_graphics_step_y",rv,FIX_BIT_GRAPHICS_STEP_Y);

  opts_->hack_flags = rv;
}

static
void
opera_lr_opts_set_hacks(opera_lr_opts_t const *opts_)
{
  FIXMODE           = opts_->hack_flags;
  g_OPTS.hack_flags = opts_->hack_flags;
}

static
void
opera_lr_opts_get(opera_lr_opts_t *opts_)
{
  opera_lr_opts_get_mem_cfg(opts_);
  opera_lr_opts_get_video_buffer(opts_);
  opera_lr_opts_get_vdlp_pixel_format(opts_);
  opera_lr_opts_get_bios(opts_);
  opera_lr_opts_get_font(opts_);
  opera_lr_opts_get_madam_matrix_engine(opts_);

  opera_lr_opts_get_active_devices(opts_);
  opera_lr_opts_get_cpu_overclock(opts_);
  opera_lr_opts_get_dsp_threaded(opts_);
  opera_lr_opts_get_hacks(opts_);
  opera_lr_opts_get_hide_lightgun_crosshairs(opts_);
  opera_lr_opts_get_high_resolution(opts_);
  opera_lr_opts_get_kprint(opts_);
  opera_lr_opts_get_nvram_shared(opts_);
  opera_lr_opts_get_nvram_version(opts_);
  opera_lr_opts_get_region(opts_);
  opera_lr_opts_get_swi_hle(opts_);
  opera_lr_opts_get_vdlp_bypass_clut(opts_);
}

static
void
opera_lr_opts_set(opera_lr_opts_t const *opts_)
{
  // Can only be set at start/restart
  opera_lr_opts_set_mem_cfg(opts_);
  opera_lr_opts_set_video_buffer(opts_);
  opera_lr_opts_set_vdlp_pixel_format(opts_);
  opera_lr_opts_set_bios(opts_);
  opera_lr_opts_set_font(opts_);
  opera_lr_opts_set_madam_matrix_engine(opts_);

  // Can be updated at any time
  opera_lr_opts_set_active_devices(opts_);
  opera_lr_opts_set_cpu_overclock(opts_);
  opera_lr_opts_set_dsp_threaded(opts_);
  opera_lr_opts_set_hacks(opts_);
  opera_lr_opts_set_hide_lightgun_crosshairs(opts_);
  opera_lr_opts_set_high_resolution(opts_);
  opera_lr_opts_set_kprint(opts_);
  opera_lr_opts_set_nvram_shared(opts_);
  opera_lr_opts_set_nvram_version(opts_);
  opera_lr_opts_set_region(opts_);
  opera_lr_opts_set_swi_hle(opts_);
  opera_lr_opts_set_vdlp_bypass_clut(opts_);

  g_OPTS.initialized_libretro = true;
  g_OPTS.initialized_opera    = true;
}

void
opera_lr_opts_process()
{
  opera_lr_opts_t opts = {0};

  opera_lr_opts_get(&opts);
  opera_lr_opts_set(&opts);
}

void
opera_lr_opts_reset()
{
  opera_lr_opts_t opts;

  opts = g_OPTS;

  if(g_OPTS.video_buffer)
    free(g_OPTS.video_buffer);

  opera_lr_dsp_destroy();

  opera_mem_destroy();

  memset(&g_OPTS,0,sizeof(g_OPTS));

  g_OPTS.initialized_libretro = opts.initialized_libretro;
  g_OPTS.vdlp_pixel_format    = opts.vdlp_pixel_format;
  g_OPTS.video_pitch_shift    = opts.video_pitch_shift;
}

void
opera_lr_opts_destroy()
{
  opera_lr_opts_reset();
}
