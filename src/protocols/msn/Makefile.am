EXTRA_DIST = PROTOCOL md5.h

pkgdir = $(libdir)/gaim

CFLAGS += -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
libmsn_la_LDFLAGS = -avoid-version

if STATIC_MSN

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = libmsn.a

libmsn_a_SOURCES = msn.c md5.c

else

st =
pkg_LTLIBRARIES = libmsn.la
noinst_LIBRARIES =

libmsn_la_SOURCES = msn.c md5.c

endif
