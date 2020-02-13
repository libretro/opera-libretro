#ifndef LIBOPERA_INLINE_H_INCLUDED
#define LIBOPERA_INLINE_H_INCLUDED

#ifndef INLINE

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define INLINE inline
#elif defined(_WIN32)
#define INLINE __inline
#elif defined(__INTEL_COMPILER)
#define INLINE __inline
#elif defined(__GNUC__)
#define INLINE __inline__
#else
#define INLINE
#endif

#endif /* INLINE */

#ifndef FORCEINLINE

#ifdef _MSC_VER
#define FORCEINLINE __forceinline
#elif defined(__GNUC__)
#define FORCEINLINE __attribute__((always_inline)) INLINE
#else
#define FORCEINLINE INLINE
#endif

#endif /* FORCEINLINE */

#endif /* LIBOPERA_INLINE_H_INCLUDED */
