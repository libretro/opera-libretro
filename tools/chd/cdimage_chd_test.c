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
 *
 *    cdimage_chd_test <reference.cue> <image.chd> --frontend <version>
 *
 * The same, with the core's files behind a frontend VFS of that version
 * made of libretro-common's own functions, as RetroArch hands out. With
 * v7 the CHD's hunks have to be lent from the frontend's mapping, none
 * copied through read; with an older one they are all read.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../../retro_cdimage.h"
#include "../../opera_lr_callbacks.h"

#include <libretro.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

/* ---- a frontend VFS of libretro-common's own functions ---- */

typedef libretro_vfs_implementation_file fe_file;

static void   *fe_chd;          /* the CHD's handle, to count its reads */
static int64_t fe_chd_read;     /* bytes of it copied through read */
static int     fe_maps;         /* maps of it that lent a mapping */

static const char *fe_get_path(struct retro_vfs_file_handle *s)
{ return retro_vfs_file_get_path_impl((fe_file *)s); }
static struct retro_vfs_file_handle *fe_open(const char *p, unsigned m,
      unsigned h)
{
   fe_file *f = retro_vfs_file_open_impl(p, m, h);
   size_t   n = strlen(p);
   if (f && n > 4 && !strcmp(p + n - 4, ".chd"))
      fe_chd = f;
   return (struct retro_vfs_file_handle *)f;
}
static int fe_close(struct retro_vfs_file_handle *s)
{
   if ((void *)s == fe_chd)
      fe_chd = NULL;
   return retro_vfs_file_close_impl((fe_file *)s);
}
static int64_t fe_size(struct retro_vfs_file_handle *s)
{ return retro_vfs_file_size_impl((fe_file *)s); }
static int64_t fe_truncate(struct retro_vfs_file_handle *s, int64_t l)
{ return retro_vfs_file_truncate_impl((fe_file *)s, l); }
static int64_t fe_tell(struct retro_vfs_file_handle *s)
{ return retro_vfs_file_tell_impl((fe_file *)s); }
static int64_t fe_seek(struct retro_vfs_file_handle *s, int64_t o, int w)
{ return retro_vfs_file_seek_impl((fe_file *)s, o, w); }
static int64_t fe_read(struct retro_vfs_file_handle *s, void *d, uint64_t l)
{
   int64_t got = retro_vfs_file_read_impl((fe_file *)s, d, l);
   if ((void *)s == fe_chd && got > 0)
      fe_chd_read += got;
   return got;
}
static int64_t fe_write(struct retro_vfs_file_handle *s, const void *d,
      uint64_t l)
{ return retro_vfs_file_write_impl((fe_file *)s, d, l); }
static int fe_flush(struct retro_vfs_file_handle *s)
{ return retro_vfs_file_flush_impl((fe_file *)s); }
static int fe_remove(const char *p) { return retro_vfs_file_remove_impl(p); }
static int fe_rename(const char *a, const char *b)
{ return retro_vfs_file_rename_impl(a, b); }
static int fe_stat(const char *p, int32_t *sz)
{ return retro_vfs_stat_impl(p, sz); }
static int fe_mkdir(const char *d) { return retro_vfs_mkdir_impl(d); }
static struct retro_vfs_dir_handle *fe_opendir(const char *d, bool h)
{ return (struct retro_vfs_dir_handle *)retro_vfs_opendir_impl(d, h); }
static bool fe_readdir(struct retro_vfs_dir_handle *d)
{ return retro_vfs_readdir_impl((libretro_vfs_implementation_dir *)d); }
static const char *fe_dirent_name(struct retro_vfs_dir_handle *d)
{ return retro_vfs_dirent_get_name_impl((libretro_vfs_implementation_dir *)d); }
static bool fe_dirent_is_dir(struct retro_vfs_dir_handle *d)
{ return retro_vfs_dirent_is_dir_impl((libretro_vfs_implementation_dir *)d); }
static int fe_closedir(struct retro_vfs_dir_handle *d)
{ return retro_vfs_closedir_impl((libretro_vfs_implementation_dir *)d); }
static const uint8_t *fe_map(struct retro_vfs_file_handle *s, int64_t *l)
{
   const uint8_t *p = retro_vfs_file_map_impl((fe_file *)s, l);
   if ((void *)s == fe_chd && p)
      fe_maps++;
   return p;
}
static bool fe_volume(struct retro_vfs_file_handle *s, unsigned w,
      struct retro_vfs_volume *v)
{ return retro_vfs_file_volume_impl((fe_file *)s, w, v); }

static struct retro_vfs_interface fe_iface;

static int frontend(unsigned version)
{
   struct retro_vfs_interface_info info;
   memset(&fe_iface, 0, sizeof(fe_iface));
   fe_iface.get_path        = fe_get_path;
   fe_iface.open            = fe_open;
   fe_iface.close           = fe_close;
   fe_iface.size            = fe_size;
   fe_iface.tell            = fe_tell;
   fe_iface.seek            = fe_seek;
   fe_iface.read            = fe_read;
   fe_iface.write           = fe_write;
   fe_iface.flush           = fe_flush;
   fe_iface.remove          = fe_remove;
   fe_iface.rename          = fe_rename;
   fe_iface.truncate        = fe_truncate;
   fe_iface.stat            = fe_stat;
   fe_iface.mkdir           = fe_mkdir;
   fe_iface.opendir         = fe_opendir;
   fe_iface.readdir         = fe_readdir;
   fe_iface.dirent_get_name = fe_dirent_name;
   fe_iface.dirent_is_dir   = fe_dirent_is_dir;
   fe_iface.closedir        = fe_closedir;
   /* Filled whatever the version: an older frontend's members past its
    * version must never be called */
   fe_iface.map             = fe_map;
   fe_iface.volume          = fe_volume;
   if (version < 3)
      return 0;
   info.required_interface_version = version;
   info.iface                      = &fe_iface;
   filestream_vfs_init(&info);
   return 1;
}

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

   int        version = 0;

   if (argc < 3)
   {
      fprintf(stderr, "usage: %s reference.cue image.chd"
            " [--frontend <version>]\n", argv[0]);
      return 2;
   }
   if (argc == 5 && !strcmp(argv[3], "--frontend"))
   {
      version = atoi(argv[4]);
      if (!frontend((unsigned)version))
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

   if (version)
   {
      /* v7 lends every hunk; an older frontend reads them all */
      unsigned want = version >= 7 ? (fe_maps == 1 && fe_chd_read == 0)
         : (fe_maps == 0 && fe_chd_read > 0);
      printf("frontend v%d: %d mapping(s), %lld CHD bytes read\n", version,
            fe_maps, (long long)fe_chd_read);
      if (!want)
         bad++;
   }

   printf("%s: %d blocks, %u mismatches\nRESULT: %s\n", argv[2],
         (int)blocks_ref, bad, bad ? "FAIL" : "PASS");

   retro_cdimage_close(ref);
   retro_cdimage_close(chd);
   free(ref);
   free(chd);
   return bad ? 1 : 0;
}
