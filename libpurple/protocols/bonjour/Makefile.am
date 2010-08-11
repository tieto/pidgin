EXTRA_DIST = \
		mdns_win32.c \
		mdns_win32.h \
		Makefile.mingw

pkgdir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

BONJOURSOURCES = \
	bonjour.c \
	bonjour.h \
	buddy.c \
	buddy.h \
	dns_sd_proxy.h \
	jabber.c \
	jabber.h \
	mdns_common.c \
	mdns_common.h \
	mdns_howl.c \
	mdns_howl.h \
	mdns_types.h

AM_CFLAGS = $(st) -DUSE_BONJOUR_HOWL

libbonjour_la_LDFLAGS = -module -avoid-version

if STATIC_BONJOUR

st = -DPURPLE_STATIC_PRPL
noinst_LIBRARIES     = libbonjour.a
libbonjour_a_SOURCES = $(BONJOURSOURCES)
libbonjour_a_CFLAGS  = $(AM_CFLAGS)
libbonjour_a_LIBADD  = $(HOWL_LIBS)

else

st =
pkg_LTLIBRARIES       = libbonjour.la
libbonjour_la_SOURCES = $(BONJOURSOURCES)
libbonjour_la_LIBADD   = $(GLIB_LIBS) $(HOWL_LIBS)

endif


AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(HOWL_CFLAGS)
