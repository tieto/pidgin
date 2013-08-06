noinst_LTLIBRARIES=libpurple-ciphers.la

if USE_NSS
AES_SOURCE = aescipher.c
endif
if USE_GNUTLS
AES_SOURCE = aescipher.c
endif

libpurple_ciphers_la_SOURCES=\
	$(AES_SOURCE) \
	descipher.c \
	des3cipher.c \
	hmaccipher.c \
	md4hash.c \
	md5hash.c \
	pbkdf2cipher.c \
	rc4cipher.c \
	sha1hash.c \
	sha256hash.c

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	$(INTGG_CFLAGS) \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GNUTLS_CFLAGS) \
	$(NSS_CFLAGS)
