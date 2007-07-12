#ifndef _DNS_SD_PROXY
#define _DNS_SD_PROXY

#include <stdint.h>

/* fixup to make pidgin compile against win32 bonjour */
#ifdef _WIN32
#define _MSL_STDINT_H
#undef bzero
#endif

#include <dns_sd.h>

/* dns_sd.h defines bzero and we also do in libc_internal.h */
#ifdef _WIN32
#undef bzero
#endif

#endif
