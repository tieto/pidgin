EXTRA_DIST = README TODO Makefile.mingw

pkgdir = $(libdir)/libpurple

SILCSOURCES = silc.c silcpurple.h buddy.c chat.c ft.c ops.c pk.c util.c wb.c wb.h

AM_CFLAGS = $(st)

libsilcpurple_la_LDFLAGS = -module -avoid-version

if STATIC_SILC

st = -DPURPLE_STATIC_PRPL $(SILC_CFLAGS)
noinst_LIBRARIES = libsilcpurple.a
pkg_LTLIBRARIES =

libsilcpurple_a_SOURCES = $(SILCSOURCES)
libsilcpurple_a_CFLAGS  = $(AM_CFLAGS)
libsilcpurple_a_LIBADD  = $(SILC_LIBS)

else

st = $(SILC_CFLAGS)
pkg_LTLIBRARIES = libsilcpurple.la
noinst_LIBRARIES =

libsilcpurple_la_SOURCES = $(SILCSOURCES)
libsilcpurple_la_LIBADD  = $(GLIB_LIBS) $(SILC_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
