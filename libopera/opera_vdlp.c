#include "hack_flags.h"
#include "inline.h"

#include "opera_arm.h"
#include "opera_core.h"
#include "opera_region.h"
#include "opera_vdl.h"
#include "opera_vdlp.h"
#include "opera_vdlp_i.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
  VDLP options TODO
  - interpolation
  - add pseudo random 3bit pattern for second clut bypass mode
*/

static vdlp_t   g_VDLP          = {0};
static uint8_t *g_VRAM          = NULL;
static void    *g_BUF           = NULL;
static void    *g_CURBUF        = NULL;
static void (*g_RENDERER)(void) = NULL;

static const uint32_t PIXELS_PER_LINE_MODULO[8] =
  {320, 384, 512, 640, 1024, 320, 320, 320};

static
INLINE
uint32_t
vram_read32(const uint32_t addr_)
{
  return *(uint32_t*)&g_VRAM[addr_ & 0x000FFFFF];
}

static
INLINE
void
vram_write32(const uint32_t addr_,
             const uint32_t val_)
{
  *((uint32_t*)&g_VRAM[addr_]) = val_;
}

static
INLINE
uint32_t
vdl_read(const uint32_t off_)
{
  return vram_read32(g_VDLP.curr_vdl + (off_ << 2));
}

static
void
vdl_set_clut(const vdl_ctrl_word_u cmd_)
{
  switch(cmd_.cvw.rgb_enable)
    {
    case 0x0:
      g_VDLP.clut_r[cmd_.cvw.addr] = cmd_.cvw.r;
      g_VDLP.clut_b[cmd_.cvw.addr] = cmd_.cvw.b;
      g_VDLP.clut_g[cmd_.cvw.addr] = cmd_.cvw.g;
      break;
    case 0x3:
      g_VDLP.clut_r[cmd_.cvw.addr] = cmd_.cvw.r;
      break;
    case 0x1:
      g_VDLP.clut_b[cmd_.cvw.addr] = cmd_.cvw.b;
      break;
    case 0x2:
      g_VDLP.clut_g[cmd_.cvw.addr] = cmd_.cvw.g;
      break;
    }
}

static
void
vdlp_process_optional_cmds(const int ctrl_word_cnt_)
{
  int i;
  int colors_only;
  vdl_ctrl_word_u cmd;

  colors_only = 0;
  for(i = 0; i < ctrl_word_cnt_; i++)
    {
      cmd.raw = vdl_read(i);
      switch((cmd.raw & 0xE0000000) >> 29)
        {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
          vdl_set_clut(cmd);
          break;
        case 0x4:
        case 0x5:
          /*
            Page 17 of US Patent 5,502,462

            If bit 31 of an optional color/control word is one, and if
            bit 30 is zero (10X), then the word contains control
            information for an audio/video output circuit 105 (not
            detailed herein) of the system. The audio/video processor
            circuitry receives this word over the S-bus 123, and
            forwards in to the audio/video output circuitry for
            processing.
          */
          break;
        case 0x6:
          if(colors_only)
            continue;
          g_VDLP.disp_ctrl.raw = cmd.raw;
          colors_only = cmd.dcw.colors_only;
          break;
        case 0x7:
          g_VDLP.bg_color.raw = cmd.raw;
          break;
        }
    }
}

static
void
vdlp_process_vdl_entry(void)
{
  uint32_t entry;
  uint32_t next_entry;
  clut_dma_ctrl_word_s *cdcw = &g_VDLP.clut_ctrl.cdcw;

  entry = vram_read32(g_VDLP.curr_vdl);
  if(!entry)
    return;

  g_VDLP.clut_ctrl.raw = entry;

  if(cdcw->curr_fba_override)
    g_VDLP.curr_bmp = vdl_read(1);

  if(cdcw->prev_fba_override)
    g_VDLP.prev_bmp = vdl_read(2);

  next_entry = vdl_read(3);
  if(cdcw->next_vdl_addr_rel)
    next_entry += (g_VDLP.curr_vdl + (4 * sizeof(uint32_t)));

  g_VDLP.curr_vdl += (4 * sizeof(uint32_t));

  vdlp_process_optional_cmds(cdcw->ctrl_word_cnt);

  g_VDLP.curr_vdl = next_entry;
  g_VDLP.line_cnt = cdcw->persist_len;
}

