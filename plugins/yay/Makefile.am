CFLAGS = -g -O2 -I../../src -I../../libfaim/faim -I../../libfaim $(GTK_CFLAGS)
LIBS = $(GTK_LIBS)

pkgdir = $(libdir)/gaim
pkg_LTLIBRARIES = libyahoo.la

libyahoo_la_SOURCES = libyahoo.c memtok.c yay.c

EXTRA_DIST = libyahoo-proto.h libyahoo.h memtok.h \
		AUTHORS COPYING ChangeLog INSTALL NEWS NOTES README
