EXTRA_DIST = README TODO ChangeLog myspace.png Makefile.mingw

pkgdir = $(libdir)/libpurple

SOURCES = myspace.c

AM_CFLAGS = $(st)

libirc_la_LDFLAGS = -module -avoid-version

if STATIC_MYSPACE

st = -DPURPLE_STATIC_PRPL
noinst_LIBRARIES = libirc.a
pkg_LTLIBRARIES =

libirc_a_SOURCES = $(SOURCES)
libirc_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libirc.la
noinst_LIBRARIES =

libirc_la_SOURCES = $(SOURCES)
libirc_la_LIBADD  = $(GLIB_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
