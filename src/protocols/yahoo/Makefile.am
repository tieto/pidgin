EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

YAHOOSOURCES = \
	crypt.c \
	yahoo.h \
	yahoo_auth.c \
	yahoo_auth.h \
	yahoochat.h \
	yahoo.c \
	yahoochat.c \
	util.c \
	yahoo_filexfer.h \
	yahoo_filexfer.c \
	yahoo_friend.h \
	yahoo_friend.c \
	yahoo_packet.h \
	yahoo_packet.c \
	yahoo_picture.c \
	yahoo_picture.h \
	yahoo_profile.c \
	ycht.c \
	ycht.h

AM_CFLAGS = $(st)

libyahoo_la_LDFLAGS = -module -avoid-version

if STATIC_YAHOO

st = -DGAIM_STATIC_PRPL
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
