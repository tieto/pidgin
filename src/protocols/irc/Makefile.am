EXTRA_DIST = PROTOCOL

pkgdir = $(libdir)/gaim

IRCSOURCES = irc.c

AM_CFLAGS = $(st)

libirc_la_LDFLAGS = -module -avoid-version

if STATIC_IRC

st = -DSTATIC
noinst_LIBRARIES = libirc.a
pkg_LTLIBRARIES =

libirc_a_SOURCES = $(IRCSOURCES)
libirc_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libirc.la
noinst_LIBRARIES =

libirc_la_SOURCES = $(IRCSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS)
