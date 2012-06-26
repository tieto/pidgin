pkgdir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

EXTRA_DIST = \
	Makefile.mingw \
	lib/COPYING

if USE_INTERNAL_LIBGADU
INTGGSOURCES = \
	lib/common.c \
	lib/compat.h \
	lib/config.h \
	lib/dcc.c \
	lib/dcc7.c \
	lib/debug.c \
	lib/debug.h \
	lib/deflate.c \
	lib/deflate.h \
	lib/encoding.c \
	lib/encoding.h \
	lib/events.c \
	lib/handlers.c \
	lib/http.c \
	lib/internal.h \
	lib/libgadu.c \
	lib/libgadu.h \
	lib/message.c \
	lib/message.h \
	lib/obsolete.c \
	lib/protocol.h \
	lib/pubdir.c \
	lib/pubdir50.c \
	lib/resolver.c \
	lib/resolver.h \
	lib/session.h \
	lib/sha1.c

INTGG_CFLAGS = -I$(top_srcdir)/libpurple/protocols/gg/lib -DGG_IGNORE_DEPRECATED
endif

if USE_GNUTLS
GADU_LIBS += $(GNUTLS_LIBS)
GADU_CFLAGS += $(GNUTLS_CFLAGS)
endif

GGSOURCES = \
	$(INTGGSOURCES) \
	gg-utils.h \
	gg-utils.c \
	confer.h \
	confer.c \
	search.h \
	search.c \
	buddylist.h \
	buddylist.c \
	gg.h \
	gg.c \
	resolver-purple.h \
	resolver-purple.c \
	image.h \
	image.c

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
	-Wall -Wextra -Werror \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(INTGG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
