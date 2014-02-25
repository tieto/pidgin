#V=0
#CFLAGS = -g -O0
GADU_EXTRA =
#GADU_EXTRA += -Werror

pkgdir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

EXTRA_DIST = \
	account.c \
	account.h \
	Makefile.mingw \
	lib/COPYING

if USE_INTERNAL_LIBGADU
INTGGSOURCES = \
	lib/common.c \
	lib/config.h \
	lib/dcc.c \
	lib/dcc7.c \
	lib/debug.c \
	lib/debug.h \
	lib/deflate.c \
	lib/deflate.h \
	lib/encoding.c \
	lib/encoding.h \
	lib/endian.c \
	lib/events.c \
	lib/fileio.h \
	lib/handlers.c \
	lib/http.c \
	lib/internal.h \
	lib/libgadu.c \
	lib/libgadu.h \
	lib/message.c \
	lib/message.h \
	lib/network.c \
	lib/network.h \
	lib/obsolete.c \
	lib/packets.pb-c.c \
	lib/packets.pb-c.h \
	lib/protobuf-c.c \
	lib/protobuf-c.h \
	lib/protobuf.c \
	lib/protobuf.h \
	lib/protocol.h \
	lib/pubdir.c \
	lib/pubdir50.c \
	lib/resolver.c \
	lib/resolver.h \
	lib/session.h \
	lib/strman.h \
	lib/sha1.c \
	lib/tvbuff.c \
	lib/tvbuff.h \
	lib/tvbuilder.c \
	lib/tvbuilder.h


INTGG_CFLAGS = -I$(top_srcdir)/libpurple/protocols/gg/lib \
	$(ZLIB_CFLAGS) \
	-DGG_IGNORE_DEPRECATED
GADU_LIBS += $(ZLIB_LIBS)
endif

if USE_GNUTLS
GADU_LIBS += $(GNUTLS_LIBS)
GADU_CFLAGS += $(GNUTLS_CFLAGS)
endif

GADU_LIBS += $(JSON_LIBS)

GGSOURCES = \
	$(INTGGSOURCES) \
	avatar.c \
	avatar.h \
	blist.c \
	blist.h \
	chat.c \
	chat.h \
	deprecated.c \
	deprecated.h \
	edisc.c \
	edisc.h \
	gg.c \
	gg.h \
	html.c \
	html.h \
	image.c \
	image.h \
	keymapper.c \
	keymapper.h \
	libgadu-events.c \
	libgadu-events.h \
	libgaduw.c \
	libgaduw.h \
	message-prpl.c \
	message-prpl.h \
	multilogon.c \
	multilogon.h \
	pubdir-prpl.c \
	pubdir-prpl.h \
	purplew.c \
	purplew.h \
	resolver-purple.c \
	resolver-purple.h \
	roster.c \
	roster.h \
	servconn.c \
	servconn.h \
	status.c \
	status.h \
	tcpsocket.c \
	tcpsocket.h \
	utils.c \
	utils.h \
	validator.c \
	validator.h \
	xml.c \
	xml.h \
	oauth/oauth.c \
	oauth/oauth.h \
	oauth/oauth-parameter.c \
	oauth/oauth-parameter.h \
	oauth/oauth-purple.c \
	oauth/oauth-purple.h

AM_CFLAGS = $(st)

libgg_la_LDFLAGS = -module -avoid-version

if STATIC_GG

st = -DPURPLE_STATIC_PRPL $(GADU_CFLAGS)
noinst_LTLIBRARIES = libgg.la
libgg_la_SOURCES = $(GGSOURCES)
libgg_la_CFLAGS  = $(AM_CFLAGS)
libgg_la_LIBADD  = $(GADU_LIBS)

else

st = $(GADU_CFLAGS)
pkg_LTLIBRARIES = libgg.la
libgg_la_SOURCES = $(GGSOURCES)
libgg_la_LIBADD  = $(GLIB_LIBS) $(GADU_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(INTGG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(JSON_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(GADU_EXTRA)
