EXTRA_DIST = protocol.txt

pkgdir = $(libdir)/gaim

CFLAGS += -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
libgg_la_LDFLAGS = -module -avoid-version

if STATIC_GG

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = libgg.a

libgg_a_SOURCES = libgg.c \
		  libgg.h \
		  common.c \
		  gg.c

libgg_a_CFLAGS = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libgg.la
noinst_LIBRARIES =

libgg_la_SOURCES = libgg.c \
		   libgg.h \
		   common.c \
		   gg.c

endif
