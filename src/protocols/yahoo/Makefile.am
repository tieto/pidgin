EXTRA_DIST = md5.h

pkgdir = $(libdir)/gaim

CFLAGS += -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
libyahoo_la_LDFLAGS = -module -avoid-version

if STATIC_YAHOO

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = libyahoo.a

libyahoo_a_SOURCES = crypt.c yahoo.c

libyahoo_a_CFLAGS = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libyahoo.la
noinst_LIBRARIES =

libyahoo_la_SOURCES = crypt.c yahoo.c

endif
