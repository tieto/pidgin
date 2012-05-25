/* Local libgadu configuration. */

/* libpurple's config */
#include <config.h>

#ifndef __GG_LIBGADU_CONFIG_H
#define __GG_LIBGADU_CONFIG_H

/* Defined if libgadu was compiled for bigendian machine. */
#undef __GG_LIBGADU_BIGENDIAN
#ifdef WORDS_BIGENDIAN
#  define __GG_LIBGADU_BIGENDIAN
#endif

/* Defined if this machine has gethostbyname_r(). */
#undef GG_CONFIG_HAVE_GETHOSTBYNAME_R

/* Defined if this machine has _exit(). */
#define GG_CONFIG_HAVE__EXIT

/* Defined if libgadu was compiled and linked with fork support. */
#undef GG_CONFIG_HAVE_FORK
#ifndef _WIN32
#  define GG_CONFIG_HAVE_FORK
#endif

/* Defined if libgadu was compiled and linked with pthread support. */
/* We don't like pthreads. */
#undef __GG_LIBGADU_HAVE_PTHREAD

/* Defined if this machine has C99-compiliant vsnprintf(). */
#undef __GG_LIBGADU_HAVE_C99_VSNPRINTF
#ifndef _WIN32
#  define __GG_LIBGADU_HAVE_C99_VSNPRINTF
#endif

/* Defined if this machine has va_copy(). */
#define __GG_LIBGADU_HAVE_VA_COPY

/* Defined if this machine has __va_copy(). */
#define __GG_LIBGADU_HAVE___VA_COPY

/* Defined if this machine supports long long. */
#undef __GG_LIBGADU_HAVE_LONG_LONG
#ifdef HAVE_LONG_LONG
#  define __GG_LIBGADU_HAVE_LONG_LONG
#endif

/* Defined if libgadu was compiled and linked with GnuTLS support. */
#undef GG_CONFIG_HAVE_GNUTLS
#ifdef HAVE_GNUTLS
#  define GG_CONFIG_HAVE_GNUTLS
#endif

/* Defined if libgadu was compiled and linked with OpenSSL support. */
/* Always undefined in Purple. */
#undef __GG_LIBGADU_HAVE_OPENSSL

/* Defined if libgadu was compiled and linked with zlib support. */
#undef GG_CONFIG_HAVE_ZLIB

/* Defined if uintX_t types are defined in <stdint.h>. */
#undef GG_CONFIG_HAVE_STDINT_H
#if HAVE_STDINT_H
#  define GG_CONFIG_HAVE_STDINT_H
#endif


#define vnsprintf g_vnsprintf

#endif
