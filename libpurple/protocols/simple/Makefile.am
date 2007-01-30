EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/libpurple

SIMPLESOURCES = \
	simple.c \
	simple.h \
	sipmsg.c \
	sipmsg.h

AM_CFLAGS = $(st) 

libsimple_la_LDFLAGS = -module -avoid-version

if STATIC_MSN

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libsimple.a
libsimple_a_SOURCES = $(SIMPLESOURCES)
libsimple_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES   = libsimple.la
libsimple_la_SOURCES = $(SIMPLESOURCES)
libsimple_la_LIBADD  = $(GLIB_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
