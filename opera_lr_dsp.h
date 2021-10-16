#ifndef OPERA_LR_DSP_H_INCLUDED
#define OPERA_LR_DSP_H_INCLUDED

void opera_lr_dsp_init(const int threaded);
void opera_lr_dsp_destroy(void);

void opera_lr_dsp_upload(void);
void opera_lr_dsp_process(void);

#endif
