EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

YAHOOSOURCES = \
	crypt.c \
	md5.h \
	yahoo.h \
	yahoochat.h \
	yahoo.c \
	yahoochat.c \
	util.c

AM_CFLAGS = $(st)

libyahoo_la_LDFLAGS = -module -avoid-version

if STATIC_YAHOO

st = -DSTATIC
noinst_LIBRARIES   = libyahoo.a
libyahoo_a_SOURCES = $(YAHOOSOURCES)
libyahoo_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES     = libyahoo.la
libyahoo_la_SOURCES = $(YAHOOSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
