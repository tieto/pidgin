EXTRA_DIST = \
		PROTOCOL \
		Makefile.mingw

pkgdir = $(libdir)/libpurple

TOCSOURCES = toc.c

AM_CFLAGS = $(st)

libtoc_la_LDFLAGS = -module -avoid-version

if STATIC_TOC

st = -DPURPLE_STATIC_PRPL
noinst_LIBRARIES = libtoc.a
libtoc_a_SOURCES = $(TOCSOURCES)
libtoc_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES   = libtoc.la
libtoc_la_SOURCES = $(TOCSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
