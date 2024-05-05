#ifndef LIBOPERA_ENDIANNESS_H_INCLUDED
#define LIBOPERA_ENDIANNESS_H_INCLUDED

#include "inline.h"

#include <stdint.h>

#if defined(__linux__)
#include <endian.h>
#endif

#if                                             \
  (defined(__BYTE_ORDER) &&                     \
   (__BYTE_ORDER == __LITTLE_ENDIAN)) ||        \
  defined(__LITTLE_ENDIAN__) ||                 \
  defined(__ARMEL__) ||                         \
  defined(__THUMBEL__) ||                       \
  defined(__AARCH64EL__) ||                     \
  defined(_MIPSEL) ||                           \
  defined(__MIPSEL) ||                          \
  defined(__MIPSEL__) ||                        \
  defined(__amd64__) ||                         \
  defined(__amd64) ||                           \
  defined(__x86_64__) ||                        \
  defined(__x86_64) ||                          \
  defined(_M_X64) ||                            \
  defined(_M_AMD64) ||                          \
  defined(__i386__) ||                          \
  defined(__i386) ||                            \
  defined(_M_IX86)
#define IS_BIG_ENDIAN 0
#define IS_LITTLE_ENDIAN 1
#elif                                           \
  (defined(__BYTE_ORDER) &&                     \
   (__BYTE_ORDER == __BIG_ENDIAN)) ||           \
  defined(__BIG_ENDIAN__) ||                    \
  defined(__ARMEB__) ||                         \
  defined(__THUMBEB__) ||                       \
  defined(__AARCH64EB__) ||                     \
  defined(_MIPSEB) ||                           \
  defined(__MIPSEB) ||                          \
  defined(__MIPSEB__) ||                        \
  defined(__hppa__) ||                          \
  defined(__m68k__) ||                          \
  defined(_M_M68K) ||                           \
  defined(__ppc__) ||                           \
  defined(__POWERPC__) ||                       \
  defined(_M_PPC) ||                            \
  defined(__ppc64__) ||                         \
  defined(__PPC64__) ||                         \
  defined(_XENON) ||                            \
  defined(__sparc__)
#define IS_BIG_ENDIAN 1
#define IS_LITTLE_ENDIAN 0
#elif defined(MSB_FIRST)
#define IS_BIG_ENDIAN 1
#define IS_LITTLE_ENDIAN 0
#else
#define IS_BIG_ENDIAN 0
#define IS_LITTLE_ENDIAN 1
#endif

#if defined(__GNUC__)
#define SWAP32(X) (__builtin_bswap32(X))
#elif defined(_MSC_VER) && _MSC_VER > 1200
#define SWAP32(X) (_byteswap_ulong(X))
#else
#define SWAP32(X)                               \
  ((((uint32_t)(X) & 0xFF000000) >> 24) |       \
   (((uint32_t)(X) & 0x00FF0000) >>  8) |       \
   (((uint32_t)(X) & 0x0000FF00) <<  8) |       \
   (((uint32_t)(X) & 0x000000FF) << 24))
#endif

#if IS_BIG_ENDIAN

#define swap32_if_little_endian(X) (X)
#define swap32_if_le(X) (X)
#define swap32_array_if_little_endian(X,Y)
#define swap32_array_if_le(X,Y)

#else

#define swap32_if_little_endian(X) (SWAP32(X))
#define swap32_if_le(X) (SWAP32(X))

static
INLINE
void
swap32_array_if_little_endian(uint32_t *array_,
                              uint64_t  size_)
{
  uint64_t i;

  for(i = 0; i < size_; i++)
    array_[i] = SWAP32(array_[i]);
}

#define swap32_array_if_le(X,Y) swap32_array_if_little_endian(X,Y)

#endif /* IS_BIG_ENDIAN */

#endif /* LIBOPERA_ENDIANNESS_H_INCLUDED */
