EXTRA_DIST = AUTHORS

pkgdir = $(libdir)/gaim

CFLAGS += -I\$(top_srcdir)/src $(st) $(DEBUG_CFLAGS)
libyahoo_la_LDFLAGS = -avoid-version

if STATIC_YAHOO

st = -DSTATIC
pkg_LTLIBRARIES =
noinst_LIBRARIES = libyahoo.a

libyahoo_a_SOURCES = \
	buddy.c conn.c internal.h login.c misc.c outgoing.c rxhandlers.c yay.c yay.h

else

st =
pkg_LTLIBRARIES = libyahoo.la
noinst_LIBRARIES =

libyahoo_la_SOURCES = \
	buddy.c conn.c internal.h login.c misc.c outgoing.c rxhandlers.c yay.c yay.h

endif
