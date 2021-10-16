#include <libretro.h>

retro_audio_sample_t       retro_audio_sample_cb       = NULL;
retro_audio_sample_batch_t retro_audio_sample_batch_cb = NULL;
retro_environment_t        retro_environment_cb        = NULL;
retro_input_poll_t         retro_input_poll_cb         = NULL;
retro_input_state_t        retro_input_state_cb        = NULL;
retro_log_printf_t         retro_log_printf_cb         = NULL;
retro_video_refresh_t      retro_video_refresh_cb      = NULL;

void
opera_lr_callbacks_set_audio_sample(retro_audio_sample_t cb_)
{
  retro_audio_sample_cb = cb_;
}

void
opera_lr_callbacks_set_audio_sample_batch(retro_audio_sample_batch_t cb_)
{
  retro_audio_sample_batch_cb = cb_;
}

void
opera_lr_callbacks_set_environment(retro_environment_t cb_)
{
  retro_environment_cb = cb_;
}

void
opera_lr_callbacks_set_log_printf(retro_log_printf_t cb_)
{
  retro_log_printf_cb = cb_;
}

void
opera_lr_callbacks_set_input_state(retro_input_state_t cb_)
{
  retro_input_state_cb = cb_;
}

void
opera_lr_callbacks_set_input_poll(retro_input_poll_t cb_)
{
  retro_input_poll_cb = cb_;
}

void
opera_lr_callbacks_set_video_refresh(retro_video_refresh_t cb_)
{
  retro_video_refresh_cb = cb_;
}
