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

#include "opera_lr_callbacks.h"
#include "opera_lr_dsp.h"
#include "lr_input.h"

#include "libopera/hack_flags.h"
#include "libopera/opera_arm.h"
#include "libopera/opera_bios.h"
#include "libopera/opera_clock.h"
#include "libopera/opera_core.h"
#include "libopera/opera_madam.h"
#include "libopera/opera_region.h"
#include "libopera/opera_vdlp.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const opera_bios_t *g_OPT_BIOS = NULL;
const opera_bios_t *g_OPT_FONT = NULL;
uint32_t g_OPT_VIDEO_WIDTH       = 0;
uint32_t g_OPT_VIDEO_HEIGHT      = 0;
uint32_t g_OPT_VIDEO_PITCH_SHIFT = 0;
uint32_t g_OPT_VDLP_FLAGS        = 0;
uint32_t g_OPT_VDLP_PIXEL_FORMAT = 0;
uint32_t g_OPT_ACTIVE_DEVICES    = 0;

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

  strncpy(key,"4do_",(sizeof(key)-1));
  strncat(key,key_,(sizeof(key)-1));
  var.key = key;
  var.value = NULL;
  rv = getvar(&var);
  if(rv && (var.value != NULL))
    return var.value;

  strncpy(key,"opera_",(sizeof(key)-1));
  strncat(key,key_,(sizeof(key)-1));
  var.key = key;
  var.value = NULL;
  rv = getvar(&var);
  if(rv && (var.value != NULL))
    return var.value;

  return NULL;
}

bool
opera_lr_opts_is_enabled(const char *key_)
{
  const char *val;

  val = getval(key_);
  if(val == NULL)
    return false;

  return (strcmp(val,"enabled") == 0);
}

const
opera_bios_t*
opera_lr_opts_get_bios(void)
{
  const char *val;
  const opera_bios_t *bios;

  val = getval("bios");
  if(val == NULL)
    return NULL;

  for(bios = opera_bios_begin(); bios != opera_bios_end(); bios++)
    {
      if(strcmp(bios->name,val))
        continue;

      return bios;
    }

  return NULL;
}

const
opera_bios_t*
opera_lr_opts_get_font(void)
{
  const char *val;
  const opera_bios_t *font;

  val = getval("font");
  if(val == NULL)
    return NULL;

  for(font = opera_bios_font_begin(); font != opera_bios_font_end(); font++)
    {
      if(strcmp(font->name,val))
        continue;

      return font;
    }

  return NULL;
}

bool
opera_lr_opts_is_nvram_shared(void)
{
  const char *val;

  val = getval("nvram_storage");
  if(val == NULL)
    return true;

  return (strcmp(val,"shared") == 0);
}

uint8_t
opera_lr_opts_nvram_version(void)
{
  const char *val;

  val = getval("nvram_version");
  if(val == NULL)
    return 0;

  return atoi(val);
}

void
opera_lr_opts_process_bios(void)
{
  g_OPT_BIOS = opera_lr_opts_get_bios();
}

void
opera_lr_opts_process_font(void)
{
  g_OPT_FONT = opera_lr_opts_get_font();
}

void
opera_lr_opts_process_region(void)
{
  const char *val;

  val = getval("region");
  if(val == NULL)
    return;

  if(!strcmp(val,"ntsc"))
    opera_region_set_NTSC();
  else if(!strcmp(val,"pal1"))
    opera_region_set_PAL1();
  else if(!strcmp(val,"pal2"))
    opera_region_set_PAL2();
}

void
opera_lr_opts_process_cpu_overlock(void)
{
  float mul;
  const char *val;

  val = getval("cpu_overclock");
  if(val == NULL)
    return;

  mul = atof(val);
  opera_clock_cpu_set_freq_mul(mul);
}

vdlp_pixel_format_e
opera_lr_opts_get_vdlp_pixel_format(void)
{
  const char *val;

  val = getval("vdlp_pixel_format");
  if(val == NULL)
    return VDLP_PIXEL_FORMAT_XRGB8888;

  if(!strcmp(val,"XRGB8888"))
    return VDLP_PIXEL_FORMAT_XRGB8888;
  else if(!strcmp(val,"RGB565"))
    return VDLP_PIXEL_FORMAT_RGB565;
  else if(!strcmp(val,"0RGB1555"))
    return VDLP_PIXEL_FORMAT_0RGB1555;

  return VDLP_PIXEL_FORMAT_XRGB8888;
}

