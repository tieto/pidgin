EXTRA_DIST = Makefile.mingw

pkgdir = $(libdir)/gaim

GGSOURCES = \
	lib/common.c \
	lib/dcc.c \
	lib/events.c \
	lib/http.c \
	lib/libgadu.c \
	lib/obsolete.c \
	lib/pubdir.c \
	lib/pubdir50.c \
	lib/compat.h \
	lib/libgadu.h \
	gg.c

AM_CFLAGS = $(st)

libgg_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

if STATIC_GG

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libgg.a
pkg_LTLIBRARIES =

libgg_a_SOURCES = $(GGSOURCES)
libgg_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libgg.la
noinst_LIBRARIES =

libgg_la_SOURCES = $(GGSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/protocols/gg/lib \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS)

