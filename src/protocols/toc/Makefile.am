EXTRA_DIST = PROTOCOL

pkgdir = $(libdir)/gaim

CFLAGS += -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
libtoc_la_LDFLAGS = -avoid-version

if STATIC_TOC

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = libtoc.a

libtoc_a_SOURCES = toc.c

else

st =
pkg_LTLIBRARIES = libtoc.la
noinst_LIBRARIES =

libtoc_la_SOURCES = toc.c

endif
