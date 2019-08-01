#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

/* Initialises any dynamic core options values
 * Note: Must be called after retro_set_environment_cb()
 */
void libretro_init_core_options(void);

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * Note: Must be called after retro_set_environment_cb()
 */
void libretro_set_core_options(void);

#endif
