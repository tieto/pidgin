noinst_LTLIBRARIES=libpurple-ciphers.la
# XXX: cipher.lo won't be updated after a change in cipher files

if USE_NSS
AES_SOURCE = aes.c
endif
if USE_GNUTLS
AES_SOURCE = aes.c
endif

libpurple_ciphers_la_SOURCES=\
	$(AES_SOURCE) \
	des.c \
	des3.c \
	gchecksum.c \
	hmac.c \
	md4.c \
	pbkdf2.c \
	rc4.c

INCLUDES = -I$(top_srcdir)/libpurple

AM_CPPFLAGS = \
	$(INTGG_CFLAGS) \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GNUTLS_CFLAGS) \
	$(NSS_CFLAGS)
