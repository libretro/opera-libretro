#ifndef LIBOPERA_LOG_H_INCLUDED
#define LIBOPERA_LOG_H_INCLUDED

enum opera_log_level_t
  {
    OPERA_LOG_DEBUG = 0,
    OPERA_LOG_INFO,
    OPERA_LOG_WARN,
    OPERA_LOG_ERROR
  };
typedef enum opera_log_level_t opera_log_level_t;

typedef void (*opera_log_printf_t)(opera_log_level_t level, const char *fmt, ...);

extern opera_log_printf_t opera_log_printf;

void opera_log_set_func(void *func);

#endif
