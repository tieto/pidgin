EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

NAPSTERSOURCES = napster.c

AM_CFLAGS = $(st)

libnapster_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

if STATIC_NAPSTER

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES     = libnapster.a
libnapster_a_SOURCES = $(NAPSTERSOURCES)
libnapster_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES       = libnapster.la
libnapster_la_SOURCES = $(NAPSTERSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
