#pragma once

#include <libretro.h>

extern retro_log_printf_t         retro_log_printf_cb;
extern retro_video_refresh_t      retro_video_refresh_cb;
extern retro_input_poll_t         retro_input_poll_cb;
extern retro_input_state_t        retro_input_state_cb;
extern retro_environment_t        retro_environment_cb;
extern retro_audio_sample_batch_t retro_audio_sample_batch_cb;

void retro_set_environment_cb(retro_environment_t cb);
void retro_set_log_printf_cb(retro_log_printf_t cb);
