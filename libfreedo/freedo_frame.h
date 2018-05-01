/* frame extraction and filtering */

#ifndef	LIBFREEDO_FRAME_H_INCLUDED
#define LIBFREEDO_FRAME_H_INCLUDED

#include "extern_c.h"

#include "freedo_vdlp.h"

EXTERN_C_BEGIN

void
freedo_frame_get_bitmap_xrgb_8888(const vdlp_frame_t *src_frame_,
                                  uint32_t           *dest_,
                                  uint32_t            width_,
                                  uint32_t            height_);

EXTERN_C_END

#endif /* LIBFREEDO_FRAME_H_INCLUDED */
