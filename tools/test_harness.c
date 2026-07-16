#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "libretro.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <zlib.h>

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <langinfo.h>
#include <poll.h>
#ifdef __linux__
#include <sched.h>
#endif
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define HARNESS_TERMINAL_BUTTONS 16
#define HARNESS_TERMINAL_HOLD_FRAMES_DEFAULT 6
#define HARNESS_TERMINAL_FPS_DEFAULT 30.0

#define HARNESS_TERMINAL_CELL_BLANK 0
#define HARNESS_TERMINAL_CELL_BG_SPACE 1
#define HARNESS_TERMINAL_CELL_HALF_UPPER 2
#define HARNESS_TERMINAL_CELL_ASCII 3

#define HARNESS_TERMINAL_RENDER_KITTY 0
#define HARNESS_TERMINAL_RENDER_SIXEL 1
#define HARNESS_TERMINAL_RENDER_HALF 2
#define HARNESS_TERMINAL_RENDER_ASCII 3

#define HARNESS_TERMINAL_IMAGE_FAILED 0
#define HARNESS_TERMINAL_IMAGE_DRAWN 1
#define HARNESS_TERMINAL_IMAGE_SKIPPED 2

#define HARNESS_TERMINAL_COLOR_MONO 0
#define HARNESS_TERMINAL_COLOR_256 1
#define HARNESS_TERMINAL_COLOR_TRUE 2

#define HARNESS_TERMINAL_KITTY_IMAGE_ID 0x4f504c52U
#define HARNESS_TERMINAL_KITTY_PLACEMENT_ID 1U
#define HARNESS_TERMINAL_KITTY_CHUNK_RAW 3072U
#define HARNESS_TERMINAL_KITTY_ZLIB_LEVEL 3
#define HARNESS_TERMINAL_KITTY_PROBE_TIMEOUT_MS 200
#define HARNESS_TERMINAL_SIXEL_PROBE_TIMEOUT_MS 200
#define HARNESS_TERMINAL_SIXEL_FALLBACK_CELL_W 6U
#define HARNESS_TERMINAL_SIXEL_FALLBACK_CELL_H 12U
#define HARNESS_TERMINAL_SIXEL_MAX_WIDTH 960U
#define HARNESS_TERMINAL_SIXEL_MAX_HEIGHT 720U
#define HARNESS_TERMINAL_KITTY_MAX_WIDTH 1920U
#define HARNESS_TERMINAL_KITTY_MAX_HEIGHT 1080U
#define HARNESS_TERMINAL_SIXEL_COLORS 256U
#define HARNESS_TERMINAL_SIXEL_RGB565_LUT_SIZE 65536U
#define HARNESS_TERMINAL_SIXEL_0RGB1555_LUT_SIZE 32768U
#define HARNESS_TERMINAL_SIXEL_SPARSE_MIN_WIDTH 256U
/* _terminal_rgb_to_256() emits xterm cube/gray indices 16 through 255. */
#define HARNESS_TERMINAL_SIXEL_SPARSE_MIN_COLORS 240U

typedef struct rom_entry_t rom_entry_t;
struct rom_entry_t
{
  const char *filename;
  const char *name;
  uint64_t    size;
  uint32_t    crc32;
};

typedef struct harness_option_t harness_option_t;
struct harness_option_t
{
  char *key;
  char *value;
};

typedef struct path_list_t path_list_t;
struct path_list_t
{
  char **items;
  size_t count;
};

typedef struct byte_buffer_t byte_buffer_t;
struct byte_buffer_t
{
  uint8_t *data;
  size_t   len;
  size_t   cap;
  bool     failed;
};

typedef struct terminal_image_cache_t terminal_image_cache_t;
struct terminal_image_cache_t
{
  uint8_t *pixels;
  size_t capacity;
  size_t row_bytes;
  size_t data_size;
  bool valid;
  int render_mode;
  enum retro_pixel_format pixel_format;
  unsigned src_width;
  unsigned src_height;
  unsigned row;
  unsigned col;
  unsigned cols;
  unsigned rows;
  unsigned image_width;
  unsigned image_height;
};

typedef struct terminal_sixel_palette_t terminal_sixel_palette_t;
struct terminal_sixel_palette_t
{
  uint8_t xterm_index[HARNESS_TERMINAL_SIXEL_COLORS];
  unsigned count;
};

typedef struct terminal_sixel_event_t terminal_sixel_event_t;
struct terminal_sixel_event_t
{
  unsigned x;
  unsigned next;
  uint8_t mask;
};

typedef struct input_event_t input_event_t;
struct input_event_t
{
  char    *spec;
  char    *button;
  unsigned id;
  bool     start_is_seconds;
  bool     duration_is_seconds;
  double   start_seconds;
  double   duration_seconds;
  uint64_t start_frame;
  uint64_t duration_frames;
  uint64_t resolved_start_frame;
  uint64_t resolved_end_frame;
  bool     triggered;
};

typedef struct screenshot_request_t screenshot_request_t;
struct screenshot_request_t
{
  uint64_t frame;
  char    *path;
  void    *data;
  size_t   data_size;
  unsigned width;
  unsigned height;
  size_t   pitch;
  enum retro_pixel_format pixel_format;
  bool     captured;
  bool     written;
};

typedef struct terminal_cell_t terminal_cell_t;
struct terminal_cell_t
{
  uint8_t fg_r;
  uint8_t fg_g;
  uint8_t fg_b;
  uint8_t bg_r;
  uint8_t bg_g;
  uint8_t bg_b;
  uint8_t glyph;
  uint8_t ch;
};

typedef uint32_t (*terminal_read_pixel_fn)(const void *data_,
                                           size_t      pitch_,
                                           unsigned    x_,
                                           unsigned    y_);

enum
  {
    SCREENSHOT_WHEN_NONE = 0,
    SCREENSHOT_WHEN_NONBLANK,
    SCREENSHOT_WHEN_CHANGED
  };

typedef struct harness_config_t harness_config_t;
struct harness_config_t
{
  char *core_path;
  char *bios_path;
  char *font_path;
  char *title_path;

  char *output_dir;
  char *work_dir;
  char *system_dir;
  char *save_dir;
  char *log_path;
  char *metrics_path;
  char *audio_path;
  bool     no_audio;
  char *screenshot_path;
  char *screenshot_every_dir;
  uint64_t screenshot_every_step;
  uint64_t screenshot_every_written;
  int      screenshot_when_mode;
  uint64_t screenshot_when_skipped;

  harness_option_t    *options;
  size_t               option_count;
  input_event_t        *inputs;
  size_t               input_count;
  screenshot_request_t *screenshots;
  size_t               screenshot_count;

  uint64_t frames;
  uint64_t benchmark_start_frame;
  uint64_t benchmark_end_frame;
  double   seconds;
  double   wall_timeout;
  int      cpu;
  bool     have_frames;
  bool     have_benchmark_start_frame;
  bool     have_benchmark_end_frame;
  bool     have_seconds;
  bool     have_cpu;
  bool     terminal_mode;
  bool     keep_work_dir;
  bool     user_work_dir;
  bool     verbose;
  bool     list_bios;
  unsigned terminal_button_hold_frames;
  double   terminal_fps;
  bool     terminal_fps_user;
  int      terminal_render_override;
  int      terminal_color_override;
};

typedef struct harness_run_t harness_run_t;
struct harness_run_t
{
  FILE *log_file;
  FILE *audio_file;
  int   log_fd;
  int   saved_stdout;
  int   saved_stderr;

  char *bios_filename;
  char *bios_name;
  char *font_filename;
  char *font_name;

  enum retro_pixel_format pixel_format;
  struct retro_system_av_info av_info;

  void   *last_frame;
  size_t  last_frame_size;
  unsigned last_width;
  unsigned last_height;
  size_t   last_pitch;
  enum retro_pixel_format last_pixel_format;

  uint64_t target_frames;
  uint64_t benchmark_start_frame;
  uint64_t benchmark_end_frame;
  uint64_t benchmark_frames_run;
  uint64_t current_frame;
  uint64_t frames_run;
  uint64_t video_frames;
  uint64_t audio_frames;
  uint64_t audio_bytes;

  double benchmark_wall_seconds;
  double wall_seconds;
  double core_fps;
  double sample_rate;

  void   *screenshot_when_prev;
  size_t  screenshot_when_prev_size;
  unsigned screenshot_when_prev_width;
  unsigned screenshot_when_prev_height;
  enum retro_pixel_format screenshot_when_prev_fmt;

  uint64_t log_debug;
  uint64_t log_info;
  uint64_t log_warn;
  uint64_t log_error;

  bool loaded_game;
  bool core_initialized;
  bool artifact_error;
  bool timed_out;
  bool benchmark_started;
  bool benchmark_finished;
  bool cpu_affinity_applied;
  bool terminal_active;
  bool terminal_quit_requested;
  int  tty_fd;
  int  tty_flags;
  int  terminal_width;
  int  terminal_height;
  int  terminal_pixel_width;
  int  terminal_pixel_height;
  bool terminal_resize_pending;
  int  terminal_escape;
  uint16_t terminal_button_mask;
  uint16_t terminal_button_mask_next;
  uint64_t terminal_button_expire[HARNESS_TERMINAL_BUTTONS];
  terminal_cell_t *terminal_cells;
  terminal_cell_t *terminal_row_cells;
  uint64_t *terminal_row_hashes;
  unsigned terminal_cache_cols;
  unsigned terminal_cache_rows;
  unsigned *terminal_src_x;
  unsigned *terminal_src_y_top;
  unsigned *terminal_src_y_bottom;
  unsigned terminal_map_width;
  unsigned terminal_map_height;
  unsigned terminal_map_cols;
  unsigned terminal_map_rows;
  unsigned terminal_map_render_mode;
  int terminal_render_mode;
  int terminal_color_mode;
  bool terminal_kitty_probed;
  bool terminal_kitty_supported;
  const char *terminal_kitty_probe_result;
  bool terminal_kitty_zlib_probed;
  bool terminal_kitty_zlib_supported;
  const char *terminal_kitty_zlib_probe_result;
  bool terminal_sixel_probed;
  bool terminal_sixel_supported;
  const char *terminal_sixel_probe_result;
  terminal_image_cache_t terminal_image_cache;
  terminal_read_pixel_fn terminal_read_pixel;
  uint64_t terminal_last_render_frame;
  uint64_t terminal_render_calls;
  uint64_t terminal_image_drawn_frames;
  uint64_t terminal_image_skipped_frames;
  uint64_t terminal_image_failed_frames;
  uint64_t terminal_image_bytes;
  uint64_t terminal_bytes_written;
  uint64_t terminal_status_last_frame;
  uint64_t terminal_status_frame_interval;
  uint16_t terminal_status_last_button_mask;
  bool terminal_status_last_quit_requested;
  bool terminal_status_valid;
  double terminal_adaptive_fps;
  double terminal_render_seconds_ema;
  double terminal_render_seconds_total;
  double terminal_render_seconds_last;
  double terminal_render_seconds_max;
  double terminal_image_seconds_total;
  double terminal_image_seconds_last;
  double terminal_image_seconds_max;
  uint8_t *terminal_rgb_buffer;
  size_t terminal_rgb_buffer_size;
  uint8_t *terminal_scaled_rgb_buffer;
  size_t terminal_scaled_rgb_buffer_size;
  z_stream terminal_kitty_zstream;
  bool terminal_kitty_zstream_initialized;
  byte_buffer_t terminal_kitty_zlib_buffer;
  byte_buffer_t terminal_png_buffer;
  unsigned *terminal_sixel_src_x;
  unsigned *terminal_sixel_src_y;
  unsigned terminal_sixel_map_src_width;
  unsigned terminal_sixel_map_src_height;
  unsigned terminal_sixel_map_out_width;
  unsigned terminal_sixel_map_out_height;
  uint8_t *terminal_sixel_source_indices;
  size_t terminal_sixel_source_indices_size;
  uint8_t *terminal_sixel_indices;
  size_t terminal_sixel_indices_size;
  uint8_t *terminal_sixel_masks;
  size_t terminal_sixel_masks_size;
  terminal_sixel_event_t *terminal_sixel_events;
  size_t terminal_sixel_event_capacity;
  char *terminal_output;
  size_t terminal_output_len;
  size_t terminal_output_cap;
  bool terminal_output_batching;
  struct termios tty_orig_termios;
};

typedef struct core_api_t core_api_t;
struct core_api_t
{
  void *handle;

  unsigned (*api_version)(void);
  void     (*set_environment)(retro_environment_t);
  void     (*set_video_refresh)(retro_video_refresh_t);
  void     (*set_audio_sample)(retro_audio_sample_t);
  void     (*set_audio_sample_batch)(retro_audio_sample_batch_t);
  void     (*set_input_poll)(retro_input_poll_t);
  void     (*set_input_state)(retro_input_state_t);
  void     (*init)(void);
  void     (*deinit)(void);
  void     (*get_system_info)(struct retro_system_info *);
  void     (*get_system_av_info)(struct retro_system_av_info *);
  void     (*set_controller_port_device)(unsigned, unsigned);
  bool     (*load_game)(const struct retro_game_info *);
  void     (*unload_game)(void);
  void     (*run)(void);
};

static const rom_entry_t BIOS_ROMS[] =
  {
    { "panafz1.bin",               "Panasonic FZ-1 (U)",              1048576, 0xc8c8ff89 },
    { "panafz1j.bin",              "Panasonic FZ-1 (J)",              1048576, 0xd9493adc },
    { "panafz1j-norsa.bin",        "Panasonic FZ-1 (J) [No RSA]",     1048576, 0x82ce67c6 },
    { "panafz10.bin",              "Panasonic FZ-10 (U)",             1048576, 0x58242cee },
    { "panafz10-norsa.bin",        "Panasonic FZ-10 (U) [No RSA]",    1048576, 0x230e6feb },
    { "panafz10e-anvil.bin",       "Panasonic FZ-10 (E) ANVIL",       1048576, 0x2495c500 },
    { "panafz10e-anvil-norsa.bin", "Panasonic FZ-10 (E) ANVIL [No RSA]", 1048576, 0x9a186221 },
    { "goldstar.bin",              "Goldstar GDO-101M",               1048576, 0xb6f5028b },
    { "sanyotry.bin",              "Sanyo Try IMP-21J",               1048576, 0xd5cbc509 },
    { "3do_arcade_saot.bin",       "3DO Arcade - SAOT",                524288, 0xb832da9a },
    { NULL, NULL, 0, 0 }
  };

static const rom_entry_t FONT_ROMS[] =
  {
    { "panafz1-kanji.bin",          "Panasonic FZ-1 (J) Kanji ROM",     933636, 0xa8e9447c },
    { "panafz1j-kanji.bin",         "Panasonic FZ-1 (J) Kanji ROM",    1048576, 0x45f478b1 },
    { "panafz10ja-anvil-kanji.bin", "Panasonic FZ-10 (J) ANVIL Kanji ROM", 1048576, 0xff7393de },
    { NULL, NULL, 0, 0 }
  };

static const char *RETROARCH_CONFIG_PATHS[] =
  {
    "~/.config/retroarch/retroarch.cfg",
    "~/.local/share/retroarch/retroarch.cfg",
    "~/.local/share/RetroArch/retroarch.cfg",
    "~/.var/app/org.libretro.RetroArch/config/retroarch/retroarch.cfg",
    "/etc/retroarch.cfg",
    NULL
  };

static const char *RETROARCH_ROM_DIRS[] =
  {
    "~/.config/retroarch/system",
    "~/.config/retroarch",
    "~/.local/share/retroarch/system",
    "~/.local/share/RetroArch/system",
    "~/.var/app/org.libretro.RetroArch/config/retroarch/system",
    "~/.var/app/org.libretro.RetroArch/data/retroarch/system",
    "~/snap/retroarch/current/.config/retroarch/system",
    "/usr/local/share/libretro/system",
    "/usr/local/share/retroarch/system",
    "/usr/share/libretro/system",
    "/usr/share/retroarch/system",
    NULL
  };

static harness_config_t g_cfg;
static harness_run_t    g_run;
static volatile sig_atomic_t g_signal_exit_requested;
static volatile sig_atomic_t g_signal_exit_number;
static volatile sig_atomic_t g_signal_resize_requested;
static bool g_terminal_force_output;
static bool g_terminal_rgb256_tables_ready;
static uint8_t g_terminal_rgb256_cube_component[256];
static uint8_t g_terminal_rgb256_cube_value[256];
static uint8_t g_terminal_rgb256_gray_low_value[256];
static uint8_t g_terminal_rgb256_gray_low_index[256];
static uint8_t g_terminal_rgb256_gray_high_value[256];
static uint8_t g_terminal_rgb256_gray_high_index[256];
static bool g_terminal_sixel_rgb565_lut_ready;
static bool g_terminal_sixel_0rgb1555_lut_ready;
static uint8_t
g_terminal_sixel_rgb565_lut[HARNESS_TERMINAL_SIXEL_RGB565_LUT_SIZE];
static uint8_t
g_terminal_sixel_0rgb1555_lut[HARNESS_TERMINAL_SIXEL_0RGB1555_LUT_SIZE];

RETRO_CALLCONV
static
void
_harness_log(enum retro_log_level level_,
             const char          *fmt_,
             ...);

static
void
_terminal_set_pixel_reader(enum retro_pixel_format fmt_);

static
void
_terminal_render_status_line(unsigned image_rows_max_,
                             unsigned display_cols_);

static
bool
_terminal_should_render_status_line(int image_result_);

static
void
_handle_process_signal(int signo_)
{
  if(signo_ == SIGWINCH)
    {
      g_signal_resize_requested = 1;
      return;
    }

  g_signal_exit_requested = 1;
  if(g_signal_exit_number == 0)
    g_signal_exit_number = signo_;
}


static
void
_install_signal_handlers(void)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = _handle_process_signal;

  (void)sigaction(SIGINT, &sa, NULL);
  (void)sigaction(SIGTERM, &sa, NULL);
  (void)sigaction(SIGHUP, &sa, NULL);
  (void)sigaction(SIGQUIT, &sa, NULL);
  (void)sigaction(SIGWINCH, &sa, NULL);
}


static
void
_print_usage(FILE *f_)
{
  fprintf(f_,
          "Usage: test-harness [--core ./opera_libretro.so] [--bios /path/to/bios.bin] [options]\n"
          "\n"
          "Core and BIOS:\n"
          "  --core PATH              libretro core shared object to load; default\n"
          "                           opera_libretro.so beside test-harness\n"
          "  --bios PATH              recognized 3DO BIOS ROM; default panafz1.bin\n"
          "                           beside test-harness, then filename search\n"
          "\n"
          "Content and duration:\n"
          "  --title PATH             optional iso/bin/chd/cue title path\n"
          "  --frames N               run N frames\n"
          "  --seconds S              run ceil(S * core_fps) frames\n"
          "  --benchmark-start-frame N first frame included in benchmark metrics\n"
          "  --benchmark-end-frame N  last frame included; extends run if needed\n"
          "  --wall-timeout S         stop after S wall seconds between frames\n"
          "  --cpu N                  pin the harness and core-created threads to CPU N\n"
          "  --input SPEC             repeatable input event, e.g. 2S=X or 120F=A\n"
          "  --input-file PATH        file of input events (one per line; # comments)\n"
          "  --terminal               show live terminal framebuffer and read\n"
          "                           keyboard controls; falls back by terminal\n"
          "                           capability\n"
          "  --terminal-fps N         cap terminal redraw rate; 0 redraws every\n"
          "                           video frame (default adaptive up to 30)\n"
          "  --terminal-render MODE   terminal render mode: auto, kitty, sixel,\n"
          "                           half, ascii\n"
          "  --terminal-color MODE    terminal color mode: auto, true, 256, mono\n"
          "  --terminal-button-hold N  keep terminal button pressed for N frames\n"
          "                           (default 6)\n"
          "\n"
          "Artifacts:\n"
          "  --output-dir DIR         default ./test-harness-runs/<timestamp-pid>\n"
          "  --log PATH               default <output-dir>/run.log\n"
          "  --metrics PATH           default <output-dir>/metrics.json\n"
          "  --audio PATH             write stereo s16le WAV\n"
          "  --no-audio               disable audio output capture\n"
          "  --screenshot PATH        write final frame as PNG\n"
          "  --screenshot-at N=PATH   write frame N as PNG, repeatable\n"
          "  --screenshot-every N=DIR write every Nth frame as PNG into DIR\n"
          "  --screenshot-when MODE   filter --screenshot-every output: NONBLANK or\n"
          "                           CHANGED (skip identical-to-last-written frames)\n"
          "\n"
          "Runtime setup:\n"
          "  --font PATH              optional recognized Kanji/font ROM; filename\n"
          "                           values search the same RetroArch directories\n"
          "  --list-bios              list recognized BIOS ROMs and exit\n"
          "  --option KEY=VALUE       repeatable core option override\n"
          "  --input supports optional holds: 2S+0.5S=A, 120F+30F=P\n"
          "  --work-dir DIR           persistent system/save staging directory\n"
          "  --keep-work-dir          keep default temporary staging directory\n"
          "  --verbose                log unsupported environment calls\n"
          "  --help                   show this help\n");
}


static
char *
_xstrdup(const char *s_)
{
  char *rv;

  if(s_ == NULL)
    return NULL;

  rv = strdup(s_);
  if(rv == NULL)
    {
      perror("strdup");
      exit(1);
    }

  return rv;
}


static
char *
_xstrndup(const char *s_,
          size_t      len_)
{
  char *rv;

  rv = (char *)malloc(len_ + 1);
  if(rv == NULL)
    {
      perror("malloc");
      exit(1);
    }

  memcpy(rv, s_, len_);
  rv[len_] = 0;
  return rv;
}


static
char *
_xasprintf2(const char *a_,
            const char *b_)
{
  size_t len;
  char *rv;
  int need_slash;

  if((a_ == NULL) || (b_ == NULL))
    return NULL;

  need_slash = ((a_[0] != 0) && (a_[strlen(a_) - 1] != '/'));
  len = strlen(a_) + strlen(b_) + (need_slash ? 2 : 1);

  rv = (char *)malloc(len);
  if(rv == NULL)
    {
      perror("malloc");
      exit(1);
    }

  snprintf(rv, len, "%s%s%s", a_, need_slash ? "/" : "", b_);
  return rv;
}


static
double
_monotonic_seconds(void)
{
  struct timespec ts;

  if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0.0;

  return (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);
}


static
const
char *
_path_basename(const char *path_)
{
  const char *slash;

  slash = strrchr(path_, '/');
  return slash ? slash + 1 : path_;
}


static
bool
_path_is_directory(const char *path_)
{
  struct stat st;

  return ((stat(path_, &st) == 0) && S_ISDIR(st.st_mode));
}


static
int
_mkdir_p(const char *path_)
{
  char tmp[PATH_MAX];
  char *p;
  size_t len;

  if((path_ == NULL) || (path_[0] == 0))
    return -1;

  len = strlen(path_);
  if(len >= sizeof(tmp))
    return -1;

  memcpy(tmp, path_, len + 1);
  if((len > 1) && (tmp[len - 1] == '/'))
    tmp[len - 1] = 0;

  for(p = tmp + 1; *p; p++)
    {
      if(*p != '/')
        continue;

      *p = 0;
      if((mkdir(tmp, 0755) != 0) && (errno != EEXIST))
        return -1;
      *p = '/';
    }

  if((mkdir(tmp, 0755) != 0) && (errno != EEXIST))
    return -1;

  return _path_is_directory(tmp) ? 0 : -1;
}


static
int
_mkdir_parent(const char *path_)
{
  char *copy;
  char *slash;
  int rv;

  copy = _xstrdup(path_);
  slash = strrchr(copy, '/');
  if(slash == NULL)
    {
      free(copy);
      return 0;
    }

  if(slash == copy)
    {
      free(copy);
      return 0;
    }

  *slash = 0;
  rv = _mkdir_p(copy);
  free(copy);
  return rv;
}


static
char *
_resolve_existing_path(const char *path_)
{
  char *rv;

  rv = realpath(path_, NULL);
  if(rv == NULL)
    return NULL;

  return rv;
}


static
bool
_path_has_separator(const char *path_)
{
  return strchr(path_, '/') != NULL;
}


static
bool
_path_is_regular_file(const char *path_)
{
  struct stat st;

  return ((stat(path_, &st) == 0) && S_ISREG(st.st_mode));
}


static
char *
_expand_user_path(const char *path_)
{
  const char *home;
  char *rv;
  size_t len;

  if(path_ == NULL)
    return NULL;

  if((path_[0] != '~') || ((path_[1] != '/') && (path_[1] != 0)))
    return _xstrdup(path_);

  home = getenv("HOME");
  if((home == NULL) || (home[0] == 0))
    return NULL;

  len = strlen(home) + strlen(path_);
  rv = (char *)malloc(len);
  if(rv == NULL)
    {
      perror("malloc");
      exit(1);
    }

  snprintf(rv, len, "%s%s", home, path_ + 1);
  return rv;
}


static
char *
_path_dirname_copy(const char *path_)
{
  char *copy;
  char *slash;

  copy = _xstrdup(path_);
  slash = strrchr(copy, '/');
  if(slash == NULL)
    {
      free(copy);
      return _xstrdup(".");
    }

  if(slash == copy)
    slash[1] = 0;
  else
    *slash = 0;

  return copy;
}


static
char *
_executable_dir(void)
{
  char path[PATH_MAX];
  ssize_t n;

  n = readlink("/proc/self/exe", path, sizeof(path) - 1U);
  if(n <= 0)
    return NULL;

  path[n] = 0;
  return _path_dirname_copy(path);
}


static
char *
_path_in_executable_dir(const char *filename_)
{
  char *dir;
  char *rv;

  dir = _executable_dir();
  if(dir == NULL)
    return _xstrdup(filename_);

  rv = _xasprintf2(dir, filename_);
  free(dir);
  return rv;
}


static
char *
_trim_whitespace(char *text_)
{
  char *end;

  while((*text_ == ' ') || (*text_ == '\t') ||
        (*text_ == '\r') || (*text_ == '\n'))
    text_++;

  end = text_ + strlen(text_);
  while((end > text_) &&
        ((end[-1] == ' ') || (end[-1] == '\t') ||
         (end[-1] == '\r') || (end[-1] == '\n')))
    end--;
  *end = 0;

  return text_;
}


static
void
_path_list_add(path_list_t *list_,
               const char  *path_)
{
  char *expanded;
  size_t i;

  if((path_ == NULL) || (path_[0] == 0))
    return;

  expanded = _expand_user_path(path_);
  if(expanded == NULL)
    return;

  if(!_path_is_directory(expanded))
    {
      free(expanded);
      return;
    }

  for(i = 0; i < list_->count; i++)
    {
      if(strcmp(list_->items[i], expanded) == 0)
        {
          free(expanded);
          return;
        }
    }

  list_->items = (char **)realloc(list_->items,
                                  sizeof(list_->items[0]) *
                                  (list_->count + 1));
  if(list_->items == NULL)
    {
      perror("realloc");
      exit(1);
    }

  list_->items[list_->count] = expanded;
  list_->count++;
}


static
void
_path_list_free(path_list_t *list_)
{
  size_t i;

  for(i = 0; i < list_->count; i++)
    free(list_->items[i]);

  free(list_->items);
  list_->items = NULL;
  list_->count = 0;
}


static
char *
_retroarch_config_value(char *line_)
{
  char *value;
  char quote;

  line_ = _trim_whitespace(line_);
  if((line_[0] == 0) || (line_[0] == '#'))
    return NULL;

  if(strncmp(line_, "system_directory", 16) != 0)
    return NULL;

  value = line_ + 16;
  if((*value != ' ') && (*value != '\t') && (*value != '='))
    return NULL;

  while((*value == ' ') || (*value == '\t'))
    value++;

  if(*value != '=')
    return NULL;
  value++;

  value = _trim_whitespace(value);
  if(value[0] == 0)
    return NULL;

  quote = value[0];
  if((quote == '"') || (quote == '\''))
    {
      char *end;

      value++;
      end = strrchr(value, quote);
      if(end != NULL)
        *end = 0;
    }

  value = _trim_whitespace(value);
  if((value[0] == 0) || (strcmp(value, "default") == 0))
    return NULL;

  return value;
}


static
char *
_read_retroarch_system_dir(const char *config_path_)
{
  FILE *f;
  char line[4096];
  char *expanded_config;
  char *config_dir;
  char *rv = NULL;

  expanded_config = _expand_user_path(config_path_);
  if(expanded_config == NULL)
    return NULL;

  f = fopen(expanded_config, "r");
  if(f == NULL)
    {
      free(expanded_config);
      return NULL;
    }

  config_dir = _path_dirname_copy(expanded_config);

  while(fgets(line, sizeof(line), f) != NULL)
    {
      char *value;

      value = _retroarch_config_value(line);
      if(value == NULL)
        continue;

      if((value[0] == '/') || (value[0] == '~'))
        rv = _expand_user_path(value);
      else
        rv = _xasprintf2(config_dir, value);
      break;
    }

  free(config_dir);
  fclose(f);
  free(expanded_config);
  return rv;
}


static
void
_collect_retroarch_rom_dirs(path_list_t *dirs_)
{
  const char *env;
  size_t i;

  env = getenv("RETROARCH_SYSTEM_DIRECTORY");
  _path_list_add(dirs_, env);
  env = getenv("LIBRETRO_SYSTEM_DIRECTORY");
  _path_list_add(dirs_, env);

  for(i = 0; RETROARCH_CONFIG_PATHS[i] != NULL; i++)
    {
      char *system_dir;

      system_dir = _read_retroarch_system_dir(RETROARCH_CONFIG_PATHS[i]);
      _path_list_add(dirs_, system_dir);
      free(system_dir);
    }

  for(i = 0; RETROARCH_ROM_DIRS[i] != NULL; i++)
    _path_list_add(dirs_, RETROARCH_ROM_DIRS[i]);
}


static
char *
_find_rom_filename_in_retroarch_dirs(const char *filename_)
{
  path_list_t dirs;
  char *rv = NULL;
  size_t i;

  memset(&dirs, 0, sizeof(dirs));
  _collect_retroarch_rom_dirs(&dirs);

  for(i = 0; i < dirs.count; i++)
    {
      char *candidate;

      candidate = _xasprintf2(dirs.items[i], filename_);
      if(_path_is_regular_file(candidate))
        {
          rv = _resolve_existing_path(candidate);
          if(rv == NULL)
            rv = _xstrdup(candidate);
          free(candidate);
          break;
        }

      free(candidate);
    }

  _path_list_free(&dirs);
  return rv;
}


static
char *
_resolve_rom_request(const char *path_)
{
  char *resolved;
  char *candidate;

  resolved = _resolve_existing_path(path_);
  if(resolved != NULL)
    return resolved;

  if(_path_has_separator(path_))
    return NULL;

  candidate = _path_in_executable_dir(path_);
  if(candidate != NULL)
    {
      if(_path_is_regular_file(candidate))
        {
          resolved = _resolve_existing_path(candidate);
          if(resolved == NULL)
            resolved = _xstrdup(candidate);
          free(candidate);
          return resolved;
        }
      free(candidate);
    }

  return _find_rom_filename_in_retroarch_dirs(path_);
}


static
void
_print_bios_list(FILE *f_)
{
  size_t i;

  fprintf(f_, "Recognized BIOS ROMs:\n");
  fprintf(f_, "  %-28s %10s  %-10s  %s\n",
          "filename", "size", "crc32", "name");

  for(i = 0; BIOS_ROMS[i].filename != NULL; i++)
    {
      char *resolved;

      fprintf(f_, "  %-28s %10" PRIu64 "  %08" PRIx32 "  %s\n",
              BIOS_ROMS[i].filename,
              BIOS_ROMS[i].size,
              BIOS_ROMS[i].crc32,
              BIOS_ROMS[i].name);

      resolved = _resolve_rom_request(BIOS_ROMS[i].filename);
      if(resolved != NULL)
        {
          fprintf(f_, "    found: %s\n", resolved);
          free(resolved);
        }
    }
}


