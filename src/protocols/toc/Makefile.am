EXTRA_DIST = \
		PROTOCOL \
		Makefile.mingw

pkgdir = $(libdir)/gaim

TOCSOURCES = toc.c

AM_CFLAGS = $(st)

libtoc_la_LDFLAGS = -module -avoid-version

if STATIC_TOC

st = -DSTATIC
noinst_LIBRARIES = libtoc.a
libtoc_a_SOURCES = $(TOCSOURCES)
libtoc_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES   = libtoc.la
libtoc_la_SOURCES = $(TOCSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS)
