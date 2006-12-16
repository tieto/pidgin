EXTRA_DIST = README TODO Makefile.mingw

pkgdir = $(libdir)/gaim

SILCSOURCES = silc.c silcgaim.h buddy.c chat.c ft.c ops.c pk.c util.c wb.c wb.h

AM_CFLAGS = $(st)

libsilcgaim_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

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
libsilcgaim_la_LIBADD  = $(SILC_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/core \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
