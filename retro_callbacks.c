#include <libretro.h>

retro_audio_sample_t       retro_audio_sample_cb       = NULL;
retro_audio_sample_batch_t retro_audio_sample_batch_cb = NULL;
retro_environment_t        retro_environment_cb        = NULL;
retro_input_poll_t         retro_input_poll_cb         = NULL;
retro_input_state_t        retro_input_state_cb        = NULL;
retro_log_printf_t         retro_log_printf_cb         = NULL;
retro_video_refresh_t      retro_video_refresh_cb      = NULL;

void
retro_set_audio_sample_cb(retro_audio_sample_t cb_)
{
  retro_audio_sample_cb = cb_;
}

void
retro_set_audio_sample_batch_cb(retro_audio_sample_batch_t cb_)
{
  retro_audio_sample_batch_cb = cb_;
}

void
retro_set_environment_cb(retro_environment_t cb_)
{
  retro_environment_cb = cb_;
}

void
retro_set_log_printf_cb(retro_log_printf_t cb_)
{
  retro_log_printf_cb = cb_;
}

void
retro_set_input_state_cb(retro_input_state_t cb_)
{
  retro_input_state_cb = cb_;
}

void
retro_set_input_poll_cb(retro_input_poll_t cb_)
{
  retro_input_poll_cb = cb_;
}

void
retro_set_video_refresh_cb(retro_video_refresh_t cb_)
{
  retro_video_refresh_cb = cb_;
}