static
void
vdlp_render_line_black(const uint32_t width_,
                       const uint32_t bytes_per_pixel_)
{
  uint8_t *dst;
  uint32_t len;

  dst = g_CURBUF;
  len = (width_ * bytes_per_pixel_);

  memset(dst,0,len);

  g_CURBUF = (dst + len);
}

static
void
vdlp_render_line_black_hires(const uint32_t width_,
                             const uint32_t bytes_per_pixel_)
{
  uint8_t *dst;
  uint32_t len;

  dst = g_CURBUF;
  len = (width_ * bytes_per_pixel_ * 2 * 2);

  memset(dst,0,len);

  g_CURBUF = (dst + len);
}

static
uint16_t
fixed_clut_to_0RGB1555(const uint16_t p_)
{
  return (p_ & 0x7FFF);
}

static
uint16_t
user_clut_to_0RGB1555(const uint16_t p_)
{
  return (((g_VDLP.clut_r[(p_ >> 0xA) & 0x1F] >> 3) << 0xA) |
          ((g_VDLP.clut_g[(p_ >> 0x5) & 0x1F] >> 3) << 0x5) |
          ((g_VDLP.clut_b[(p_ >> 0x0) & 0x1F] >> 3) << 0x0));
}

static
uint16_t
background_to_0RGB1555(void)
{
  return (((g_VDLP.bg_color.bvw.r >> 3) << 0xA) |
          ((g_VDLP.bg_color.bvw.g >> 3) << 0x5) |
          ((g_VDLP.bg_color.bvw.b >> 3) << 0x0));
}

static
uint16_t
vdlp_render_pixel_0RGB1555(const uint16_t p_)
{
  if(p_ == 0)
    return background_to_0RGB1555();

  return user_clut_to_0RGB1555(p_);
}

static
uint16_t
vdlp_render_pixel_0RGB1555_bypass_clut(const uint16_t p_)
{
  if(p_ == 0)
    return background_to_0RGB1555();

  if(p_ & 0x8000)
    return fixed_clut_to_0RGB1555(p_);

  return user_clut_to_0RGB1555(p_);
}

static void vdlp_render_line_0RGB1555(void)
{
  int x;
  uint32_t *src;
  uint16_t *dst;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black(width,sizeof(uint16_t));
    return;
  }

  dst = g_CURBUF;
  src = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  if(!g_VDLP.disp_ctrl.dcw.clut_bypass)
    {
      for(x = 0; x < width; x++)
        dst[x] = vdlp_render_pixel_0RGB1555(*(uint16_t*)&src[x]);
    }
  else
    {
      for(x = 0; x < width; x++)
        dst[x] = vdlp_render_pixel_0RGB1555_bypass_clut(*(uint16_t*)&src[x]);
    }

  g_CURBUF = (dst + width);
}

static void vdlp_render_line_0RGB1555_bypass_clut(void)
{
  int x;
  uint32_t *src;
  uint16_t *dst;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black(width,sizeof(uint16_t));
    return;
  }

  dst = g_CURBUF;
  src = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  for(x = 0; x < width; x++)
    dst[x] = fixed_clut_to_0RGB1555(*(uint16_t*)&src[x]);

  g_CURBUF = (dst + width);
}

