#ifndef OPERA_LR_DSP_H_INCLUDED
#define OPERA_LR_DSP_H_INCLUDED

#include <stdbool.h>

void opera_lr_dsp_init(const bool threaded);
void opera_lr_dsp_destroy(void);

void opera_lr_dsp_upload(void);
void opera_lr_dsp_process(void);

#endif
