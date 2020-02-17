#ifndef LIBOPERA_VDLP_I_H_INCLUDED
#define LIBOPERA_VDLP_I_H_INCLUDED

#include "static_assert.h"

#include <stdint.h>

#define CLUT_LEN 32

/* Find details in docs: ppgfldr/ggsfldr/gpgfldr/4gpgd.html */
typedef struct clut_dma_ctrl_word_s clut_dma_ctrl_word_s;
struct clut_dma_ctrl_word_s
{
  uint32_t persist_len:9;        /*  8 - 0 */
  uint32_t ctrl_word_cnt:6;      /* 14 - 9 */
  uint32_t prev_fba_override:1;  /* 15 */
  uint32_t curr_fba_override:1;  /* 16 */
  uint32_t prev_fba_tick:1 ;     /* 17 */
  uint32_t next_vdl_addr_rel:1;  /* 18 */
  uint32_t vertical_mode:1;      /* 19 */
  uint32_t padding0:1;           /* 20 */
  uint32_t enable_dma:1;         /* 21 */
  uint32_t padding1:1;           /* 22 */
  uint32_t fba_incr_modulo:3;    /* 25 - 23 */
  uint32_t padding2:6;           /* 31 - 26 */
};

/* ctrl_color == 0 if color value word */
typedef struct color_value_word_s color_value_word_s;
struct color_value_word_s
{
  uint32_t b:8;              /*  7 - 0  */
  uint32_t g:8;             /* 15 - 8  */
  uint32_t r:8;               /* 23 - 16 */
  uint32_t addr:5;              /* 28 - 24 */
  uint32_t rgb_enable:2;        /* 30 - 29 */
  uint32_t ctrl_color:1;        /* 31      */
};


/* id == 0b11100000 if background value word */
typedef struct background_value_word_s background_value_word_s;
struct background_value_word_s
{
  uint32_t b:8;                 /*  7 - 0  */
  uint32_t g:8;                 /* 15 - 8  */
  uint32_t r:8;                 /* 23 - 16 */
  uint32_t id:8;                /* 31 - 24 */
};

/* ctrl_color == 0x110 */
typedef struct display_ctrl_word_s display_ctrl_word_s;
struct display_ctrl_word_s
{
  uint32_t vi_off_1_line:1;     /* 0 */
  uint32_t colors_only:1;       /* 1 */
  uint32_t hi_on:1;             /* 2 */
  uint32_t vi_on:1;             /* 3 */
  uint32_t line_blue_lsb:2;     /* 5 - 4 */
  uint32_t line_hsrc:2;         /* 7 - 6 */
  uint32_t line_vsrc:2;         /* 9 - 8 */
  uint32_t swap_pen:1;          /* 10 */
  uint32_t msb_replication:1;   /* 11 */
  uint32_t random:1;            /* 12 */
  uint32_t window_hi_on:1;      /* 13 */
  uint32_t window_vi_on:1;      /* 14 */
  uint32_t window_blue_lsb:2;   /* 16 - 15 */
  uint32_t window_hsrc:2;       /* 18 - 17 */
  uint32_t window_vsrc:2;       /* 20 - 19 */
  uint32_t swap_hv:1;           /* 21 */
  uint32_t enable_bg_color_detector:1; /* 22 */
  uint32_t tran_true:1;         /* 23 */
  uint32_t src_sel:1;           /* 24 */
  uint32_t clut_bypass:1;       /* 25 */
  uint32_t reserved:1;          /* 26 */
  uint32_t pal_ntsc:1;          /* 27 */
  uint32_t null:1;              /* 28 */
  uint32_t ctrl_color:3;        /* 31 - 29 */
};

/* ctrl_color == 0x10 */
typedef struct av_output_ctrl_word_s av_output_ctrl_word_s;
struct av_output_ctrl_word_s
{
  uint32_t padding:30;          /* 29 - 0 */
  uint32_t ctrl_color:2;        /* 31 - 30 */
};

typedef union background_value_word_u background_value_word_u;
union background_value_word_u
{
  uint32_t raw;
  background_value_word_s bvw;
};

typedef union clut_dma_ctrl_word_u clut_dma_ctrl_word_u;
union clut_dma_ctrl_word_u
{
  uint32_t raw;
  clut_dma_ctrl_word_s cdcw;
};

typedef union display_ctrl_word_u display_ctrl_word_u;
union display_ctrl_word_u
{
  uint32_t raw;
  display_ctrl_word_s dcw;
};

typedef union vdl_ctrl_word_u vdl_ctrl_word_u;
union vdl_ctrl_word_u
{
  uint32_t raw;
  color_value_word_s      cvw;
  background_value_word_s bvw;
  display_ctrl_word_s     dcw;
  av_output_ctrl_word_s   aocw;
};

typedef struct vdlp_s vdlp_t;
struct vdlp_s
{
  uint8_t  clut_r[CLUT_LEN];
  uint8_t  clut_g[CLUT_LEN];
  uint8_t  clut_b[CLUT_LEN];
  uint32_t background_color;
  uint32_t head_vdl;
  uint32_t curr_vdl;
  uint32_t prev_bmp;
  uint32_t curr_bmp;
  background_value_word_u bg_color;
  clut_dma_ctrl_word_u    clut_ctrl;
  display_ctrl_word_u     disp_ctrl;
  int32_t line_cnt;
};

#if 0
STATIC_ASSERT(sizeof(background_value_word_u) == sizeof(uint32_t),
              background_value_word_not_4_bytes);
STATIC_ASSERT(sizeof(clut_dma_ctrl_word_u) == sizeof(uint32_t),
              clut_dma_ctrl_word_not_4_bytes);
STATIC_ASSERT(sizeof(display_ctrl_word_u) == sizeof(uint32_t),
              display_ctrl_word_not_4_bytes);
STATIC_ASSERT(sizeof(vdl_ctrl_word_u) == sizeof(uint32_t),
              vdl_ctrl_word_not_4_bytes);
#endif

#endif /* OPERA_VDLP_I_H_INCLUDED */
