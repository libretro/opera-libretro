#ifndef LIBRETRO_4DO_RETRO_CALLBACKS_H_INCLUDED
#define LIBRETRO_4DO_RETRO_CALLBACKS_H_INCLUDED

#include <libretro.h>

extern retro_audio_sample_t       retro_audio_sample_cb;
extern retro_audio_sample_batch_t retro_audio_sample_batch_cb;
extern retro_environment_t        retro_environment_cb;
extern retro_input_poll_t         retro_input_poll_cb;
extern retro_input_state_t        retro_input_state_cb;
extern retro_log_printf_t         retro_log_printf_cb;
extern retro_video_refresh_t      retro_video_refresh_cb;

void retro_set_audio_sample_cb(retro_audio_sample_t cb_);
void retro_set_audio_sample_batch_cb(retro_audio_sample_batch_t cb_);
void retro_set_environment_cb(retro_environment_t cb_);
void retro_set_input_poll_cb(retro_input_poll_t cb_);
void retro_set_input_state_cb(retro_input_state_t cb_);
void retro_set_log_printf_cb(retro_log_printf_t cb_);
void retro_set_video_refresh_cb(retro_video_refresh_t cb_);

#endif /* LIBRETRO_4DO_RETRO_CALLBACKS_H_INCLUDED */
