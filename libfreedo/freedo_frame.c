#include <stdint.h>
#include "boolean.h"

#include "freedo_core.h"
#include "freedo_frame.h"
#include "freedo_vdlp.h"

/*
  for(int i = 0; i < 32; i++)
    FIXED_CLUT[i] = (((i & 0x1F) << 3) | ((i >> 2) & 7));
*/
static const uint8_t FIXED_CLUT[32] =
  {
    0x00, 0x08, 0x10, 0x18,
    0x21, 0x29, 0x31, 0x39,
    0x42, 0x4A, 0x52, 0x5A,
    0x63, 0x6B, 0x73, 0x7B,
    0x84, 0x8C, 0x94, 0x9C,
    0xA5, 0xAD, 0xB5, 0xBD,
    0xC6, 0xCE, 0xD6, 0xDE,
    0xE7, 0xEF, 0xF7, 0xFF
  };


void
freedo_frame_get_bitmap_xrgb_8888(const vdlp_frame_t *src_frame_,
                                  uint32_t           *dest_,
                                  uint32_t            width_,
                                  uint32_t            height_)
{
  uint32_t x;
  uint32_t y;

  for (y = 0; y < height_; y++)
    {
      const vdlp_line_t *line       = &src_frame_->lines[y];
      const uint16_t    *pixel      = line->line;
      const int          fixed_clut = (line->xOUTCONTROLL & 0x2000000);

      for(x = 0; x < width_; x++)
        {
          if(pixel[x] == 0)
            {
              /*
               *dest_ = (((line->xBACKGROUND & 0x001F) << 0) |
                         ((line->xBACKGROUND & 0x03E0) << 3) |
                         ((line->xBACKGROUND & 0x7C00) << 6));
              */

              /*
                The code above (a modified version of the original
                which did the same) is assuming the data is 16bit but
                xBACKGROUND is clearly defined and set as 32bit in the
                VDLP module. Hense the change below.
              */
              *dest_ = line->xBACKGROUND;
            }
          else if(fixed_clut && (pixel[x] & 0x8000))
            {
              *dest_ = ((FIXED_CLUT[(pixel[x] >>  0) & 0x1F] << 0) |
                        (FIXED_CLUT[(pixel[x] >>  5) & 0x1F] << 8) |
                        (FIXED_CLUT[(pixel[x] >> 10) & 0x1F] << 16));
            }
          else
            {
              *dest_ = ((line->xCLUTB[(pixel[x] >>  0) & 0x1F] << 0) |
                        (line->xCLUTG[(pixel[x] >>  5) & 0x1F] << 8) |
                        (line->xCLUTR[(pixel[x] >> 10) & 0x1F] << 16));
            }

          dest_++;;
        }
    }
}
