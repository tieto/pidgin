EXTRA_DIST = PROTOCOL md5.h

pkgdir = $(libdir)/gaim

MSNSOURCES = \
	ft.c \
	msg.c \
	msg.h \
	msn.c \
	msn.h \
	switchboard.c \
	switchboard.h

AM_CFLAGS = $(st)

libmsn_la_LDFLAGS = -module -avoid-version

if STATIC_MSN

st = -DSTATIC
noinst_LIBRARIES = libmsn.a
libmsn_a_SOURCES = $(MSNSOURCES)
libmsn_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES   = libmsn.la
libmsn_la_SOURCES = $(MSNSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS)
