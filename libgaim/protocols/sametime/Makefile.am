
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


libsametime_la_LDFLAGS = -module -avoid-version
libsametime_la_LIBADD = $(GLIB_LIBS) $(MEANWHILE_LIBS)


AM_CFLAGS = \
	$(GLIB_CFLAGS) $(MEANWHILE_CFLAGS) \
	$(DEBUG_CFLAGS) \
	-I$(top_srcdir)/libgaim


AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"sametime\" \
	$(st)

