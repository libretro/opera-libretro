#ifndef LIBOPERA_VDLP_H_INCLUDED
#define LIBOPERA_VDLP_H_INCLUDED

#include "extern_c.h"

#include <stdint.h>

#define VDLP_FLAG_NONE          0
#define VDLP_FLAG_CLUT_BYPASS   (1<<0)
#define VDLP_FLAG_HIRES_CEL     (1<<1)
#define VDLP_FLAG_INTERPOLATION (1<<2)
#define VDLP_FLAGS              (VDLP_FLAG_CLUT_BYPASS|VDLP_FLAG_HIRES_CEL|VDLP_FLAG_INTERPOLATION)

enum vdlp_pixel_format_e
  {
    VDLP_PIXEL_FORMAT_0RGB1555,
    VDLP_PIXEL_FORMAT_XRGB8888,
    VDLP_PIXEL_FORMAT_RGB565
  };

typedef enum vdlp_pixel_format_e vdlp_pixel_format_e;

EXTERN_C_BEGIN

void     opera_vdlp_init(uint8_t *vram_);

void     opera_vdlp_set_vdl_head(const uint32_t addr);
void     opera_vdlp_process_line(int line);

uint32_t opera_vdlp_state_size(void);
void     opera_vdlp_state_save(void *buf);
void     opera_vdlp_state_load(const void *buf);

int      opera_vdlp_configure(void *buf,
                              vdlp_pixel_format_e pf,
                              uint32_t flags);

EXTERN_C_END

#endif /* LIBOPERA_VDLP_H_INCLUDED */
