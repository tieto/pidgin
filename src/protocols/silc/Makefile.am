EXTRA_DIST = README TODO

pkgdir = $(libdir)/gaim

SILCSOURCES = silc.c silcgaim.h buddy.c chat.c ft.c ops.c pk.c util.c

AM_CFLAGS = $(st)

libsilc_la_LDFLAGS = -module -avoid-version

if STATIC_IRC

st = -DGAIM_STATIC_PRPL $(SILC_INCLUDES)
noinst_LIBRARIES = libsilc.a
pkg_LTLIBRARIES =

libsilc_a_SOURCES = $(SILCSOURCES)
libsilc_a_CFLAGS  = $(AM_CFLAGS)
libsilc_a_LIBADD  = $(SILC_LIBS)

else

st = $(SILC_INCLUDES)
pkg_LTLIBRARIES = libsilc.la
noinst_LIBRARIES =

libsilc_la_SOURCES = $(SILCSOURCES)
libsilc_la_LIBADD  = $(SILC_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
