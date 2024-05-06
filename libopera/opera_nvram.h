#ifndef OPERA_NVRAM_H_INCLUDED
#define OPERA_NVRAM_H_INCLUDED

#include "boolean.h"

bool opera_nvram_initialized(void *buf, const int bufsize);
void opera_nvram_init(void *buf, const int bufsize);

#endif
