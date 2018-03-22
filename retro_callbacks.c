#include <libretro.h>

retro_audio_sample_batch_t retro_audio_sample_batch_cb = NULL;
retro_environment_t        retro_environment_cb        = NULL;
retro_input_poll_t         retro_input_poll_cb         = NULL;
retro_input_state_t        retro_input_state_cb        = NULL;
retro_log_printf_t         retro_log_printf_cb         = NULL;
retro_video_refresh_t      retro_video_refresh_cb      = NULL;

void
retro_set_environment_cb(retro_environment_t cb)
{
  retro_environment_cb = cb;
}

void
retro_set_log_printf_cb(retro_log_printf_t cb)
{
  retro_log_printf_cb = cb;
}

void
retro_set_input_state_cb(retro_input_state_t cb)
{
  retro_input_state_cb = cb;
}

void
retro_set_input_poll_cb(retro_input_poll_t cb)
{
  retro_input_poll_cb = cb;
}