uint32_t
opera_lr_opts_get_vdlp_flags(void)
{
  uint32_t flags;

  flags = VDLP_FLAG_NONE;
  if(opera_lr_opts_is_enabled("high_resolution"))
    flags |= VDLP_FLAG_HIRES_CEL;
  if(opera_lr_opts_is_enabled("vdlp_bypass_clut"))
    flags |= VDLP_FLAG_CLUT_BYPASS;

  return flags;
}

void
opera_lr_opts_process_vdlp_flags(void)
{
  g_OPT_VDLP_FLAGS = opera_lr_opts_get_vdlp_flags();
}

void
opera_lr_opts_process_high_resolution(void)
{
  bool rv;

  rv = opera_lr_opts_is_enabled("high_resolution");
  if(rv)
    {
      HIRESMODE          = 1;
      g_OPT_VIDEO_WIDTH  = (opera_region_width()  << 1);
      g_OPT_VIDEO_HEIGHT = (opera_region_height() << 1);
    }
  else
    {
      HIRESMODE          = 0;
      g_OPT_VIDEO_WIDTH  = opera_region_width();
      g_OPT_VIDEO_HEIGHT = opera_region_height();
    }
}

void
opera_lr_opts_process_vdlp_pixel_format(void)
{
  static bool set = false;

  if(set == true)
    return;

  g_OPT_VDLP_PIXEL_FORMAT = opera_lr_opts_get_vdlp_pixel_format();
  switch(g_OPT_VDLP_PIXEL_FORMAT)
    {
    default:
    case VDLP_PIXEL_FORMAT_XRGB8888:
      g_OPT_VIDEO_PITCH_SHIFT = 2;
      break;
    case VDLP_PIXEL_FORMAT_0RGB1555:
    case VDLP_PIXEL_FORMAT_RGB565:
      g_OPT_VIDEO_PITCH_SHIFT = 1;
      break;
    }

  set = true;
}

void
opera_lr_opts_process_active_devices(void)
{
  const char *val;

  g_OPT_ACTIVE_DEVICES = 1;

  val = getval("active_devices");
  if(val)
    g_OPT_ACTIVE_DEVICES = atoi(val);

  if(g_OPT_ACTIVE_DEVICES > LR_INPUT_MAX_DEVICES)
    g_OPT_ACTIVE_DEVICES = 1;
}

void
opera_lr_opts_process_madam_matrix_engine(void)
{
  const char *val;

  val = getval("madam_matrix_engine");
  if(val == NULL)
    return;

  if(strcmp(val,"software") == 0)
    opera_madam_me_mode_software();
  else
    opera_madam_me_mode_hardware();
}

void
opera_lr_opts_process_debug(void)
{
  bool rv;

  rv = opera_lr_opts_is_enabled("kprint");
  if(rv)
    opera_madam_kprint_enable();
  else
    opera_madam_kprint_disable();
}

void
opera_lr_opts_process_dsp_threaded(void)
{
  bool rv;

  rv = opera_lr_opts_is_enabled("dsp_threaded");

  opera_lr_dsp_init(rv);
}

void
opera_lr_opts_process_swi_hle(void)
{
  bool rv;

  rv = getval("swi_hle");

  opera_arm_swi_hle_set(rv);
}

static
uint32_t
set_reset_bits(const char *key_,
               uint32_t    input_,
               uint32_t    bitmask_)
{
  return (opera_lr_opts_is_enabled(key_) ?
          (input_ |  bitmask_) :
          (input_ & ~bitmask_));
}

void
opera_lr_opts_process_hacks(void)
{
  FIXMODE = set_reset_bits("hack_timing_1",FIXMODE,FIX_BIT_TIMING_1);
  FIXMODE = set_reset_bits("hack_timing_3",FIXMODE,FIX_BIT_TIMING_3);
  FIXMODE = set_reset_bits("hack_timing_5",FIXMODE,FIX_BIT_TIMING_5);
  FIXMODE = set_reset_bits("hack_timing_6",FIXMODE,FIX_BIT_TIMING_6);
  FIXMODE = set_reset_bits("hack_graphics_step_y",FIXMODE,FIX_BIT_GRAPHICS_STEP_Y);
}

void
opera_lr_opts_process(void)
{
  opera_lr_opts_process_bios();
  opera_lr_opts_process_font();
  opera_lr_opts_process_region();
  opera_lr_opts_process_vdlp_pixel_format();
  opera_lr_opts_process_high_resolution();
  opera_lr_opts_process_vdlp_flags();
  opera_lr_opts_process_cpu_overlock();
  opera_lr_opts_process_dsp_threaded();
  opera_lr_opts_process_active_devices();
  opera_lr_opts_process_debug();
  opera_lr_opts_process_madam_matrix_engine();
  opera_lr_opts_process_swi_hle();
  opera_lr_opts_process_hacks();
}
