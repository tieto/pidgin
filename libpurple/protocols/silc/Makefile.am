EXTRA_DIST = README TODO Makefile.mingw

pkgdir = $(libdir)/libpurple

SILCSOURCES = silc.c silcpurple.h buddy.c chat.c ft.c ops.c pk.c util.c wb.c wb.h

AM_CFLAGS = $(st)

libsilcgaim_la_LDFLAGS = -module -avoid-version

if STATIC_SILC

st = -DGAIM_STATIC_PRPL $(SILC_CFLAGS)
noinst_LIBRARIES = libsilcgaim.a
pkg_LTLIBRARIES =

libsilcgaim_a_SOURCES = $(SILCSOURCES)
libsilcgaim_a_CFLAGS  = $(AM_CFLAGS)
libsilcgaim_a_LIBADD  = $(SILC_LIBS)

else

st = $(SILC_CFLAGS)
pkg_LTLIBRARIES = libsilcgaim.la
noinst_LIBRARIES =

libsilcgaim_la_SOURCES = $(SILCSOURCES)
libsilcgaim_la_LIBADD  = $(GLIB_LIBS) $(SILC_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
