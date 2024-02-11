#include "opera_log.h"

#include <stddef.h>

static void opera_log_printf_null(opera_log_level_t level, const char *fmt, ...);

opera_log_printf_t opera_log_printf = opera_log_printf_null;

static
void
opera_log_printf_null(opera_log_level_t level_,
                      const char*       fmt_,
                      ...)
{
  (void)level_;
  (void)fmt_;
}

void
opera_log_set_func(void *func_)
{
  if(func_ == NULL)
    func_ = opera_log_printf_null;

  opera_log_printf = func_;
}
