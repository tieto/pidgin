EXTRA_DIST = PROTOCOL Makefile.mingw

pkgdir = $(libdir)/gaim

IRCSOURCES = irc.c parse.c cmds.c msgs.c irc.h dcc_send.c

AM_CFLAGS = $(st)

libirc_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

if STATIC_IRC

st = -DGAIM_STATIC_PRPL
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
	-I$(top_srcdir)/core \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
