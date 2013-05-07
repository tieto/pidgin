noinst_LTLIBRARIES=libpurple-ciphers.la
# XXX: cipher.lo won't be updated after a change in cipher files

libpurple_ciphers_la_SOURCES=\
	des.c \
	gchecksum.c \
	hmac.c \
	md4.c \
	pbkdf2.c \
	rc4.c

noinst_HEADERS =\
	ciphers.h

INCLUDES = -I$(top_srcdir)/libpurple

AM_CPPFLAGS = \
	$(INTGG_CFLAGS) \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
