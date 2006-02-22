EXTRA_DIST = Makefile.mingw

pkgdir = $(libdir)/gaim

GGSOURCES = \
	utils.h \
	utils.c \
	confer.h \
	confer.c \
	search.h \
	search.c \
	buddylist.h \
	buddylist.c \
	gg.h \
	gg.c

AM_CFLAGS = $(st)

libgg_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

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
libgg_la_LIBADD  = $(GADU_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)