static
int
_remove_tree(const char *path_)
{
  DIR *dir;
  struct dirent *ent;
  struct stat st;

  if(lstat(path_, &st) != 0)
    return 0;

  if(!S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
    return unlink(path_);

  dir = opendir(path_);
  if(dir == NULL)
    return -1;

  while((ent = readdir(dir)) != NULL)
    {
      char *child;

      if((strcmp(ent->d_name, ".") == 0) ||
         (strcmp(ent->d_name, "..") == 0))
        continue;

      child = _xasprintf2(path_, ent->d_name);
      if(_remove_tree(child) != 0)
        {
          free(child);
          closedir(dir);
          return -1;
        }
      free(child);
    }

  closedir(dir);
  return rmdir(path_);
}


static
void
_report_error(const char *fmt_,
              ...)
{
  va_list ap;

  va_start(ap, fmt_);
  if(g_run.saved_stderr >= 0)
    {
      va_list copy;
      va_copy(copy, ap);
      vdprintf(g_run.saved_stderr, fmt_, copy);
      va_end(copy);
    }
  else
    {
      va_list copy;
      va_copy(copy, ap);
      vfprintf(stderr, fmt_, copy);
      va_end(copy);
    }

  if(g_run.log_file != NULL)
    {
      vfprintf(g_run.log_file, fmt_, ap);
      fflush(g_run.log_file);
    }
  va_end(ap);
}


static
int
_parse_u64(const char *s_,
           uint64_t   *out_)
{
  const char *p;
  char *end;
  unsigned long long value;

  p = s_;
  while((*p == ' ') || (*p == '\t') ||
        (*p == '\r') || (*p == '\n'))
    p++;

  if((*p < '0') || (*p > '9'))
    return -1;

  errno = 0;
  value = strtoull(s_, &end, 10);
  if((errno != 0) || (end == s_) || (*end != 0))
    return -1;

  *out_ = (uint64_t)value;
  return 0;
}


static
int
_parse_int_value(const char *s_,
                 int        *out_)
{
  char *end;
  long value;

  errno = 0;
  value = strtol(s_, &end, 10);
  if((errno != 0) || (end == s_) || (*end != 0) ||
     (value < 0) || (value > INT_MAX))
    return -1;

  *out_ = (int)value;
  return 0;
}


static
int
_parse_double_value(const char *s_,
                    double     *out_)
{
  char *end;
  double value;

  errno = 0;
  value = strtod(s_, &end);
  if((errno != 0) || (end == s_) || (*end != 0) ||
     !isfinite(value) || (value < 0.0))
    return -1;

  *out_ = value;
  return 0;
}


static
void
_normalize_option_key(const char *key_,
                      char       *out_,
                      size_t      out_size_)
{
  if(strncmp(key_, "opera_", 6) == 0)
    snprintf(out_, out_size_, "%s", key_);
  else
    snprintf(out_, out_size_, "opera_%s", key_);
}


static
bool
_option_exists(const char *key_)
{
  char normalized[128];
  size_t i;

  _normalize_option_key(key_, normalized, sizeof(normalized));
  for(i = 0; i < g_cfg.option_count; i++)
    {
      if(strcmp(g_cfg.options[i].key, normalized) == 0)
        return true;
    }

  return false;
}


static
void
_add_option_normalized(const char *key_,
                       const char *value_)
{
  char normalized[128];
  size_t i;

  if((key_ == NULL) || (key_[0] == 0) || (value_ == NULL))
    {
      fprintf(stderr, "invalid option\n");
      exit(1);
    }

  _normalize_option_key(key_, normalized, sizeof(normalized));

  for(i = 0; i < g_cfg.option_count; i++)
    {
      if(strcmp(g_cfg.options[i].key, normalized) != 0)
        continue;

      free(g_cfg.options[i].value);
      g_cfg.options[i].value = _xstrdup(value_);
      return;
    }

  g_cfg.options = (harness_option_t *)realloc(g_cfg.options,
                                              sizeof(g_cfg.options[0]) * (g_cfg.option_count + 1));
  if(g_cfg.options == NULL)
    {
      perror("realloc");
      exit(1);
    }

  g_cfg.options[g_cfg.option_count].key   = _xstrdup(normalized);
  g_cfg.options[g_cfg.option_count].value = _xstrdup(value_);
  g_cfg.option_count++;
}


static
void
_add_option_default(const char *key_,
                    const char *value_)
{
  if((key_ == NULL) || (key_[0] == 0) || (value_ == NULL))
    return;

  if(_option_exists(key_))
    return;

  _add_option_normalized(key_, value_);
}


static
void
_add_option_from_arg(const char *arg_)
{
  char *copy;
  char *eq;

  copy = _xstrdup(arg_);
  eq = strchr(copy, '=');
  if((eq == NULL) || (eq == copy))
    {
      fprintf(stderr, "invalid --option, expected KEY=VALUE: %s\n", arg_);
      exit(1);
    }

  *eq = 0;
  _add_option_normalized(copy, eq + 1);
  free(copy);
}


static
int
_input_button_id(const char *button_,
                 unsigned   *id_)
{
  if(!strcasecmp(button_, "A"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_Y;
  else if(!strcasecmp(button_, "B"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_B;
  else if(!strcasecmp(button_, "C"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_A;
  else if(!strcasecmp(button_, "X") || !strcasecmp(button_, "STOP"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_SELECT;
  else if(!strcasecmp(button_, "P") ||
          !strcasecmp(button_, "PLAY") ||
          !strcasecmp(button_, "PAUSE") ||
          !strcasecmp(button_, "START"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_START;
  else if(!strcasecmp(button_, "L"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_L;
  else if(!strcasecmp(button_, "R"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_R;
  else if(!strcasecmp(button_, "UP"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_UP;
  else if(!strcasecmp(button_, "DOWN"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_DOWN;
  else if(!strcasecmp(button_, "LEFT"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_LEFT;
  else if(!strcasecmp(button_, "RIGHT"))
    *id_ = RETRO_DEVICE_ID_JOYPAD_RIGHT;
  else
    return -1;

  return 0;
}


static
int
_parse_input_component(const char *text_,
                       bool       *is_seconds_,
                       double     *seconds_,
                       uint64_t   *frames_)
{
  char *number;
  char unit;
  size_t len;
  int rv;

  if((text_ == NULL) || (text_[0] == 0))
    return -1;

  len = strlen(text_);
  unit = text_[len - 1];
  number = _xstrdup(text_);
  number[len - 1] = 0;

  if((unit == 'S') || (unit == 's'))
    {
      *is_seconds_ = true;
      rv = _parse_double_value(number, seconds_);
      *frames_ = 0;
    }
  else if((unit == 'F') || (unit == 'f'))
    {
      *is_seconds_ = false;
      rv = _parse_u64(number, frames_);
      *seconds_ = 0.0;
    }
  else
    {
      rv = -1;
    }

  free(number);
  return rv;
}


static
void
_add_input_from_arg(const char *arg_)
{
  char *copy;
  char *eq;
  char *plus;
  input_event_t event;

  memset(&event, 0, sizeof(event));

  copy = _xstrdup(arg_);
  eq = strchr(copy, '=');
  if((eq == NULL) || (eq == copy) || (eq[1] == 0))
    {
      fprintf(stderr, "invalid --input, expected WHEN=BUTTON: %s\n", arg_);
      exit(1);
    }

  *eq = 0;
  plus = strchr(copy, '+');
  if(plus != NULL)
    *plus = 0;

  if(_parse_input_component(copy,
                            &event.start_is_seconds,
                            &event.start_seconds,
                            &event.start_frame) != 0)
    {
      fprintf(stderr, "invalid --input start component: %s\n", arg_);
      exit(1);
    }

  if(plus != NULL)
    {
      if(_parse_input_component(plus + 1,
                                &event.duration_is_seconds,
                                &event.duration_seconds,
                                &event.duration_frames) != 0)
        {
          fprintf(stderr, "invalid --input duration component: %s\n", arg_);
          exit(1);
        }

      if((event.duration_is_seconds && (event.duration_seconds <= 0.0)) ||
         (!event.duration_is_seconds && (event.duration_frames == 0)))
        {
          fprintf(stderr, "invalid --input duration, must be greater than zero: %s\n", arg_);
          exit(1);
        }
    }
  else
    {
      event.duration_is_seconds = false;
      event.duration_frames = 1;
    }

  if(_input_button_id(eq + 1, &event.id) != 0)
    {
      fprintf(stderr,
              "invalid --input button '%s' in %s; supported: A B C X P L R UP DOWN LEFT RIGHT\n",
              eq + 1,
              arg_);
      exit(1);
    }

  event.spec = _xstrdup(arg_);
  event.button = _xstrdup(eq + 1);

  g_cfg.inputs = (input_event_t *)realloc(g_cfg.inputs,
                                          sizeof(g_cfg.inputs[0]) *
                                          (g_cfg.input_count + 1));
  if(g_cfg.inputs == NULL)
    {
      perror("realloc");
      exit(1);
    }

  g_cfg.inputs[g_cfg.input_count] = event;
  g_cfg.input_count++;
  free(copy);
}


static
void
_add_screenshot_arg(const char *arg_)
{
  char *copy;
  char *eq;
  uint64_t frame;

  copy = _xstrdup(arg_);
  eq = strchr(copy, '=');
  if((eq == NULL) || (eq == copy) || (eq[1] == 0))
    {
      fprintf(stderr, "invalid --screenshot-at, expected FRAME=PATH: %s\n", arg_);
      exit(1);
    }

  *eq = 0;
  if((_parse_u64(copy, &frame) != 0) || (frame == 0))
    {
      fprintf(stderr, "invalid screenshot frame: %s\n", copy);
      exit(1);
    }

  g_cfg.screenshots = (screenshot_request_t *)realloc(g_cfg.screenshots,
                                                      sizeof(g_cfg.screenshots[0]) *
                                                      (g_cfg.screenshot_count + 1));
  if(g_cfg.screenshots == NULL)
    {
      perror("realloc");
      exit(1);
    }

  memset(&g_cfg.screenshots[g_cfg.screenshot_count],
         0,
         sizeof(g_cfg.screenshots[g_cfg.screenshot_count]));
  g_cfg.screenshots[g_cfg.screenshot_count].frame = frame;
  g_cfg.screenshots[g_cfg.screenshot_count].path  = _xstrdup(eq + 1);
  g_cfg.screenshot_count++;
  free(copy);
}


static
void
_add_screenshot_every_arg(const char *arg_)
{
  char *copy;
  char *eq;
  uint64_t step;

  copy = _xstrdup(arg_);
  eq = strchr(copy, '=');
  if((eq == NULL) || (eq == copy) || (eq[1] == 0))
    {
      fprintf(stderr, "invalid --screenshot-every, expected STEP=DIR: %s\n", arg_);
      exit(1);
    }

  *eq = 0;
  if((_parse_u64(copy, &step) != 0) || (step == 0))
    {
      fprintf(stderr, "invalid screenshot-every step: %s\n", copy);
      exit(1);
    }

  if(_mkdir_p(eq + 1) != 0)
    {
      fprintf(stderr, "unable to create screenshot-every dir: %s\n", eq + 1);
      exit(1);
    }

  g_cfg.screenshot_every_dir  = _xstrdup(eq + 1);
  g_cfg.screenshot_every_step = step;
  free(copy);
}


static
void
_add_screenshot_when_arg(const char *arg_)
{
  if(!strcasecmp(arg_, "NONBLANK"))
    g_cfg.screenshot_when_mode = SCREENSHOT_WHEN_NONBLANK;
  else if(!strcasecmp(arg_, "CHANGED"))
    g_cfg.screenshot_when_mode = SCREENSHOT_WHEN_CHANGED;
  else
    {
      fprintf(stderr,
              "invalid --screenshot-when value: %s; expected NONBLANK or CHANGED\n",
              arg_);
      exit(1);
    }
}


static
int
_parse_terminal_render_mode(const char *arg_)
{
  if(!strcasecmp(arg_, "auto"))
    return -1;
  if(!strcasecmp(arg_, "kitty"))
    return HARNESS_TERMINAL_RENDER_KITTY;
  if(!strcasecmp(arg_, "sixel"))
    return HARNESS_TERMINAL_RENDER_SIXEL;
  if(!strcasecmp(arg_, "half") || !strcasecmp(arg_, "unicode"))
    return HARNESS_TERMINAL_RENDER_HALF;
  if(!strcasecmp(arg_, "ascii"))
    return HARNESS_TERMINAL_RENDER_ASCII;

  fprintf(stderr,
          "invalid --terminal-render value: %s; expected auto, kitty, sixel, half, or ascii\n",
          arg_);
  exit(1);
}


static
int
_parse_terminal_color_mode(const char *arg_)
{
  if(!strcasecmp(arg_, "auto"))
    return -1;
  if(!strcasecmp(arg_, "true") || !strcasecmp(arg_, "truecolor") ||
     !strcasecmp(arg_, "24bit"))
    return HARNESS_TERMINAL_COLOR_TRUE;
  if(!strcasecmp(arg_, "256") || !strcasecmp(arg_, "256color"))
    return HARNESS_TERMINAL_COLOR_256;
  if(!strcasecmp(arg_, "mono") || !strcasecmp(arg_, "none"))
    return HARNESS_TERMINAL_COLOR_MONO;

  fprintf(stderr,
          "invalid --terminal-color value: %s; expected auto, true, 256, or mono\n",
          arg_);
  exit(1);
}


static
void
_add_inputs_from_file(const char *path_)
{
  FILE *f;
  char *line;
  size_t cap;
  ssize_t n;

  f = fopen(path_, "r");
  if(f == NULL)
    {
      fprintf(stderr, "unable to open --input-file: %s: %s\n",
              path_, strerror(errno));
      exit(1);
    }

  line = NULL;
  cap = 0;
  while((n = getline(&line, &cap, f)) != -1)
    {
      char *s;
      char *e;

      if(n > 0 && line[n - 1] == '\n')
        {
          line[n - 1] = 0;
          n--;
        }
      if(n > 0 && line[n - 1] == '\r')
        {
          line[n - 1] = 0;
          n--;
        }

      s = line;
      while((*s == ' ') || (*s == '\t'))
        s++;

      if((*s == 0) || (*s == '#'))
        continue;

      e = s + strlen(s);
      while((e > s) && ((e[-1] == ' ') || (e[-1] == '\t')))
        {
          e--;
          *e = 0;
        }

      if(*s != 0)
        _add_input_from_arg(s);
    }

  free(line);
  fclose(f);
}


static
void
_parse_args(int    argc_,
            char **argv_)
{
  int i;

  memset(&g_cfg, 0, sizeof(g_cfg));
  g_run.saved_stdout = -1;
  g_run.saved_stderr = -1;
  g_run.log_fd       = -1;
  g_run.tty_fd       = -1;
  g_run.tty_flags    = -1;
  g_run.pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
  g_run.terminal_render_mode = -1;
  g_run.terminal_color_mode = -1;
  g_run.terminal_kitty_probe_result = "not_run";
  g_run.terminal_kitty_zlib_probe_result = "not_run";
  g_run.terminal_sixel_probe_result = "not_run";
  _terminal_set_pixel_reader(g_run.pixel_format);
  g_cfg.terminal_button_hold_frames = HARNESS_TERMINAL_HOLD_FRAMES_DEFAULT;
  g_cfg.terminal_fps = HARNESS_TERMINAL_FPS_DEFAULT;
  g_cfg.terminal_render_override = -1;
  g_cfg.terminal_color_override = -1;

  for(i = 1; i < argc_; i++)
    {
      const char *arg = argv_[i];

      if(strcmp(arg, "--help") == 0)
        {
          _print_usage(stdout);
          exit(0);
        }
      else if(strcmp(arg, "--core") == 0)
        g_cfg.core_path = _xstrdup(argv_[++i]);
      else if(strcmp(arg, "--bios") == 0)
        g_cfg.bios_path = _xstrdup(argv_[++i]);
      else if(strcmp(arg, "--font") == 0)
        g_cfg.font_path = _xstrdup(argv_[++i]);
      else if(strcmp(arg, "--title") == 0)
        g_cfg.title_path = _xstrdup(argv_[++i]);
      else if(strcmp(arg, "--output-dir") == 0)
        g_cfg.output_dir = _xstrdup(argv_[++i]);
      else if(strcmp(arg, "--work-dir") == 0)
        {
          g_cfg.work_dir      = _xstrdup(argv_[++i]);
          g_cfg.user_work_dir = true;
        }
      else if(strcmp(arg, "--log") == 0)
        g_cfg.log_path = _xstrdup(argv_[++i]);
      else if(strcmp(arg, "--metrics") == 0)
        g_cfg.metrics_path = _xstrdup(argv_[++i]);
      else if(strcmp(arg, "--audio") == 0)
        {
          free(g_cfg.audio_path);
          g_cfg.audio_path = _xstrdup(argv_[++i]);
          g_cfg.no_audio = false;
        }
      else if(strcmp(arg, "--terminal") == 0)
        g_cfg.terminal_mode = true;
      else if(strcmp(arg, "--terminal-fps") == 0)
        {
          if((_parse_double_value(argv_[++i], &g_cfg.terminal_fps) != 0) ||
             !isfinite(g_cfg.terminal_fps) || (g_cfg.terminal_fps < 0.0))
            {
              fprintf(stderr, "--terminal-fps must be a non-negative number\n");
              exit(1);
            }
          g_cfg.terminal_fps_user = true;
        }
      else if(strcmp(arg, "--terminal-render") == 0)
        g_cfg.terminal_render_override = _parse_terminal_render_mode(argv_[++i]);
      else if(strcmp(arg, "--terminal-color") == 0)
        g_cfg.terminal_color_override = _parse_terminal_color_mode(argv_[++i]);
      else if(strcmp(arg, "--terminal-button-hold") == 0)
        {
          uint64_t hold_frames;

          if(_parse_u64(argv_[++i], &hold_frames) != 0)
            {
              fprintf(stderr, "invalid --terminal-button-hold value\n");
              exit(1);
            }

          if((hold_frames == 0) || (hold_frames > UINT_MAX))
            {
              fprintf(stderr, "--terminal-button-hold must be between 1 and %u\n", UINT_MAX);
              exit(1);
            }

          g_cfg.terminal_button_hold_frames = (unsigned)hold_frames;
        }
      else if(strcmp(arg, "--screenshot") == 0)
        g_cfg.screenshot_path = _xstrdup(argv_[++i]);
      else if(strcmp(arg, "--frames") == 0)
        {
          if(_parse_u64(argv_[++i], &g_cfg.frames) != 0)
            {
              fprintf(stderr, "invalid --frames value\n");
              exit(1);
            }
          g_cfg.have_frames = true;
        }
      else if(strcmp(arg, "--benchmark-start-frame") == 0)
        {
          if((_parse_u64(argv_[++i], &g_cfg.benchmark_start_frame) != 0) ||
             (g_cfg.benchmark_start_frame == 0))
            {
              fprintf(stderr, "invalid --benchmark-start-frame value\n");
              exit(1);
            }
          g_cfg.have_benchmark_start_frame = true;
        }
      else if(strcmp(arg, "--benchmark-end-frame") == 0)
        {
          if((_parse_u64(argv_[++i], &g_cfg.benchmark_end_frame) != 0) ||
             (g_cfg.benchmark_end_frame == 0))
            {
              fprintf(stderr, "invalid --benchmark-end-frame value\n");
              exit(1);
            }
          g_cfg.have_benchmark_end_frame = true;
        }
      else if(strcmp(arg, "--seconds") == 0)
        {
          if(_parse_double_value(argv_[++i], &g_cfg.seconds) != 0)
            {
              fprintf(stderr, "invalid --seconds value\n");
              exit(1);
            }
          g_cfg.have_seconds = true;
        }
      else if(strcmp(arg, "--wall-timeout") == 0)
        {
          if(_parse_double_value(argv_[++i], &g_cfg.wall_timeout) != 0)
            {
              fprintf(stderr, "invalid --wall-timeout value\n");
              exit(1);
            }
        }
      else if(strcmp(arg, "--cpu") == 0)
        {
          if(_parse_int_value(argv_[++i], &g_cfg.cpu) != 0)
            {
              fprintf(stderr, "invalid --cpu value\n");
              exit(1);
            }
          g_cfg.have_cpu = true;
        }
      else if(strcmp(arg, "--option") == 0)
        _add_option_from_arg(argv_[++i]);
      else if(strcmp(arg, "--input") == 0)
        _add_input_from_arg(argv_[++i]);
      else if(strcmp(arg, "--input-file") == 0)
        _add_inputs_from_file(argv_[++i]);
      else if(strcmp(arg, "--screenshot-at") == 0)
        _add_screenshot_arg(argv_[++i]);
      else if(strcmp(arg, "--screenshot-every") == 0)
        _add_screenshot_every_arg(argv_[++i]);
      else if(strcmp(arg, "--screenshot-when") == 0)
        _add_screenshot_when_arg(argv_[++i]);
      else if(strcmp(arg, "--keep-work-dir") == 0)
        g_cfg.keep_work_dir = true;
      else if(strcmp(arg, "--no-audio") == 0)
        {
          g_cfg.no_audio = true;
          free(g_cfg.audio_path);
          g_cfg.audio_path = NULL;
        }
      else if(strcmp(arg, "--verbose") == 0)
        g_cfg.verbose = true;
      else if(strcmp(arg, "--list-bios") == 0)
        g_cfg.list_bios = true;
      else
        {
          fprintf(stderr, "unknown argument: %s\n", arg);
          _print_usage(stderr);
          exit(1);
        }
    }
}


static
void
_validate_arg_followers(int    argc_,
                        char **argv_)
{
  int i;

  for(i = 1; i < argc_; i++)
    {
      const char *arg = argv_[i];
      bool needs_value =
        !strcmp(arg, "--core") ||
        !strcmp(arg, "--bios") ||
        !strcmp(arg, "--font") ||
        !strcmp(arg, "--title") ||
        !strcmp(arg, "--output-dir") ||
        !strcmp(arg, "--work-dir") ||
        !strcmp(arg, "--log") ||
        !strcmp(arg, "--metrics") ||
        !strcmp(arg, "--audio") ||
        !strcmp(arg, "--screenshot") ||
        !strcmp(arg, "--frames") ||
        !strcmp(arg, "--benchmark-start-frame") ||
        !strcmp(arg, "--benchmark-end-frame") ||
        !strcmp(arg, "--seconds") ||
        !strcmp(arg, "--wall-timeout") ||
        !strcmp(arg, "--cpu") ||
        !strcmp(arg, "--terminal-fps") ||
        !strcmp(arg, "--terminal-render") ||
        !strcmp(arg, "--terminal-color") ||
        !strcmp(arg, "--terminal-button-hold") ||
        !strcmp(arg, "--option") ||
        !strcmp(arg, "--input") ||
        !strcmp(arg, "--input-file") ||
        !strcmp(arg, "--screenshot-at") ||
        !strcmp(arg, "--screenshot-every") ||
        !strcmp(arg, "--screenshot-when");

      if(needs_value)
        {
          if((i + 1 >= argc_) || (strncmp(argv_[i + 1], "--", 2) == 0))
            {
              fprintf(stderr, "%s requires a value\n", arg);
              exit(1);
            }
          i++;
        }
    }
}


static
char *
_default_output_dir(void)
{
  char stamp[64];
  char leaf[128];
  time_t now;
  struct tm tm_now;

  now = time(NULL);
  localtime_r(&now, &tm_now);
  strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm_now);
  snprintf(leaf, sizeof(leaf), "%s-%ld", stamp, (long)getpid());

  if(_mkdir_p("test-harness-runs") != 0)
    {
      perror("mkdir test-harness-runs");
      exit(1);
    }

  return _xasprintf2("test-harness-runs", leaf);
}


static
void
_prepare_paths(void)
{
  char *resolved;

  if(g_cfg.core_path == NULL)
    g_cfg.core_path = _path_in_executable_dir("opera_libretro.so");

  if(g_cfg.bios_path == NULL)
    {
      char *default_bios = _path_in_executable_dir("panafz1.bin");

      if((default_bios != NULL) && _path_is_regular_file(default_bios))
        g_cfg.bios_path = default_bios;
      else
        {
          free(default_bios);
          g_cfg.bios_path = _xstrdup("panafz1.bin");
        }
    }

  resolved = _resolve_existing_path(g_cfg.core_path);
  if(resolved == NULL)
    {
      fprintf(stderr, "unable to resolve core path: %s\n", g_cfg.core_path);
      exit(1);
    }
  free(g_cfg.core_path);
  g_cfg.core_path = resolved;

  resolved = _resolve_rom_request(g_cfg.bios_path);
  if(resolved == NULL)
    {
      if(_path_has_separator(g_cfg.bios_path))
        fprintf(stderr, "unable to resolve BIOS path: %s\n", g_cfg.bios_path);
      else
        fprintf(stderr,
                "unable to find BIOS file %s beside test-harness, "
                "in the current directory, or common RetroArch "
                "system directories\n",
                g_cfg.bios_path);
      exit(1);
    }
  free(g_cfg.bios_path);
  g_cfg.bios_path = resolved;

  if(g_cfg.font_path != NULL)
    {
      resolved = _resolve_rom_request(g_cfg.font_path);
      if(resolved == NULL)
        {
          if(_path_has_separator(g_cfg.font_path))
            fprintf(stderr, "unable to resolve font path: %s\n", g_cfg.font_path);
          else
            fprintf(stderr,
                    "unable to find font file %s in the current directory "
                    "or common RetroArch system directories\n",
                    g_cfg.font_path);
          exit(1);
        }
      free(g_cfg.font_path);
      g_cfg.font_path = resolved;
    }

  if(g_cfg.title_path != NULL)
    {
      resolved = _resolve_existing_path(g_cfg.title_path);
      if(resolved == NULL)
        {
          fprintf(stderr, "unable to resolve title path: %s\n", g_cfg.title_path);
          exit(1);
        }
      free(g_cfg.title_path);
      g_cfg.title_path = resolved;
    }

  if(g_cfg.output_dir == NULL)
    g_cfg.output_dir = _default_output_dir();

  if(_mkdir_p(g_cfg.output_dir) != 0)
    {
      fprintf(stderr, "unable to create output dir: %s\n", g_cfg.output_dir);
      exit(1);
    }

  if(g_cfg.work_dir == NULL)
    g_cfg.work_dir = _xasprintf2(g_cfg.output_dir, "work");

  g_cfg.system_dir = _xasprintf2(g_cfg.work_dir, "system");
  g_cfg.save_dir   = _xasprintf2(g_cfg.work_dir, "save");

  if((_mkdir_p(g_cfg.system_dir) != 0) || (_mkdir_p(g_cfg.save_dir) != 0))
    {
      fprintf(stderr, "unable to create work directories under: %s\n", g_cfg.work_dir);
      exit(1);
    }

  if(g_cfg.log_path == NULL)
    g_cfg.log_path = _xasprintf2(g_cfg.output_dir, "run.log");
  if(g_cfg.metrics_path == NULL)
    g_cfg.metrics_path = _xasprintf2(g_cfg.output_dir, "metrics.json");

  if(_mkdir_parent(g_cfg.log_path) != 0)
    {
      fprintf(stderr, "unable to create log parent dir: %s\n", g_cfg.log_path);
      exit(1);
    }
  if(_mkdir_parent(g_cfg.metrics_path) != 0)
    {
      fprintf(stderr, "unable to create metrics parent dir: %s\n", g_cfg.metrics_path);
      exit(1);
    }
}


static
int
_setup_logging(void)
{
  g_run.saved_stdout = dup(STDOUT_FILENO);
  g_run.saved_stderr = dup(STDERR_FILENO);
  if((g_run.saved_stdout < 0) || (g_run.saved_stderr < 0))
    return -1;

  g_run.log_fd = open(g_cfg.log_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if(g_run.log_fd < 0)
    return -1;

  fflush(NULL);
  if((dup2(g_run.log_fd, STDOUT_FILENO) < 0) ||
     (dup2(g_run.log_fd, STDERR_FILENO) < 0))
    return -1;

  g_run.log_file = fdopen(dup(g_run.log_fd), "a");
  if(g_run.log_file == NULL)
    return -1;

  setvbuf(g_run.log_file, NULL, _IOLBF, 0);
  return 0;
}


static
void
_restore_logging(void)
{
  fflush(NULL);

  if(g_run.saved_stdout >= 0)
    {
      dup2(g_run.saved_stdout, STDOUT_FILENO);
      close(g_run.saved_stdout);
      g_run.saved_stdout = -1;
    }

  if(g_run.saved_stderr >= 0)
    {
      dup2(g_run.saved_stderr, STDERR_FILENO);
      close(g_run.saved_stderr);
      g_run.saved_stderr = -1;
    }

  if(g_run.log_file != NULL)
    {
      fclose(g_run.log_file);
      g_run.log_file = NULL;
    }

  if(g_run.log_fd >= 0)
    {
      close(g_run.log_fd);
      g_run.log_fd = -1;
    }
}


static
int
_apply_cpu_affinity(void)
{
  if(!g_cfg.have_cpu)
    return 0;

#ifdef __linux__
  {
    cpu_set_t set;

    if(g_cfg.cpu >= CPU_SETSIZE)
      {
        _report_error("requested CPU %d exceeds CPU_SETSIZE %d\n",
                      g_cfg.cpu, CPU_SETSIZE);
        return -1;
      }

    CPU_ZERO(&set);
    CPU_SET(g_cfg.cpu, &set);

    if(sched_setaffinity(0, sizeof(set), &set) != 0)
      {
        _report_error("failed to pin process to CPU %d: %s\n",
                      g_cfg.cpu, strerror(errno));
        return -1;
      }

    g_run.cpu_affinity_applied = true;
    _harness_log(RETRO_LOG_INFO,
                 "[Harness]: pinned process affinity to CPU %d\n",
                 g_cfg.cpu);
    return 0;
  }
#else
  _report_error("--cpu is only supported on Linux in this harness\n");
  return -1;
#endif
}


static
uint32_t
_crc32_file(FILE *f_)
{
  uint8_t buf[32768];
  uint32_t crc = 0xffffffffU;
  size_t n;

  while((n = fread(buf, 1, sizeof(buf), f_)) > 0)
    {
      size_t i;
      for(i = 0; i < n; i++)
        {
          int bit;
          crc ^= buf[i];
          for(bit = 0; bit < 8; bit++)
            {
              uint32_t mask = 0U - (crc & 1U);
              crc = (crc >> 1) ^ (0xedb88320U & mask);
            }
        }
    }

  return ~crc;
}


static
int
_file_crc32_and_size(const char *path_,
                     uint32_t   *crc_out_,
                     uint64_t   *size_out_)
{
  FILE *f;
  struct stat st;

  if(stat(path_, &st) != 0)
    return -1;

  f = fopen(path_, "rb");
  if(f == NULL)
    return -1;

  *crc_out_  = _crc32_file(f);
  *size_out_ = (uint64_t)st.st_size;

  if(ferror(f))
    {
      fclose(f);
      return -1;
    }

  fclose(f);
  return 0;
}


static
const
rom_entry_t *
_find_rom_by_basename(const rom_entry_t *entries_,
                      const char        *path_)
{
  const char *base = _path_basename(path_);

  while(entries_->filename != NULL)
    {
      if(strcmp(base, entries_->filename) == 0)
        return entries_;
      entries_++;
    }

  return NULL;
}


static
const
rom_entry_t *
_find_rom_by_hash(const rom_entry_t *entries_,
                  uint64_t           size_,
                  uint32_t           crc_)
{
  while(entries_->filename != NULL)
    {
      if((entries_->size == size_) && (entries_->crc32 == crc_))
        return entries_;
      entries_++;
    }

  return NULL;
}


static
bool
_same_file(const char *a_,
           const char *b_)
{
  struct stat sa;
  struct stat sb;

  return ((stat(a_, &sa) == 0) &&
          (stat(b_, &sb) == 0) &&
          (sa.st_dev == sb.st_dev) &&
          (sa.st_ino == sb.st_ino));
}


static
int
_stage_rom(const char        *source_path_,
           const rom_entry_t *entries_,
           const char        *kind_,
           char             **filename_out_,
           char             **name_out_)
{
  const rom_entry_t *by_base;
  const rom_entry_t *by_hash;
  const rom_entry_t *entry;
  uint32_t crc;
  uint64_t size;
  char *target;

  if(_file_crc32_and_size(source_path_, &crc, &size) != 0)
    {
      _report_error("unable to inspect %s ROM: %s\n", kind_, source_path_);
      return -1;
    }

  by_base = _find_rom_by_basename(entries_, source_path_);
  by_hash = _find_rom_by_hash(entries_, size, crc);

  if((by_base != NULL) && (by_hash != by_base))
    {
      if(by_hash != NULL)
        _report_error("%s ROM basename matched %s but content matched %s; "
                      "use a filename that matches the ROM content\n",
                      kind_, by_base->filename, by_hash->filename);
      else
        _report_error("%s ROM basename matched %s but content did not match "
                      "the known size/crc32 (got size=%" PRIu64 " "
                      "crc32=%08" PRIx32 ", expected size=%" PRIu64 " "
                      "crc32=%08" PRIx32 ")\n",
                      kind_, by_base->filename, size, crc, by_base->size, by_base->crc32);
      return -1;
    }

  entry = by_hash ? by_hash : by_base;

  if(entry == NULL)
    {
      _report_error("unrecognized %s ROM: %s (size=%" PRIu64 " crc32=%08" PRIx32 ")\n",
                    kind_, source_path_, size, crc);
      return -1;
    }

  target = _xasprintf2(g_cfg.system_dir, entry->filename);

  if(_same_file(source_path_, target))
    {
      free(target);
      *filename_out_ = _xstrdup(entry->filename);
      *name_out_     = _xstrdup(entry->name);
      return 0;
    }

  if(access(target, F_OK) == 0)
    {
      uint32_t target_crc;
      uint64_t target_size;

      if((_file_crc32_and_size(target, &target_crc, &target_size) != 0) ||
         (target_size != entry->size) ||
         (target_crc != entry->crc32))
        {
          _report_error("staged %s ROM already exists with different content: %s\n",
                        kind_, target);
          free(target);
          return -1;
        }

      free(target);
      *filename_out_ = _xstrdup(entry->filename);
      *name_out_     = _xstrdup(entry->name);
      return 0;
    }

  if(symlink(source_path_, target) != 0)
    {
      _report_error("unable to symlink %s ROM at %s: %s\n",
                    kind_, target, strerror(errno));
      free(target);
      return -1;
    }

  free(target);
  *filename_out_ = _xstrdup(entry->filename);
  *name_out_     = _xstrdup(entry->name);
  return 0;
}


static
const
char *
_find_option_value(const char *key_)
{
  size_t i;

  for(i = 0; i < g_cfg.option_count; i++)
    {
      if(strcmp(g_cfg.options[i].key, key_) == 0)
        return g_cfg.options[i].value;
    }

  return NULL;
}


static
const
char *
_core_option_default_value(const struct retro_core_option_value *values_,
                           const char                          *default_value_)
{
  if(default_value_ != NULL)
    return default_value_;

  if((values_ != NULL) && (values_[0].value != NULL))
    return values_[0].value;

  return NULL;
}


static
void
_add_core_option_definition_defaults(const struct retro_core_option_definition *definitions_)
{
  size_t i;

  if(definitions_ == NULL)
    return;

  for(i = 0; definitions_[i].key != NULL; i++)
    _add_option_default(definitions_[i].key,
                        _core_option_default_value(definitions_[i].values,
                                                   definitions_[i].default_value));
}


static
void
_add_core_option_v2_definition_defaults(const struct retro_core_option_v2_definition *definitions_)
{
  size_t i;

  if(definitions_ == NULL)
    return;

  for(i = 0; definitions_[i].key != NULL; i++)
    _add_option_default(definitions_[i].key,
                        _core_option_default_value(definitions_[i].values,
                                                   definitions_[i].default_value));
}


static
void
_add_core_options_intl_defaults(const struct retro_core_options_intl *options_)
{
  if(options_ == NULL)
    return;

  _add_core_option_definition_defaults(options_->us);
}


static
void
_add_core_options_v2_defaults(const struct retro_core_options_v2 *options_)
{
  if(options_ == NULL)
    return;

  _add_core_option_v2_definition_defaults(options_->definitions);
}


static
void
_add_core_options_v2_intl_defaults(const struct retro_core_options_v2_intl *options_)
{
  if(options_ == NULL)
    return;

  _add_core_options_v2_defaults(options_->us);
}


static
char *
_legacy_variable_default_value(const char *value_)
{
  const char *start;
  const char *end;

  if(value_ == NULL)
    return NULL;

  start = strchr(value_, ';');
  if(start == NULL)
    return NULL;

  start++;
  while(*start == ' ')
    start++;

  if(*start == 0)
    return NULL;

  end = strchr(start, '|');
  if(end == NULL)
    end = start + strlen(start);

  if(end == start)
    return NULL;

  return _xstrndup(start, (size_t)(end - start));
}


static
void
_add_variable_defaults(const struct retro_variable *variables_)
{
  size_t i;

  if(variables_ == NULL)
    return;

  for(i = 0; variables_[i].key != NULL; i++)
    {
      char *default_value;

      default_value = _legacy_variable_default_value(variables_[i].value);
      _add_option_default(variables_[i].key, default_value);
      free(default_value);
    }
}


static
const
char *
_log_level_name(enum retro_log_level level_)
{
  switch(level_)
    {
    case RETRO_LOG_DEBUG:
      return "debug";
    case RETRO_LOG_INFO:
      return "info";
    case RETRO_LOG_WARN:
      return "warn";
    case RETRO_LOG_ERROR:
      return "error";
    default:
      break;
    }

  return "unknown";
}


RETRO_CALLCONV
static
void
_harness_log(enum retro_log_level level_,
             const char          *fmt_,
             ...)
{
  va_list ap;

  switch(level_)
    {
    case RETRO_LOG_DEBUG:
      g_run.log_debug++;
      break;
    case RETRO_LOG_INFO:
      g_run.log_info++;
      break;
    case RETRO_LOG_WARN:
      g_run.log_warn++;
      break;
    case RETRO_LOG_ERROR:
      g_run.log_error++;
      break;
    default:
      break;
    }

  if(g_run.log_file == NULL)
    return;

  fprintf(g_run.log_file, "[libretro:%s] ", _log_level_name(level_));
  va_start(ap, fmt_);
  vfprintf(g_run.log_file, fmt_, ap);
  va_end(ap);
  fflush(g_run.log_file);
}


RETRO_CALLCONV
static
bool
_harness_environment(unsigned cmd_,
                     void    *data_)
{
  switch(cmd_)
    {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
      *(const char **)data_ = g_cfg.system_dir;
      return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
      *(const char **)data_ = g_cfg.save_dir;
      return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
      *(unsigned *)data_ = 2;
      return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
      *(unsigned *)data_ = RETRO_LANGUAGE_ENGLISH;
      return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
      _add_core_options_v2_intl_defaults((const struct retro_core_options_v2_intl *)data_);
      return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
      _add_core_options_v2_defaults((const struct retro_core_options_v2 *)data_);
      return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
      _add_core_options_intl_defaults((const struct retro_core_options_intl *)data_);
      return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
      _add_core_option_definition_defaults((const struct retro_core_option_definition *)data_);
      return true;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
      return true;
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
      {
        struct retro_vfs_interface_info *vfs_info =
          (struct retro_vfs_interface_info *)data_;

        if(vfs_info == NULL)
          return false;

        if(vfs_info->required_interface_version == 0)
          return false;

        vfs_info->iface = NULL;
        return true;
      }
    case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS:
      *(unsigned *)data_ = 1;
      return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
      _add_variable_defaults((const struct retro_variable *)data_);
      return true;
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
      return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE:
      {
        struct retro_variable *var = (struct retro_variable *)data_;
        const char *value = _find_option_value(var->key);
        var->value = value;
        return (value != NULL);
      }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
      *(bool *)data_ = false;
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      {
        enum retro_pixel_format fmt = *(const enum retro_pixel_format *)data_;

        if((fmt != RETRO_PIXEL_FORMAT_0RGB1555) &&
           (fmt != RETRO_PIXEL_FORMAT_XRGB8888) &&
           (fmt != RETRO_PIXEL_FORMAT_RGB565))
          return false;

        g_run.pixel_format = fmt;
        _terminal_set_pixel_reader(fmt);
        return true;
      }
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      g_run.av_info = *(const struct retro_system_av_info *)data_;
      return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
      g_run.av_info.geometry = *(const struct retro_game_geometry *)data_;
      return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      ((struct retro_log_callback *)data_)->log = _harness_log;
      return true;
    default:
      if(g_cfg.verbose && (g_run.log_file != NULL))
        fprintf(g_run.log_file, "[harness] unsupported environment command %u\n", cmd_);
      return false;
    }
}


static
uint8_t
_scale_5_to_8(uint32_t v_)
{
  return (uint8_t)((v_ * 255U) / 31U);
}


static
uint8_t
_scale_6_to_8(uint32_t v_)
{
  return (uint8_t)((v_ * 255U) / 63U);
}


static
uint32_t
_terminal_read_pixel_0rgb1555(const void *data_,
                              size_t      pitch_,
                              unsigned    x_,
                              unsigned    y_)
{
  const uint8_t *row = (const uint8_t *)data_ + (pitch_ * y_);
  uint16_t p = ((const uint16_t *)row)[x_];
  uint32_t r = _scale_5_to_8((p >> 10) & 0x1f);
  uint32_t g = _scale_5_to_8((p >> 5) & 0x1f);
  uint32_t b = _scale_5_to_8(p & 0x1f);

  return (r << 16U) | (g << 8U) | b;
}


static
uint32_t
_terminal_read_pixel_xrgb8888(const void *data_,
                              size_t      pitch_,
                              unsigned    x_,
                              unsigned    y_)
{
  const uint8_t *row = (const uint8_t *)data_ + (pitch_ * y_);
  uint32_t p = ((const uint32_t *)row)[x_];

  return p & 0x00ffffffU;
}


static
uint32_t
_terminal_read_pixel_rgb565(const void *data_,
                            size_t      pitch_,
                            unsigned    x_,
                            unsigned    y_)
{
  const uint8_t *row = (const uint8_t *)data_ + (pitch_ * y_);
  uint16_t p = ((const uint16_t *)row)[x_];
  uint32_t r = _scale_5_to_8((p >> 11) & 0x1f);
  uint32_t g = _scale_6_to_8((p >> 5) & 0x3f);
  uint32_t b = _scale_5_to_8(p & 0x1f);

  return (r << 16U) | (g << 8U) | b;
}


static
void
_terminal_set_pixel_reader(enum retro_pixel_format fmt_)
{
  switch(fmt_)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888:
      g_run.terminal_read_pixel = _terminal_read_pixel_xrgb8888;
      break;
    case RETRO_PIXEL_FORMAT_RGB565:
      g_run.terminal_read_pixel = _terminal_read_pixel_rgb565;
      break;
    case RETRO_PIXEL_FORMAT_0RGB1555:
    default:
      g_run.terminal_read_pixel = _terminal_read_pixel_0rgb1555;
      break;
    }
}


static
const
char *
_pixel_format_name(enum retro_pixel_format fmt_)
{
  switch(fmt_)
    {
    case RETRO_PIXEL_FORMAT_0RGB1555:
      return "0RGB1555";
    case RETRO_PIXEL_FORMAT_XRGB8888:
      return "XRGB8888";
    case RETRO_PIXEL_FORMAT_RGB565:
      return "RGB565";
    default:
      break;
    }

  return "unknown";
}


static
unsigned
_pixel_size(enum retro_pixel_format fmt_);

static
bool
_terminal_ensure_u8_buffer(uint8_t **buffer_,
                           size_t   *size_,
                           size_t    needed_);


static
bool
_pack_frame_rgb_into(const void             *data_,
                     unsigned                width_,
                     unsigned                height_,
                     size_t                  pitch_,
                     enum retro_pixel_format fmt_,
                     uint8_t                *packed_,
                     size_t                  stride_)
{
  size_t row_stride;
  unsigned y;

  if((data_ == NULL) || (packed_ == NULL) || (width_ == 0) || (height_ == 0))
    return false;

  if(width_ > (UINT_MAX / 3U))
    return false;
  row_stride = (size_t)width_ * 3U;
  if(stride_ < row_stride)
    return false;

  for(y = 0; y < height_; y++)
    {
      const uint8_t *row = (const uint8_t *)data_ + ((size_t)y * pitch_);
      uint8_t *out = packed_ + (stride_ * y);
      unsigned x;

      switch(fmt_)
        {
        case RETRO_PIXEL_FORMAT_XRGB8888:
          for(x = 0; x < width_; x++)
            {
              uint32_t p = ((const uint32_t *)row)[x];

              out[x * 3U + 0U] = (uint8_t)((p >> 16U) & 0xffU);
              out[x * 3U + 1U] = (uint8_t)((p >> 8U) & 0xffU);
              out[x * 3U + 2U] = (uint8_t)(p & 0xffU);
            }
          break;
        case RETRO_PIXEL_FORMAT_RGB565:
          for(x = 0; x < width_; x++)
            {
              uint16_t p = ((const uint16_t *)row)[x];

              out[x * 3U + 0U] = _scale_5_to_8((p >> 11U) & 0x1fU);
              out[x * 3U + 1U] = _scale_6_to_8((p >> 5U) & 0x3fU);
              out[x * 3U + 2U] = _scale_5_to_8(p & 0x1fU);
            }
          break;
        case RETRO_PIXEL_FORMAT_0RGB1555:
        default:
          for(x = 0; x < width_; x++)
            {
              uint16_t p = ((const uint16_t *)row)[x];

              out[x * 3U + 0U] = _scale_5_to_8((p >> 10U) & 0x1fU);
              out[x * 3U + 1U] = _scale_5_to_8((p >> 5U) & 0x1fU);
              out[x * 3U + 2U] = _scale_5_to_8(p & 0x1fU);
            }
          break;
        }
    }

  return true;
}


static
uint8_t *
_pack_frame_rgb(const void             *data_,
                unsigned                width_,
                unsigned                height_,
                size_t                  pitch_,
                enum retro_pixel_format fmt_,
                size_t                 *stride_out_)
{
  uint8_t *packed;
  size_t stride;

  if(stride_out_ != NULL)
    *stride_out_ = 0;

  if((data_ == NULL) || (width_ == 0) || (height_ == 0))
    return NULL;

  if(width_ > (UINT_MAX / 3U))
    return NULL;
  stride = (size_t)width_ * 3U;

  if((stride > 0) && (height_ > (SIZE_MAX / stride)))
    return NULL;

  packed = (uint8_t *)malloc(stride * height_);
  if(packed == NULL)
    return NULL;

  if(!_pack_frame_rgb_into(data_, width_, height_, pitch_, fmt_, packed, stride))
    {
      free(packed);
      return NULL;
    }

  if(stride_out_ != NULL)
    *stride_out_ = stride;

  return packed;
}


static
int
_write_png(const char             *path_,
           const void             *data_,
           unsigned                width_,
           unsigned                height_,
           size_t                  pitch_,
           enum retro_pixel_format fmt_)
{
  uint8_t *packed;
  size_t   stride;

  if((data_ == NULL) || (width_ == 0) || (height_ == 0))
    return -1;

  if(_mkdir_parent(path_) != 0)
    return -1;

  packed = _pack_frame_rgb(data_, width_, height_, pitch_, fmt_, &stride);
  if(packed == NULL)
    return -1;

  if(stbi_write_png(path_, (int)width_, (int)height_, 3,
                    packed, (int)stride) == 0)
    {
      free(packed);
      return -1;
    }

  free(packed);
  return 0;
}


static
bool
_frame_copy_size(unsigned  height_,
                 size_t    pitch_,
                 size_t   *size_out_)
{
  if((height_ == 0) || (pitch_ == 0))
    return false;

  if((size_t)height_ > (SIZE_MAX / pitch_))
    return false;

  *size_out_ = pitch_ * (size_t)height_;
  return true;
}


static
unsigned
_pixel_size(enum retro_pixel_format fmt_)
{
  switch(fmt_)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888: return 4;
    case RETRO_PIXEL_FORMAT_RGB565:
    case RETRO_PIXEL_FORMAT_0RGB1555: return 2;
    default:                          return 0;
  }
}


static
bool
_terminal_image_source_layout(const void             *data_,
                              unsigned                width_,
                              unsigned                height_,
                              size_t                  pitch_,
                              enum retro_pixel_format fmt_,
                              size_t                 *row_bytes_out_,
                              size_t                 *data_size_out_)
{
  unsigned psize;
  size_t row_bytes;

  if(row_bytes_out_ != NULL)
    *row_bytes_out_ = 0;
  if(data_size_out_ != NULL)
    *data_size_out_ = 0;

  psize = _pixel_size(fmt_);
  if((data_ == NULL) || (width_ == 0U) || (height_ == 0U) ||
     (psize == 0U) || (row_bytes_out_ == NULL) || (data_size_out_ == NULL))
    return false;

  if((size_t)width_ > (SIZE_MAX / (size_t)psize))
    return false;
  row_bytes = (size_t)width_ * (size_t)psize;
  if(pitch_ < row_bytes)
    return false;
  if((size_t)height_ > (SIZE_MAX / row_bytes))
    return false;
  if((height_ > 1U) &&
     ((size_t)(height_ - 1U) > ((SIZE_MAX - row_bytes) / pitch_)))
    return false;

  *row_bytes_out_ = row_bytes;
  *data_size_out_ = row_bytes * (size_t)height_;
  return true;
}


static
void
_terminal_invalidate_image_cache(void)
{
  g_run.terminal_image_cache.valid = false;
}


static
bool
_terminal_image_cache_matches(const void             *data_,
                              size_t                  pitch_,
                              int                     render_mode_,
                              unsigned                src_width_,
                              unsigned                src_height_,
                              enum retro_pixel_format fmt_,
                              unsigned                row_,
                              unsigned                col_,
                              unsigned                cols_,
                              unsigned                rows_,
                              unsigned                image_width_,
                              unsigned                image_height_)
{
  terminal_image_cache_t *cache = &g_run.terminal_image_cache;
  const uint8_t *source;
  const uint8_t *cached;
  size_t row_bytes;
  size_t data_size;
  unsigned y;

  if(!cache->valid ||
     (cache->render_mode != render_mode_) ||
     (cache->src_width != src_width_) ||
     (cache->src_height != src_height_) ||
     (cache->pixel_format != fmt_) ||
     (cache->row != row_) ||
     (cache->col != col_) ||
     (cache->cols != cols_) ||
     (cache->rows != rows_) ||
     (cache->image_width != image_width_) ||
     (cache->image_height != image_height_))
    return false;

  if(!_terminal_image_source_layout(data_, src_width_, src_height_, pitch_, fmt_,
                                    &row_bytes, &data_size) ||
     (cache->pixels == NULL) || (cache->capacity < data_size) ||
     (cache->row_bytes != row_bytes) || (cache->data_size != data_size))
    return false;

  if(pitch_ == row_bytes)
    return memcmp(data_, cache->pixels, data_size) == 0;

  source = (const uint8_t *)data_;
  cached = cache->pixels;
  for(y = 0; y < src_height_; y++)
    {
      if(memcmp(source, cached, row_bytes) != 0)
        return false;
      source += pitch_;
      cached += row_bytes;
    }

  return true;
}


static
void
_terminal_store_image_cache(const void             *data_,
                            size_t                  pitch_,
                            int                     render_mode_,
                            unsigned                src_width_,
                            unsigned                src_height_,
                            enum retro_pixel_format fmt_,
                            unsigned                row_,
                            unsigned                col_,
                            unsigned                cols_,
                            unsigned                rows_,
                            unsigned                image_width_,
                            unsigned                image_height_)
{
  terminal_image_cache_t *cache = &g_run.terminal_image_cache;
  const uint8_t *source;
  uint8_t *cached;
  size_t row_bytes;
  size_t data_size;
  unsigned y;

  cache->valid = false;
  if(!_terminal_image_source_layout(data_, src_width_, src_height_, pitch_, fmt_,
                                    &row_bytes, &data_size) ||
     !_terminal_ensure_u8_buffer(&cache->pixels, &cache->capacity, data_size))
    return;

  if(pitch_ == row_bytes)
    memcpy(cache->pixels, data_, data_size);
  else
    {
      source = (const uint8_t *)data_;
      cached = cache->pixels;
      for(y = 0; y < src_height_; y++)
        {
          memcpy(cached, source, row_bytes);
          source += pitch_;
          cached += row_bytes;
        }
    }

  cache->row_bytes = row_bytes;
  cache->data_size = data_size;
  cache->render_mode = render_mode_;
  cache->src_width = src_width_;
  cache->src_height = src_height_;
  cache->pixel_format = fmt_;
  cache->row = row_;
  cache->col = col_;
  cache->cols = cols_;
  cache->rows = rows_;
  cache->image_width = image_width_;
  cache->image_height = image_height_;
  cache->valid = true;
}


static
bool
_terminal_ensure_u8_buffer(uint8_t **buffer_,
                           size_t   *size_,
                           size_t    needed_)
{
  uint8_t *next;
  size_t next_size;

  if((buffer_ == NULL) || (size_ == NULL))
    return false;
  if(needed_ == 0)
    return true;
  if(*size_ >= needed_)
    return true;

  next_size = (*size_ > 0) ? *size_ : 65536U;
  while(next_size < needed_)
    {
      if(next_size > (SIZE_MAX / 2U))
        {
          next_size = needed_;
          break;
        }
      next_size *= 2U;
    }

  next = (uint8_t *)realloc(*buffer_, next_size);
  if(next == NULL)
    return false;

  *buffer_ = next;
  *size_ = next_size;
  return true;
}


static
bool
_terminal_pack_frame_rgb(const void             *data_,
                         unsigned                width_,
                         unsigned                height_,
                         size_t                  pitch_,
                         enum retro_pixel_format fmt_,
                         uint8_t               **packed_out_,
                         size_t                 *stride_out_)
{
  size_t stride;
  size_t needed;

  if(packed_out_ != NULL)
    *packed_out_ = NULL;
  if(stride_out_ != NULL)
    *stride_out_ = 0;

  if((data_ == NULL) || (width_ == 0U) || (height_ == 0U))
    return false;
  if(width_ > (UINT_MAX / 3U))
    return false;
  stride = (size_t)width_ * 3U;
  if((stride > 0) && ((size_t)height_ > (SIZE_MAX / stride)))
    return false;
  needed = stride * (size_t)height_;

  if(!_terminal_ensure_u8_buffer(&g_run.terminal_rgb_buffer,
                                 &g_run.terminal_rgb_buffer_size,
                                 needed))
    return false;
  if(!_pack_frame_rgb_into(data_, width_, height_, pitch_, fmt_,
                           g_run.terminal_rgb_buffer, stride))
    return false;

  if(packed_out_ != NULL)
    *packed_out_ = g_run.terminal_rgb_buffer;
  if(stride_out_ != NULL)
    *stride_out_ = stride;
  return true;
}


static
bool
_frame_is_blank(const void             *data_,
                unsigned                width_,
                unsigned                height_,
                size_t                  pitch_,
                enum retro_pixel_format fmt_)
{
  unsigned psize;
  const uint8_t *row;
  unsigned y;

  psize = _pixel_size(fmt_);
  if((psize == 0) || (data_ == NULL) || (width_ == 0))
    return false;

  row = (const uint8_t *)data_;
  for(y = 0; y < height_; y++)
    {
      size_t row_bytes = (size_t)width_ * psize;
      size_t i;
      for(i = 0; i < row_bytes; i++)
        if(row[i] != 0)
          return false;
      row += pitch_;
    }
  return true;
}


static
bool
_frame_equals_prev(const void             *data_,
                   unsigned                width_,
                   unsigned                height_,
                   size_t                  pitch_,
                   enum retro_pixel_format fmt_)
{
  unsigned psize;
  const uint8_t *row;
  const uint8_t *prow;
  unsigned y;

  psize = _pixel_size(fmt_);
  if((psize == 0) || (data_ == NULL) || (g_run.screenshot_when_prev == NULL))
    return false;

  if((g_run.screenshot_when_prev_width  != width_)  ||
     (g_run.screenshot_when_prev_height != height_) ||
     (g_run.screenshot_when_prev_fmt    != fmt_))
    return false;

  row  = (const uint8_t *)data_;
  prow = (const uint8_t *)g_run.screenshot_when_prev;
  for(y = 0; y < height_; y++)
    {
      if(memcmp(row, prow, (size_t)width_ * psize) != 0)
        return false;
      row  += pitch_;
      prow += (size_t)width_ * psize;
    }
  return true;
}


static
void
_store_screenshot_when_prev(const void             *data_,
                            unsigned                width_,
                            unsigned                height_,
                            size_t                  pitch_,
                            enum retro_pixel_format fmt_)
{
  unsigned psize;
  size_t row_bytes;
  size_t total;
  const uint8_t *row;
  uint8_t *dst;
  void *next;
  unsigned y;

  psize = _pixel_size(fmt_);
  if((psize == 0) || (data_ == NULL) || (width_ == 0))
    return;

  row_bytes = (size_t)width_ * psize;
  total     = row_bytes * height_;
  if(total > g_run.screenshot_when_prev_size)
    {
      next = realloc(g_run.screenshot_when_prev, total);
      if(next == NULL)
        return;
      g_run.screenshot_when_prev      = next;
      g_run.screenshot_when_prev_size = total;
    }

  row = (const uint8_t *)data_;
  dst = (uint8_t *)g_run.screenshot_when_prev;
  for(y = 0; y < height_; y++)
    {
      memcpy(dst, row, row_bytes);
      row += pitch_;
      dst += row_bytes;
    }

  g_run.screenshot_when_prev_width  = width_;
  g_run.screenshot_when_prev_height = height_;
  g_run.screenshot_when_prev_fmt    = fmt_;
}


static
int
_capture_screenshot_frame(screenshot_request_t  *shot_,
                          const void            *data_,
                          unsigned               width_,
                          unsigned               height_,
                          size_t                 pitch_,
                          enum retro_pixel_format fmt_)
{
  size_t size;

  if((shot_ == NULL) || (data_ == NULL) || (width_ == 0) ||
     !_frame_copy_size(height_, pitch_, &size))
    return -1;

  if(size > shot_->data_size)
    {
      void *next = realloc(shot_->data, size);
      if(next == NULL)
        return -1;

      shot_->data      = next;
      shot_->data_size = size;
    }

  memcpy(shot_->data, data_, size);
  shot_->width        = width_;
  shot_->height       = height_;
  shot_->pitch        = pitch_;
  shot_->pixel_format = fmt_;
  shot_->captured     = true;
  return 0;
}


static
void
_store_last_frame(const void             *data_,
                  unsigned                width_,
                  unsigned                height_,
                  size_t                  pitch_,
                  enum retro_pixel_format fmt_)
{
  size_t size;

  if((data_ == NULL) || !_frame_copy_size(height_, pitch_, &size))
    return;

  g_run.last_width        = width_;
  g_run.last_height       = height_;
  g_run.last_pitch        = pitch_;
  g_run.last_pixel_format = fmt_;

  /* The pixel buffer is only consumed by the final screenshot, which is
     written solely when --screenshot is supplied. Skip the per-frame copy
     otherwise: it is a full-framebuffer pass the common benchmark path does
     not need. */
  if(g_cfg.screenshot_path == NULL)
    return;

  if(size > g_run.last_frame_size)
    {
      void *next = realloc(g_run.last_frame, size);
      if(next == NULL)
        {
          g_run.artifact_error = true;
          return;
        }
      g_run.last_frame      = next;
      g_run.last_frame_size = size;
    }

  memcpy(g_run.last_frame, data_, size);
  g_run.last_width        = width_;
  g_run.last_height       = height_;
  g_run.last_pitch        = pitch_;
  g_run.last_pixel_format = fmt_;
}


static
void
_terminal_render_frame(const void             *data_,
                       unsigned                width_,
                       unsigned                height_,
                       size_t                  pitch_,
                       enum retro_pixel_format  fmt_);

static
bool
_terminal_should_render(void);

static
void
_terminal_move_cursor(unsigned row_,
                      unsigned col_);

static
unsigned
_terminal_u64_ceil_div(uint64_t value_,
                       uint64_t divisor_);

static
void
_terminal_write_all(const void *data_,
                    size_t       size_);

static
void
_terminal_write_spaces(size_t count_)
{
  static const char spaces[] = "                                                                ";
  size_t len = sizeof(spaces) - 1;

  while(count_ > 0)
    {
      size_t chunk = (count_ < len) ? count_ : len;
      _terminal_write_all(spaces, chunk);
      count_ -= chunk;
    }
}


static
void
_terminal_write_repeat_char(char     ch_,
                            unsigned count_)
{
  char buf[128];

  memset(buf, ch_, sizeof(buf));
  while(count_ > 0U)
    {
      size_t chunk = (count_ < sizeof(buf)) ? (size_t)count_ : sizeof(buf);
      _terminal_write_all(buf, chunk);
      count_ -= (unsigned)chunk;
    }
}


static
void
_terminal_write_repeat_utf8(const char *utf8_,
                            size_t      len_,
                            unsigned    count_)
{
  char buf[192];
  unsigned per_chunk;

  if((utf8_ == NULL) || (len_ == 0U) || (len_ > sizeof(buf)))
    return;

  per_chunk = (unsigned)(sizeof(buf) / len_);
  if(per_chunk == 0U)
    per_chunk = 1U;

  while(count_ > 0U)
    {
      unsigned reps = (count_ < per_chunk) ? count_ : per_chunk;
      unsigned i;

      for(i = 0; i < reps; i++)
        memcpy(buf + ((size_t)i * len_), utf8_, len_);

      _terminal_write_all(buf, (size_t)reps * len_);
      count_ -= reps;
    }
}


static
bool
_ascii_contains_case(const char *haystack_,
                     const char *needle_)
{
  size_t needle_len;

  if((haystack_ == NULL) || (needle_ == NULL))
    return false;

  needle_len = strlen(needle_);
  if(needle_len == 0)
    return true;

  for(; *haystack_ != 0; haystack_++)
    {
      size_t i;

      for(i = 0; i < needle_len; i++)
        {
          unsigned char a = (unsigned char)haystack_[i];
          unsigned char b = (unsigned char)needle_[i];

          if(a == 0)
            return false;
          if(tolower(a) != tolower(b))
            break;
        }

      if(i == needle_len)
        return true;
    }

  return false;
}


static
const char *
_terminal_render_mode_name(int mode_)
{
  switch(mode_)
    {
    case HARNESS_TERMINAL_RENDER_KITTY:
      return "kitty";
    case HARNESS_TERMINAL_RENDER_SIXEL:
      return "sixel";
    case HARNESS_TERMINAL_RENDER_HALF:
      return "half";
    case HARNESS_TERMINAL_RENDER_ASCII:
      return "ascii";
    default:
      break;
    }

  return "none";
}


static
const char *
_terminal_render_override_name(int mode_)
{
  if(mode_ < 0)
    return "auto";

  return _terminal_render_mode_name(mode_);
}


static
const char *
_find_bytes(const char *haystack_,
            size_t      haystack_len_,
            const char *needle_,
            size_t      needle_len_)
{
  size_t i;

  if((haystack_ == NULL) || (needle_ == NULL) || (needle_len_ == 0) ||
     (haystack_len_ < needle_len_))
    return NULL;

  for(i = 0; i <= haystack_len_ - needle_len_; i++)
    {
      if(memcmp(haystack_ + i, needle_, needle_len_) == 0)
        return haystack_ + i;
    }

  return NULL;
}


static
bool
_terminal_probe_write_all(const void *data_,
                          size_t      size_)
{
  while((data_ != NULL) && (size_ > 0))
    {
      ssize_t n;

      n = write(g_run.tty_fd, data_, size_);
      if(n < 0)
        {
          if(errno == EINTR)
            continue;
          if((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
              struct pollfd pfd;
              int rv;

              pfd.fd = g_run.tty_fd;
              pfd.events = POLLOUT;
              pfd.revents = 0;
              rv = poll(&pfd, 1, 50);
              if(rv > 0)
                continue;
            }
          return false;
        }

      if(n == 0)
        return false;

      data_ = (const char *)data_ + n;
      size_ -= (size_t)n;
    }

  return true;
}


typedef int (*terminal_probe_parse_fn)(const char  *buffer_,
                                       size_t       len_,
                                       const char **result_,
                                       void        *data_);

typedef const char *(*terminal_probe_timeout_result_fn)(void *data_);


static
bool
_terminal_probe_read_response(const void                       *query_,
                              size_t                            query_len_,
                              unsigned                          timeout_ms_,
                              terminal_probe_parse_fn           parse_,
                              void                             *parse_data_,
                              terminal_probe_timeout_result_fn  timeout_result_,
                              const char                      **result_)
{
  char buffer[1024];
  size_t len = 0;
  double deadline;
  bool eof = false;

  memset(buffer, 0, sizeof(buffer));

  if(result_ != NULL)
    *result_ = "timeout";

  /* discard any pending input (e.g. user keystrokes) before probing so
     it cannot be mistaken for a probe response */
  if(g_run.tty_fd >= 0)
    {
      for(;;)
        {
          char tmp[256];
          ssize_t n;

          n = read(g_run.tty_fd, tmp, sizeof(tmp));
          if(n <= 0)
            break;
        }
    }

  if(!_terminal_probe_write_all(query_, query_len_))
    {
      if(result_ != NULL)
        *result_ = "write_failed";
      return false;
    }

  deadline = _monotonic_seconds() + ((double)timeout_ms_ / 1000.0);

  while(!eof && (_monotonic_seconds() < deadline))
    {
      struct pollfd pfd;
      const char *parse_result = NULL;
      double now;
      int timeout_ms;
      int status;
      int rv;

      if((len > 0) && (parse_ != NULL))
        {
          status = parse_(buffer, len, &parse_result, parse_data_);
          if(status != 0)
            {
              if(result_ != NULL)
                *result_ = (parse_result != NULL) ? parse_result :
                  ((status > 0) ? "ok" : "unsupported");
              return status > 0;
            }
        }

      now = _monotonic_seconds();
      timeout_ms = (int)ceil((deadline - now) * 1000.0);
      if(timeout_ms < 1)
        timeout_ms = 1;

      pfd.fd = g_run.tty_fd;
      pfd.events = POLLIN;
      pfd.revents = 0;
      rv = poll(&pfd, 1, timeout_ms);
      if(rv < 0)
        {
          if(errno == EINTR)
            continue;
          if(result_ != NULL)
            *result_ = "read_failed";
          return false;
        }
      if(rv == 0)
        break;

      for(;;)
        {
          ssize_t n;

          if(len >= sizeof(buffer))
            {
              if(result_ != NULL)
                *result_ = "response_overflow";
              return false;
            }

          n = read(g_run.tty_fd, buffer + len, sizeof(buffer) - len);
          if(n > 0)
            {
              len += (size_t)n;
              if(parse_ != NULL)
                {
                  parse_result = NULL;
                  status = parse_(buffer, len, &parse_result, parse_data_);
                  if(status != 0)
                    {
                      if(result_ != NULL)
                        *result_ = (parse_result != NULL) ? parse_result :
                          ((status > 0) ? "ok" : "unsupported");
                      return status > 0;
                    }
                }
              continue;
            }

          if(n == 0)
            {
              /* persistent EOF (e.g. pty hung up): stop probing rather
                 than spinning until the deadline */
              eof = true;
              break;
            }

          if(errno == EINTR)
            continue;
          if((errno == EAGAIN) || (errno == EWOULDBLOCK))
            break;

          if(result_ != NULL)
            *result_ = "read_failed";
          return false;
        }
    }

  if((result_ != NULL) && (timeout_result_ != NULL))
    {
      const char *timeout_result = timeout_result_(parse_data_);

      if(timeout_result != NULL)
        *result_ = timeout_result;
    }

  return false;
}


static
bool
_terminal_buffer_has_da_response(const char *buffer_,
                                 size_t      len_)
{
  size_t i;

  if(buffer_ == NULL)
    return false;

  for(i = 0; i + 2U < len_; i++)
    {
      size_t j;

      if((buffer_[i] != '\x1b') || (buffer_[i + 1U] != '['))
        continue;

      for(j = i + 2U; j < len_; j++)
        {
          unsigned char c = (unsigned char)buffer_[j];

          if(c == 'c')
            return true;
          if((c < 0x30U) || (c > 0x7eU))
            break;
        }
    }

  return false;
}


static
bool
_terminal_probe_response_done(const char  *buffer_,
                              size_t       len_,
                              bool        *supported_,
                              const char **result_)
{
  static const char marker[] = "\x1b_Gi=31;";
  static const char st[] = "\x1b\\";
  const char *p;
  const char *status;
  size_t status_off;

  p = _find_bytes(buffer_, len_, marker, sizeof(marker) - 1U);
  if(p == NULL)
    return false;

  status = p + sizeof(marker) - 1U;
  status_off = (size_t)(status - buffer_);
  if((len_ >= status_off + 2U) && (status[0] == 'O') && (status[1] == 'K'))
    {
      *supported_ = true;
      *result_ = "ok";
      return true;
    }

  if(_find_bytes(status, len_ - status_off, st, sizeof(st) - 1U) != NULL)
    {
      *supported_ = false;
      *result_ = "rejected";
      return true;
    }

  return false;
}


typedef struct terminal_kitty_probe_context_t terminal_kitty_probe_context_t;
struct terminal_kitty_probe_context_t
{
  bool da_seen;
};


static
int
_terminal_parse_kitty_probe(const char  *buffer_,
                            size_t       len_,
                            const char **result_,
                            void        *data_)
{
  terminal_kitty_probe_context_t *ctx =
    (terminal_kitty_probe_context_t *)data_;
  const char *result = NULL;
  bool supported = false;

  if(_terminal_probe_response_done(buffer_, len_, &supported, &result))
    {
      if(result_ != NULL)
        *result_ = result;
      return supported ? 1 : -1;
    }

  if(ctx != NULL)
    ctx->da_seen = ctx->da_seen || _terminal_buffer_has_da_response(buffer_,
                                                                    len_);
  return 0;
}


static
const char *
_terminal_kitty_probe_timeout_result(void *data_)
{
  terminal_kitty_probe_context_t *ctx =
    (terminal_kitty_probe_context_t *)data_;

  return (ctx != NULL) && ctx->da_seen ? "no_graphics_response" : "timeout";
}


static
bool
_terminal_probe_kitty(void)
{
  static const char query[] =
    "\x1b_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\x1b\\\x1b[c";
  static const char zlib_query[] =
    "\x1b_Gi=31,s=1,v=1,a=q,t=d,f=24,o=z;"
    "eF5jYGAAAAADAAE=\x1b\\\x1b[c";
  terminal_kitty_probe_context_t ctx;

  memset(&ctx, 0, sizeof(ctx));
  g_run.terminal_kitty_probed = true;
  g_run.terminal_kitty_supported = false;
  g_run.terminal_kitty_probe_result = "timeout";
  g_run.terminal_kitty_zlib_probed = false;
  g_run.terminal_kitty_zlib_supported = false;
  g_run.terminal_kitty_zlib_probe_result = "not_run";

  g_run.terminal_kitty_supported =
    _terminal_probe_read_response(query,
                                  sizeof(query) - 1U,
                                  HARNESS_TERMINAL_KITTY_PROBE_TIMEOUT_MS,
                                  _terminal_parse_kitty_probe,
                                  &ctx,
                                  _terminal_kitty_probe_timeout_result,
                                  &g_run.terminal_kitty_probe_result);
  if(!g_run.terminal_kitty_supported)
    return false;

  memset(&ctx, 0, sizeof(ctx));
  g_run.terminal_kitty_zlib_probed = true;
  g_run.terminal_kitty_zlib_probe_result = "timeout";
  g_run.terminal_kitty_zlib_supported =
    _terminal_probe_read_response(zlib_query,
                                  sizeof(zlib_query) - 1U,
                                  HARNESS_TERMINAL_KITTY_PROBE_TIMEOUT_MS,
                                  _terminal_parse_kitty_probe,
                                  &ctx,
                                  _terminal_kitty_probe_timeout_result,
                                  &g_run.terminal_kitty_zlib_probe_result);
  if(!g_run.terminal_kitty_zlib_supported)
    _harness_log(RETRO_LOG_INFO,
                 "[Harness]: Kitty zlib probe failed (%s); "
                 "using PNG payloads\n",
                 g_run.terminal_kitty_zlib_probe_result);

  return true;
}


static
int
_terminal_sixel_da_status(const char *buffer_,
                          size_t      len_)
{
  size_t i;
  bool complete_da = false;

  if(buffer_ == NULL)
    return 0;

  for(i = 0; i + 2U < len_; i++)
    {
      size_t j;
      unsigned param = 0;
      bool have_param = false;
      bool private_da = false;

      if((buffer_[i] != '\x1b') || (buffer_[i + 1U] != '['))
        continue;

      j = i + 2U;
      if((j < len_) && (buffer_[j] == '?'))
        {
          private_da = true;
          j++;
        }

      while(j < len_)
        {
          unsigned char c = (unsigned char)buffer_[j++];

          if((c >= '0') && (c <= '9'))
            {
              if(param <= (UINT_MAX / 10U))
                param = (param * 10U) + (unsigned)(c - '0');
              have_param = true;
              continue;
            }

          if((c == ';') || (c == ':'))
            {
              if(private_da && have_param && (param == 4U))
                return 1;
              param = 0;
              have_param = false;
              continue;
            }

          if(c == 'c')
            {
              if(private_da && have_param && (param == 4U))
                return 1;
              complete_da = true;
            }

          break;
        }
    }

  return complete_da ? -1 : 0;
}


static
int
_terminal_parse_sixel_probe(const char  *buffer_,
                            size_t       len_,
                            const char **result_,
                            void        *data_)
{
  int status;

  (void)data_;

  status = _terminal_sixel_da_status(buffer_, len_);
  if(status > 0)
    {
      if(result_ != NULL)
        *result_ = "ok";
      return 1;
    }
  if(status < 0)
    {
      if(result_ != NULL)
        *result_ = "not_advertised";
      return -1;
    }

  return 0;
}


static
bool
_terminal_probe_sixel(void)
{
  static const char query[] = "\x1b[c";

  g_run.terminal_sixel_probed = true;
  g_run.terminal_sixel_supported = false;
  g_run.terminal_sixel_probe_result = "timeout";

  g_run.terminal_sixel_supported =
    _terminal_probe_read_response(query,
                                  sizeof(query) - 1U,
                                  HARNESS_TERMINAL_SIXEL_PROBE_TIMEOUT_MS,
                                  _terminal_parse_sixel_probe,
                                  NULL,
                                  NULL,
                                  &g_run.terminal_sixel_probe_result);
  return g_run.terminal_sixel_supported;
}


static
bool
_terminal_inside_tmux(void)
{
  const char *tmux = getenv("TMUX");

  return ((tmux != NULL) && (tmux[0] != 0));
}


static
void
_terminal_disable_sixel_in_tmux(void)
{
  g_run.terminal_sixel_probed = false;
  g_run.terminal_sixel_supported = false;
  g_run.terminal_sixel_probe_result = "disabled_in_tmux";
}


static
void
_terminal_detect_capabilities(void)
{
  const char *codeset;
  const char *term;
  const char *colorterm;
  bool utf8;
  bool truecolor;
  bool color256;
  int fallback_render;

  (void)setlocale(LC_CTYPE, "");

  g_run.terminal_kitty_probed = false;
  g_run.terminal_kitty_supported = false;
  g_run.terminal_kitty_probe_result = "not_requested";
  g_run.terminal_kitty_zlib_probed = false;
  g_run.terminal_kitty_zlib_supported = false;
  g_run.terminal_kitty_zlib_probe_result = "not_requested";
  g_run.terminal_sixel_probed = false;
  g_run.terminal_sixel_supported = false;
  g_run.terminal_sixel_probe_result = "not_requested";

  codeset = nl_langinfo(CODESET);
  term = getenv("TERM");
  colorterm = getenv("COLORTERM");

  utf8 = _ascii_contains_case(codeset, "UTF-8") ||
    _ascii_contains_case(codeset, "UTF8") ||
    _ascii_contains_case(getenv("LC_ALL"), "UTF-8") ||
    _ascii_contains_case(getenv("LC_CTYPE"), "UTF-8") ||
    _ascii_contains_case(getenv("LANG"), "UTF-8");

  truecolor = _ascii_contains_case(colorterm, "truecolor") ||
    _ascii_contains_case(colorterm, "24bit") ||
    _ascii_contains_case(term, "-direct") ||
    _ascii_contains_case(term, "truecolor");
  color256 = truecolor || _ascii_contains_case(term, "256color");

  if((getenv("NO_COLOR") != NULL) || (term == NULL) || (strcmp(term, "dumb") == 0))
    {
      truecolor = false;
      color256 = false;
    }

  if(g_cfg.terminal_color_override >= 0)
    g_run.terminal_color_mode = g_cfg.terminal_color_override;
  else if(truecolor)
    g_run.terminal_color_mode = HARNESS_TERMINAL_COLOR_TRUE;
  else if(color256)
    g_run.terminal_color_mode = HARNESS_TERMINAL_COLOR_256;
  else
    g_run.terminal_color_mode = HARNESS_TERMINAL_COLOR_MONO;

  fallback_render = (utf8 && (g_run.terminal_color_mode != HARNESS_TERMINAL_COLOR_MONO)) ?
    HARNESS_TERMINAL_RENDER_HALF : HARNESS_TERMINAL_RENDER_ASCII;

  if(g_cfg.terminal_render_override == HARNESS_TERMINAL_RENDER_KITTY)
    {
      if(_terminal_probe_kitty())
        g_run.terminal_render_mode = HARNESS_TERMINAL_RENDER_KITTY;
      else
        {
          g_run.terminal_render_mode = fallback_render;
          _harness_log(RETRO_LOG_WARN,
                       "[Harness]: Kitty graphics probe failed (%s); "
                       "falling back to %s renderer\n",
                       g_run.terminal_kitty_probe_result,
                       _terminal_render_mode_name(g_run.terminal_render_mode));
        }
    }
  else if(g_cfg.terminal_render_override == HARNESS_TERMINAL_RENDER_SIXEL)
    {
      if(_terminal_inside_tmux())
        {
          _terminal_disable_sixel_in_tmux();
          g_run.terminal_render_mode = fallback_render;
          _harness_log(RETRO_LOG_WARN,
                       "[Harness]: Sixel graphics disabled inside tmux; "
                       "falling back to %s renderer\n",
                       _terminal_render_mode_name(g_run.terminal_render_mode));
        }
      else if(_terminal_probe_sixel())
        g_run.terminal_render_mode = HARNESS_TERMINAL_RENDER_SIXEL;
      else
        {
          g_run.terminal_render_mode = fallback_render;
          _harness_log(RETRO_LOG_WARN,
                       "[Harness]: Sixel graphics probe failed (%s); "
                       "falling back to %s renderer\n",
                       g_run.terminal_sixel_probe_result,
                       _terminal_render_mode_name(g_run.terminal_render_mode));
        }
    }
  else if(g_cfg.terminal_render_override >= 0)
    g_run.terminal_render_mode = g_cfg.terminal_render_override;
  else if(g_run.terminal_color_mode == HARNESS_TERMINAL_COLOR_MONO)
    g_run.terminal_render_mode = fallback_render;
  else if(_terminal_probe_kitty())
    g_run.terminal_render_mode = HARNESS_TERMINAL_RENDER_KITTY;
  else if(_terminal_inside_tmux())
    {
      _terminal_disable_sixel_in_tmux();
      g_run.terminal_render_mode = fallback_render;
    }
  else if(_terminal_probe_sixel())
    g_run.terminal_render_mode = HARNESS_TERMINAL_RENDER_SIXEL;
  else
    g_run.terminal_render_mode = fallback_render;
}


static
bool
_terminal_adaptive_enabled(void)
{
  return g_cfg.terminal_mode && !g_cfg.terminal_fps_user &&
    (g_cfg.terminal_fps > 0.0) && (g_run.core_fps > 0.0);
}


static
double
_terminal_effective_fps(void)
{
  if(_terminal_adaptive_enabled() && (g_run.terminal_adaptive_fps > 0.0))
    return g_run.terminal_adaptive_fps;

  return g_cfg.terminal_fps;
}


static
void
_terminal_update_adaptive_fps(double render_seconds_)
{
  double fps;
  double target_fps;
  double max_fps;
  double min_fps = 5.0;

  if(!_terminal_adaptive_enabled() || (render_seconds_ <= 0.0) ||
     !isfinite(render_seconds_))
    return;

  if(g_run.terminal_render_seconds_ema <= 0.0)
    g_run.terminal_render_seconds_ema = render_seconds_;
  else
    g_run.terminal_render_seconds_ema =
      (g_run.terminal_render_seconds_ema * 0.85) + (render_seconds_ * 0.15);

  fps = _terminal_effective_fps();
  max_fps = g_cfg.terminal_fps;
  if(max_fps > g_run.core_fps)
    max_fps = g_run.core_fps;
  if(max_fps < min_fps)
    min_fps = max_fps;

  if(g_run.terminal_render_seconds_ema > (0.70 / fps))
    {
      target_fps = 0.70 / g_run.terminal_render_seconds_ema;
      if(target_fps < min_fps)
        target_fps = min_fps;
      if(target_fps < fps)
        g_run.terminal_adaptive_fps = target_fps;
    }
  else if((fps < max_fps) &&
          (g_run.terminal_render_seconds_ema < (0.25 / fps)))
    {
      fps += 1.0;
      if(fps > max_fps)
        fps = max_fps;
      g_run.terminal_adaptive_fps = fps;
    }
}


static
bool
_terminal_should_render(void)
{
  double step_d;
  uint64_t step;
  double fps;

  if(!g_cfg.terminal_mode)
    return false;

  if(g_run.terminal_last_render_frame == 0)
    return true;

  fps = _terminal_effective_fps();

  if((fps <= 0.0) || (g_run.core_fps <= 0.0))
    return true;

  if(fps >= g_run.core_fps)
    return true;

  step_d = ceil(g_run.core_fps / fps);
  if(!isfinite(step_d) || (step_d <= 1.0))
    return true;
  if(step_d >= (double)UINT64_MAX)
    step = UINT64_MAX;
  else
    step = (uint64_t)step_d;

  return (g_run.current_frame - g_run.terminal_last_render_frame) >= step;
}


RETRO_CALLCONV
static
void
_harness_video_refresh(const void *data_,
                       unsigned    width_,
                       unsigned    height_,
                       size_t      pitch_)
{
  size_t i;

  g_run.video_frames++;
  _store_last_frame(data_, width_, height_, pitch_, g_run.pixel_format);

  for(i = 0; i < g_cfg.screenshot_count; i++)
    {
      screenshot_request_t *shot = &g_cfg.screenshots[i];
      if(shot->captured || (shot->frame != g_run.current_frame))
        continue;

      if(_capture_screenshot_frame(shot, data_, width_, height_,
                                   pitch_, g_run.pixel_format) != 0)
        {
          _harness_log(RETRO_LOG_ERROR,
                       "[Harness]: failed capturing screenshot %s\n",
                       shot->path);
          g_run.artifact_error = true;
        }
    }

  if((g_cfg.screenshot_every_step > 0) && (data_ != NULL) &&
     (g_run.current_frame > 0) &&
     ((g_run.current_frame % g_cfg.screenshot_every_step) == 0))
    {
      bool write_it = true;

      if(g_cfg.screenshot_when_mode == SCREENSHOT_WHEN_NONBLANK)
        write_it = !_frame_is_blank(data_, width_, height_,
                                    pitch_, g_run.pixel_format);
      else if(g_cfg.screenshot_when_mode == SCREENSHOT_WHEN_CHANGED)
        write_it = !_frame_equals_prev(data_, width_, height_,
                                       pitch_, g_run.pixel_format);

      if(write_it)
        {
          char path[PATH_MAX];
          int n;

          n = snprintf(path, sizeof(path), "%s/frame%06" PRIu64 ".png",
                       g_cfg.screenshot_every_dir, g_run.current_frame);
          if((n > 0) && ((size_t)n < sizeof(path)))
            {
              if(_write_png(path, data_, width_, height_, pitch_,
                            g_run.pixel_format) != 0)
                {
                  _harness_log(RETRO_LOG_ERROR,
                               "[Harness]: failed writing periodic screenshot %s\n",
                               path);
                  g_run.artifact_error = true;
                }
              else
                {
                  g_cfg.screenshot_every_written++;
                  if(g_cfg.screenshot_when_mode == SCREENSHOT_WHEN_CHANGED)
                    _store_screenshot_when_prev(data_, width_, height_,
                                                pitch_, g_run.pixel_format);
                }
            }
        }
      else
        g_cfg.screenshot_when_skipped++;
    }

  if(_terminal_should_render())
    {
      double render_start = _monotonic_seconds();
      double render_seconds;

      g_run.terminal_render_calls++;
      _terminal_render_frame(data_, width_, height_, pitch_, g_run.pixel_format);
      g_run.terminal_last_render_frame = g_run.current_frame;
      render_seconds = _monotonic_seconds() - render_start;
      if((render_seconds >= 0.0) && isfinite(render_seconds))
        {
          g_run.terminal_render_seconds_last = render_seconds;
          g_run.terminal_render_seconds_total += render_seconds;
          if(render_seconds > g_run.terminal_render_seconds_max)
            g_run.terminal_render_seconds_max = render_seconds;
        }
      _terminal_update_adaptive_fps(render_seconds);
    }

}


static
void
_write_u16_le(FILE    *f_,
              uint16_t v_)
{
  uint8_t b[2];

  b[0] = (uint8_t)(v_ & 0xff);
  b[1] = (uint8_t)((v_ >> 8) & 0xff);
  fwrite(b, 1, sizeof(b), f_);
}


static
void
_write_u32_le(FILE    *f_,
              uint32_t v_)
{
  uint8_t b[4];

  b[0] = (uint8_t)(v_ & 0xff);
  b[1] = (uint8_t)((v_ >> 8) & 0xff);
  b[2] = (uint8_t)((v_ >> 16) & 0xff);
  b[3] = (uint8_t)((v_ >> 24) & 0xff);
  fwrite(b, 1, sizeof(b), f_);
}


static
void
_write_wav_header(FILE    *f_,
                  uint32_t sample_rate_,
                  uint32_t data_bytes_)
{
  uint16_t channels = 2;
  uint16_t bits = 16;
  uint32_t byte_rate = sample_rate_ * channels * (bits / 8);
  uint16_t block_align = channels * (bits / 8);

  fwrite("RIFF", 1, 4, f_);
  _write_u32_le(f_, 36U + data_bytes_);
  fwrite("WAVE", 1, 4, f_);
  fwrite("fmt ", 1, 4, f_);
  _write_u32_le(f_, 16);
  _write_u16_le(f_, 1);
  _write_u16_le(f_, channels);
  _write_u32_le(f_, sample_rate_);
  _write_u32_le(f_, byte_rate);
  _write_u16_le(f_, block_align);
  _write_u16_le(f_, bits);
  fwrite("data", 1, 4, f_);
  _write_u32_le(f_, data_bytes_);
}


static
int
_open_audio(void)
{
  uint32_t sample_rate;

  if(g_cfg.no_audio || (g_cfg.audio_path == NULL))
    return 0;

  if(_mkdir_parent(g_cfg.audio_path) != 0)
    return -1;

  g_run.audio_file = fopen(g_cfg.audio_path, "wb+");
  if(g_run.audio_file == NULL)
    return -1;

  sample_rate = (uint32_t)((g_run.sample_rate > 0.0) ? g_run.sample_rate : 44100.0);
  _write_wav_header(g_run.audio_file, sample_rate, 0);
  return ferror(g_run.audio_file) ? -1 : 0;
}


static
void
_close_audio(void)
{
  uint32_t sample_rate;
  uint32_t data_bytes;

  if(g_run.audio_file == NULL)
    return;

  sample_rate = (uint32_t)((g_run.sample_rate > 0.0) ? g_run.sample_rate : 44100.0);
  data_bytes  = (g_run.audio_bytes > UINT32_MAX) ? UINT32_MAX : (uint32_t)g_run.audio_bytes;

  fflush(g_run.audio_file);
  fseek(g_run.audio_file, 0, SEEK_SET);
  _write_wav_header(g_run.audio_file, sample_rate, data_bytes);
  fclose(g_run.audio_file);
  g_run.audio_file = NULL;
}


RETRO_CALLCONV
static
void
_harness_audio_sample(int16_t left_,
                      int16_t right_)
{
  int16_t frame[2];

  frame[0] = left_;
  frame[1] = right_;
  g_run.audio_frames++;

  if(g_run.audio_file == NULL)
    return;

  if(fwrite(frame, sizeof(frame), 1, g_run.audio_file) != 1)
    g_run.artifact_error = true;
  else
    g_run.audio_bytes += sizeof(frame);
}


RETRO_CALLCONV
static
size_t
_harness_audio_sample_batch(const int16_t *data_,
                            size_t         frames_)
{
  size_t bytes;

  g_run.audio_frames += frames_;

  if((g_run.audio_file == NULL) || (data_ == NULL) || (frames_ == 0))
    return frames_;

  bytes = frames_ * 2 * sizeof(int16_t);
  if(fwrite(data_, 1, bytes, g_run.audio_file) != bytes)
    g_run.artifact_error = true;
  else
    g_run.audio_bytes += bytes;

  return frames_;
}


static
void
_sleep_for_seconds(double seconds_)
{
  struct timespec req;
  struct timespec rem;

  if(seconds_ <= 0.0)
    return;

  req.tv_sec = (time_t)seconds_;
  req.tv_nsec = (long)((seconds_ - (double)req.tv_sec) * 1000000000.0);

  while(!g_signal_exit_requested &&
        (nanosleep(&req, &rem) != 0) && (errno == EINTR))
    req = rem;
}


static
unsigned
_terminal_u64_ceil_div(uint64_t value_,
                       uint64_t divisor_)
{
  if(divisor_ == 0ULL)
    return 0;

  if(value_ > (UINT64_MAX - (divisor_ - 1ULL)))
    return 0;

  return (unsigned)((value_ + divisor_ - 1ULL) / divisor_);
}


static
bool
_terminal_u64_mul_checked(uint64_t  a_,
                          uint64_t  b_,
                          uint64_t *out_)
{
  if((a_ != 0ULL) && (b_ > (UINT64_MAX / a_)))
    return false;

  *out_ = a_ * b_;
  return true;
}


static
bool
_terminal_aspect_for_layout(unsigned  width_,
                            unsigned  height_,
                            unsigned  display_cols_,
                            unsigned  display_rows_,
                            uint64_t *n_,
                            uint64_t *d_)
{
  uint64_t n;
  uint64_t d;
  uint64_t tmp;

  if(((g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_KITTY) ||
      (g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_SIXEL)) &&
     (g_run.terminal_pixel_width > 0) &&
     (g_run.terminal_pixel_height > 0))
    {
      if(!_terminal_u64_mul_checked((uint64_t)width_,
                                    (uint64_t)g_run.terminal_pixel_height,
                                    &tmp) ||
         !_terminal_u64_mul_checked(tmp, (uint64_t)display_cols_, &n) ||
         !_terminal_u64_mul_checked((uint64_t)height_,
                                    (uint64_t)g_run.terminal_pixel_width,
                                    &tmp) ||
         !_terminal_u64_mul_checked(tmp, (uint64_t)display_rows_, &d) ||
         (n == 0ULL) || (d == 0ULL))
        return false;

      *n_ = n;
      *d_ = d;
      return true;
    }

  if(width_ > (UINT_MAX / 2U))
    return false;

  *n_ = (uint64_t)width_ * 2ULL;
  *d_ = (uint64_t)height_;
  return true;
}


static
char *
_terminal_append_uint(char    *out_,
                      unsigned value_)
{
  char tmp[10];
  unsigned n = 0;

  if(value_ == 0U)
    {
      *out_++ = '0';
      return out_;
    }

  while(value_ > 0U)
    {
      tmp[n++] = (char)('0' + (value_ % 10U));
      value_ /= 10U;
    }

  while(n > 0U)
    *out_++ = tmp[--n];

  return out_;
}


static
char *
_terminal_append_literal(char       *out_,
                         const char *literal_)
{
  while(*literal_ != 0)
    *out_++ = *literal_++;

  return out_;
}


static
void
_terminal_rgb256_init_tables(void)
{
  unsigned i;

  if(g_terminal_rgb256_tables_ready)
    return;

  for(i = 0; i < 256U; i++)
    {
      unsigned cube;
      unsigned gray_low_step;
      unsigned gray_high_step;

      if(i <= 47U)
        cube = 0U;
      else if(i <= 115U)
        cube = 1U;
      else if(i <= 155U)
        cube = 2U;
      else if(i <= 195U)
        cube = 3U;
      else if(i <= 235U)
        cube = 4U;
      else
        cube = 5U;

      if(i <= 8U)
        gray_low_step = gray_high_step = 0U;
      else if(i >= 238U)
        gray_low_step = gray_high_step = 23U;
      else
        {
          gray_low_step = (i - 8U) / 10U;
          gray_high_step = gray_low_step + 1U;
        }

      g_terminal_rgb256_cube_component[i] = (uint8_t)cube;
      g_terminal_rgb256_cube_value[i] = (uint8_t)((cube == 0U) ? 0U :
                                                  (55U + (cube * 40U)));
      g_terminal_rgb256_gray_low_value[i] =
        (uint8_t)(8U + (gray_low_step * 10U));
      g_terminal_rgb256_gray_low_index[i] =
        (uint8_t)(232U + gray_low_step);
      g_terminal_rgb256_gray_high_value[i] =
        (uint8_t)(8U + (gray_high_step * 10U));
      g_terminal_rgb256_gray_high_index[i] =
        (uint8_t)(232U + gray_high_step);
    }

  g_terminal_rgb256_tables_ready = true;
}


static
uint8_t
_terminal_rgb_to_256(uint32_t rgb_)
{
  unsigned r = (rgb_ >> 16U) & 0xffU;
  unsigned g = (rgb_ >> 8U) & 0xffU;
  unsigned b = rgb_ & 0xffU;
  unsigned gray;
  unsigned cr;
  unsigned cg;
  unsigned cb;
  unsigned cube_r;
  unsigned cube_g;
  unsigned cube_b;
  unsigned gray_v;
  unsigned gray_high_v;
  unsigned cube_diff;
  unsigned gray_diff;
  unsigned gray_high_diff;
  uint8_t gray_index;
  unsigned lo;
  unsigned hi;

  if(!g_terminal_rgb256_tables_ready)
    _terminal_rgb256_init_tables();

  cr = g_terminal_rgb256_cube_component[r];
  cg = g_terminal_rgb256_cube_component[g];
  cb = g_terminal_rgb256_cube_component[b];
  cube_r = g_terminal_rgb256_cube_value[r];
  cube_g = g_terminal_rgb256_cube_value[g];
  cube_b = g_terminal_rgb256_cube_value[b];
  cube_diff = (r > cube_r ? r - cube_r : cube_r - r) +
    (g > cube_g ? g - cube_g : cube_g - g) +
    (b > cube_b ? b - cube_b : cube_b - b);

  lo = r;
  hi = r;
  if(g < lo)
    lo = g;
  if(b < lo)
    lo = b;
  if(g > hi)
    hi = g;
  if(b > hi)
    hi = b;
  gray = r + g + b - lo - hi;
  gray_v = g_terminal_rgb256_gray_low_value[gray];
  gray_diff = (r > gray_v ? r - gray_v : gray_v - r) +
    (g > gray_v ? g - gray_v : gray_v - g) +
    (b > gray_v ? b - gray_v : gray_v - b);
  gray_index = g_terminal_rgb256_gray_low_index[gray];
  gray_high_v = g_terminal_rgb256_gray_high_value[gray];
  gray_high_diff = (r > gray_high_v ? r - gray_high_v : gray_high_v - r) +
    (g > gray_high_v ? g - gray_high_v : gray_high_v - g) +
    (b > gray_high_v ? b - gray_high_v : gray_high_v - b);
  if(gray_high_diff < gray_diff)
    {
      gray_diff = gray_high_diff;
      gray_index = g_terminal_rgb256_gray_high_index[gray];
    }

  if(gray_diff < cube_diff)
    return gray_index;

  return (uint8_t)(16U + (36U * cr) + (6U * cg) + cb);
}


static
void
_terminal_sixel_init_rgb565_lut(void)
{
  unsigned p;

  if(g_terminal_sixel_rgb565_lut_ready)
    return;

  _terminal_rgb256_init_tables();
  for(p = 0; p < HARNESS_TERMINAL_SIXEL_RGB565_LUT_SIZE; p++)
    {
      uint32_t rgb =
        ((uint32_t)_scale_5_to_8((p >> 11U) & 0x1fU) << 16U) |
        ((uint32_t)_scale_6_to_8((p >> 5U) & 0x3fU) << 8U) |
        _scale_5_to_8(p & 0x1fU);

      g_terminal_sixel_rgb565_lut[p] = _terminal_rgb_to_256(rgb);
    }

  g_terminal_sixel_rgb565_lut_ready = true;
}


static
void
_terminal_sixel_init_0rgb1555_lut(void)
{
  unsigned p;

  if(g_terminal_sixel_0rgb1555_lut_ready)
    return;

  _terminal_rgb256_init_tables();
  for(p = 0; p < HARNESS_TERMINAL_SIXEL_0RGB1555_LUT_SIZE; p++)
    {
      uint32_t rgb =
        ((uint32_t)_scale_5_to_8((p >> 10U) & 0x1fU) << 16U) |
        ((uint32_t)_scale_5_to_8((p >> 5U) & 0x1fU) << 8U) |
        _scale_5_to_8(p & 0x1fU);

      g_terminal_sixel_0rgb1555_lut[p] = _terminal_rgb_to_256(rgb);
    }

  g_terminal_sixel_0rgb1555_lut_ready = true;
}


static
void
_terminal_store_color(uint32_t  rgb_,
                      uint8_t  *r_,
                      uint8_t  *g_,
                      uint8_t  *b_)
{
  if(g_run.terminal_color_mode == HARNESS_TERMINAL_COLOR_TRUE)
    {
      *r_ = (uint8_t)((rgb_ >> 16U) & 0xffU);
      *g_ = (uint8_t)((rgb_ >> 8U) & 0xffU);
      *b_ = (uint8_t)(rgb_ & 0xffU);
    }
  else if(g_run.terminal_color_mode == HARNESS_TERMINAL_COLOR_256)
    {
      *r_ = _terminal_rgb_to_256(rgb_);
      *g_ = 0;
      *b_ = 0;
    }
  else
    {
      *r_ = 0;
      *g_ = 0;
      *b_ = 0;
    }
}


static
void
_terminal_move_cursor(unsigned row_,
                      unsigned col_)
{
  char seq[32];
  char *p = seq;

  if(row_ == 0U)
    row_ = 1U;

  if(col_ == 0U)
    col_ = 1U;

  *p++ = '\x1b';
  *p++ = '[';
  p = _terminal_append_uint(p, row_);
  *p++ = ';';
  p = _terminal_append_uint(p, col_);
  *p++ = 'H';

  _terminal_write_all(seq, (size_t)(p - seq));
}


static
void
_terminal_write_sgr_rgb(bool    set_fg_,
                        uint8_t fg_r_,
                        uint8_t fg_g_,
                        uint8_t fg_b_,
                        bool    set_bg_,
                        uint8_t bg_r_,
                        uint8_t bg_g_,
                        uint8_t bg_b_)
{
  char seq[64];
  char *p = seq;

  if(!set_fg_ && !set_bg_)
    return;

  if(g_run.terminal_color_mode == HARNESS_TERMINAL_COLOR_MONO)
    return;

  *p++ = '\x1b';
  *p++ = '[';

  if(set_fg_)
    {
      if(g_run.terminal_color_mode == HARNESS_TERMINAL_COLOR_256)
        {
          p = _terminal_append_literal(p, "38;5;");
          p = _terminal_append_uint(p, fg_r_);
        }
      else
        {
          p = _terminal_append_literal(p, "38;2;");
          p = _terminal_append_uint(p, fg_r_);
          *p++ = ';';
          p = _terminal_append_uint(p, fg_g_);
          *p++ = ';';
          p = _terminal_append_uint(p, fg_b_);
        }
    }

  if(set_bg_)
    {
      if(set_fg_)
        *p++ = ';';
      if(g_run.terminal_color_mode == HARNESS_TERMINAL_COLOR_256)
        {
          p = _terminal_append_literal(p, "48;5;");
          p = _terminal_append_uint(p, bg_r_);
        }
      else
        {
          p = _terminal_append_literal(p, "48;2;");
          p = _terminal_append_uint(p, bg_r_);
          *p++ = ';';
          p = _terminal_append_uint(p, bg_g_);
          *p++ = ';';
          p = _terminal_append_uint(p, bg_b_);
        }
    }

  *p++ = 'm';
  _terminal_write_all(seq, (size_t)(p - seq));
}


static
void
_terminal_write_direct(const void *data_,
                       size_t      size_)
{
  while((g_terminal_force_output || !g_signal_exit_requested) &&
        (data_ != NULL) && (size_ > 0))
    {
      ssize_t n;

      n = write(g_run.tty_fd, data_, size_);
      if(n < 0)
        {
          if(errno == EINTR)
            {
              if(g_signal_exit_requested && !g_terminal_force_output)
                return;
              continue;
            }
          if((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
              struct pollfd pfd;
              int rv;

              pfd.fd = g_run.tty_fd;
              pfd.events = POLLOUT;
              pfd.revents = 0;

              do
                {
                  rv = poll(&pfd, 1, g_signal_exit_requested ? 100 : -1);
                }
              while((g_terminal_force_output || !g_signal_exit_requested) &&
                    (rv < 0) && (errno == EINTR));

              if(rv > 0)
                continue;
            }
          return;
        }

      if(n == 0)
        {
          struct pollfd pfd;
          int rv;

          pfd.fd = g_run.tty_fd;
          pfd.events = POLLOUT;
          pfd.revents = 0;

          do
            {
              rv = poll(&pfd, 1, g_signal_exit_requested ? 100 : -1);
            }
          while((g_terminal_force_output || !g_signal_exit_requested) &&
                (rv < 0) && (errno == EINTR));

          if(rv > 0)
            continue;
          return;
        }

      data_  = (const char *)data_ + n;
      size_ -= (size_t)n;
      if(g_run.terminal_bytes_written <= (UINT64_MAX - (uint64_t)n))
        g_run.terminal_bytes_written += (uint64_t)n;
      else
        g_run.terminal_bytes_written = UINT64_MAX;
    }
}


static
bool
_terminal_output_reserve(size_t add_)
{
  char *next;
  size_t next_cap;

  if(add_ == 0)
    return true;

  if(g_run.terminal_output_cap - g_run.terminal_output_len >= add_)
    return true;

  if(add_ > (SIZE_MAX - g_run.terminal_output_len))
    return false;

  next_cap = (g_run.terminal_output_cap > 0) ? g_run.terminal_output_cap : 65536U;
  while(next_cap < (g_run.terminal_output_len + add_))
    {
      if(next_cap > (SIZE_MAX / 2U))
        {
          next_cap = g_run.terminal_output_len + add_;
          break;
        }
      next_cap *= 2U;
    }

  next = realloc(g_run.terminal_output, next_cap);
  if(next == NULL)
    return false;

  g_run.terminal_output = next;
  g_run.terminal_output_cap = next_cap;
  return true;
}


static
void
_terminal_output_append(const void *data_,
                        size_t      size_)
{
  if((data_ == NULL) || (size_ == 0))
    return;

  if(!_terminal_output_reserve(size_))
    {
      g_run.terminal_output_batching = false;
      if(g_run.terminal_output_len > 0)
        {
          _terminal_write_direct(g_run.terminal_output, g_run.terminal_output_len);
          g_run.terminal_output_len = 0;
        }
      _terminal_write_direct(data_, size_);
      return;
    }

  memcpy(g_run.terminal_output + g_run.terminal_output_len, data_, size_);
  g_run.terminal_output_len += size_;
}


static
void
_terminal_output_begin(void)
{
  g_run.terminal_output_len = 0;
  g_run.terminal_output_batching = true;
}


static
void
_terminal_output_flush(void)
{
  g_run.terminal_output_batching = false;
  if(g_run.terminal_output_len > 0)
    {
      _terminal_write_direct(g_run.terminal_output, g_run.terminal_output_len);
      g_run.terminal_output_len = 0;
    }
}


static
void
_terminal_free_output_buffer(void)
{
  free(g_run.terminal_output);
  g_run.terminal_output = NULL;
  g_run.terminal_output_len = 0;
  g_run.terminal_output_cap = 0;
  g_run.terminal_output_batching = false;
}


static
void
_terminal_write_all(const void *data_,
                    size_t      size_)
{
  if(g_run.terminal_output_batching)
    _terminal_output_append(data_, size_);
  else
    _terminal_write_direct(data_, size_);
}


static
bool
_byte_buffer_reserve(byte_buffer_t *buffer_,
                     size_t         add_)
{
  uint8_t *next;
  size_t next_cap;

  if((buffer_ == NULL) || buffer_->failed)
    return false;
  if(add_ == 0)
    return true;
  if(add_ > (SIZE_MAX - buffer_->len))
    {
      buffer_->failed = true;
      return false;
    }
  if(buffer_->cap - buffer_->len >= add_)
    return true;

  next_cap = (buffer_->cap > 0) ? buffer_->cap : 65536U;
  while(next_cap < (buffer_->len + add_))
    {
      if(next_cap > (SIZE_MAX / 2U))
        {
          next_cap = buffer_->len + add_;
          break;
        }
      next_cap *= 2U;
    }

  next = (uint8_t *)realloc(buffer_->data, next_cap);
  if(next == NULL)
    {
      buffer_->failed = true;
      return false;
    }

  buffer_->data = next;
  buffer_->cap = next_cap;
  return true;
}


static
void
_byte_buffer_append(byte_buffer_t *buffer_,
                    const void    *data_,
                    size_t         size_)
{
  if((buffer_ == NULL) || (data_ == NULL) || (size_ == 0))
    return;

  if(!_byte_buffer_reserve(buffer_, size_))
    return;

  memcpy(buffer_->data + buffer_->len, data_, size_);
  buffer_->len += size_;
}


static
void
_byte_buffer_reset(byte_buffer_t *buffer_)
{
  if(buffer_ == NULL)
    return;

  buffer_->len = 0;
  buffer_->failed = false;
}


static
void
_byte_buffer_free(byte_buffer_t *buffer_)
{
  if(buffer_ == NULL)
    return;

  free(buffer_->data);
  memset(buffer_, 0, sizeof(*buffer_));
}


static
void
_stbi_write_to_byte_buffer(void *context_,
                           void *data_,
                           int   size_)
{
  byte_buffer_t *buffer = (byte_buffer_t *)context_;

  if(size_ <= 0)
    return;

  _byte_buffer_append(buffer, data_, (size_t)size_);
}


static
size_t
_base64_encode_chunk(const uint8_t *src_,
                     size_t         len_,
                     char          *dst_)
{
  static const char b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i = 0;
  size_t o = 0;
  size_t full = len_ - (len_ % 3U);

  while(i < full)
    {
      unsigned a = src_[i++];
      unsigned b = src_[i++];
      unsigned c = src_[i++];
      unsigned triple = (a << 16U) | (b << 8U) | c;

      dst_[o++] = b64[(triple >> 18U) & 0x3fU];
      dst_[o++] = b64[(triple >> 12U) & 0x3fU];
      dst_[o++] = b64[(triple >> 6U) & 0x3fU];
      dst_[o++] = b64[triple & 0x3fU];
    }

  if(i < len_)
    {
      unsigned a = src_[i++];
      bool have_b = i < len_;
      unsigned b = have_b ? src_[i++] : 0U;
      unsigned triple = (a << 16U) | (b << 8U);

      dst_[o++] = b64[(triple >> 18U) & 0x3fU];
      dst_[o++] = b64[(triple >> 12U) & 0x3fU];
      dst_[o++] = have_b ? b64[(triple >> 6U) & 0x3fU] : '=';
      dst_[o++] = '=';
    }

  return o;
}


static
void
_terminal_record_image_seconds(double seconds_)
{
  if((seconds_ < 0.0) || !isfinite(seconds_))
    return;

  g_run.terminal_image_seconds_last = seconds_;
  g_run.terminal_image_seconds_total += seconds_;
  if(seconds_ > g_run.terminal_image_seconds_max)
    g_run.terminal_image_seconds_max = seconds_;
}


static
bool
_terminal_kitty_target_size(unsigned  src_width_,
                            unsigned  src_height_,
                            unsigned  cols_,
                            unsigned  rows_,
                            unsigned *width_out_,
                            unsigned *height_out_)
{
  uint64_t cell_w;
  uint64_t cell_h;
  uint64_t max_w;
  uint64_t max_h;
  uint64_t fit_h;
  uint64_t fit_w;
  uint64_t scaled;

  *width_out_ = src_width_;
  *height_out_ = src_height_;

  if((src_width_ == 0U) || (src_height_ == 0U) ||
     (cols_ == 0U) || (rows_ == 0U) ||
     (g_run.terminal_pixel_width <= 0) ||
     (g_run.terminal_pixel_height <= 0) ||
     (g_run.terminal_width <= 0) ||
     (g_run.terminal_height <= 0))
    {
      /* No cell metrics: cap to a sane upper bound so we never emit a
         full native-resolution image when the terminal gives no sizing
         hints. The terminal scales the image into the cell grid anyway. */
      *width_out_ = src_width_;
      *height_out_ = src_height_;
      if(*width_out_ > HARNESS_TERMINAL_KITTY_MAX_WIDTH)
        {
          *height_out_ = (unsigned)((uint64_t)*height_out_ *
                                    HARNESS_TERMINAL_KITTY_MAX_WIDTH /
                                    *width_out_);
          *width_out_ = HARNESS_TERMINAL_KITTY_MAX_WIDTH;
        }
      if(*height_out_ > HARNESS_TERMINAL_KITTY_MAX_HEIGHT)
        {
          *width_out_ = (unsigned)((uint64_t)*width_out_ *
                                   HARNESS_TERMINAL_KITTY_MAX_HEIGHT /
                                   *height_out_);
          *height_out_ = HARNESS_TERMINAL_KITTY_MAX_HEIGHT;
        }
      if(*width_out_ == 0U)
        *width_out_ = 1U;
      if(*height_out_ == 0U)
        *height_out_ = 1U;
      return true;
    }

  cell_w = (uint64_t)g_run.terminal_pixel_width /
    (uint64_t)g_run.terminal_width;
  cell_h = (uint64_t)g_run.terminal_pixel_height /
    (uint64_t)g_run.terminal_height;
  if((cell_w == 0ULL) || (cell_h == 0ULL))
    return true;

  if(!_terminal_u64_mul_checked((uint64_t)cols_, cell_w, &max_w) ||
     !_terminal_u64_mul_checked((uint64_t)rows_, cell_h, &max_h) ||
     (max_w == 0ULL) || (max_h == 0ULL))
    return false;

  if(max_w > (uint64_t)src_width_)
    max_w = (uint64_t)src_width_;
  if(max_h > (uint64_t)src_height_)
    max_h = (uint64_t)src_height_;
  if((max_w == (uint64_t)src_width_) && (max_h == (uint64_t)src_height_))
    return true;

  if(!_terminal_u64_mul_checked((uint64_t)src_height_, max_w, &scaled))
    return false;
  fit_h = (scaled + ((uint64_t)src_width_ / 2ULL)) / (uint64_t)src_width_;
  if((fit_h == 0ULL) || (fit_h > max_h))
    {
      fit_h = max_h;
      if(!_terminal_u64_mul_checked((uint64_t)src_width_, fit_h, &scaled))
        return false;
      fit_w = (scaled + ((uint64_t)src_height_ / 2ULL)) /
        (uint64_t)src_height_;
      if(fit_w == 0ULL)
        fit_w = 1ULL;
    }
  else
    {
      fit_w = max_w;
    }

  if((fit_w > UINT_MAX) || (fit_h > UINT_MAX))
    return false;

  *width_out_ = (unsigned)fit_w;
  *height_out_ = (unsigned)fit_h;
  return true;
}


static
bool
_terminal_resize_rgb_nearest(const uint8_t *src_,
                             size_t         src_stride_,
                             unsigned       src_width_,
                             unsigned       src_height_,
                             uint8_t       *dst_,
                             size_t         dst_stride_,
                             unsigned       dst_width_,
                             unsigned       dst_height_)
{
  unsigned y;

  if((src_ == NULL) || (dst_ == NULL) ||
     (src_width_ == 0U) || (src_height_ == 0U) ||
     (dst_width_ == 0U) || (dst_height_ == 0U) ||
     (src_width_ > (UINT_MAX / 3U)) ||
     (dst_width_ > (UINT_MAX / 3U)) ||
     (src_stride_ < ((size_t)src_width_ * 3U)) ||
     (dst_stride_ < ((size_t)dst_width_ * 3U)))
    return false;

  for(y = 0; y < dst_height_; y++)
    {
      uint64_t sample_y = (((uint64_t)y * (uint64_t)src_height_) +
                           ((uint64_t)src_height_ / 2ULL)) /
        (uint64_t)dst_height_;
      const uint8_t *src_row;
      uint8_t *dst_row = dst_ + ((size_t)y * dst_stride_);
      unsigned x;

      if(sample_y >= (uint64_t)src_height_)
        sample_y = (uint64_t)src_height_ - 1ULL;
      src_row = src_ + ((size_t)sample_y * src_stride_);

      for(x = 0; x < dst_width_; x++)
        {
          uint64_t sample_x = (((uint64_t)x * (uint64_t)src_width_) +
                               ((uint64_t)src_width_ / 2ULL)) /
            (uint64_t)dst_width_;
          const uint8_t *src;
          uint8_t *dst;

          if(sample_x >= (uint64_t)src_width_)
            sample_x = (uint64_t)src_width_ - 1ULL;

          src = src_row + ((size_t)sample_x * 3U);
          dst = dst_row + ((size_t)x * 3U);
          dst[0] = src[0];
          dst[1] = src[1];
          dst[2] = src[2];
        }
    }

  return true;
}


static
bool
_terminal_compress_kitty_rgb(const uint8_t  *rgb_,
                             size_t          rgb_len_,
                             const uint8_t **compressed_out_,
                             size_t         *compressed_len_out_)
{
  byte_buffer_t *compressed = &g_run.terminal_kitty_zlib_buffer;
  z_stream *stream = &g_run.terminal_kitty_zstream;
  uLong bound;
  int status;

  if(compressed_out_ != NULL)
    *compressed_out_ = NULL;
  if(compressed_len_out_ != NULL)
    *compressed_len_out_ = 0;

  if((rgb_ == NULL) || (rgb_len_ == 0U) || (rgb_len_ > UINT_MAX))
    return false;

  if(!g_run.terminal_kitty_zstream_initialized)
    {
      memset(stream, 0, sizeof(*stream));
      status = deflateInit(stream, HARNESS_TERMINAL_KITTY_ZLIB_LEVEL);
      if(status != Z_OK)
        return false;
      g_run.terminal_kitty_zstream_initialized = true;
    }
  else if(deflateReset(stream) != Z_OK)
    {
      return false;
    }

  bound = deflateBound(stream, (uLong)rgb_len_);
  if((bound == 0UL) || (bound > UINT_MAX))
    return false;

  _byte_buffer_reset(compressed);
  if(!_byte_buffer_reserve(compressed, (size_t)bound))
    return false;

  stream->next_in = (Bytef *)rgb_;
  stream->avail_in = (uInt)rgb_len_;
  stream->next_out = compressed->data;
  stream->avail_out = (uInt)bound;
  status = deflate(stream, Z_FINISH);
  if((status != Z_STREAM_END) || (stream->avail_in != 0U))
    return false;

  compressed->len = (size_t)bound - (size_t)stream->avail_out;
  if(compressed->len == 0U)
    return false;

  if(compressed_out_ != NULL)
    *compressed_out_ = compressed->data;
  if(compressed_len_out_ != NULL)
    *compressed_len_out_ = compressed->len;
  return true;
}


static
void
_terminal_write_kitty_image(const uint8_t *data_,
                            size_t         data_len_,
                            unsigned       image_width_,
                            unsigned       image_height_,
                            bool           zlib_rgb_,
                            unsigned       cols_,
                            unsigned       rows_)
{
  size_t off = 0;
  bool first = true;

  while(off < data_len_)
    {
      char payload[((HARNESS_TERMINAL_KITTY_CHUNK_RAW + 2U) / 3U) * 4U];
      char header[192];
      size_t raw_len = data_len_ - off;
      size_t payload_len;
      bool more;
      int n;

      if(raw_len > HARNESS_TERMINAL_KITTY_CHUNK_RAW)
        raw_len = HARNESS_TERMINAL_KITTY_CHUNK_RAW;

      payload_len = _base64_encode_chunk(data_ + off, raw_len, payload);
      off += raw_len;
      more = off < data_len_;

      if(first)
        {
          if(zlib_rgb_)
            n = snprintf(header,
                         sizeof(header),
                         "\x1b_Ga=T,t=d,f=24,s=%u,v=%u,o=z,i=%u,p=%u,"
                         "q=2,c=%u,r=%u,C=1,m=%u;",
                         image_width_,
                         image_height_,
                         HARNESS_TERMINAL_KITTY_IMAGE_ID,
                         HARNESS_TERMINAL_KITTY_PLACEMENT_ID,
                         cols_,
                         rows_,
                         more ? 1U : 0U);
          else
            n = snprintf(header,
                         sizeof(header),
                         "\x1b_Ga=T,t=d,f=100,i=%u,p=%u,q=2,c=%u,r=%u,"
                         "C=1,m=%u;",
                         HARNESS_TERMINAL_KITTY_IMAGE_ID,
                         HARNESS_TERMINAL_KITTY_PLACEMENT_ID,
                         cols_,
                         rows_,
                         more ? 1U : 0U);
        }
      else
        n = snprintf(header,
                     sizeof(header),
                     "\x1b_Gq=2,m=%u;",
                     more ? 1U : 0U);

      if((n <= 0) || ((size_t)n >= sizeof(header)))
        return;

      _terminal_write_all(header, (size_t)n);
      _terminal_write_all(payload, payload_len);
      _terminal_write_all("\x1b\\", 2U);
      first = false;
    }
}


static
int
_terminal_render_kitty_image(const void             *data_,
                             unsigned                width_,
                             unsigned                height_,
                             size_t                  pitch_,
                             enum retro_pixel_format fmt_,
                             unsigned                row_,
                             unsigned                col_,
                             unsigned                cols_,
                             unsigned                rows_)
{
  byte_buffer_t *png;
  const uint8_t *payload;
  uint8_t *packed;
  uint8_t *image_pixels;
  size_t payload_len = 0;
  size_t stride;
  size_t image_stride;
  size_t image_size;
  unsigned image_width;
  unsigned image_height;
  double start;
  bool ok = false;
  bool zlib_rgb = false;

  if((cols_ == 0U) || (rows_ == 0U) || (width_ > (unsigned)INT_MAX) ||
     (height_ > (unsigned)INT_MAX))
    return HARNESS_TERMINAL_IMAGE_FAILED;

  if(!_terminal_kitty_target_size(width_, height_, cols_, rows_,
                                  &image_width, &image_height))
    return HARNESS_TERMINAL_IMAGE_FAILED;

  if(_terminal_image_cache_matches(data_,
                                   pitch_,
                                   HARNESS_TERMINAL_RENDER_KITTY,
                                   width_,
                                   height_,
                                   fmt_,
                                   row_,
                                   col_,
                                   cols_,
                                   rows_,
                                   image_width,
                                   image_height))
    return HARNESS_TERMINAL_IMAGE_SKIPPED;

  start = _monotonic_seconds();
  if(!_terminal_pack_frame_rgb(data_, width_, height_, pitch_, fmt_,
                               &packed, &stride))
    {
      _terminal_record_image_seconds(_monotonic_seconds() - start);
      return HARNESS_TERMINAL_IMAGE_FAILED;
    }

  image_pixels = packed;
  image_stride = stride;
  if((image_width != width_) || (image_height != height_))
    {
      size_t needed;

      if((image_width > (UINT_MAX / 3U)) ||
         ((size_t)image_height > (SIZE_MAX / ((size_t)image_width * 3U))))
        {
          _terminal_record_image_seconds(_monotonic_seconds() - start);
          return HARNESS_TERMINAL_IMAGE_FAILED;
        }

      image_stride = (size_t)image_width * 3U;
      needed = image_stride * (size_t)image_height;
      if(!_terminal_ensure_u8_buffer(&g_run.terminal_scaled_rgb_buffer,
                                     &g_run.terminal_scaled_rgb_buffer_size,
                                     needed) ||
         !_terminal_resize_rgb_nearest(packed,
                                       stride,
                                       width_,
                                       height_,
                                       g_run.terminal_scaled_rgb_buffer,
                                       image_stride,
                                       image_width,
                                       image_height))
        {
          _terminal_record_image_seconds(_monotonic_seconds() - start);
          return HARNESS_TERMINAL_IMAGE_FAILED;
        }
      image_pixels = g_run.terminal_scaled_rgb_buffer;
    }

  if((image_stride > 0U) &&
     ((size_t)image_height > (SIZE_MAX / image_stride)))
    {
      _terminal_record_image_seconds(_monotonic_seconds() - start);
      return HARNESS_TERMINAL_IMAGE_FAILED;
    }
  image_size = image_stride * (size_t)image_height;

  payload = NULL;
  if(g_run.terminal_kitty_zlib_supported)
    zlib_rgb = _terminal_compress_kitty_rgb(image_pixels,
                                            image_size,
                                            &payload,
                                            &payload_len);

  if(!zlib_rgb)
    {
      png = &g_run.terminal_png_buffer;
      _byte_buffer_reset(png);
      ok = (stbi_write_png_to_func(_stbi_write_to_byte_buffer,
                                   png,
                                   (int)image_width,
                                   (int)image_height,
                                   3,
                                   image_pixels,
                                   (int)image_stride) != 0) &&
        !png->failed && (png->len > 0);

      if(ok)
        {
          payload = png->data;
          payload_len = png->len;
        }
    }
  else
    {
      ok = true;
    }

  if(!ok)
    {
      _terminal_record_image_seconds(_monotonic_seconds() - start);
      return HARNESS_TERMINAL_IMAGE_FAILED;
    }

  _terminal_move_cursor(row_, col_);
  _terminal_write_kitty_image(payload,
                              payload_len,
                              image_width,
                              image_height,
                              zlib_rgb,
                              cols_,
                              rows_);
  _terminal_store_image_cache(data_,
                              pitch_,
                              HARNESS_TERMINAL_RENDER_KITTY,
                              width_,
                              height_,
                              fmt_,
                              row_,
                              col_,
                              cols_,
                              rows_,
                              image_width,
                              image_height);
  _terminal_record_image_seconds(_monotonic_seconds() - start);
  return HARNESS_TERMINAL_IMAGE_DRAWN;
}


static
void
_terminal_sixel_color_percent(uint8_t   index_,
                              unsigned *r_,
                              unsigned *g_,
                              unsigned *b_)
{
  static const uint8_t basic[16][3] =
    {
      {   0,   0,   0 }, { 128,   0,   0 }, {   0, 128,   0 }, { 128, 128,   0 },
      {   0,   0, 128 }, { 128,   0, 128 }, {   0, 128, 128 }, { 192, 192, 192 },
      { 128, 128, 128 }, { 255,   0,   0 }, {   0, 255,   0 }, { 255, 255,   0 },
      {   0,   0, 255 }, { 255,   0, 255 }, {   0, 255, 255 }, { 255, 255, 255 }
    };
  unsigned r;
  unsigned g;
  unsigned b;

  if(index_ < 16U)
    {
      r = basic[index_][0];
      g = basic[index_][1];
      b = basic[index_][2];
    }
  else if(index_ < 232U)
    {
      unsigned v = (unsigned)index_ - 16U;
      unsigned br = v % 6U;
      unsigned bg = (v / 6U) % 6U;
      unsigned bb = (v / 36U) % 6U;

      r = (bb == 0U) ? 0U : (55U + (bb * 40U));
      g = (bg == 0U) ? 0U : (55U + (bg * 40U));
      b = (br == 0U) ? 0U : (55U + (br * 40U));
    }
  else
    {
      r = g = b = 8U + (((unsigned)index_ - 232U) * 10U);
    }

  *r_ = (r * 100U + 127U) / 255U;
  *g_ = (g * 100U + 127U) / 255U;
  *b_ = (b * 100U + 127U) / 255U;
}


static
void
_terminal_write_sixel_color_define(unsigned slot_,
                                   uint8_t  xterm_index_)
{
  char seq[48];
  unsigned r;
  unsigned g;
  unsigned b;
  int n;

  _terminal_sixel_color_percent(xterm_index_, &r, &g, &b);
  n = snprintf(seq, sizeof(seq), "#%u;2;%u;%u;%u", slot_, r, g, b);
  if((n > 0) && ((size_t)n < sizeof(seq)))
    _terminal_write_all(seq, (size_t)n);
}


static
void
_terminal_write_sixel_color_select(unsigned index_)
{
  char seq[16];
  int n;

  n = snprintf(seq, sizeof(seq), "#%u", index_);
  if((n > 0) && ((size_t)n < sizeof(seq)))
    _terminal_write_all(seq, (size_t)n);
}


static
void
_terminal_write_sixel_run_char(char     ch_,
                               unsigned count_)
{
  char seq[32];

  if(count_ == 0U)
    return;

  if(count_ >= 4U)
    {
      int n = snprintf(seq, sizeof(seq), "!%u%c", count_, ch_);
      if((n > 0) && ((size_t)n < sizeof(seq)))
        _terminal_write_all(seq, (size_t)n);
      return;
    }

  _terminal_write_repeat_char(ch_, count_);
}


static
bool
_terminal_sixel_ensure_event_capacity(size_t needed_)
{
  terminal_sixel_event_t *next;

  if(g_run.terminal_sixel_event_capacity >= needed_)
    return true;
  if(needed_ > (SIZE_MAX / sizeof(*next)))
    return false;

  next = realloc(g_run.terminal_sixel_events, needed_ * sizeof(*next));
  if(next == NULL)
    return false;

  g_run.terminal_sixel_events = next;
  g_run.terminal_sixel_event_capacity = needed_;
  return true;
}


static
bool
_terminal_sixel_accumulate_run(char      ch_,
                               unsigned  count_,
                               char     *pending_ch_,
                               unsigned *pending_count_)
{
  if(count_ == 0U)
    return true;

  if(*pending_count_ == 0U)
    {
      *pending_ch_ = ch_;
      *pending_count_ = count_;
      return true;
    }

  if(*pending_ch_ == ch_)
    {
      if(count_ > (UINT_MAX - *pending_count_))
        return false;
      *pending_count_ += count_;
      return true;
    }

  _terminal_write_sixel_run_char(*pending_ch_, *pending_count_);
  *pending_ch_ = ch_;
  *pending_count_ = count_;
  return true;
}


static
bool
_terminal_sixel_target_size(unsigned  src_width_,
                            unsigned  src_height_,
                            unsigned  cols_,
                            unsigned  rows_,
                            unsigned *width_out_,
                            unsigned *height_out_)
{
  uint64_t cell_w;
  uint64_t cell_h;
  uint64_t max_w;
  uint64_t max_h;
  uint64_t fit_w;
  uint64_t fit_h;

  if((src_width_ == 0U) || (src_height_ == 0U) ||
     (cols_ == 0U) || (rows_ == 0U))
    return false;

  cell_w = HARNESS_TERMINAL_SIXEL_FALLBACK_CELL_W;
  cell_h = HARNESS_TERMINAL_SIXEL_FALLBACK_CELL_H;

  if((g_run.terminal_pixel_width > 0) && (g_run.terminal_width > 0))
    {
      uint64_t v = (uint64_t)g_run.terminal_pixel_width /
        (uint64_t)g_run.terminal_width;
      if(v > 0ULL)
        cell_w = v;
    }
  if((g_run.terminal_pixel_height > 0) && (g_run.terminal_height > 0))
    {
      uint64_t v = (uint64_t)g_run.terminal_pixel_height /
        (uint64_t)g_run.terminal_height;
      if(v > 0ULL)
        cell_h = v;
    }

  if(!_terminal_u64_mul_checked((uint64_t)cols_, cell_w, &max_w) ||
     !_terminal_u64_mul_checked((uint64_t)rows_, cell_h, &max_h) ||
     (max_w == 0ULL) || (max_h == 0ULL))
    return false;

  /* Sixel has no placement-time scaling, so emit the full cell-grid
     raster reserved by the layout. */
  fit_w = max_w;
  fit_h = max_h;

  if(fit_w > HARNESS_TERMINAL_SIXEL_MAX_WIDTH)
    {
      fit_h = (fit_h * HARNESS_TERMINAL_SIXEL_MAX_WIDTH + (fit_w / 2ULL)) /
        fit_w;
      fit_w = HARNESS_TERMINAL_SIXEL_MAX_WIDTH;
      if(fit_h == 0ULL)
        fit_h = 1ULL;
    }
  if(fit_h > HARNESS_TERMINAL_SIXEL_MAX_HEIGHT)
    {
      fit_w = (fit_w * HARNESS_TERMINAL_SIXEL_MAX_HEIGHT + (fit_h / 2ULL)) /
        fit_h;
      fit_h = HARNESS_TERMINAL_SIXEL_MAX_HEIGHT;
      if(fit_w == 0ULL)
        fit_w = 1ULL;
    }

  if((fit_w > UINT_MAX) || (fit_h > UINT_MAX))
    return false;

  *width_out_ = (unsigned)fit_w;
  *height_out_ = (unsigned)fit_h;
  return true;
}


static
bool
_terminal_sixel_ensure_coord_cache(unsigned src_width_,
                                   unsigned src_height_,
                                   unsigned out_width_,
                                   unsigned out_height_)
{
  unsigned *src_x;
  unsigned *src_y;
  unsigned x;
  unsigned y;

  if((src_width_ == 0U) || (src_height_ == 0U) ||
     (out_width_ == 0U) || (out_height_ == 0U))
    return false;

  if((g_run.terminal_sixel_src_x != NULL) &&
     (g_run.terminal_sixel_src_y != NULL) &&
     (g_run.terminal_sixel_map_src_width == src_width_) &&
     (g_run.terminal_sixel_map_src_height == src_height_) &&
     (g_run.terminal_sixel_map_out_width == out_width_) &&
     (g_run.terminal_sixel_map_out_height == out_height_))
    return true;

  src_x = malloc((size_t)out_width_ * sizeof(*src_x));
  src_y = malloc((size_t)out_height_ * sizeof(*src_y));
  if((src_x == NULL) || (src_y == NULL))
    {
      free(src_x);
      free(src_y);
      return false;
    }

  for(x = 0; x < out_width_; x++)
    {
      uint64_t sample = (((uint64_t)x * (uint64_t)src_width_) +
                         ((uint64_t)src_width_ / 2ULL)) /
        (uint64_t)out_width_;

      src_x[x] = (sample >= (uint64_t)src_width_)
        ? (src_width_ - 1U)
        : (unsigned)sample;
    }

  for(y = 0; y < out_height_; y++)
    {
      uint64_t sample = (((uint64_t)y * (uint64_t)src_height_) +
                         ((uint64_t)src_height_ / 2ULL)) /
        (uint64_t)out_height_;

      src_y[y] = (sample >= (uint64_t)src_height_)
        ? (src_height_ - 1U)
        : (unsigned)sample;
    }

  free(g_run.terminal_sixel_src_x);
  free(g_run.terminal_sixel_src_y);
  g_run.terminal_sixel_src_x = src_x;
  g_run.terminal_sixel_src_y = src_y;
  g_run.terminal_sixel_map_src_width = src_width_;
  g_run.terminal_sixel_map_src_height = src_height_;
  g_run.terminal_sixel_map_out_width = out_width_;
  g_run.terminal_sixel_map_out_height = out_height_;
  return true;
}


static
void
_terminal_sixel_quantize_xrgb_source(const void *data_,
                                     unsigned    src_width_,
                                     unsigned    src_height_,
                                     size_t      pitch_,
                                     uint8_t    *indices_)
{
  unsigned y;

  for(y = 0; y < src_height_; y++)
    {
      const uint8_t *src_row =
        (const uint8_t *)data_ + (pitch_ * (size_t)y);
      uint8_t *dst_row = indices_ + ((size_t)y * (size_t)src_width_);
      unsigned x;

      for(x = 0; x < src_width_; x++)
        {
          uint32_t rgb = ((const uint32_t *)src_row)[x] & 0x00ffffffU;

          dst_row[x] = _terminal_rgb_to_256(rgb);
        }
    }
}


static
void
_terminal_sixel_scale_indices(const uint8_t *src_indices_,
                              unsigned       src_width_,
                              unsigned       out_width_,
                              unsigned       out_height_,
                              uint8_t       *indices_,
                              bool           used_xterm_[HARNESS_TERMINAL_SIXEL_COLORS])
{
  unsigned y;

  for(y = 0; y < out_height_; y++)
    {
      const uint8_t *src_row = src_indices_ +
        ((size_t)g_run.terminal_sixel_src_y[y] * (size_t)src_width_);
      uint8_t *dst_row = indices_ + ((size_t)y * (size_t)out_width_);
      unsigned x;

      for(x = 0; x < out_width_; x++)
        {
          uint8_t index = src_row[g_run.terminal_sixel_src_x[x]];

          dst_row[x] = index;
          used_xterm_[index] = true;
        }
    }
}


static
void
_terminal_sixel_quantize_mapped(const void *data_,
                                unsigned    out_width_,
                                unsigned    out_height_,
                                size_t      pitch_,
                                enum retro_pixel_format fmt_,
                                uint8_t    *indices_,
                                bool        used_xterm_[HARNESS_TERMINAL_SIXEL_COLORS])
{
  unsigned y;

  switch(fmt_)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888:
      for(y = 0; y < out_height_; y++)
        {
          const uint8_t *src_row = (const uint8_t *)data_ +
            (pitch_ * (size_t)g_run.terminal_sixel_src_y[y]);
          uint8_t *dst_row = indices_ + ((size_t)y * (size_t)out_width_);
          unsigned x;

          for(x = 0; x < out_width_; x++)
            {
              uint32_t rgb;
              uint8_t index;

              rgb = ((const uint32_t *)src_row)
                [g_run.terminal_sixel_src_x[x]] & 0x00ffffffU;
              index = _terminal_rgb_to_256(rgb);
              dst_row[x] = index;
              used_xterm_[index] = true;
            }
        }
      break;
    case RETRO_PIXEL_FORMAT_RGB565:
      _terminal_sixel_init_rgb565_lut();
      for(y = 0; y < out_height_; y++)
        {
          const uint8_t *src_row = (const uint8_t *)data_ +
            (pitch_ * (size_t)g_run.terminal_sixel_src_y[y]);
          uint8_t *dst_row = indices_ + ((size_t)y * (size_t)out_width_);
          unsigned x;

          for(x = 0; x < out_width_; x++)
            {
              uint16_t p;
              uint8_t index;

              p = ((const uint16_t *)src_row)
                [g_run.terminal_sixel_src_x[x]];
              index = g_terminal_sixel_rgb565_lut[p];
              dst_row[x] = index;
              used_xterm_[index] = true;
            }
        }
      break;
    case RETRO_PIXEL_FORMAT_0RGB1555:
    default:
      _terminal_sixel_init_0rgb1555_lut();
      for(y = 0; y < out_height_; y++)
        {
          const uint8_t *src_row = (const uint8_t *)data_ +
            (pitch_ * (size_t)g_run.terminal_sixel_src_y[y]);
          uint8_t *dst_row = indices_ + ((size_t)y * (size_t)out_width_);
          unsigned x;

          for(x = 0; x < out_width_; x++)
            {
              uint16_t p;
              uint8_t index;

              p = ((const uint16_t *)src_row)
                [g_run.terminal_sixel_src_x[x]];
              index = g_terminal_sixel_0rgb1555_lut[p & 0x7fffU];
              dst_row[x] = index;
              used_xterm_[index] = true;
            }
        }
      break;
    }
}


static
bool
_terminal_sixel_quantize_frame(const void *data_,
                               unsigned    src_width_,
                               unsigned    src_height_,
                               size_t      pitch_,
                               enum retro_pixel_format fmt_,
                               unsigned    out_width_,
                               unsigned    out_height_,
                               uint8_t    *indices_,
                               size_t      indices_size_,
                               bool        used_xterm_[HARNESS_TERMINAL_SIXEL_COLORS])
{
  size_t out_count;
  size_t src_count;

  if((data_ == NULL) || (indices_ == NULL) ||
     (src_width_ == 0U) || (src_height_ == 0U) ||
     (out_width_ == 0U) || (out_height_ == 0U) ||
     ((size_t)out_height_ > (SIZE_MAX / (size_t)out_width_)) ||
     ((size_t)src_height_ > (SIZE_MAX / (size_t)src_width_)))
    return false;

  out_count = (size_t)out_width_ * (size_t)out_height_;
  src_count = (size_t)src_width_ * (size_t)src_height_;
  if((indices_size_ < out_count) ||
     !_terminal_sixel_ensure_coord_cache(src_width_,
                                         src_height_,
                                         out_width_,
                                         out_height_))
    return false;

  memset(used_xterm_, 0,
         HARNESS_TERMINAL_SIXEL_COLORS * sizeof(used_xterm_[0]));

  if((fmt_ == RETRO_PIXEL_FORMAT_XRGB8888) && (out_count > src_count))
    {
      if(!_terminal_ensure_u8_buffer(&g_run.terminal_sixel_source_indices,
                                     &g_run.terminal_sixel_source_indices_size,
                                     src_count))
        return false;

      _terminal_sixel_quantize_xrgb_source(
        data_,
        src_width_,
        src_height_,
        pitch_,
        g_run.terminal_sixel_source_indices);
      _terminal_sixel_scale_indices(g_run.terminal_sixel_source_indices,
                                    src_width_,
                                    out_width_,
                                    out_height_,
                                    indices_,
                                    used_xterm_);
      return true;
    }

  _terminal_sixel_quantize_mapped(data_,
                                  out_width_,
                                  out_height_,
                                  pitch_,
                                  fmt_,
                                  indices_,
                                  used_xterm_);

  return true;
}


static
void
_terminal_sixel_compact_palette(uint8_t                  *indices_,
                                size_t                    count_,
                                const bool                used_xterm_[HARNESS_TERMINAL_SIXEL_COLORS],
                                terminal_sixel_palette_t *palette_)
{
  uint8_t xterm_to_slot[HARNESS_TERMINAL_SIXEL_COLORS];
  unsigned i;
  size_t n;

  memset(xterm_to_slot, 0, sizeof(xterm_to_slot));
  memset(palette_, 0, sizeof(*palette_));

  for(i = 0; i < HARNESS_TERMINAL_SIXEL_COLORS; i++)
    {
      if(!used_xterm_[i])
        continue;

      xterm_to_slot[i] = (uint8_t)palette_->count;
      palette_->xterm_index[palette_->count] = (uint8_t)i;
      palette_->count++;
    }

  for(n = 0; n < count_; n++)
    indices_[n] = xterm_to_slot[indices_[n]];
}


static
bool
_terminal_sixel_build_band_events(
  const uint8_t *indices_,
  unsigned       width_,
  unsigned       y_,
  unsigned       band_rows_,
  unsigned       palette_count_,
  unsigned       band_colors_[HARNESS_TERMINAL_SIXEL_COLORS],
  unsigned      *band_color_count_,
  unsigned       heads_[HARNESS_TERMINAL_SIXEL_COLORS],
  unsigned      *event_count_)
{
  const uint8_t *rows[6];
  bool seen[HARNESS_TERMINAL_SIXEL_COLORS];
  unsigned tails[HARNESS_TERMINAL_SIXEL_COLORS];
  unsigned color;
  unsigned sub_y;
  unsigned x;

  memset(seen, 0, sizeof(seen));
  *band_color_count_ = 0;
  *event_count_ = 0;

  for(color = 0; color < HARNESS_TERMINAL_SIXEL_COLORS; color++)
    heads_[color] = tails[color] = UINT_MAX;

  for(sub_y = 0; sub_y < band_rows_; sub_y++)
    rows[sub_y] = indices_ +
      ((size_t)(y_ + sub_y) * (size_t)width_);

  /* Keep the dense encoder's row-major first-use order so color planes and
     the resulting DCS byte stream remain stable. */
  for(sub_y = 0; sub_y < band_rows_; sub_y++)
    for(x = 0; x < width_; x++)
      {
        uint8_t slot = rows[sub_y][x];

        if(slot >= palette_count_)
          return false;
        if(!seen[slot])
          {
            seen[slot] = true;
            band_colors_[(*band_color_count_)++] = slot;
          }
      }

  /* Visit columns monotonically so each color's sparse events are already
     ordered and its final event retains the full six-row band extent. */
  for(x = 0; x < width_; x++)
    {
      uint8_t column_colors[6];
      uint8_t column_masks[6];
      unsigned column_count = 0;

      for(sub_y = 0; sub_y < band_rows_; sub_y++)
        {
          uint8_t slot = rows[sub_y][x];
          uint8_t bit = (uint8_t)(1U << sub_y);
          unsigned column;

          if(slot >= palette_count_)
            return false;

          for(column = 0; column < column_count; column++)
            if(column_colors[column] == slot)
              break;

          if(column < column_count)
            column_masks[column] |= bit;
          else
            {
              column_colors[column_count] = slot;
              column_masks[column_count] = bit;
              column_count++;
            }
        }

      for(color = 0; color < column_count; color++)
        {
          terminal_sixel_event_t *event;
          unsigned slot = column_colors[color];

          if((size_t)*event_count_ >=
             g_run.terminal_sixel_event_capacity)
            return false;

          event = &g_run.terminal_sixel_events[*event_count_];
          event->x = x;
          event->next = UINT_MAX;
          event->mask = column_masks[color];

          if(heads_[slot] == UINT_MAX)
            heads_[slot] = *event_count_;
          else
            {
              if((tails[slot] == UINT_MAX) ||
                 (tails[slot] >= *event_count_))
                return false;
              g_run.terminal_sixel_events[tails[slot]].next = *event_count_;
            }

          tails[slot] = *event_count_;
          (*event_count_)++;
        }
    }

  return true;
}


static
bool
_terminal_write_sixel_event_plane(unsigned head_,
                                  unsigned event_count_,
                                  unsigned width_)
{
  char pending_ch = 0;
  unsigned pending_count = 0;
  unsigned position = 0;
  unsigned event_index = head_;

  if(head_ == UINT_MAX)
    return false;

  while(event_index != UINT_MAX)
    {
      const terminal_sixel_event_t *event;
      unsigned next;

      if(event_index >= event_count_)
        return false;
      event = &g_run.terminal_sixel_events[event_index];
      if((event->x < position) || (event->x >= width_) ||
         (event->mask == 0U) || (event->mask > 63U))
        return false;

      if(!_terminal_sixel_accumulate_run('?',
                                         event->x - position,
                                         &pending_ch,
                                         &pending_count) ||
         !_terminal_sixel_accumulate_run((char)(63U + event->mask),
                                         1U,
                                         &pending_ch,
                                         &pending_count))
        return false;

      position = event->x + 1U;
      next = event->next;
      if((next != UINT_MAX) &&
         ((next <= event_index) || (next >= event_count_)))
        return false;
      event_index = next;
    }

  _terminal_write_sixel_run_char(pending_ch, pending_count);
  return true;
}


static
bool
_terminal_write_sixel_dense_bands(const uint8_t *indices_,
                                  unsigned       width_,
                                  unsigned       height_)
{
  uint8_t *masks = g_run.terminal_sixel_masks;
  unsigned color;
  unsigned y;

  for(y = 0; y < height_; y += 6U)
    {
      bool band_zeroed[HARNESS_TERMINAL_SIXEL_COLORS];
      unsigned band_colors[HARNESS_TERMINAL_SIXEL_COLORS];
      unsigned band_last[HARNESS_TERMINAL_SIXEL_COLORS];
      unsigned band_color_count = 0;
      bool first_color = true;
      unsigned sub_y;

      memset(band_zeroed, 0, sizeof(band_zeroed));

      for(sub_y = 0; (sub_y < 6U) && ((y + sub_y) < height_); sub_y++)
        {
          const uint8_t *row = indices_ +
            ((size_t)(y + sub_y) * (size_t)width_);
          uint8_t bit = (uint8_t)(1U << sub_y);
          unsigned x;

          for(x = 0; x < width_; x++)
            {
              uint8_t slot = row[x];
              uint8_t *color_masks;

              color_masks = masks + ((size_t)slot * (size_t)width_);

              if(!band_zeroed[slot])
                {
                  memset(color_masks, 0, width_);
                  band_zeroed[slot] = true;
                  band_last[slot] = 0;
                  band_colors[band_color_count++] = slot;
                }

              color_masks[x] |= bit;
              if(band_last[slot] < (x + 1U))
                band_last[slot] = x + 1U;
            }
        }

      for(color = 0; color < band_color_count; color++)
        {
          unsigned slot = band_colors[color];
          const uint8_t *color_masks =
            masks + ((size_t)slot * (size_t)width_);
          unsigned last = band_last[slot];
          unsigned x;

          if(!first_color)
            _terminal_write_all("$", 1U);
          first_color = false;
          _terminal_write_sixel_color_select(slot);

          for(x = 0; x < last; )
            {
              char ch = (char)(63U + color_masks[x]);
              unsigned start = x;

              do
                x++;
              while((x < last) && (color_masks[x] == color_masks[start]));
              _terminal_write_sixel_run_char(ch, x - start);
            }
        }

      if((y + 6U) < height_)
        _terminal_write_all("-", 1U);
    }

  return true;
}


static
bool
_terminal_write_sixel_indices(const uint8_t *indices_,
                              unsigned       width_,
                              unsigned       height_,
                              const terminal_sixel_palette_t *palette_)
{
  uint64_t event_capacity_u64;
  uint64_t mask_size_u64;
  size_t event_capacity;
  size_t mask_size;
  unsigned max_band_rows;
  unsigned color;
  unsigned y;
  bool use_sparse;
  char raster[64];
  int n;

  if((indices_ == NULL) || (palette_ == NULL) || (palette_->count == 0U) ||
     (palette_->count > HARNESS_TERMINAL_SIXEL_COLORS) ||
     (width_ == 0U) || (height_ == 0U))
    return false;

  if(!_terminal_u64_mul_checked((uint64_t)width_,
                                (uint64_t)palette_->count,
                                &mask_size_u64) ||
     (mask_size_u64 > (uint64_t)SIZE_MAX))
    return false;

  /* Dense planes remain faster for PO'ed's <=92-color frames. The sparse
     crossover is consistent for full 256-color frames at tested widths. */
  use_sparse = (width_ >= HARNESS_TERMINAL_SIXEL_SPARSE_MIN_WIDTH) &&
    (palette_->count >= HARNESS_TERMINAL_SIXEL_SPARSE_MIN_COLORS);
  if(use_sparse)
    {
      max_band_rows = (height_ < 6U) ? height_ : 6U;
      if(!_terminal_u64_mul_checked((uint64_t)width_,
                                    (uint64_t)max_band_rows,
                                    &event_capacity_u64) ||
         (event_capacity_u64 > (uint64_t)UINT_MAX) ||
         (event_capacity_u64 >
          ((uint64_t)SIZE_MAX / sizeof(terminal_sixel_event_t))))
        return false;

      event_capacity = (size_t)event_capacity_u64;
      if(!_terminal_sixel_ensure_event_capacity(event_capacity))
        return false;
    }
  else
    {
      mask_size = (size_t)mask_size_u64;
      if(!_terminal_ensure_u8_buffer(&g_run.terminal_sixel_masks,
                                     &g_run.terminal_sixel_masks_size,
                                     mask_size))
        return false;
    }

  _terminal_write_all("\x1bP0;0;0q", 8U);
  n = snprintf(raster, sizeof(raster), "\"1;1;%u;%u", width_, height_);
  if((n <= 0) || ((size_t)n >= sizeof(raster)))
    {
      _terminal_write_all("\x1b\\", 2U);
      return false;
    }
  _terminal_write_all(raster, (size_t)n);

  for(color = 0; color < palette_->count; color++)
    _terminal_write_sixel_color_define(color, palette_->xterm_index[color]);

  if(!use_sparse)
    {
      if(!_terminal_write_sixel_dense_bands(indices_,
                                             width_,
                                             height_))
        goto sixel_fail;
      _terminal_write_all("\x1b\\", 2U);
      return true;
    }

  for(y = 0; y < height_; )
    {
      unsigned band_colors[HARNESS_TERMINAL_SIXEL_COLORS];
      unsigned heads[HARNESS_TERMINAL_SIXEL_COLORS];
      unsigned band_color_count;
      unsigned event_count;
      unsigned band_rows = height_ - y;
      bool first_color = true;

      if(band_rows > 6U)
        band_rows = 6U;

      if(!_terminal_sixel_build_band_events(indices_,
                                             width_,
                                             y,
                                             band_rows,
                                             palette_->count,
                                             band_colors,
                                             &band_color_count,
                                             heads,
                                             &event_count))
        goto sixel_fail;

      for(color = 0; color < band_color_count; color++)
        {
          unsigned slot = band_colors[color];

          if(!first_color)
            _terminal_write_all("$", 1U);
          first_color = false;

          _terminal_write_sixel_color_select(slot);

          if(!_terminal_write_sixel_event_plane(heads[slot],
                                                 event_count,
                                                 width_))
            goto sixel_fail;
        }

      y += band_rows;
      if(y < height_)
        _terminal_write_all("-", 1U);
    }

  _terminal_write_all("\x1b\\", 2U);
  return true;

sixel_fail:
  _terminal_write_all("\x1b\\", 2U);
  return false;
}


static
void
_terminal_clear_sixel_image(void);


static
int
_terminal_render_sixel_image(const void             *data_,
                             unsigned                width_,
                             unsigned                height_,
                             size_t                  pitch_,
                             enum retro_pixel_format fmt_,
                             unsigned                row_,
                             unsigned                col_,
                             unsigned                cols_,
                             unsigned                rows_)
{
  bool used_xterm[HARNESS_TERMINAL_SIXEL_COLORS];
  terminal_sixel_palette_t palette;
  uint8_t *indices;
  size_t index_count;
  unsigned image_width;
  unsigned image_height;
  double start;
  bool ok;

  if(!_terminal_sixel_target_size(width_, height_, cols_, rows_,
                                   &image_width, &image_height))
    return HARNESS_TERMINAL_IMAGE_FAILED;

  if(_terminal_image_cache_matches(data_,
                                   pitch_,
                                   HARNESS_TERMINAL_RENDER_SIXEL,
                                   width_,
                                   height_,
                                   fmt_,
                                   row_,
                                   col_,
                                   cols_,
                                   rows_,
                                   image_width,
                                   image_height))
    return HARNESS_TERMINAL_IMAGE_SKIPPED;

  if((size_t)image_height > (SIZE_MAX / (size_t)image_width))
    return HARNESS_TERMINAL_IMAGE_FAILED;
  index_count = (size_t)image_width * (size_t)image_height;

  start = _monotonic_seconds();
  if(!_terminal_ensure_u8_buffer(&g_run.terminal_sixel_indices,
                                 &g_run.terminal_sixel_indices_size,
                                 index_count))
    {
      _terminal_record_image_seconds(_monotonic_seconds() - start);
      return HARNESS_TERMINAL_IMAGE_FAILED;
    }

  indices = g_run.terminal_sixel_indices;
  if(!_terminal_sixel_quantize_frame(data_,
                                     width_,
                                     height_,
                                     pitch_,
                                     fmt_,
                                     image_width,
                                     image_height,
                                     indices,
                                     index_count,
                                     used_xterm))
    {
      _terminal_record_image_seconds(_monotonic_seconds() - start);
      return HARNESS_TERMINAL_IMAGE_FAILED;
    }

  _terminal_sixel_compact_palette(indices,
                                  index_count,
                                  used_xterm,
                                  &palette);
  _terminal_move_cursor(row_, col_);
  ok = _terminal_write_sixel_indices(indices, image_width, image_height,
                                     &palette);
  if(ok)
    _terminal_store_image_cache(data_,
                                pitch_,
                                HARNESS_TERMINAL_RENDER_SIXEL,
                                width_,
                                height_,
                                fmt_,
                                row_,
                                col_,
                                cols_,
                                rows_,
                                image_width,
                                image_height);
  _terminal_record_image_seconds(_monotonic_seconds() - start);
  return ok ? HARNESS_TERMINAL_IMAGE_DRAWN : HARNESS_TERMINAL_IMAGE_FAILED;
}


static
void
_terminal_delete_kitty_image(void)
{
  char seq[96];
  int n;

  n = snprintf(seq,
               sizeof(seq),
               "\x1b_Ga=d,d=I,i=%u\x1b\\",
               HARNESS_TERMINAL_KITTY_IMAGE_ID);
  if((n > 0) && ((size_t)n < sizeof(seq)))
    _terminal_write_all(seq, (size_t)n);
}


static
void
_terminal_clear_sixel_image(void)
{
  /*
   * Sixel has no portable image-id delete operation like Kitty graphics.
   * Clear the alternate-screen buffer before geometry-driven redraws or
   * restoring so terminals that keep sixel graphics in their display layer
   * do not leave stale frame contents behind.
   */
  _terminal_write_all("\x1b[0m\x1b[H\x1b[2J",
                      sizeof("\x1b[0m\x1b[H\x1b[2J") - 1);
}


static
void
_terminal_clear_restored_sixel_image(void)
{
  /*
   * Some terminals restore or retain Sixel graphics after leaving the
   * alternate screen. Clear once on the restored normal screen too so a
   * clean exit does not leave the last frame visible.
   */
  _terminal_write_all("\x1b[0m\x1b[H\x1b[2J\x1b[3J",
                      sizeof("\x1b[0m\x1b[H\x1b[2J\x1b[3J") - 1);
}


static
void
_terminal_update_size(void)
{
  struct winsize ws;

  if(g_signal_resize_requested)
    {
      g_signal_resize_requested = 0;
      g_run.terminal_resize_pending = true;
    }

  if(!g_run.terminal_resize_pending &&
     (g_run.terminal_width > 0) && (g_run.terminal_height > 0))
    return;

  g_run.terminal_resize_pending = false;

  if(ioctl(g_run.tty_fd, TIOCGWINSZ, &ws) != 0)
    {
      g_run.terminal_width = 80;
      g_run.terminal_height = 24;
      g_run.terminal_pixel_width = 0;
      g_run.terminal_pixel_height = 0;
      return;
    }

  if(ws.ws_col > 0)
    g_run.terminal_width = ws.ws_col;
  if(ws.ws_row > 0)
    g_run.terminal_height = ws.ws_row;
  g_run.terminal_pixel_width = (ws.ws_xpixel > 0) ? ws.ws_xpixel : 0;
  g_run.terminal_pixel_height = (ws.ws_ypixel > 0) ? ws.ws_ypixel : 0;
}


static
void
_terminal_free_cell_cache(void)
{
  free(g_run.terminal_cells);
  free(g_run.terminal_row_cells);
  free(g_run.terminal_row_hashes);

  g_run.terminal_cells = NULL;
  g_run.terminal_row_cells = NULL;
  g_run.terminal_row_hashes = NULL;
  g_run.terminal_cache_cols = 0;
  g_run.terminal_cache_rows = 0;
}


static
void
_terminal_free_coord_cache(void)
{
  free(g_run.terminal_src_x);
  free(g_run.terminal_src_y_top);
  free(g_run.terminal_src_y_bottom);

  g_run.terminal_src_x = NULL;
  g_run.terminal_src_y_top = NULL;
  g_run.terminal_src_y_bottom = NULL;
  g_run.terminal_map_width = 0;
  g_run.terminal_map_height = 0;
  g_run.terminal_map_cols = 0;
  g_run.terminal_map_rows = 0;
  g_run.terminal_map_render_mode = 0;
}


static
void
_terminal_free_image_buffers(void)
{
  if(g_run.terminal_kitty_zstream_initialized)
    {
      (void)deflateEnd(&g_run.terminal_kitty_zstream);
      memset(&g_run.terminal_kitty_zstream, 0,
             sizeof(g_run.terminal_kitty_zstream));
      g_run.terminal_kitty_zstream_initialized = false;
    }

  free(g_run.terminal_rgb_buffer);
  free(g_run.terminal_scaled_rgb_buffer);
  free(g_run.terminal_sixel_src_x);
  free(g_run.terminal_sixel_src_y);
  free(g_run.terminal_sixel_source_indices);
  free(g_run.terminal_sixel_indices);
  free(g_run.terminal_sixel_masks);
  free(g_run.terminal_sixel_events);
  free(g_run.terminal_image_cache.pixels);
  _byte_buffer_free(&g_run.terminal_kitty_zlib_buffer);
  _byte_buffer_free(&g_run.terminal_png_buffer);

  g_run.terminal_rgb_buffer = NULL;
  g_run.terminal_rgb_buffer_size = 0;
  g_run.terminal_scaled_rgb_buffer = NULL;
  g_run.terminal_scaled_rgb_buffer_size = 0;
  g_run.terminal_sixel_src_x = NULL;
  g_run.terminal_sixel_src_y = NULL;
  g_run.terminal_sixel_map_src_width = 0;
  g_run.terminal_sixel_map_src_height = 0;
  g_run.terminal_sixel_map_out_width = 0;
  g_run.terminal_sixel_map_out_height = 0;
  g_run.terminal_sixel_source_indices = NULL;
  g_run.terminal_sixel_source_indices_size = 0;
  g_run.terminal_sixel_indices = NULL;
  g_run.terminal_sixel_indices_size = 0;
  g_run.terminal_sixel_masks = NULL;
  g_run.terminal_sixel_masks_size = 0;
  g_run.terminal_sixel_events = NULL;
  g_run.terminal_sixel_event_capacity = 0;
  memset(&g_run.terminal_image_cache, 0,
         sizeof(g_run.terminal_image_cache));
}


static
bool
_terminal_ensure_coord_cache(unsigned width_,
                             unsigned height_,
                             unsigned target_cols_,
                             unsigned target_rows_,
                             unsigned render_mode_)
{
  unsigned *src_x;
  unsigned *src_y_top;
  unsigned *src_y_bottom;
  unsigned x;
  unsigned y;
  unsigned sub_rows;

  if((width_ == 0U) || (height_ == 0U) ||
     (target_cols_ == 0U) || (target_rows_ == 0U))
    return false;

  if((g_run.terminal_src_x != NULL) &&
     (g_run.terminal_src_y_top != NULL) &&
     (g_run.terminal_src_y_bottom != NULL) &&
     (g_run.terminal_map_width == width_) &&
     (g_run.terminal_map_height == height_) &&
     (g_run.terminal_map_cols == target_cols_) &&
     (g_run.terminal_map_rows == target_rows_) &&
     (g_run.terminal_map_render_mode == render_mode_))
    return true;

  if(target_rows_ > (UINT_MAX / 2U))
    return false;

  src_x = malloc((size_t)target_cols_ * sizeof(*src_x));
  src_y_top = malloc((size_t)target_rows_ * sizeof(*src_y_top));
  src_y_bottom = malloc((size_t)target_rows_ * sizeof(*src_y_bottom));
  if((src_x == NULL) || (src_y_top == NULL) || (src_y_bottom == NULL))
    {
      free(src_x);
      free(src_y_top);
      free(src_y_bottom);
      return false;
    }

  for(x = 0; x < target_cols_; x++)
    {
      uint64_t sample = (((uint64_t)x * (uint64_t)width_) +
                         ((uint64_t)width_ / 2ULL)) /
        (uint64_t)target_cols_;
      src_x[x] = (sample >= (uint64_t)width_) ? (width_ - 1U) : (unsigned)sample;
    }

  sub_rows = (render_mode_ == HARNESS_TERMINAL_RENDER_HALF) ?
    (target_rows_ * 2U) : target_rows_;
  for(y = 0; y < target_rows_; y++)
    {
      unsigned sub_y = (render_mode_ == HARNESS_TERMINAL_RENDER_HALF) ?
        (y * 2U) : y;
      uint64_t top = (((uint64_t)sub_y * (uint64_t)height_) +
                      ((uint64_t)height_ / 2ULL)) / (uint64_t)sub_rows;
      uint64_t bottom = (render_mode_ == HARNESS_TERMINAL_RENDER_HALF) ?
        ((((uint64_t)(sub_y + 1U) * (uint64_t)height_) +
          ((uint64_t)height_ / 2ULL)) / (uint64_t)sub_rows) : top;

      src_y_top[y] = (top >= (uint64_t)height_) ? (height_ - 1U) : (unsigned)top;
      src_y_bottom[y] = (bottom >= (uint64_t)height_) ? (height_ - 1U) : (unsigned)bottom;
    }

  _terminal_free_coord_cache();
  g_run.terminal_src_x = src_x;
  g_run.terminal_src_y_top = src_y_top;
  g_run.terminal_src_y_bottom = src_y_bottom;
  g_run.terminal_map_width = width_;
  g_run.terminal_map_height = height_;
  g_run.terminal_map_cols = target_cols_;
  g_run.terminal_map_rows = target_rows_;
  g_run.terminal_map_render_mode = render_mode_;

  return true;
}


static
bool
_terminal_ensure_cell_cache(unsigned rows_,
                            unsigned cols_)
{
  terminal_cell_t *cells;
  terminal_cell_t *row_cells;
  uint64_t *row_hashes;
  size_t count;
  unsigned y;

  if((rows_ == 0U) || (cols_ == 0U))
    return false;

  if((g_run.terminal_cells != NULL) &&
     (g_run.terminal_row_cells != NULL) &&
     (g_run.terminal_cache_rows == rows_) &&
     (g_run.terminal_cache_cols == cols_))
    return true;

  if((size_t)rows_ > (SIZE_MAX / (size_t)cols_))
    return false;

  count = (size_t)rows_ * (size_t)cols_;
  if(count > (SIZE_MAX / sizeof(*cells)))
    return false;

  cells = malloc(count * sizeof(*cells));
  row_cells = malloc((size_t)cols_ * sizeof(*row_cells));
  row_hashes = malloc((size_t)rows_ * sizeof(*row_hashes));
  if((cells == NULL) || (row_cells == NULL) || (row_hashes == NULL))
    {
      free(cells);
      free(row_cells);
      free(row_hashes);
      return false;
    }

  memset(cells, 0, count * sizeof(*cells));
  for(y = 0; y < rows_; y++)
    row_hashes[y] = UINT64_MAX;
  _terminal_free_cell_cache();

  g_run.terminal_cells = cells;
  g_run.terminal_row_cells = row_cells;
  g_run.terminal_row_hashes = row_hashes;
  g_run.terminal_cache_rows = rows_;
  g_run.terminal_cache_cols = cols_;

  return true;
}


static
void
_terminal_fill_blank_cells(terminal_cell_t *cells_,
                           unsigned         count_)
{
  memset(cells_, 0, (size_t)count_ * sizeof(cells_[0]));
}


static
uint64_t
_terminal_hash_cells(const terminal_cell_t *cells_,
                     unsigned               count_)
{
  const uint8_t *p = (const uint8_t *)cells_;
  size_t len = (size_t)count_ * sizeof(*cells_);
  uint64_t hash = 1469598103934665603ULL;
  size_t i;

  for(i = 0; i < len; i++)
    {
      hash ^= p[i];
      hash *= 1099511628211ULL;
    }

  return hash;
}


static
void
_terminal_emit_cells(unsigned               row_,
                     unsigned               col_,
                     const terminal_cell_t *cells_,
                     unsigned               cols_)
{
  static const char half_upper[] = "\xe2\x96\x80";
  unsigned x = 0;
  bool have_fg = false;
  bool have_bg = false;
  uint8_t prev_fg_r = 0;
  uint8_t prev_fg_g = 0;
  uint8_t prev_fg_b = 0;
  uint8_t prev_bg_r = 0;
  uint8_t prev_bg_g = 0;
  uint8_t prev_bg_b = 0;

  _terminal_move_cursor(row_, col_);
  _terminal_write_all("\x1b[0m", sizeof("\x1b[0m") - 1);

  while(x < cols_)
    {
      const terminal_cell_t *cell = &cells_[x];

      if(cell->glyph == HARNESS_TERMINAL_CELL_BLANK)
        {
          unsigned start = x;

          do
            x++;
          while((x < cols_) &&
                (cells_[x].glyph == HARNESS_TERMINAL_CELL_BLANK));

          if(have_fg || have_bg)
            {
              _terminal_write_all("\x1b[0m", sizeof("\x1b[0m") - 1);
              have_fg = false;
              have_bg = false;
            }
          _terminal_write_spaces((size_t)(x - start));
          continue;
        }

      bool need_bg = !have_bg || (cell->bg_r != prev_bg_r) ||
        (cell->bg_g != prev_bg_g) || (cell->bg_b != prev_bg_b);

      if(cell->glyph == HARNESS_TERMINAL_CELL_BG_SPACE)
        {
          unsigned start = x;

          if(need_bg)
            {
              _terminal_write_sgr_rgb(false, 0, 0, 0,
                                      true, cell->bg_r, cell->bg_g, cell->bg_b);
              prev_bg_r = cell->bg_r;
              prev_bg_g = cell->bg_g;
              prev_bg_b = cell->bg_b;
              have_bg = true;
            }

          do
            x++;
          while((x < cols_) &&
                (cells_[x].glyph == HARNESS_TERMINAL_CELL_BG_SPACE) &&
                (cells_[x].bg_r == prev_bg_r) &&
                (cells_[x].bg_g == prev_bg_g) &&
                (cells_[x].bg_b == prev_bg_b));

          _terminal_write_spaces((size_t)(x - start));
          continue;
        }

      if(cell->glyph == HARNESS_TERMINAL_CELL_ASCII)
        {
          unsigned start = x;
          char ch = (char)((cell->ch == 0U) ? ' ' : cell->ch);
          bool need_fg = !have_fg || (cell->fg_r != prev_fg_r) ||
            (cell->fg_g != prev_fg_g) || (cell->fg_b != prev_fg_b);

          if(need_fg)
            {
              _terminal_write_sgr_rgb(true, cell->fg_r, cell->fg_g, cell->fg_b,
                                      false, 0, 0, 0);
              prev_fg_r = cell->fg_r;
              prev_fg_g = cell->fg_g;
              prev_fg_b = cell->fg_b;
              have_fg = true;
            }

          do
            x++;
          while((x < cols_) &&
                (cells_[x].glyph == HARNESS_TERMINAL_CELL_ASCII) &&
                (cells_[x].ch == (uint8_t)ch) &&
                (cells_[x].fg_r == prev_fg_r) &&
                (cells_[x].fg_g == prev_fg_g) &&
                (cells_[x].fg_b == prev_fg_b));

          _terminal_write_repeat_char(ch, x - start);
          continue;
        }

      bool need_fg = !have_fg || (cell->fg_r != prev_fg_r) ||
        (cell->fg_g != prev_fg_g) || (cell->fg_b != prev_fg_b);

      if(need_fg || need_bg)
        {
          _terminal_write_sgr_rgb(need_fg, cell->fg_r, cell->fg_g, cell->fg_b,
                                  need_bg, cell->bg_r, cell->bg_g, cell->bg_b);
          if(need_fg)
            {
              prev_fg_r = cell->fg_r;
              prev_fg_g = cell->fg_g;
              prev_fg_b = cell->fg_b;
              have_fg = true;
            }
          if(need_bg)
            {
              prev_bg_r = cell->bg_r;
              prev_bg_g = cell->bg_g;
              prev_bg_b = cell->bg_b;
              have_bg = true;
            }
        }

      {
        unsigned start = x;

        do
          x++;
        while((x < cols_) &&
              (cells_[x].glyph == HARNESS_TERMINAL_CELL_HALF_UPPER) &&
              (cells_[x].fg_r == prev_fg_r) &&
              (cells_[x].fg_g == prev_fg_g) &&
              (cells_[x].fg_b == prev_fg_b) &&
              (cells_[x].bg_r == prev_bg_r) &&
              (cells_[x].bg_g == prev_bg_g) &&
              (cells_[x].bg_b == prev_bg_b));

        _terminal_write_repeat_utf8(half_upper, sizeof(half_upper) - 1,
                                    x - start);
      }
    }

  _terminal_write_all("\x1b[0m", sizeof("\x1b[0m") - 1);
}


static
bool
_terminal_button_from_key(int      key_,
                          unsigned *id_)
{
  switch(key_)
    {
    case 'w':
    case 'W':
      *id_ = RETRO_DEVICE_ID_JOYPAD_UP;
      return true;
    case 'a':
    case 'A':
      *id_ = RETRO_DEVICE_ID_JOYPAD_LEFT;
      return true;
    case 's':
    case 'S':
      *id_ = RETRO_DEVICE_ID_JOYPAD_DOWN;
      return true;
    case 'd':
    case 'D':
      *id_ = RETRO_DEVICE_ID_JOYPAD_RIGHT;
      return true;
    case 'j':
    case 'J':
      *id_ = RETRO_DEVICE_ID_JOYPAD_Y;
      return true;
    case 'k':
    case 'K':
      *id_ = RETRO_DEVICE_ID_JOYPAD_B;
      return true;
    case 'l':
    case 'L':
      *id_ = RETRO_DEVICE_ID_JOYPAD_A;
      return true;
    case 'u':
    case 'U':
      *id_ = RETRO_DEVICE_ID_JOYPAD_L;
      return true;
    case 'i':
    case 'I':
      *id_ = RETRO_DEVICE_ID_JOYPAD_R;
      return true;
    case '\'':
      *id_ = RETRO_DEVICE_ID_JOYPAD_SELECT;
      return true;
    default:
      return false;
    }
}


static
void
_terminal_press_button(unsigned button_)
{
  if(button_ >= HARNESS_TERMINAL_BUTTONS)
    return;

  g_run.terminal_button_mask_next |= (uint16_t)(1U << button_);
}


static
void
_terminal_update_button_mask(uint64_t frame_)
{
  unsigned i;

  for(i = 0; i < HARNESS_TERMINAL_BUTTONS; i++)
    {
      if((g_run.terminal_button_mask_next & (1U << i)) != 0)
        {
          g_run.terminal_button_expire[i] =
            (UINT64_MAX - frame_ <= (uint64_t)g_cfg.terminal_button_hold_frames) ?
            UINT64_MAX :
            (frame_ + (uint64_t)(g_cfg.terminal_button_hold_frames == 0 ? 1 :
                                 g_cfg.terminal_button_hold_frames));
        }
    }

  g_run.terminal_button_mask_next = 0;

  g_run.terminal_button_mask = 0;
  for(i = 0; i < HARNESS_TERMINAL_BUTTONS; i++)
    {
      uint64_t expire = g_run.terminal_button_expire[i];

      if((expire != 0) && (frame_ < expire))
        g_run.terminal_button_mask |= (uint16_t)(1U << i);
      else
        g_run.terminal_button_expire[i] = 0;
    }
}


static
void
_terminal_parse_input_byte(uint8_t byte_)
{
  int key = (int)byte_;
  bool button_ok;
  unsigned button;

  if(g_run.terminal_escape == 1)
    {
      if(key == '[')
        {
          g_run.terminal_escape = 2;
          return;
        }
      else if(key == 'O')
        {
          g_run.terminal_escape = 3;
          return;
        }
      else if(key == 27)
        {
          g_run.terminal_quit_requested = true;
          g_run.terminal_escape = 0;
          return;
        }

      g_run.terminal_escape = 0;
    }

  if(g_run.terminal_escape == 2)
    {
      if(((key >= '0') && (key <= '9')) || (key == ';') || (key == '?'))
        return;

      if(key == 'A')
        _terminal_press_button(RETRO_DEVICE_ID_JOYPAD_UP);
      else if(key == 'B')
        _terminal_press_button(RETRO_DEVICE_ID_JOYPAD_DOWN);
      else if(key == 'D')
        _terminal_press_button(RETRO_DEVICE_ID_JOYPAD_LEFT);
      else if(key == 'C')
        _terminal_press_button(RETRO_DEVICE_ID_JOYPAD_RIGHT);

      g_run.terminal_escape = 0;
      return;
    }

  if(g_run.terminal_escape == 3)
    {
      if(key == 'A')
        _terminal_press_button(RETRO_DEVICE_ID_JOYPAD_UP);
      else if(key == 'B')
        _terminal_press_button(RETRO_DEVICE_ID_JOYPAD_DOWN);
      else if(key == 'D')
        _terminal_press_button(RETRO_DEVICE_ID_JOYPAD_LEFT);
      else if(key == 'C')
        _terminal_press_button(RETRO_DEVICE_ID_JOYPAD_RIGHT);

      g_run.terminal_escape = 0;
      return;
    }

  if(key == 27)
    {
      g_run.terminal_escape = 1;
      return;
    }

  if((key == 10) || (key == 13))
    {
      _terminal_press_button(RETRO_DEVICE_ID_JOYPAD_START);
      return;
    }

  if(key == 3)
    {
      g_run.terminal_quit_requested = true;
      return;
    }

  button_ok = _terminal_button_from_key(key, &button);
  if(button_ok)
    _terminal_press_button(button);
}


static
void
_terminal_poll_input(void)
{
  uint8_t buffer[128];
  ssize_t n;

  if(!g_run.terminal_active || (g_run.tty_fd < 0))
    return;

  while((n = read(g_run.tty_fd, buffer, sizeof(buffer))) > 0)
    {
      size_t i;

      for(i = 0; i < (size_t)n; i++)
        _terminal_parse_input_byte(buffer[i]);
    }

  if((n < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINTR))
    _harness_log(RETRO_LOG_WARN,
                 "[Harness]: terminal input read failed: %s\n", strerror(errno));

  _terminal_update_button_mask(g_run.current_frame);
}


static
int
_terminal_open(void)
{
  struct termios raw;

  g_run.tty_fd = open("/dev/tty", O_RDWR | O_NOCTTY);
  if(g_run.tty_fd < 0)
    {
      _harness_log(RETRO_LOG_ERROR,
                   "[Harness]: unable to open terminal: %s\n",
                   strerror(errno));
      g_run.tty_fd = -1;
      return -1;
    }

  memset(g_run.terminal_button_expire, 0, sizeof(g_run.terminal_button_expire));
  g_run.terminal_button_mask = 0;
  g_run.terminal_button_mask_next = 0;
  g_run.terminal_resize_pending = true;
  g_run.terminal_escape = 0;
  g_run.terminal_quit_requested = false;
  g_run.terminal_last_render_frame = 0;
  g_run.terminal_adaptive_fps = g_cfg.terminal_fps;
  g_run.terminal_render_seconds_ema = 0.0;

  g_run.tty_flags = fcntl(g_run.tty_fd, F_GETFL, 0);
  if(g_run.tty_flags < 0)
    {
      g_run.tty_flags = -1;
    }

  if(tcgetattr(g_run.tty_fd, &g_run.tty_orig_termios) != 0)
    {
      close(g_run.tty_fd);
      g_run.tty_fd = -1;
      return -1;
    }

  raw = g_run.tty_orig_termios;
  raw.c_iflag &= ~(IXON | ICRNL | INLCR | IGNCR | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~OPOST;
  raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;

  if(tcsetattr(g_run.tty_fd, TCSAFLUSH, &raw) != 0)
    {
      close(g_run.tty_fd);
      g_run.tty_fd = -1;
      return -1;
    }

  if(g_run.tty_flags >= 0)
    (void)fcntl(g_run.tty_fd, F_SETFL, g_run.tty_flags | O_NONBLOCK);

  /* enter the alternate screen before capability probing so any
     keystrokes typed during the probe window land in the (hidden,
     cleared) alternate buffer rather than the user's shell */
  _terminal_write_all("\x1b[?1049h\x1b[?25l",
                      sizeof("\x1b[?1049h\x1b[?25l") - 1);

  _terminal_update_size();
  _terminal_detect_capabilities();
  g_run.terminal_active = true;

  _terminal_write_all("\x1b[2J", sizeof("\x1b[2J") - 1);

  return 0;
}


static
void
_terminal_close(void)
{
  const bool sixel = (g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_SIXEL);

  if(g_run.tty_fd < 0)
    return;

  g_terminal_force_output = true;
  _terminal_output_flush();
  if(g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_KITTY)
    _terminal_delete_kitty_image();
  else if(sixel)
    _terminal_clear_sixel_image();
  _terminal_write_all("\x1b[0m\x1b[?25h\x1b[?1049l",
                      sizeof("\x1b[0m\x1b[?25h\x1b[?1049l") - 1);
  if(sixel)
    _terminal_clear_restored_sixel_image();
  g_terminal_force_output = false;

  if((g_run.tty_flags >= 0))
    (void)fcntl(g_run.tty_fd, F_SETFL, g_run.tty_flags);

  (void)tcsetattr(g_run.tty_fd, TCSAFLUSH, &g_run.tty_orig_termios);

  close(g_run.tty_fd);
  g_run.tty_fd = -1;
  g_run.terminal_active = false;
  _terminal_free_cell_cache();
  _terminal_free_coord_cache();
  _terminal_free_image_buffers();
  _terminal_free_output_buffer();
}


static
bool
_terminal_should_render_status_line(int image_result_)
{
  double step_d;

  if(image_result_ != HARNESS_TERMINAL_IMAGE_SKIPPED)
    return true;
  if(!g_run.terminal_status_valid)
    return true;
  if((g_run.terminal_status_last_button_mask != g_run.terminal_button_mask) ||
     (g_run.terminal_status_last_quit_requested !=
      g_run.terminal_quit_requested))
    return true;

  if(g_run.core_fps <= 0.0)
    return false;

  if(g_run.terminal_status_frame_interval == 0)
    {
      step_d = ceil(g_run.core_fps);
      if(!isfinite(step_d) || (step_d <= 0.0))
        return false;
      g_run.terminal_status_frame_interval =
        (step_d >= (double)UINT64_MAX) ? UINT64_MAX : (uint64_t)step_d;
    }

  return (g_run.current_frame - g_run.terminal_status_last_frame) >=
    g_run.terminal_status_frame_interval;
}


static
void
_terminal_note_status_line_rendered(void)
{
  g_run.terminal_status_valid = true;
  g_run.terminal_status_last_frame = g_run.current_frame;
  g_run.terminal_status_last_button_mask = g_run.terminal_button_mask;
  g_run.terminal_status_last_quit_requested =
    g_run.terminal_quit_requested;
}


static
void
_terminal_render_status_line(unsigned image_rows_max_,
                             unsigned display_cols_)
{
  char status[256];
  size_t status_len;
  int n;

  if(display_cols_ == 0U)
    return;

  n = snprintf(status,
               sizeof(status),
               "frame=%" PRIu64 " input_mask=0x%04x %s",
               g_run.current_frame,
               g_run.terminal_button_mask,
               g_run.terminal_quit_requested ? "(quit requested)" :
               "(WASD/Arrows=UDLR, J=A, K=B, L=C, '=X, "
               "U=LT, I=RT, Enter=Play, Esc Esc=Quit)");
  if(n <= 0)
    return;

  status_len = (size_t)n;
  if(status_len > display_cols_)
    status_len = display_cols_;

  _terminal_move_cursor(image_rows_max_ + 1U, 1U);
  _terminal_write_all("\x1b[0m", sizeof("\x1b[0m") - 1);
  _terminal_write_all(status, status_len);
  if(status_len < display_cols_)
    _terminal_write_spaces(display_cols_ - status_len);
  _terminal_note_status_line_rendered();
}


static
void
_terminal_render_frame(const void             *data_,
                       unsigned                width_,
                       unsigned                height_,
                       size_t                  pitch_,
                       enum retro_pixel_format  fmt_)
{
  unsigned y;
  static int last_display_cols;
  static int last_display_rows;
  static int last_terminal_pixel_width;
  static int last_terminal_pixel_height;
  static int last_display_render_mode = -1;
  static unsigned last_image_src_width;
  static unsigned last_image_src_height;
  static unsigned last_image_pad_left;
  static unsigned last_image_pad_top;
  static unsigned last_image_target_cols;
  static unsigned last_image_target_rows;
  unsigned display_cols;
  unsigned image_rows_max;
  unsigned display_rows;
  unsigned target_cols;
  unsigned target_rows;
  unsigned full_rows_for_full_width;
  unsigned full_cols_for_full_rows;
  unsigned pad_left;
  unsigned pad_top;
  uint64_t src_aspect_ratio_n;
  uint64_t src_aspect_ratio_d;
  uint64_t layout_value;
  terminal_cell_t *row_cells;
  size_t cell_size;
  terminal_read_pixel_fn read_pixel;

  if(!g_run.terminal_active || (data_ == NULL) ||
     (width_ == 0) || (height_ == 0))
    return;

  _terminal_update_size();
  if((g_run.terminal_width < 1) || (g_run.terminal_height < 1))
    return;

  display_cols = (unsigned)g_run.terminal_width;
  display_rows = (unsigned)g_run.terminal_height;

  if((display_cols == 0) || (display_rows == 0))
    return;

  image_rows_max = (display_rows > 1U) ? (display_rows - 1U) : 1U;

  if(!_terminal_aspect_for_layout(width_, height_, display_cols, display_rows,
                                  &src_aspect_ratio_n, &src_aspect_ratio_d))
    return;

  if(!_terminal_u64_mul_checked(src_aspect_ratio_d,
                                (uint64_t)display_cols,
                                &layout_value))
    return;
  full_rows_for_full_width = _terminal_u64_ceil_div(layout_value,
                                                    src_aspect_ratio_n);
  if(!_terminal_u64_mul_checked(src_aspect_ratio_n,
                                (uint64_t)image_rows_max,
                                &layout_value))
    return;
  full_cols_for_full_rows = _terminal_u64_ceil_div(layout_value,
                                                   src_aspect_ratio_d);

  if((full_rows_for_full_width == 0U) || (full_cols_for_full_rows == 0U))
    return;

  if(full_rows_for_full_width > image_rows_max)
    {
      target_cols = full_cols_for_full_rows;
      target_rows = image_rows_max;
    }
  else
    {
      target_cols = display_cols;
      target_rows = full_rows_for_full_width;
    }

  if(target_cols > display_cols)
    target_cols = display_cols;
  if(target_rows > image_rows_max)
    target_rows = image_rows_max;

  if((target_cols == 0U) || (target_rows == 0U))
    return;

  pad_left = (display_cols > target_cols)
    ? ((display_cols - target_cols) / 2U)
    : 0U;
  pad_top = (image_rows_max > target_rows)
    ? ((image_rows_max - target_rows) / 2U)
    : 0U;

  if((g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_KITTY) ||
     (g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_SIXEL))
    {
      int image_result;
      size_t image_output_start;
      size_t image_output_end;
      bool display_layout_changed;
      bool image_layout_changed;
      bool reset_image_output;

      _terminal_output_begin();

      display_layout_changed =
        (display_cols != (unsigned)last_display_cols) ||
        (display_rows != (unsigned)last_display_rows) ||
        (last_display_render_mode != g_run.terminal_render_mode);
      image_layout_changed =
        (width_ != last_image_src_width) ||
        (height_ != last_image_src_height) ||
        (pad_left != last_image_pad_left) ||
        (pad_top != last_image_pad_top) ||
        (target_cols != last_image_target_cols) ||
        (target_rows != last_image_target_rows) ||
        (g_run.terminal_pixel_width != last_terminal_pixel_width) ||
        (g_run.terminal_pixel_height != last_terminal_pixel_height);
      reset_image_output =
        display_layout_changed ||
        ((g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_SIXEL) &&
         image_layout_changed);

      if(reset_image_output)
        {
          if(g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_SIXEL)
            _terminal_clear_sixel_image();
          else
            _terminal_write_all("\x1b[2J\x1b[H", 7);
          _terminal_invalidate_image_cache();
          last_display_cols = (int)display_cols;
          last_display_rows = (int)display_rows;
          last_terminal_pixel_width = g_run.terminal_pixel_width;
          last_terminal_pixel_height = g_run.terminal_pixel_height;
          last_display_render_mode = g_run.terminal_render_mode;
          last_image_src_width = width_;
          last_image_src_height = height_;
          last_image_pad_left = pad_left;
          last_image_pad_top = pad_top;
          last_image_target_cols = target_cols;
          last_image_target_rows = target_rows;
        }

      image_output_start = g_run.terminal_output_len;
      if(g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_KITTY)
        image_result = _terminal_render_kitty_image(data_,
                                                    width_,
                                                    height_,
                                                    pitch_,
                                                    fmt_,
                                                    pad_top + 1U,
                                                    pad_left + 1U,
                                                    target_cols,
                                                    target_rows);
      else
        image_result = _terminal_render_sixel_image(data_,
                                                    width_,
                                                    height_,
                                                    pitch_,
                                                    fmt_,
                                                    pad_top + 1U,
                                                    pad_left + 1U,
                                                    target_cols,
                                                    target_rows);
      image_output_end = g_run.terminal_output_len;
      if(image_output_end >= image_output_start)
        {
          size_t image_bytes = image_output_end - image_output_start;

          if((uint64_t)image_bytes <=
             (UINT64_MAX - g_run.terminal_image_bytes))
            g_run.terminal_image_bytes += (uint64_t)image_bytes;
          else
            g_run.terminal_image_bytes = UINT64_MAX;
        }

      if(image_result == HARNESS_TERMINAL_IMAGE_DRAWN)
        g_run.terminal_image_drawn_frames++;
      else if(image_result == HARNESS_TERMINAL_IMAGE_SKIPPED)
        g_run.terminal_image_skipped_frames++;
      else
        g_run.terminal_image_failed_frames++;

      if((display_rows > 1U) &&
         _terminal_should_render_status_line(image_result))
        _terminal_render_status_line(image_rows_max, display_cols);

      _terminal_output_flush();
      return;
    }

  if(!_terminal_ensure_coord_cache(width_, height_, target_cols, target_rows,
                                   (unsigned)g_run.terminal_render_mode))
    return;

  if(!_terminal_ensure_cell_cache(image_rows_max, display_cols))
    return;

  if(g_run.terminal_read_pixel == NULL)
    _terminal_set_pixel_reader(fmt_);
  read_pixel = g_run.terminal_read_pixel;

  row_cells = g_run.terminal_row_cells;
  cell_size = sizeof(*row_cells);

  _terminal_output_begin();

  if((display_cols != (unsigned)last_display_cols) ||
     (display_rows != (unsigned)last_display_rows) ||
     (last_display_render_mode != g_run.terminal_render_mode))
    {
      size_t cache_count = (size_t)image_rows_max * (size_t)display_cols;

      _terminal_write_all("\x1b[2J\x1b[H", 7);
      memset(g_run.terminal_cells, 0xff, cache_count * sizeof(*g_run.terminal_cells));
      for(y = 0; y < image_rows_max; y++)
        g_run.terminal_row_hashes[y] = UINT64_MAX;
      last_display_cols = (int)display_cols;
      last_display_rows = (int)display_rows;
      last_display_render_mode = g_run.terminal_render_mode;
    }

  for(y = 0; y < image_rows_max; y++)
    {
      bool draw_row = (y >= pad_top) && (y < (pad_top + target_rows));
      terminal_cell_t *cached_row =
        &g_run.terminal_cells[(size_t)y * (size_t)display_cols];
      uint64_t row_hash;
      unsigned x2;

      _terminal_fill_blank_cells(row_cells, display_cols);

      if(draw_row)
        {
          unsigned map_y = y - pad_top;
          unsigned src_y_top = g_run.terminal_src_y_top[map_y];
          unsigned src_y_bottom = g_run.terminal_src_y_bottom[map_y];

          for(x2 = 0; x2 < target_cols; x2++)
            {
              unsigned src_x = g_run.terminal_src_x[x2];
              terminal_cell_t *cell = &row_cells[pad_left + x2];
              uint32_t top = read_pixel(data_, pitch_, src_x, src_y_top);

              if(g_run.terminal_render_mode == HARNESS_TERMINAL_RENDER_HALF)
                {
                  uint32_t bottom = read_pixel(data_, pitch_, src_x, src_y_bottom);

                  _terminal_store_color(bottom, &cell->bg_r, &cell->bg_g,
                                        &cell->bg_b);
                  _terminal_store_color(top, &cell->fg_r, &cell->fg_g,
                                        &cell->fg_b);
                  cell->glyph = HARNESS_TERMINAL_CELL_HALF_UPPER;
                }
              else
                {
                  static const char ramp[] = " .:-=+*#%@";
                  unsigned r = (top >> 16U) & 0xffU;
                  unsigned g = (top >> 8U) & 0xffU;
                  unsigned b = top & 0xffU;
                  unsigned bright = (299U * r + 587U * g + 114U * b) / 1000U;
                  size_t idx = ((size_t)bright * (sizeof(ramp) - 2U)) / 255U;

                  _terminal_store_color(top, &cell->fg_r, &cell->fg_g,
                                        &cell->fg_b);
                  cell->glyph = HARNESS_TERMINAL_CELL_ASCII;
                  cell->ch = (uint8_t)ramp[idx];
                }
            }
        }

      row_hash = _terminal_hash_cells(row_cells, display_cols);
      if(row_hash != g_run.terminal_row_hashes[y])
        {
          unsigned first = 0;
          unsigned last = display_cols;

          while((first < display_cols) &&
                (memcmp(&row_cells[first], &cached_row[first], cell_size) == 0))
            first++;

          while((last > first) &&
                (memcmp(&row_cells[last - 1U], &cached_row[last - 1U], cell_size) == 0))
            last--;

          if(first < last)
            {
              _terminal_emit_cells(y + 1U, first + 1U, &row_cells[first], last - first);
              memcpy(&cached_row[first], &row_cells[first], (size_t)(last - first) * cell_size);
            }
          g_run.terminal_row_hashes[y] = row_hash;
        }
    }

  if(display_rows > 1U)
    _terminal_render_status_line(image_rows_max, display_cols);

  _terminal_output_flush();
}


RETRO_CALLCONV
static
void
_harness_input_poll(void)
{
  if(!g_cfg.terminal_mode)
    return;

  _terminal_poll_input();
}


RETRO_CALLCONV
static
int16_t
_harness_input_state(unsigned port_,
                     unsigned device_,
                     unsigned index_,
                     unsigned id_)
{
  size_t i;
  uint16_t mask;

  (void)index_;

  if((port_ != 0) || ((device_ & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD))
    return 0;

  mask = 0;
  for(i = 0; i < g_cfg.input_count; i++)
    {
      input_event_t *event = &g_cfg.inputs[i];
      bool active = ((g_run.current_frame >= event->resolved_start_frame) &&
                     (g_run.current_frame < event->resolved_end_frame));

      if(!active)
        continue;

      event->triggered = true;
      if(id_ == RETRO_DEVICE_ID_JOYPAD_MASK)
        mask |= (uint16_t)(1U << event->id);
      else if(id_ == event->id)
        return 1;
    }

  if(g_cfg.terminal_mode)
    {
      _terminal_update_button_mask(g_run.current_frame);
      mask |= g_run.terminal_button_mask;
    }

  if(id_ == RETRO_DEVICE_ID_JOYPAD_MASK)
    return (int16_t)mask;

  if((id_ < HARNESS_TERMINAL_BUTTONS) &&
     ((mask & (uint16_t)(1U << id_)) != 0))
    return 1;

  return 0;
}


static
int
_load_symbol(void       *handle_,
             const char *name_,
             void      **out_)
{
  dlerror();
  *out_ = dlsym(handle_, name_);
  if(*out_ == NULL)
    {
      const char *err = dlerror();
      _report_error("missing core symbol %s%s%s\n",
                    name_,
                    err ? ": " : "",
                    err ? err : "");
      return -1;
    }

  return 0;
}


static
int
_load_core_api(core_api_t *api_,
               const char *path_)
{
  memset(api_, 0, sizeof(*api_));

  api_->handle = dlopen(path_, RTLD_NOW | RTLD_LOCAL);
  if(api_->handle == NULL)
    {
      _report_error("dlopen failed for %s: %s\n", path_, dlerror());
      return -1;
    }

#define load_required(FIELD, SYMBOL)                    \
  do                                                    \
    {                                                   \
      if(_load_symbol(api_->handle,                     \
                      SYMBOL,                           \
                      (void **)&api_->FIELD) != 0)      \
        return -1;                                      \
    }                                                   \
  while(0)

  load_required(api_version,                "retro_api_version");
  load_required(set_environment,            "retro_set_environment");
  load_required(set_video_refresh,          "retro_set_video_refresh");
  load_required(set_audio_sample,           "retro_set_audio_sample");
  load_required(set_audio_sample_batch,     "retro_set_audio_sample_batch");
  load_required(set_input_poll,             "retro_set_input_poll");
  load_required(set_input_state,            "retro_set_input_state");
  load_required(init,                       "retro_init");
  load_required(deinit,                     "retro_deinit");
  load_required(get_system_info,            "retro_get_system_info");
  load_required(get_system_av_info,         "retro_get_system_av_info");
  load_required(set_controller_port_device, "retro_set_controller_port_device");
  load_required(load_game,                  "retro_load_game");
  load_required(unload_game,                "retro_unload_game");
  load_required(run,                        "retro_run");

#undef load_required

  return 0;
}


static
int
_ceil_to_u64(double    value_,
             uint64_t *out_)
{
  uint64_t whole;

  if(!isfinite(value_) || (value_ < 0.0) ||
     (value_ >= (double)(UINT64_MAX / 2ULL)))
    return -1;

  if(value_ <= 0.0)
    {
      *out_ = 0;
      return 0;
    }

  whole = (uint64_t)value_;
  if((double)whole < value_)
    whole++;

  *out_ = whole;
  return 0;
}


static
int
_compute_target_frames(void)
{
  uint64_t by_seconds;

  g_run.core_fps = g_run.av_info.timing.fps;
  if(!isfinite(g_run.core_fps) || (g_run.core_fps <= 0.0))
    g_run.core_fps = 60.0;

  g_run.sample_rate = g_run.av_info.timing.sample_rate;
  if(!isfinite(g_run.sample_rate) || (g_run.sample_rate <= 0.0))
    g_run.sample_rate = 44100.0;

  if(g_cfg.have_frames)
    g_run.target_frames = g_cfg.frames;
  else if(g_cfg.terminal_mode)
    g_run.target_frames = UINT64_MAX;
  else
    g_run.target_frames = 300;

  if(g_cfg.have_seconds)
    {
      if(_ceil_to_u64(g_cfg.seconds * g_run.core_fps, &by_seconds) != 0)
        {
          _report_error("--seconds value produces an unsupported frame count: %g\n",
                        g_cfg.seconds);
          return -1;
        }

      if(g_cfg.have_frames)
        {
          if(by_seconds < g_run.target_frames)
            g_run.target_frames = by_seconds;
        }
      else
        {
          g_run.target_frames = by_seconds;
        }
    }

  return 0;
}


static
int
_configure_benchmark_window(void)
{
  uint64_t start_frame = 1;
  uint64_t end_frame;

  if(g_cfg.have_benchmark_start_frame)
    start_frame = g_cfg.benchmark_start_frame;

  end_frame = g_cfg.have_benchmark_end_frame ?
    g_cfg.benchmark_end_frame : g_run.target_frames;

  if(start_frame > end_frame)
    {
      _report_error("benchmark start frame %" PRIu64
                    " is after end frame %" PRIu64 "\n",
                    start_frame,
                    end_frame);
      return -1;
    }

  if(end_frame > g_run.target_frames)
    g_run.target_frames = end_frame;

  g_run.benchmark_start_frame = start_frame;
  g_run.benchmark_end_frame   = end_frame;
  return 0;
}


static
int
_frame_from_seconds(double    seconds_,
                    uint64_t *frame_out_)
{
  double value;
  uint64_t frame;

  if(seconds_ <= 0.0)
    {
      *frame_out_ = 1;
      return 0;
    }

  value = seconds_ * g_run.core_fps;
  if(!isfinite(value) || (value >= (double)(UINT64_MAX / 2ULL)))
    return -1;

  frame = (uint64_t)value + 1;
  if(frame == 0)
    return -1;

  *frame_out_ = frame;
  return 0;
}


static
int
_resolve_input_events(void)
{
  size_t i;

  for(i = 0; i < g_cfg.input_count; i++)
    {
      input_event_t *event = &g_cfg.inputs[i];
      uint64_t duration;

      if(event->start_is_seconds)
        {
          if(_frame_from_seconds(event->start_seconds,
                                 &event->resolved_start_frame) != 0)
            {
              _report_error("input event start produces an unsupported frame: %s\n",
                            event->spec);
              return -1;
            }
        }
      else
        event->resolved_start_frame = event->start_frame ? event->start_frame : 1;

      if(event->duration_is_seconds)
        {
          if(_ceil_to_u64(event->duration_seconds * g_run.core_fps,
                          &duration) != 0)
            {
              _report_error("input event duration produces an unsupported frame count: %s\n",
                            event->spec);
              return -1;
            }
        }
      else
        duration = event->duration_frames;

      if(duration == 0)
        duration = 1;

      if(UINT64_MAX - event->resolved_start_frame < duration)
        {
          _report_error("input event range overflows: %s\n", event->spec);
          return -1;
        }

      event->resolved_end_frame = event->resolved_start_frame + duration;

      if(event->resolved_start_frame > g_run.target_frames)
        _harness_log(RETRO_LOG_WARN,
                     "[Harness]: input event is after the requested run window: %s\n",
                     event->spec);
    }

  return 0;
}


static
void
_json_string(FILE       *f_,
             const char *s_)
{
  fputc('"', f_);
  if(s_ != NULL)
    {
      while(*s_)
        {
          unsigned char c = (unsigned char)*s_++;

          switch(c)
            {
            case '\\':
              fputs("\\\\", f_);
              break;
            case '"':
              fputs("\\\"", f_);
              break;
            case '\n':
              fputs("\\n", f_);
              break;
            case '\r':
              fputs("\\r", f_);
              break;
            case '\t':
              fputs("\\t", f_);
              break;
            default:
              if(c < 0x20)
                fprintf(f_, "\\u%04x", c);
              else
                fputc(c, f_);
              break;
            }
        }
    }
  fputc('"', f_);
}


static
void
_write_input_events_json(FILE *f_)
{
  size_t i;

  fprintf(f_, "  \"input_event_count\": %zu,\n", g_cfg.input_count);
  fprintf(f_, "  \"input_events\": [");
  for(i = 0; i < g_cfg.input_count; i++)
    {
      const input_event_t *event = &g_cfg.inputs[i];

      fprintf(f_, "%s\n    { \"spec\": ", (i == 0) ? "" : ",");
      _json_string(f_, event->spec);
      fprintf(f_, ", \"button\": ");
      _json_string(f_, event->button);
      fprintf(f_,
              ", \"start_frame\": %" PRIu64
              ", \"end_frame\": %" PRIu64
              ", \"triggered\": %s }",
              event->resolved_start_frame,
              event->resolved_end_frame,
              event->triggered ? "true" : "false");
    }
  fprintf(f_, "%s  ],\n", g_cfg.input_count ? "\n" : "");
}


static
void
_write_core_options_json(FILE *f_)
{
  size_t i;

  fprintf(f_, "  \"core_option_count\": %zu,\n", g_cfg.option_count);
  fprintf(f_, "  \"core_options\": [");
  for(i = 0; i < g_cfg.option_count; i++)
    {
      const harness_option_t *option = &g_cfg.options[i];

      fprintf(f_, "%s\n    { \"key\": ", (i == 0) ? "" : ",");
      _json_string(f_, option->key);
      fprintf(f_, ", \"value\": ");
      _json_string(f_, option->value);
      fprintf(f_, " }");
    }
  fprintf(f_, "%s  ],\n", g_cfg.option_count ? "\n" : "");
}


static
int
_write_metrics(const char *status_,
               int         exit_code_)
{
  FILE *f;
  double emulated_seconds;
  double average_fps;
  double speed_multiplier;
  double benchmark_emulated_seconds;
  double benchmark_average_fps;
  double benchmark_speed_multiplier;

  if(_mkdir_parent(g_cfg.metrics_path) != 0)
    return -1;

  f = fopen(g_cfg.metrics_path, "wb");
  if(f == NULL)
    return -1;

  emulated_seconds = (g_run.core_fps > 0.0) ? ((double)g_run.frames_run / g_run.core_fps) : 0.0;
  average_fps = (g_run.wall_seconds > 0.0) ? ((double)g_run.frames_run / g_run.wall_seconds) : 0.0;
  speed_multiplier = (g_run.core_fps > 0.0) ? (average_fps / g_run.core_fps) : 0.0;
  benchmark_emulated_seconds =
    (g_run.core_fps > 0.0) ? ((double)g_run.benchmark_frames_run / g_run.core_fps) : 0.0;
  benchmark_average_fps =
    (g_run.benchmark_wall_seconds > 0.0) ?
    ((double)g_run.benchmark_frames_run / g_run.benchmark_wall_seconds) : 0.0;
  benchmark_speed_multiplier =
    (g_run.core_fps > 0.0) ? (benchmark_average_fps / g_run.core_fps) : 0.0;

  fprintf(f, "{\n");
  fprintf(f, "  \"status\": ");
  _json_string(f, status_);
  fprintf(f, ",\n  \"exit_code\": %d,\n", exit_code_);
  fprintf(f, "  \"core_path\": ");
  _json_string(f, g_cfg.core_path);
  fprintf(f, ",\n  \"title_path\": ");
  if(g_cfg.title_path)
    _json_string(f, g_cfg.title_path);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"bios_path\": ");
  _json_string(f, g_cfg.bios_path);
  fprintf(f, ",\n  \"bios_filename\": ");
  _json_string(f, g_run.bios_filename);
  fprintf(f, ",\n  \"bios_name\": ");
  _json_string(f, g_run.bios_name);
  fprintf(f, ",\n  \"font_path\": ");
  if(g_cfg.font_path)
    _json_string(f, g_cfg.font_path);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"font_filename\": ");
  if(g_run.font_filename)
    _json_string(f, g_run.font_filename);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"output_dir\": ");
  _json_string(f, g_cfg.output_dir);
  fprintf(f, ",\n  \"work_dir\": ");
  _json_string(f, g_cfg.work_dir);
  fprintf(f, ",\n  \"system_dir\": ");
  _json_string(f, g_cfg.system_dir);
  fprintf(f, ",\n  \"save_dir\": ");
  _json_string(f, g_cfg.save_dir);
  fprintf(f, ",\n  \"log_path\": ");
  _json_string(f, g_cfg.log_path);
  fprintf(f, ",\n  \"metrics_path\": ");
  _json_string(f, g_cfg.metrics_path);
  fprintf(f, ",\n  \"audio_path\": ");
  if(g_cfg.audio_path)
    _json_string(f, g_cfg.audio_path);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"screenshot_path\": ");
  if(g_cfg.screenshot_path)
    _json_string(f, g_cfg.screenshot_path);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"screenshot_every_dir\": ");
  if(g_cfg.screenshot_every_dir)
    _json_string(f, g_cfg.screenshot_every_dir);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"screenshot_every_step\": ");
  if(g_cfg.screenshot_every_step > 0)
    fprintf(f, "%" PRIu64, g_cfg.screenshot_every_step);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"screenshot_every_written\": %" PRIu64,
          g_cfg.screenshot_every_written);
  fprintf(f, ",\n  \"screenshot_when_mode\": ");
  switch(g_cfg.screenshot_when_mode)
    {
    case SCREENSHOT_WHEN_NONBLANK: fprintf(f, "\"nonblank\""); break;
    case SCREENSHOT_WHEN_CHANGED:  fprintf(f, "\"changed\"");  break;
    default:                       fputs("null", f);          break;
    }
  fprintf(f, ",\n  \"screenshot_when_skipped\": %" PRIu64,
          g_cfg.screenshot_when_skipped);
  fprintf(f, ",\n  \"requested_frames\": ");
  if(g_cfg.have_frames)
    fprintf(f, "%" PRIu64, g_cfg.frames);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"requested_seconds\": ");
  if(g_cfg.have_seconds)
    fprintf(f, "%.9f", g_cfg.seconds);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"requested_benchmark_start_frame\": ");
  if(g_cfg.have_benchmark_start_frame)
    fprintf(f, "%" PRIu64, g_cfg.benchmark_start_frame);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"requested_benchmark_end_frame\": ");
  if(g_cfg.have_benchmark_end_frame)
    fprintf(f, "%" PRIu64, g_cfg.benchmark_end_frame);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"cpu\": ");
  if(g_cfg.have_cpu)
    fprintf(f, "%d", g_cfg.cpu);
  else
    fputs("null", f);
  fprintf(f, ",\n  \"cpu_affinity_applied\": %s",
          g_run.cpu_affinity_applied ? "true" : "false");
  fprintf(f, ",\n  \"terminal_mode\": %s",
          g_cfg.terminal_mode ? "true" : "false");
  fprintf(f, ",\n  \"terminal_render_requested\": ");
  _json_string(f, _terminal_render_override_name(g_cfg.terminal_render_override));
  fprintf(f, ",\n  \"terminal_renderer\": ");
  if(g_run.terminal_render_mode >= 0)
    _json_string(f, _terminal_render_mode_name(g_run.terminal_render_mode));
  else
    fputs("null", f);
  fprintf(f, ",\n  \"terminal_kitty_probed\": %s",
          g_run.terminal_kitty_probed ? "true" : "false");
  fprintf(f, ",\n  \"terminal_kitty_supported\": %s",
          g_run.terminal_kitty_supported ? "true" : "false");
  fprintf(f, ",\n  \"terminal_kitty_probe_result\": ");
  _json_string(f, g_run.terminal_kitty_probe_result ?
               g_run.terminal_kitty_probe_result : "not_run");
  fprintf(f, ",\n  \"terminal_kitty_zlib_probed\": %s",
          g_run.terminal_kitty_zlib_probed ? "true" : "false");
  fprintf(f, ",\n  \"terminal_kitty_zlib_supported\": %s",
          g_run.terminal_kitty_zlib_supported ? "true" : "false");
  fprintf(f, ",\n  \"terminal_kitty_zlib_probe_result\": ");
  _json_string(f, g_run.terminal_kitty_zlib_probe_result ?
               g_run.terminal_kitty_zlib_probe_result : "not_run");
  fprintf(f, ",\n  \"terminal_sixel_probed\": %s",
          g_run.terminal_sixel_probed ? "true" : "false");
  fprintf(f, ",\n  \"terminal_sixel_supported\": %s",
          g_run.terminal_sixel_supported ? "true" : "false");
  fprintf(f, ",\n  \"terminal_sixel_probe_result\": ");
  _json_string(f, g_run.terminal_sixel_probe_result ?
               g_run.terminal_sixel_probe_result : "not_run");
  fprintf(f, ",\n  \"terminal_render_calls\": %" PRIu64,
          g_run.terminal_render_calls);
  fprintf(f, ",\n  \"terminal_render_seconds\": %.9f",
          g_run.terminal_render_seconds_total);
  fprintf(f, ",\n  \"terminal_render_last_seconds\": %.9f",
          g_run.terminal_render_seconds_last);
  fprintf(f, ",\n  \"terminal_render_max_seconds\": %.9f",
          g_run.terminal_render_seconds_max);
  fprintf(f, ",\n  \"terminal_render_seconds_ema\": %.9f",
          g_run.terminal_render_seconds_ema);
  fprintf(f, ",\n  \"terminal_adaptive_fps\": %.9f",
          g_run.terminal_adaptive_fps);
  fprintf(f, ",\n  \"terminal_image_drawn_frames\": %" PRIu64,
          g_run.terminal_image_drawn_frames);
  fprintf(f, ",\n  \"terminal_image_skipped_frames\": %" PRIu64,
          g_run.terminal_image_skipped_frames);
  fprintf(f, ",\n  \"terminal_image_failed_frames\": %" PRIu64,
          g_run.terminal_image_failed_frames);
  fprintf(f, ",\n  \"terminal_image_bytes\": %" PRIu64,
          g_run.terminal_image_bytes);
  fprintf(f, ",\n  \"terminal_image_seconds\": %.9f",
          g_run.terminal_image_seconds_total);
  fprintf(f, ",\n  \"terminal_image_last_seconds\": %.9f",
          g_run.terminal_image_seconds_last);
  fprintf(f, ",\n  \"terminal_image_max_seconds\": %.9f",
          g_run.terminal_image_seconds_max);
  fprintf(f, ",\n  \"terminal_bytes_written\": %" PRIu64,
          g_run.terminal_bytes_written);
  fprintf(f, ",\n");
  _write_core_options_json(f);
  fprintf(f, "  \"target_frames\": %" PRIu64 ",\n", g_run.target_frames);
  fprintf(f, "  \"benchmark_start_frame\": %" PRIu64 ",\n", g_run.benchmark_start_frame);
  fprintf(f, "  \"benchmark_end_frame\": %" PRIu64 ",\n", g_run.benchmark_end_frame);
  fprintf(f, "  \"benchmark_frames_run\": %" PRIu64 ",\n", g_run.benchmark_frames_run);
  fprintf(f, "  \"benchmark_emulated_seconds\": %.9f,\n", benchmark_emulated_seconds);
  fprintf(f, "  \"benchmark_wall_seconds\": %.9f,\n", g_run.benchmark_wall_seconds);
  fprintf(f, "  \"benchmark_average_fps\": %.9f,\n", benchmark_average_fps);
  fprintf(f, "  \"benchmark_speed_multiplier\": %.9f,\n", benchmark_speed_multiplier);
  _write_input_events_json(f);
  fprintf(f, "  \"frames_run\": %" PRIu64 ",\n", g_run.frames_run);
  fprintf(f, "  \"video_frames\": %" PRIu64 ",\n", g_run.video_frames);
  fprintf(f, "  \"audio_frames\": %" PRIu64 ",\n", g_run.audio_frames);
  fprintf(f, "  \"audio_bytes\": %" PRIu64 ",\n", g_run.audio_bytes);
  fprintf(f, "  \"core_fps\": %.9f,\n", g_run.core_fps);
  fprintf(f, "  \"sample_rate\": %.9f,\n", g_run.sample_rate);
  fprintf(f, "  \"emulated_seconds\": %.9f,\n", emulated_seconds);
  fprintf(f, "  \"wall_seconds\": %.9f,\n", g_run.wall_seconds);
  fprintf(f, "  \"average_fps\": %.9f,\n", average_fps);
  fprintf(f, "  \"speed_multiplier\": %.9f,\n", speed_multiplier);
  fprintf(f, "  \"last_width\": %u,\n", g_run.last_width);
  fprintf(f, "  \"last_height\": %u,\n", g_run.last_height);
  fprintf(f, "  \"last_pitch\": %zu,\n", g_run.last_pitch);
  fprintf(f, "  \"pixel_format\": ");
  _json_string(f, _pixel_format_name(g_run.last_pixel_format));
  fprintf(f, ",\n  \"log_counts\": {\n");
  fprintf(f, "    \"debug\": %" PRIu64 ",\n", g_run.log_debug);
  fprintf(f, "    \"info\": %" PRIu64 ",\n", g_run.log_info);
  fprintf(f, "    \"warn\": %" PRIu64 ",\n", g_run.log_warn);
  fprintf(f, "    \"error\": %" PRIu64 "\n", g_run.log_error);
  fprintf(f, "  }\n");
  fprintf(f, "}\n");

  return (fclose(f) == 0) ? 0 : -1;
}


static
int
_write_final_screenshot(void)
{
  if(g_cfg.screenshot_path == NULL)
    return 0;

  if(g_run.last_frame == NULL)
    {
      _report_error("no video frame was captured for final screenshot\n");
      return -1;
    }

  return _write_png(g_cfg.screenshot_path,
                    g_run.last_frame,
                    g_run.last_width,
                    g_run.last_height,
                    g_run.last_pitch,
                    g_run.last_pixel_format);
}


static
int
_write_requested_screenshots(void)
{
  size_t i;
  int rv = 0;

  for(i = 0; i < g_cfg.screenshot_count; i++)
    {
      screenshot_request_t *shot = &g_cfg.screenshots[i];

      if(!shot->captured)
        {
          _report_error("requested screenshot frame was not reached: %" PRIu64 "\n",
                        shot->frame);
          rv = -1;
          continue;
        }

      if(_write_png(shot->path,
                    shot->data,
                    shot->width,
                    shot->height,
                    shot->pitch,
                    shot->pixel_format) != 0)
        {
          _report_error("failed writing screenshot %s\n", shot->path);
          rv = -1;
          continue;
        }

      shot->written = true;
    }

  return rv;
}


static
void
_free_screenshot_captures(void)
{
  size_t i;

  for(i = 0; i < g_cfg.screenshot_count; i++)
    {
      free(g_cfg.screenshots[i].data);
      g_cfg.screenshots[i].data = NULL;
      g_cfg.screenshots[i].data_size = 0;
    }
}


int
main(int    argc_,
     char **argv_)
{
  core_api_t api;
  struct retro_system_info sys_info;
  struct retro_game_info game_info;
  const char *status = "error";
  int exit_code = 1;
  double start;
  double end;
  double benchmark_start = 0.0;
  double average_fps;
  uint64_t frame;

  memset(&api, 0, sizeof(api));
  memset(&g_run, 0, sizeof(g_run));
  g_run.saved_stdout = -1;
  g_run.saved_stderr = -1;
  g_run.log_fd = -1;
  g_run.pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
  g_run.last_pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;

  _install_signal_handlers();
  _validate_arg_followers(argc_, argv_);
  _parse_args(argc_, argv_);

  if(g_cfg.list_bios)
    {
      _print_bios_list(stdout);
      return 0;
    }

  _prepare_paths();

  if(_setup_logging() != 0)
    {
      perror("setup logging");
      goto cleanup;
    }

  _harness_log(RETRO_LOG_INFO, "[Harness]: output directory: %s\n", g_cfg.output_dir);

  if(_apply_cpu_affinity() != 0)
    {
      status = "affinity_error";
      goto cleanup;
    }

  if(_stage_rom(g_cfg.bios_path, BIOS_ROMS, "BIOS",
                &g_run.bios_filename, &g_run.bios_name) != 0)
    goto cleanup;
  _add_option_normalized("opera_bios", g_run.bios_filename);

  if(g_cfg.font_path != NULL)
    {
      if(_stage_rom(g_cfg.font_path, FONT_ROMS, "font",
                    &g_run.font_filename, &g_run.font_name) != 0)
        goto cleanup;
      _add_option_normalized("opera_font", g_run.font_filename);
    }

  if(_load_core_api(&api, g_cfg.core_path) != 0)
    goto cleanup;

  if(api.api_version() != RETRO_API_VERSION)
    {
      _report_error("unsupported libretro API version: core=%u harness=%u\n",
                    api.api_version(), RETRO_API_VERSION);
      goto cleanup;
    }

  memset(&sys_info, 0, sizeof(sys_info));
  api.get_system_info(&sys_info);

  api.set_environment(_harness_environment);
  api.set_video_refresh(_harness_video_refresh);
  api.set_audio_sample(_harness_audio_sample);
  api.set_audio_sample_batch(_harness_audio_sample_batch);
  api.set_input_poll(_harness_input_poll);
  api.set_input_state(_harness_input_state);

  if(g_cfg.terminal_mode && (_terminal_open() != 0))
    {
      _harness_log(RETRO_LOG_WARN,
                   "[Harness]: terminal mode unavailable; continuing without live terminal\n"
                   "          details: %s\n", strerror(errno));
      g_cfg.terminal_mode = false;
    }

  api.init();
  g_run.core_initialized = true;

  api.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

  memset(&game_info, 0, sizeof(game_info));
  game_info.path = g_cfg.title_path;

  if(!api.load_game(g_cfg.title_path ? &game_info : NULL))
    {
      _report_error("retro_load_game failed\n");
      goto cleanup;
    }
  g_run.loaded_game = true;

  memset(&g_run.av_info, 0, sizeof(g_run.av_info));
  api.get_system_av_info(&g_run.av_info);
  if(_compute_target_frames() != 0)
    goto cleanup;
  if(_configure_benchmark_window() != 0)
    goto cleanup;
  if(_resolve_input_events() != 0)
    goto cleanup;
  if(_open_audio() != 0)
    {
      _report_error("unable to open audio output: %s\n", g_cfg.audio_path);
      goto cleanup;
    }

  start = _monotonic_seconds();
  while(g_run.frames_run < g_run.target_frames)
    {
      if(g_signal_exit_requested)
        break;

      frame = g_run.frames_run + 1;
      if(frame == g_run.benchmark_start_frame)
        {
          benchmark_start = _monotonic_seconds();
          g_run.benchmark_started = true;
        }

      g_run.current_frame = frame;
      double frame_start = _monotonic_seconds();
      api.run();
      g_run.frames_run++;

      if((frame >= g_run.benchmark_start_frame) &&
         (frame <= g_run.benchmark_end_frame))
        g_run.benchmark_frames_run++;

      if(frame == g_run.benchmark_end_frame)
        {
          g_run.benchmark_wall_seconds = _monotonic_seconds() - benchmark_start;
          g_run.benchmark_finished = true;
        }

      if(g_signal_exit_requested)
        break;

      if(g_run.terminal_quit_requested)
        break;

      if(g_cfg.terminal_mode)
        {
          double frame_elapsed = _monotonic_seconds() - frame_start;
          if(g_run.core_fps > 0.0)
            _sleep_for_seconds((1.0 / g_run.core_fps) - frame_elapsed);
        }

      if((g_cfg.wall_timeout > 0.0) &&
         ((_monotonic_seconds() - start) >= g_cfg.wall_timeout))
        {
          g_run.timed_out = true;
          break;
        }

    }
  end = _monotonic_seconds();
  g_run.wall_seconds = end - start;
  if(g_run.benchmark_started && !g_run.benchmark_finished)
    g_run.benchmark_wall_seconds = end - benchmark_start;

  if(_write_final_screenshot() != 0)
    g_run.artifact_error = true;
  if(_write_requested_screenshots() != 0)
    g_run.artifact_error = true;

  if(g_run.timed_out)
    {
      status = "timeout";
      exit_code = 124;
    }
  else if(g_signal_exit_requested)
    {
      int signo = (int)g_signal_exit_number;

      status = "signal";
      exit_code = 128 + ((signo > 0) ? signo : SIGTERM);
    }
  else if(g_run.terminal_quit_requested)
    {
      status = "quit";
      exit_code = 0;
    }
  else if(g_run.artifact_error)
    {
      status = "artifact_error";
      exit_code = 1;
    }
  else
    {
      status = "ok";
      exit_code = 0;
    }

 cleanup:
  _terminal_close();
  _close_audio();

  if(g_run.loaded_game && api.unload_game != NULL)
    api.unload_game();
  if(g_run.core_initialized && api.deinit != NULL)
    api.deinit();
  if(api.handle != NULL)
    dlclose(api.handle);

  if(g_cfg.metrics_path != NULL)
    {
      if(_write_metrics(status, exit_code) != 0)
        {
          _report_error("failed writing metrics: %s\n", g_cfg.metrics_path);
          if(exit_code == 0)
            {
              status = "metrics_error";
              exit_code = 1;
            }
        }
    }

  if(!g_cfg.user_work_dir && !g_cfg.keep_work_dir && (g_cfg.work_dir != NULL))
    {
      if(_remove_tree(g_cfg.work_dir) != 0)
        _report_error("warning: failed to remove work dir: %s\n", g_cfg.work_dir);
    }

  _restore_logging();

  average_fps = (g_run.wall_seconds > 0.0) ?
    ((double)g_run.frames_run / g_run.wall_seconds) : 0.0;

  if(exit_code == 0)
    {
      fprintf(stderr,
              "test-harness: ok, frames=%" PRIu64
              ", elapsed=%.3fs, fps=%.2f, log=%s, metrics=%s\n",
              g_run.frames_run,
              g_run.wall_seconds,
              average_fps,
              g_cfg.log_path,
              g_cfg.metrics_path);
    }
  else
    {
      fprintf(stderr,
              "test-harness: %s, frames=%" PRIu64
              ", elapsed=%.3fs, fps=%.2f, log=%s, metrics=%s\n",
              status,
              g_run.frames_run,
              g_run.wall_seconds,
              average_fps,
              g_cfg.log_path ? g_cfg.log_path : "(none)",
              g_cfg.metrics_path ? g_cfg.metrics_path : "(none)");
    }

  free(g_run.last_frame);
  free(g_run.screenshot_when_prev);
  _free_screenshot_captures();
  return exit_code;
}
