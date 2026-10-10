#pragma once

#include <stddef.h>
#include <stdint.h>

#define OPERA_BIOS_BUILTIN_ID "opera-boot-internal"

struct opera_bios_s
{
  // External file basename; NULL for the built-in Opera Boot descriptor.
  const char *filename;
  const char *name;
  uint32_t    size;
  char        region;
  const char *version;
  const char *date;
  uint8_t     crc32[4];
  uint8_t     md5sum[16];
  uint8_t     sha1sum[20];
};

typedef struct opera_bios_s opera_bios_t;

const opera_bios_t *opera_bios_begin(void);
const opera_bios_t *opera_bios_end(void);

// Returns the immutable embedded Opera Boot descriptor, also the first BIOS entry.
const opera_bios_t *opera_bios_builtin(void);

// Writes original on-disk bytes to caller-owned, non-NULL data_.
// capacity_ must cover opera_bios_builtin()->size. Returns bytes written or -1;
// on failure output may be partial. Bytes beyond that size are left untouched.
// The caller owns zero padding and host byte-order conversion.
int opera_bios_load_builtin(uint8_t *data_, size_t capacity_);

const opera_bios_t *opera_bios_font_begin(void);
const opera_bios_t *opera_bios_font_end(void);