static void vdlp_render_line_0RGB1555_hires(void)
{
  int x;
  uint16_t *dst0;
  uint16_t *dst1;
  uint32_t *src0;
  uint32_t *src1;
  uint32_t *src2;
  uint32_t *src3;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black(width,sizeof(uint16_t));
    return;
  }

  dst0 = g_CURBUF;
  dst1 = (dst0 + (width << 1));
  src0 = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  src1 = (src0 + ((1024 * 1024) / sizeof(uint32_t)));
  src2 = (src1 + ((1024 * 1024) / sizeof(uint32_t)));
  src3 = (src2 + ((1024 * 1024) / sizeof(uint32_t)));
  if(!g_VDLP.disp_ctrl.dcw.clut_bypass)
    {
      for(x = 0; x < width; x++)
        {
          *dst0++ = vdlp_render_pixel_0RGB1555(*(uint16_t*)&src0[x]);
          *dst0++ = vdlp_render_pixel_0RGB1555(*(uint16_t*)&src1[x]);
          *dst1++ = vdlp_render_pixel_0RGB1555(*(uint16_t*)&src2[x]);
          *dst1++ = vdlp_render_pixel_0RGB1555(*(uint16_t*)&src3[x]);
        }
    }
  else
    {
      for(x = 0; x < width; x++)
        {
          *dst0++ = vdlp_render_pixel_0RGB1555_bypass_clut(*(uint16_t*)&src0[x]);
          *dst0++ = vdlp_render_pixel_0RGB1555_bypass_clut(*(uint16_t*)&src1[x]);
          *dst1++ = vdlp_render_pixel_0RGB1555_bypass_clut(*(uint16_t*)&src2[x]);
          *dst1++ = vdlp_render_pixel_0RGB1555_bypass_clut(*(uint16_t*)&src3[x]);
        }
    }

  g_CURBUF = dst1;
}

static void vdlp_render_line_0RGB1555_hires_bypass_clut(void)
{
  int x;
  uint16_t *dst0;
  uint16_t *dst1;
  uint32_t *src0;
  uint32_t *src1;
  uint32_t *src2;
  uint32_t *src3;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black(width,sizeof(uint16_t));
    return;
  }

  dst0 = g_CURBUF;
  dst1 = (dst0 + (width << 1));
  src0 = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  src1 = (src0 + ((1024 * 1024) / sizeof(uint32_t)));
  src2 = (src1 + ((1024 * 1024) / sizeof(uint32_t)));
  src3 = (src2 + ((1024 * 1024) / sizeof(uint32_t)));
  for(x = 0; x < width; x++)
    {
      *dst0++ = fixed_clut_to_0RGB1555(*(uint16_t*)&src0[x]);
      *dst0++ = fixed_clut_to_0RGB1555(*(uint16_t*)&src1[x]);
      *dst1++ = fixed_clut_to_0RGB1555(*(uint16_t*)&src2[x]);
      *dst1++ = fixed_clut_to_0RGB1555(*(uint16_t*)&src3[x]);
    }

  g_CURBUF = dst1;
}

static
uint16_t
fixed_clut_to_RGB565(const uint16_t p_)
{
  return (((p_ & 0x7FE0) << 1) | (p_ & 0x001F));
}

static
uint16_t
user_clut_to_RGB565(const uint16_t p_)
{
  return (((g_VDLP.clut_r[(p_ >> 0xA) & 0x1F] >> 3) << 0xB) |
          ((g_VDLP.clut_g[(p_ >> 0x5) & 0x1F] >> 2) << 0x5) |
          ((g_VDLP.clut_b[(p_ >> 0x0) & 0x1F] >> 3) << 0x0));
}

static
uint16_t
background_to_RGB565(void)
{
  return (((g_VDLP.bg_color.bvw.r >> 3) << 0xB) |
          ((g_VDLP.bg_color.bvw.g >> 2) << 0x5) |
          ((g_VDLP.bg_color.bvw.b >> 3) << 0x0));
}

static
uint16_t
vdlp_render_pixel_RGB565(const uint16_t p_)
{
  if(p_ == 0)
    return background_to_RGB565();

  return user_clut_to_RGB565(p_);
}

static
uint16_t
vdlp_render_pixel_RGB565_bypass_clut(const uint16_t p_)
{
  if(p_ == 0)
    return background_to_RGB565();

  if(p_ & 0x8000)
    return fixed_clut_to_RGB565(p_);

  return user_clut_to_RGB565(p_);
}

