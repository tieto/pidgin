EXTRA_DIST = \
	Makefile.mingw

pkgdir = @PURPLE_PLUGINDIR@

YAHOOSOURCES = \
	util.c \
	yahoochat.h \
	yahoochat.c \
	yahoo.c \
	yahoo.h \
	yahoojp.c \
	yahoojp.h \
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
	ycht.h \
	ymsg.c \
	ymsg.h

AM_CFLAGS = $(st)

libyahoo_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

if STATIC_YAHOO

st = -DPURPLE_STATIC_PRPL
noinst_LTLIBRARIES  = libyahoo.la
libyahoo_la_SOURCES = $(YAHOOSOURCES)
libyahoo_la_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES     = libyahoo.la
libyahoo_la_SOURCES = $(YAHOOSOURCES)
libyahoo_la_LIBADD  = @PURPLE_LIBS@

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(DEBUG_CFLAGS)
