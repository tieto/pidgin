EXTRA_DIST = rfc1459.txt

pkgdir = $(libdir)/gaim

CFLAGS += -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
libirc_la_LDFLAGS = -avoid-version

if STATIC_IRC

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = libirc.a

libirc_a_SOURCES = irc.c

else

st =
pkg_LTLIBRARIES = libirc.la
noinst_LIBRARIES =

libirc_la_SOURCES = irc.c

endif
