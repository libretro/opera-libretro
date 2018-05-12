#ifndef LIBFREEDO_ENDIANNESS_H_INCLUDED
#define LIBFREEDO_ENDIANNESS_H_INCLUDED

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
#elif defined(MSB_FIRST)
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

#if defined(__GNUC__)
#define SWAP32(X) (__builtin_bswap32(X))
#elif defined(_MSC_VER) && _MSC_VER > 1200
#define SWAP32(X) (_byteswap_ulong(X))
#else
#define SWAP32(X) \
  ((((uint32_t)(X) & 0xFF000000) >> 24) |       \
   (((uint32_t)(X) & 0x00FF0000) >>  8) |       \
   (((uint32_t)(X) & 0x0000FF00) <<  8) |       \
   (((uint32_t)(X) & 0x000000FF) << 24))
#endif

#if IS_BIG_ENDIAN
#define SWAP32_IF_LITTLE_ENDIAN(X) (X)
#else
#define SWAP32_IF_LITTLE_ENDIAN(X) (SWAP32(X))
#endif

#undef IS_BIG_ENDIAN

#endif /* LIBFREEDO_ENDIANNESS_H_INCLUDED */
