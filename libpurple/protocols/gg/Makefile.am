EXTRA_DIST = \
	Makefile.mingw \
	lib/COPYING \
	lib/common.c \
	lib/config.h \
	lib/dcc7.c \
	lib/dcc.c \
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
	lib/protobuf.c \
	lib/protobuf-c.c \
	lib/protobuf-c.h \
	lib/protobuf.h \
	lib/protocol.h \
	lib/pubdir50.c \
	lib/pubdir.c \
	lib/resolver.c \
	lib/resolver.h \
	lib/session.h \
	lib/sha1.c \
	lib/strman.h \
	lib/tvbuff.c \
	lib/tvbuff.h \
	lib/tvbuilder.c \
	lib/tvbuilder.h

pkgdir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

if ! HAVE_LIBGADU
INTGG_SOURCES = \
	lib/common.c \
	lib/config.h \
	lib/dcc7.c \
	lib/dcc.c \
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
	lib/protobuf.c \
	lib/protobuf-c.c \
	lib/protobuf-c.h \
	lib/protobuf.h \
	lib/protocol.h \
	lib/pubdir50.c \
	lib/pubdir.c \
	lib/resolver.c \
	lib/resolver.h \
	lib/session.h \
	lib/sha1.c \
	lib/strman.h \
	lib/tvbuff.c \
	lib/tvbuff.h \
	lib/tvbuilder.c \
	lib/tvbuilder.h

INTGG_LIBS = $(ZLIB_LIBS) $(GNUTLS_LIBS)
INTGG_CFLAGS = \
	-I$(top_srcdir)/libpurple/protocols/gg/lib \
	$(ZLIB_CFLAGS) \
	$(GNUTLS_CFLAGS) \
	-DGG_IGNORE_DEPRECATED

endif

GGSOURCES = \
	$(INTGG_SOURCES) \
	gg-utils.h \
	gg-utils.c \
	confer.h \
	confer.c \
	search.h \
	search.c \
	buddylist.h \
	buddylist.c \
	gg.h \
	gg.c

AM_CFLAGS = $(st)

libgg_la_LDFLAGS = -module -avoid-version

if STATIC_GG

st = -DPURPLE_STATIC_PRPL
noinst_LTLIBRARIES = libgg.la
libgg_la_SOURCES = $(GGSOURCES)
libgg_la_CFLAGS  = $(AM_CFLAGS)
libgg_la_LIBADD  = $(LIBGADU_LIBS) $(INTGG_LIBS)

else

st =
pkg_LTLIBRARIES = libgg.la
libgg_la_SOURCES = $(GGSOURCES)
libgg_la_LIBADD  = $(GLIB_LIBS) $(LIBGADU_LIBS) $(INTGG_LIBS)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(LIBGADU_CFLAGS) \
	$(INTGG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
