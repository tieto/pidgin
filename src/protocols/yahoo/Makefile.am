pkgdir = $(libdir)/gaim

YAHOOSOURCES = \
	crypt.c \
	md5.h \
	yahoo.c

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
	$(DEBUG_CFLAGS)
