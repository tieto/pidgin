EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

TREPIASOURCES = \
	profile.c \
	profile.h \
	trepia.c

AM_CFLAGS = $(st)

libtrepia_la_LDFLAGS = -module -avoid-version

if STATIC_TREPIA

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libtrepia.a
libtrepia_a_SOURCES = $(TREPIASOURCES)
libtrepia_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES   = libtrepia.la
libtrepia_la_SOURCES = $(TREPIASOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
