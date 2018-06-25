#ifndef LIBFREEDO_BIOS_H_INCLUDED
#define LIBFREEDO_BIOS_H_INCLUDED

#include <stdint.h>

struct freedo_bios_s
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

typedef struct freedo_bios_s freedo_bios_t;

const freedo_bios_t *freedo_bios_begin(void);
const freedo_bios_t *freedo_bios_end(void);

const freedo_bios_t *freedo_bios_font_begin(void);
const freedo_bios_t *freedo_bios_font_end(void);

#endif /* LIBFREEDO_BIOS_H_INCLUDED */
