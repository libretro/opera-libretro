#ifndef LIBOPERA_BIOS_H_INCLUDED
#define LIBOPERA_BIOS_H_INCLUDED

#include <stdint.h>

struct opera_bios_s
{
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

const opera_bios_t *opera_bios_font_begin(void);
const opera_bios_t *opera_bios_font_end(void);

#endif /* LIBOPERA_BIOS_H_INCLUDED */