static void vdlp_render_line_RGB565(void)
{
  int x;
  uint32_t *src;
  uint16_t *dst;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black(width,sizeof(uint16_t));
    return;
  }

  dst = g_CURBUF;
  src = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  if(!g_VDLP.disp_ctrl.dcw.clut_bypass)
    {
      for(x = 0; x < width; x++)
        dst[x] = vdlp_render_pixel_RGB565(*(uint16_t*)&src[x]);
    }
  else
    {
      for(x = 0; x < width; x++)
        dst[x] = vdlp_render_pixel_RGB565_bypass_clut(*(uint16_t*)&src[x]);
    }

  g_CURBUF = (dst + width);
}

static void vdlp_render_line_RGB565_bypass_clut(void)
{
  int x;
  uint32_t *src;
  uint16_t *dst;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black(width,sizeof(uint16_t));
    return;
  }

  dst = g_CURBUF;
  src = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  for(x = 0; x < width; x++)
    dst[x] = fixed_clut_to_RGB565(*(uint16_t*)&src[x]);

  g_CURBUF = (dst + width);
}

static void vdlp_render_line_RGB565_hires(void)
{
  int x;
  uint16_t *dst0;
  uint16_t *dst1;
  uint32_t *src0;
  uint32_t *src1;
  uint32_t *src2;
  uint32_t *src3;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black_hires(width,sizeof(uint16_t));
    return;
  }

  dst0 = g_CURBUF;
  dst1 = (dst0 + (width << 1));
  src0 = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  src1 = (src0 + ((1024 * 1024) / sizeof(uint32_t)));
  src2 = (src1 + ((1024 * 1024) / sizeof(uint32_t)));
  src3 = (src2 + ((1024 * 1024) / sizeof(uint32_t)));
  if(!g_VDLP.disp_ctrl.dcw.clut_bypass)
    {
      for(x = 0; x < width; x++)
        {
          *dst0++ = vdlp_render_pixel_RGB565(*(uint16_t*)&src0[x]);
          *dst0++ = vdlp_render_pixel_RGB565(*(uint16_t*)&src1[x]);
          *dst1++ = vdlp_render_pixel_RGB565(*(uint16_t*)&src2[x]);
          *dst1++ = vdlp_render_pixel_RGB565(*(uint16_t*)&src3[x]);
        }
    }
  else
    {
      for(x = 0; x < width; x++)
        {
          *dst0++ = vdlp_render_pixel_RGB565_bypass_clut(*(uint16_t*)&src0[x]);
          *dst0++ = vdlp_render_pixel_RGB565_bypass_clut(*(uint16_t*)&src1[x]);
          *dst1++ = vdlp_render_pixel_RGB565_bypass_clut(*(uint16_t*)&src2[x]);
          *dst1++ = vdlp_render_pixel_RGB565_bypass_clut(*(uint16_t*)&src3[x]);
        }
    }

  g_CURBUF = dst1;
}

static void vdlp_render_line_RGB565_hires_bypass_clut(void)
{
  int x;
  uint16_t *dst0;
  uint16_t *dst1;
  uint32_t *src0;
  uint32_t *src1;
  uint32_t *src2;
  uint32_t *src3;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black_hires(width,sizeof(uint16_t));
    return;
  }

  dst0 = g_CURBUF;
  dst1 = (dst0 + (width << 1));
  src0 = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  src1 = (src0 + ((1024 * 1024) / sizeof(uint32_t)));
  src2 = (src1 + ((1024 * 1024) / sizeof(uint32_t)));
  src3 = (src2 + ((1024 * 1024) / sizeof(uint32_t)));
  for(x = 0; x < width; x++)
    {
      *dst0++ = fixed_clut_to_RGB565(*(uint16_t*)&src0[x]);
      *dst0++ = fixed_clut_to_RGB565(*(uint16_t*)&src1[x]);
      *dst1++ = fixed_clut_to_RGB565(*(uint16_t*)&src2[x]);
      *dst1++ = fixed_clut_to_RGB565(*(uint16_t*)&src3[x]);
    }

  g_CURBUF = dst1;
}

