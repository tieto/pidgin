EXTRA_DIST = \
	Makefile.mingw \
	lib/common.c \
	lib/compat.h \
	lib/COPYING \
	lib/dcc.c \
	lib/events.c \
	lib/http.c \
	lib/libgadu.c \
	lib/libgadu-config.h \
	lib/libgadu.h \
	lib/obsolete.c \
	lib/pubdir50.c \
	lib/pubdir.c

pkgdir = $(libdir)/libpurple

if USE_INTERNAL_LIBGADU
INTGGSOURCES = \
	lib/common.c \
	lib/compat.h \
	lib/dcc.c \
	lib/events.c \
	lib/http.c \
	lib/libgadu.c \
	lib/libgadu-config.h \
	lib/libgadu.h \
	lib/obsolete.c \
	lib/pubdir50.c \
	lib/pubdir.c

INTGG_CFLAGS = -I$(top_srcdir)/libpurple/protocols/gg/lib
endif

GGSOURCES = \
	$(INTGGSOURCES) \
	gg-utils.h \
	gg-utils.c \
	confer.h \
	confer.c \
	search.h \
	search.c \
	buddylist.h \
	buddylist.c \
	gg.h \
	gg.c

AM_CFLAGS = $(st)

libgg_la_LDFLAGS = -module -avoid-version

if STATIC_GG

st = -DGAIM_STATIC_PRPL $(GADU_CFLAGS)
noinst_LIBRARIES = libgg.a
pkg_LTLIBRARIES =

libgg_a_SOURCES = $(GGSOURCES)
libgg_a_CFLAGS  = $(AM_CFLAGS)
libgg_a_LIBADD  = $(GADU_LIBS)

else

st = $(GADU_CFLAGS)
pkg_LTLIBRARIES = libgg.la
noinst_LIBRARIES =

libgg_la_SOURCES = $(GGSOURCES)
libgg_la_LIBADD  = $(GLIB_LIBS) $(GADU_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	$(INTGG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)

