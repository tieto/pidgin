EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

NAPSTERSOURCES = napster.c

AM_CFLAGS = $(st)

libnapster_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

st =
pkg_LTLIBRARIES       = libnapster.la
libnapster_la_SOURCES = $(NAPSTERSOURCES)

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