static
uint32_t
fixed_clut_to_XRGB8888(const uint16_t p_)
{
  return (((p_ & 0x7C00) << 0x9) |
          ((p_ & 0x03E0) << 0x6) |
          ((p_ & 0x001F) << 0x3));
}

static
uint32_t
user_clut_to_XRGB8888(const uint16_t p_)
{
  return ((g_VDLP.clut_r[(p_ >> 0xA) & 0x1F] << 0x10) |
          (g_VDLP.clut_g[(p_ >> 0x5) & 0x1F] << 0x08) |
          (g_VDLP.clut_b[(p_ >> 0x0) & 0x1F] << 0x00));
}

static
uint32_t
vdlp_render_pixel_XRGB8888(const uint16_t p_)
{
  if(p_ == 0)
    return g_VDLP.bg_color.raw;

  return user_clut_to_XRGB8888(p_);
}

static
uint32_t
vdlp_render_pixel_XRGB8888_bypass_clut(const uint16_t p_)
{
  if(p_ == 0)
    return g_VDLP.bg_color.raw;

  if(p_ & 0x8000)
    return fixed_clut_to_XRGB8888(p_);

  return user_clut_to_XRGB8888(p_);
}

static void vdlp_render_line_XRGB8888(void)
{
  int x;
  uint32_t *src;
  uint32_t *dst;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black(width,sizeof(uint32_t));
    return;
  }

  dst = g_CURBUF;
  src = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  if(!g_VDLP.disp_ctrl.dcw.clut_bypass)
    {
      for(x = 0; x < width; x++)
        dst[x] = vdlp_render_pixel_XRGB8888(*(uint16_t*)&src[x]);
    }
  else
    {
      for(x = 0; x < width; x++)
        dst[x] = vdlp_render_pixel_XRGB8888_bypass_clut(*(uint16_t*)&src[x]);
    }

  g_CURBUF = (dst + width);
}

static void vdlp_render_line_XRGB8888_bypass_clut(void)
{
  int x;
  uint32_t *src;
  uint32_t *dst;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black(width,sizeof(uint32_t));
    return;
  }

  dst = g_CURBUF;
  src = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  for(x = 0; x < width; x++)
    dst[x] = fixed_clut_to_XRGB8888(*(uint16_t*)&src[x]);

  g_CURBUF = (dst + width);
}

static void vdlp_render_line_XRGB8888_hires(void)
{
  int x;
  uint32_t *dst0;
  uint32_t *dst1;
  uint32_t *src0;
  uint32_t *src1;
  uint32_t *src2;
  uint32_t *src3;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black_hires(width,sizeof(uint32_t));
    return;
  }

  dst0 = g_CURBUF;
  dst1 = (dst0 + (width << 1));
  src0 = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  src1 = (src0 + ((1024 * 1024) / sizeof(uint32_t)));
  src2 = (src1 + ((1024 * 1024) / sizeof(uint32_t)));
  src3 = (src2 + ((1024 * 1024) / sizeof(uint32_t)));
  if(!g_VDLP.disp_ctrl.dcw.clut_bypass)
    {
      for(x = 0; x < width; x++)
        {
          *dst0++ = vdlp_render_pixel_XRGB8888(*(uint16_t*)&src0[x]);
          *dst0++ = vdlp_render_pixel_XRGB8888(*(uint16_t*)&src1[x]);
          *dst1++ = vdlp_render_pixel_XRGB8888(*(uint16_t*)&src2[x]);
          *dst1++ = vdlp_render_pixel_XRGB8888(*(uint16_t*)&src3[x]);
        }
    }
  else
    {
      for(x = 0; x < width; x++)
        {
          *dst0++ = vdlp_render_pixel_XRGB8888_bypass_clut(*(uint16_t*)&src0[x]);
          *dst0++ = vdlp_render_pixel_XRGB8888_bypass_clut(*(uint16_t*)&src1[x]);
          *dst1++ = vdlp_render_pixel_XRGB8888_bypass_clut(*(uint16_t*)&src2[x]);
          *dst1++ = vdlp_render_pixel_XRGB8888_bypass_clut(*(uint16_t*)&src3[x]);
        }
    }

  g_CURBUF = dst1;
}

