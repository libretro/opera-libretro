#ifndef OPERA_LR_CALLBACKS_H_INCLUDED
#define OPERA_LR_CALLBACKS_H_INCLUDED

#include <libretro.h>

extern retro_audio_sample_t       retro_audio_sample_cb;
extern retro_audio_sample_batch_t retro_audio_sample_batch_cb;
extern retro_environment_t        retro_environment_cb;
extern retro_input_poll_t         retro_input_poll_cb;
extern retro_input_state_t        retro_input_state_cb;
extern retro_log_printf_t         retro_log_printf_cb;
extern retro_video_refresh_t      retro_video_refresh_cb;

void opera_lr_callbacks_set_audio_sample(retro_audio_sample_t cb);
void opera_lr_callbacks_set_audio_sample_batch(retro_audio_sample_batch_t cb);
void opera_lr_callbacks_set_environment(retro_environment_t cb);
void opera_lr_callbacks_set_input_poll(retro_input_poll_t cb);
void opera_lr_callbacks_set_input_state(retro_input_state_t cb);
void opera_lr_callbacks_set_log_printf(retro_log_printf_t cb);
void opera_lr_callbacks_set_video_refresh(retro_video_refresh_t cb);

#endif
