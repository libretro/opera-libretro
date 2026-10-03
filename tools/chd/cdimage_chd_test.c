/* CHD reader check against the plain image it was made from.
 *
 *    cdimage_chd_test <reference.cue> <image.chd>
 *
 * Opens both through retro_cdimage_open -- the CUE through the cue/bin
 * path, the CHD through rchd -- and requires them to agree on the track
 * table and the logical block count, and on every sector of the disc as
 * retro_cdimage_read_sector returns it, read both raw (2352 bytes) and
 * cooked (2048, the user data the drive emulation asks for).
 *
 * The CHDs run.sh feeds this are compressed with one CD codec family
 * each (cdzs, cdlz, cdzl, cdfl), so a codec the build cannot decode
 * shows up as a failed read or a mismatch.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../../retro_cdimage.h"
#include "../../opera_lr_callbacks.h"

static void test_log(enum retro_log_level level, const char *fmt, ...)
{
   va_list ap;
   if (level < RETRO_LOG_WARN)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

static unsigned compare(cdimage_t *ref, cdimage_t *chd, size_t blocks,
      size_t size, uint8_t *a, uint8_t *b)
{
   unsigned bad = 0;
   size_t   lba;

   for (lba = 0; lba < blocks; lba++)
   {
      ssize_t ra, rb;
      memset(a, 0xA5, size);
      memset(b, 0x5A, size);
      ra = retro_cdimage_read_sector(ref, lba, a, size);
      rb = retro_cdimage_read_sector(chd, lba, b, size);
      if (ra != rb || ra <= 0 || memcmp(a, b, (size_t)ra))
      {
         if (bad < 8)
            printf("sector %u (%u bytes): read %d/%d%s\n", (unsigned)lba,
                  (unsigned)size, (int)ra, (int)rb,
                  (ra == rb && ra > 0) ? ", contents differ" : "");
         bad++;
      }
   }
   return bad;
}

int main(int argc, char **argv)
{
   cdimage_t *ref;
   cdimage_t *chd;
   uint8_t    a[2352];
   uint8_t    b[2352];
   unsigned   bad = 0;
   ssize_t    blocks_ref;
   ssize_t    blocks_chd;
   int        i;

   if (argc < 3)
   {
      fprintf(stderr, "usage: %s reference.cue image.chd\n", argv[0]);
      return 2;
   }
   opera_lr_callbacks_set_log_printf(test_log);

   ref = (cdimage_t *)calloc(1, sizeof(*ref));
   chd = (cdimage_t *)calloc(1, sizeof(*chd));
   if (!ref || !chd
         || retro_cdimage_open(argv[1], ref)
         || retro_cdimage_open(argv[2], chd))
   {
      printf("%s: open failed\nRESULT: FAIL\n", argv[2]);
      return 1;
   }

   blocks_ref = retro_cdimage_get_number_of_logical_blocks(ref);
   blocks_chd = retro_cdimage_get_number_of_logical_blocks(chd);
   if (blocks_ref != blocks_chd || blocks_ref <= 0)
   {
      printf("logical blocks differ: %d vs %d\n", (int)blocks_ref,
            (int)blocks_chd);
      bad++;
   }
   if (ref->num_tracks != chd->num_tracks)
   {
      printf("track count differs: %d vs %d\n", ref->num_tracks,
            chd->num_tracks);
      bad++;
   }
   for (i = 0; i < ref->num_tracks && i < chd->num_tracks; i++)
   {
      if (ref->tracks[i].type != chd->tracks[i].type
            || ref->tracks[i].start_lba != chd->tracks[i].start_lba
            || ref->tracks[i].frames != chd->tracks[i].frames)
      {
         printf("track %d differs: type %d/%d lba %u/%u frames %u/%u\n",
               i + 1, (int)ref->tracks[i].type, (int)chd->tracks[i].type,
               (unsigned)ref->tracks[i].start_lba,
               (unsigned)chd->tracks[i].start_lba,
               (unsigned)ref->tracks[i].frames,
               (unsigned)chd->tracks[i].frames);
         bad++;
      }
   }

   if (!bad)
   {
      bad += compare(ref, chd, (size_t)blocks_ref, 2352, a, b);
      bad += compare(ref, chd, (size_t)blocks_ref, 2048, a, b);
   }

   printf("%s: %d blocks, %u mismatches\nRESULT: %s\n", argv[2],
         (int)blocks_ref, bad, bad ? "FAIL" : "PASS");

   retro_cdimage_close(ref);
   retro_cdimage_close(chd);
   free(ref);
   free(chd);
   return bad ? 1 : 0;
}
