pkgdir = $(libdir)/gaim

CFLAGS += -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
libnapster_la_LDFLAGS = -module -avoid-version

if STATIC_NAPSTER

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = libnapster.a

libnapster_a_SOURCES = napster.c

libnapster_a_CFLAGS = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libnapster.la
noinst_LIBRARIES =

libnapster_la_SOURCES = napster.c

endif
