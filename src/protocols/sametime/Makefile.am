SUBDIRS = meanwhile

EXTRA_DIST = Makefile.mingw


pkgdir = $(libdir)/gaim


noinst_HEADERS = sametime.h

SAMETIMESOURCES = sametime.c


if STATIC_SAMETIME

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES   = libsametime.a
libsametime_a_SOURCES = $(SAMETIMESOURCES)
libsametime_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES     = libsametime.la
libsametime_la_SOURCES = $(SAMETIMESOURCES)


endif


libsametime_la_LDFLAGS = -module -avoid-version -no-undefined
libsametime_la_LIBADD = $(GLIB_LIBS) meanwhile/libmeanwhile.la


AM_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	-I$(top_srcdir)/src \
	-Imeanwhile


AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"meanwhile\" \
	$(st)