static
void
vdlp_render_line_XRGB8888_hires_bypass_clut(void)
{
  int x;
  uint32_t *dst0;
  uint32_t *dst1;
  uint32_t *src0;
  uint32_t *src1;
  uint32_t *src2;
  uint32_t *src3;
  int width = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];
  if(!g_VDLP.clut_ctrl.cdcw.enable_dma)
  {
    vdlp_render_line_black_hires(width,sizeof(uint32_t));
    return;
  }

  dst0 = g_CURBUF;
  dst1 = (dst0 + (width << 1));
  src0 = (uint32_t*)(g_VRAM + ((g_VDLP.curr_bmp^2) & 0x0FFFFF));
  src1 = (src0 + ((1024 * 1024) / sizeof(uint32_t)));
  src2 = (src1 + ((1024 * 1024) / sizeof(uint32_t)));
  src3 = (src2 + ((1024 * 1024) / sizeof(uint32_t)));
  for(x = 0; x < width; x++)
    {
      *dst0++ = fixed_clut_to_XRGB8888(*(uint16_t*)&src0[x]);
      *dst0++ = fixed_clut_to_XRGB8888(*(uint16_t*)&src1[x]);
      *dst1++ = fixed_clut_to_XRGB8888(*(uint16_t*)&src2[x]);
      *dst1++ = fixed_clut_to_XRGB8888(*(uint16_t*)&src3[x]);
    }

  g_CURBUF = dst1;
}


/* tick / increment frame buffer address */
static
uint32_t
tick_fba(const uint32_t fba_)
{
  uint32_t modulo;

  modulo = PIXELS_PER_LINE_MODULO[g_VDLP.clut_ctrl.cdcw.fba_incr_modulo];

  return (fba_ + ((fba_ & 2) ? ((modulo << 2) - 2) : 2));
}

static
INLINE
int
visible_scanline(const int line_)
{
  return ((line_ >= opera_region_start_scanline()) &&
          (line_  < opera_region_end_scanline()));
}

/*
  See ppgfldr/ggsfldr/gpgfldr/2gpgb.html for details on the frame
  buffer layout.

  320x240 by 16bit stored in "left/right" pairs. High order 16bits
  represent X,Y and low order bits represent X,Y+1. Starting from
  0,0 and going left to right and top to bottom to 319,239.
*/
void
opera_vdlp_process_line(int line_)
{
  int y;

  if(line_ < 5)
    return;

  if(line_ == 5)
    {
      g_CURBUF = g_BUF;
      g_VDLP.curr_vdl = g_VDLP.head_vdl;
      vdlp_process_vdl_entry();
    }

  if(g_VDLP.line_cnt == 0)
    vdlp_process_vdl_entry();

  if(visible_scanline(line_))
    g_RENDERER();

  g_VDLP.prev_bmp = ((g_VDLP.clut_ctrl.cdcw.prev_fba_tick) ?
                     tick_fba(g_VDLP.prev_bmp) : g_VDLP.curr_bmp);
  g_VDLP.curr_bmp = tick_fba(g_VDLP.curr_bmp);

  g_VDLP.disp_ctrl.dcw.vi_off_1_line = 0;
  g_VDLP.line_cnt--;
}


