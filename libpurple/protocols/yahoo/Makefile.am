EXTRA_DIST = \
	Makefile.mingw

pkgdir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

YAHOOSOURCES = \
	libymsg.c \
	libymsg.h \
	util.c \
	yahoochat.h \
	yahoochat.c \
	yahoo_aliases.c \
	yahoo_aliases.h \
	yahoo_doodle.h \
	yahoo_doodle.c \
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

libyahoo_la_LDFLAGS   = -module @PLUGIN_LDFLAGS@
libyahoojp_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

if STATIC_YAHOO

st = -DPURPLE_STATIC_PRPL
noinst_LTLIBRARIES  = libymsg.la
libymsg_la_SOURCES = $(YAHOOSOURCES) libyahoo.c libyahoojp.c
libymsg_la_CFLAGS  = $(AM_CFLAGS)
libymsg_la_LDFLAGS = @PLUGIN_LDFLAGS@

else

st =
pkg_LTLIBRARIES     = libymsg.la libyahoo.la libyahoojp.la

libymsg_la_SOURCES = $(YAHOOSOURCES)
libymsg_la_LIBADD  = @PURPLE_LIBS@
libymsg_la_LDFLAGS = @PLUGIN_LDFLAGS@

libyahoo_la_SOURCES = libyahoo.c
libyahoo_la_LIBADD = libymsg.la

libyahoojp_la_SOURCES = libyahoojp.c
libyahoojp_la_LIBADD = libymsg.la

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
