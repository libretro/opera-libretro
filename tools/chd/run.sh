#!/bin/sh
# CHD read regression test.
#
#   sh tools/chd/run.sh
#
# Writes a synthetic two-track image (tools/chd/make_fixture.py), compresses
# it with each CD codec family chdman offers -- zstd (cdzs), LZMA (cdlz),
# zlib (cdzl), FLAC (cdfl) -- and checks every sector of each CHD against
# the CUE it came from with cdimage_chd_test, built under AddressSanitizer +
# UBSan with leak detection.
#
# The preprocessor flags are read back out of the core's own build, so the
# test decodes with exactly the codec set the core is built with.
#
# Needs chdman (MAME tools) and python3. Fails rather than skips when either
# is missing: a CHD check that quietly does nothing is how a codec stops
# working without anyone noticing.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"

command -v chdman >/dev/null || { echo "chdman not found" >&2; exit 1; }
command -v python3 >/dev/null || { echo "python3 not found" >&2; exit 1; }

CC=${CC:-cc}
DEFS=$(make -n -B retro_cdimage.o | grep -o -- '-D[A-Za-z0-9_]*\(="[A-Za-z_]*"\)\{0,1\}' \
       | grep -v GIT_VERSION | sort -u | tr '\n' ' ')
case "$DEFS" in
   *-DHAVE_CHD*) ;;
   *) echo "core build does not define HAVE_CHD" >&2; exit 1 ;;
esac

L=libretro-common
WORK=${TMPDIR:-/tmp}/cdimage_chd_test.$$
mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT

SRC="tools/chd/cdimage_chd_test.c retro_cdimage.c cuefile.c
     opera_lr_callbacks.c
     $L/formats/chd/rchd.c $L/encodings/encoding_huffman.c
     $L/encodings/encoding_rzstd.c $L/encodings/encoding_deflate.c
     $L/encodings/encoding_crc32.c $L/formats/7z/r7z_lzma.c
     $L/formats/flac/rflac.c $L/streams/chd_stream.c
     $L/streams/file_stream.c $L/streams/file_stream_transforms.c
     $L/streams/memory_stream.c $L/streams/interface_stream.c
     $L/encodings/encoding_utf.c $L/features/features_cpu.c
     $L/vfs/vfs_implementation.c $L/vfs/vfs_implementation_cdrom.c
     $L/cdrom/cdrom.c $L/compat/compat_strcasestr.c
     $L/compat/compat_posix_string.c $L/compat/compat_strl.c
     $L/compat/compat_snprintf.c $L/compat/fopen_utf8.c
     $L/memmap/memmap.c $L/string/stdstring.c $L/string/rstrtod.c
     $L/file/file_path.c
     $L/file/file_path_io.c $L/file/retro_dirent.c $L/lists/dir_list.c
     $L/lists/string_list.c $L/memmap/memalign.c $L/time/rtime.c
     $L/rthreads/rthreads.c"

# eval, so the quoted INLINE="inline" in DEFS reaches the compiler as the
# Makefile passes it; SRC is flattened first, since a newline inside eval
# would end the command.
SRC=$(echo $SRC)
eval "$CC -O1 -g $DEFS -fsanitize=address,undefined \
   -fno-sanitize-recover=undefined -I. -I$L/include -Ilibopera \
   -o \"$WORK/cdimage_chd_test\" $SRC -lm -lpthread"

python3 tools/chd/make_fixture.py "$WORK/base"

export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
fail=0
for c in cdzs cdlz cdzl cdfl; do
   case $c in
      cdzs) name="CD Zstandard" ;;
      cdlz) name="CD LZMA" ;;
      cdzl) name="CD Deflate" ;;
      cdfl) name="CD FLAC" ;;
   esac
   chdman createcd -f -c "$c" -i "$WORK/base.cue" -o "$WORK/$c.chd" \
      >/dev/null 2>&1
   # A codec that fell back to storing hunks uncompressed would leave
   # nothing for this test to decode, so its row in the hunk table has to
   # be there.
   chdman info -v -i "$WORK/$c.chd" | grep -q "%  $name" \
      || { echo "$c.chd holds no $name hunks" >&2; exit 1; }
   "$WORK/cdimage_chd_test" "$WORK/base.cue" "$WORK/$c.chd" | tail -1 \
      | grep -q PASS || { echo "FAIL: $c"; fail=1; }
done

if [ $fail = 0 ]; then
   echo "cdimage_chd_test: all images PASS"
else
   exit 1
fi
