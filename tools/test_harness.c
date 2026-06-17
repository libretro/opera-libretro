#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "libretro.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#ifdef __linux__
#include <sched.h>
#endif
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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
  char *screenshot_path;

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
  bool     keep_work_dir;
  bool     user_work_dir;
  bool     verbose;
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

RETRO_CALLCONV
static
void
_harness_log(enum retro_log_level level_,
             const char          *fmt_,
             ...);

static
void
_print_usage(FILE *f_)
{
  fprintf(f_,
          "Usage: test-harness --core ./opera_libretro.so [--bios /path/to/bios.bin] [options]\n"
          "\n"
          "Required:\n"
          "  --core PATH              libretro core shared object to load\n"
          "  --bios PATH              recognized 3DO BIOS ROM; default panafz1.bin\n"
          "                           filename values search RetroArch system dirs\n"
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
          "\n"
          "Artifacts:\n"
          "  --output-dir DIR         default ./test-harness-runs/<timestamp-pid>\n"
          "  --log PATH               default <output-dir>/run.log\n"
          "  --metrics PATH           default <output-dir>/metrics.json\n"
          "  --audio PATH             write stereo s16le WAV\n"
          "  --screenshot PATH        write final frame as PPM\n"
          "  --screenshot-at N=PATH   write frame N as PPM, repeatable\n"
          "\n"
          "Runtime setup:\n"
          "  --font PATH              optional recognized Kanji/font ROM; filename\n"
          "                           values search the same RetroArch directories\n"
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

  resolved = _resolve_existing_path(path_);
  if(resolved != NULL)
    return resolved;

  if(_path_has_separator(path_))
    return NULL;

  return _find_rom_filename_in_retroarch_dirs(path_);
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
_parse_args(int    argc_,
            char **argv_)
{
  int i;

  memset(&g_cfg, 0, sizeof(g_cfg));
  g_run.saved_stdout = -1;
  g_run.saved_stderr = -1;
  g_run.log_fd       = -1;
  g_run.pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;

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
        g_cfg.audio_path = _xstrdup(argv_[++i]);
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
      else if(strcmp(arg, "--screenshot-at") == 0)
        _add_screenshot_arg(argv_[++i]);
      else if(strcmp(arg, "--keep-work-dir") == 0)
        g_cfg.keep_work_dir = true;
      else if(strcmp(arg, "--verbose") == 0)
        g_cfg.verbose = true;
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
        !strcmp(arg, "--option") ||
        !strcmp(arg, "--input") ||
        !strcmp(arg, "--screenshot-at");

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
    {
      fprintf(stderr, "--core is required\n");
      exit(1);
    }

  if(g_cfg.bios_path == NULL)
    g_cfg.bios_path = _xstrdup("panafz1.bin");

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
                "unable to find BIOS file %s in the current directory or common RetroArch system directories\n",
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
                    "unable to find font file %s in the current directory or common RetroArch system directories\n",
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
        _report_error("%s ROM basename matched %s but content matched %s; use a filename that matches the ROM content\n",
                      kind_, by_base->filename, by_hash->filename);
      else
        _report_error("%s ROM basename matched %s but content did not match the known size/crc32 (got size=%" PRIu64 " crc32=%08" PRIx32 ", expected size=%" PRIu64 " crc32=%08" PRIx32 ")\n",
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
      g_run.pixel_format = *(const enum retro_pixel_format *)data_;
      return ((g_run.pixel_format == RETRO_PIXEL_FORMAT_0RGB1555) ||
              (g_run.pixel_format == RETRO_PIXEL_FORMAT_XRGB8888) ||
              (g_run.pixel_format == RETRO_PIXEL_FORMAT_RGB565));
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
int
_write_ppm(const char             *path_,
           const void             *data_,
           unsigned                width_,
           unsigned                height_,
           size_t                  pitch_,
           enum retro_pixel_format fmt_)
{
  FILE *f;
  unsigned y;

  if((data_ == NULL) || (width_ == 0) || (height_ == 0))
    return -1;

  if(_mkdir_parent(path_) != 0)
    return -1;

  f = fopen(path_, "wb");
  if(f == NULL)
    return -1;

  fprintf(f, "P6\n%u %u\n255\n", width_, height_);

  for(y = 0; y < height_; y++)
    {
      const uint8_t *row = (const uint8_t *)data_ + (pitch_ * y);
      unsigned x;

      for(x = 0; x < width_; x++)
        {
          uint8_t rgb[3];

          if(fmt_ == RETRO_PIXEL_FORMAT_XRGB8888)
            {
              uint32_t p = ((const uint32_t *)row)[x];
              rgb[0] = (uint8_t)((p >> 16) & 0xff);
              rgb[1] = (uint8_t)((p >> 8)  & 0xff);
              rgb[2] = (uint8_t)(p & 0xff);
            }
          else if(fmt_ == RETRO_PIXEL_FORMAT_RGB565)
            {
              uint16_t p = ((const uint16_t *)row)[x];
              rgb[0] = _scale_5_to_8((p >> 11) & 0x1f);
              rgb[1] = _scale_6_to_8((p >> 5)  & 0x3f);
              rgb[2] = _scale_5_to_8(p & 0x1f);
            }
          else
            {
              uint16_t p = ((const uint16_t *)row)[x];
              rgb[0] = _scale_5_to_8((p >> 10) & 0x1f);
              rgb[1] = _scale_5_to_8((p >> 5)  & 0x1f);
              rgb[2] = _scale_5_to_8(p & 0x1f);
            }

          if(fwrite(rgb, 1, sizeof(rgb), f) != sizeof(rgb))
            {
              fclose(f);
              return -1;
            }
        }
    }

  return (fclose(f) == 0) ? 0 : -1;
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

  if(g_cfg.audio_path == NULL)
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

RETRO_CALLCONV
static
void
_harness_input_poll(void)
{
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

  if(id_ == RETRO_DEVICE_ID_JOYPAD_MASK)
    return (int16_t)mask;

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

  return _write_ppm(g_cfg.screenshot_path,
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

      if(_write_ppm(shot->path,
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
  uint64_t frame;

  memset(&api, 0, sizeof(api));
  memset(&g_run, 0, sizeof(g_run));
  g_run.saved_stdout = -1;
  g_run.saved_stderr = -1;
  g_run.log_fd = -1;
  g_run.pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
  g_run.last_pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;

  _validate_arg_followers(argc_, argv_);
  _parse_args(argc_, argv_);
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
      frame = g_run.frames_run + 1;
      if(frame == g_run.benchmark_start_frame)
        {
          benchmark_start = _monotonic_seconds();
          g_run.benchmark_started = true;
        }

      g_run.current_frame = frame;
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

  if(exit_code == 0)
    {
      fprintf(stderr,
              "test-harness: ok, frames=%" PRIu64 ", log=%s, metrics=%s\n",
              g_run.frames_run,
              g_cfg.log_path,
              g_cfg.metrics_path);
    }
  else
    {
      fprintf(stderr,
              "test-harness: %s, frames=%" PRIu64 ", log=%s, metrics=%s\n",
              status,
              g_run.frames_run,
              g_cfg.log_path ? g_cfg.log_path : "(none)",
              g_cfg.metrics_path ? g_cfg.metrics_path : "(none)");
    }

  free(g_run.last_frame);
  _free_screenshot_captures();
  return exit_code;
}
