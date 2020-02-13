#ifndef LIBRETRO_LR_DSP_H_INCLUDED
#define LIBRETRO_LR_DSP_H_INCLUDED

void lr_dsp_init(const int threaded);
void lr_dsp_destroy(void);

void lr_dsp_upload(void);
void lr_dsp_process(void);

#endif