void
opera_vdlp_init(uint8_t *vram_)
{
  uint32_t i;
  static const uint32_t StartupVDL[]=
    { /* Startup VDL at address 0x2B0000 */
      0x00004410, 0x002C0000, 0x002C0000, 0x002B0098,
      0x00000000, 0x01080808, 0x02101010, 0x03191919,
      0x04212121, 0x05292929, 0x06313131, 0x073A3A3A,
      0x08424242, 0x094A4A4A, 0x0A525252, 0x0B5A5A5A,
      0x0C636363, 0x0D6B6B6B, 0x0E737373, 0x0F7B7B7B,
      0x10848484, 0x118C8C8C, 0x12949494, 0x139C9C9C,
      0x14A5A5A5, 0x15ADADAD, 0x16B5B5B5, 0x17BDBDBD,
      0x18C5C5C5, 0x19CECECE, 0x1AD6D6D6, 0x1BDEDEDE,
      0x1CE6E6E6, 0x1DEFEFEF, 0x1EF8F8F8, 0x1FFFFFFF,
      0xE0010101, 0xC001002C, 0x002180EF, 0x002C0000,
      0x002C0000, 0x002B00A8, 0x00000000, 0x002C0000,
      0x002C0000, 0x002B0000
    };

  g_VRAM = vram_;
  g_VDLP.head_vdl = 0xB0000;
  g_RENDERER = vdlp_render_line_XRGB8888;

  for(i = 0; i < (sizeof(StartupVDL)/sizeof(uint32_t)); i++)
    vram_write32((0xB0000 + (i * sizeof(uint32_t))),StartupVDL[i]);
}

void
opera_vdlp_set_vdl_head(const uint32_t addr_)
{
  g_VDLP.head_vdl = addr_;
}

uint32_t
opera_vdlp_state_size(void)
{
  return sizeof(vdlp_t);
}

void
opera_vdlp_state_save(void *buf_)
{
  //memcpy(buf_,&vdl,sizeof(vdlp_datum_t));
}

void
opera_vdlp_state_load(const void *buf_)
{
  //memcpy(&vdl,buf_,sizeof(vdlp_datum_t));
}

/*
  Is having all these renderers nice? No. But given the performance
  sensitivy of this code and impact it can have on a lower end system
  such verbosity is necessary.
*/
void*
get_renderer(vdlp_pixel_format_e pf_,
             uint32_t            flags_)
{
  switch(pf_)
    {
    case VDLP_PIXEL_FORMAT_0RGB1555:
      switch(flags_ & VDLP_FLAGS)
        {
        case VDLP_FLAG_NONE:
          return vdlp_render_line_0RGB1555;
        case VDLP_FLAG_CLUT_BYPASS:
          return vdlp_render_line_0RGB1555_bypass_clut;
        case VDLP_FLAG_HIRES_CEL:
          return vdlp_render_line_0RGB1555_hires;
        case VDLP_FLAG_CLUT_BYPASS|VDLP_FLAG_HIRES_CEL:
          return vdlp_render_line_0RGB1555_hires_bypass_clut;
        }
      break;
    case VDLP_PIXEL_FORMAT_RGB565:
      switch(flags_ & VDLP_FLAGS)
        {
        case VDLP_FLAG_NONE:
          return vdlp_render_line_RGB565;
        case VDLP_FLAG_CLUT_BYPASS:
          return vdlp_render_line_RGB565_bypass_clut;
        case VDLP_FLAG_HIRES_CEL:
          return vdlp_render_line_RGB565_hires;
        case VDLP_FLAG_CLUT_BYPASS|VDLP_FLAG_HIRES_CEL:
          return vdlp_render_line_RGB565_hires_bypass_clut;
        }
      break;
    case VDLP_PIXEL_FORMAT_XRGB8888:
      switch(flags_ & VDLP_FLAGS)
        {
        case VDLP_FLAG_NONE:
          return vdlp_render_line_XRGB8888;
        case VDLP_FLAG_CLUT_BYPASS:
          return vdlp_render_line_XRGB8888_bypass_clut;
        case VDLP_FLAG_HIRES_CEL:
          return vdlp_render_line_XRGB8888_hires;
        case VDLP_FLAG_CLUT_BYPASS|VDLP_FLAG_HIRES_CEL:
          return vdlp_render_line_XRGB8888_hires_bypass_clut;
        }
      break;
    }

  return NULL;
}

int
opera_vdlp_configure(void                *buf_,
                     vdlp_pixel_format_e  pf_,
                     uint32_t             flags_)
{
  g_BUF = buf_;

  g_RENDERER = get_renderer(pf_,flags_);
  if(g_RENDERER)
    return -1;

  return 0;
}
