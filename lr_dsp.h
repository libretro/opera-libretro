#ifndef LIBRETRO_4DO_LR_DSP_H_INCLUDED
#define LIBRETRO_4DO_LR_DSP_H_INCLUDED

#include <boolean.h>

void lr_dsp_init(const bool threaded_);
void lr_dsp_destroy(void);

void lr_dsp_upload(void);
void lr_dsp_process(void);

#endif /* LIBRETRO_4DO_LR_DSP_H_INCLUDED */
