EXTRA_DIST = \
	mdns_win32.c \
	dns_sd_proxy.c \
	dns_sd_proxy.h \
	Makefile.mingw

pkgdir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

BONJOURSOURCES = \
	bonjour.c \
	bonjour.h \
	buddy.c \
	buddy.h \
	jabber.c \
	jabber.h \
	mdns_avahi.c \
	mdns_common.c \
	mdns_common.h \
	mdns_interface.h \
	mdns_types.h \
	parser.c \
	parser.h \
	bonjour_ft.c \
	bonjour_ft.h 

AM_CFLAGS = $(st)

libbonjour_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

if STATIC_BONJOUR

st = -DPURPLE_STATIC_PRPL
noinst_LTLIBRARIES    = libbonjour.la
libbonjour_la_SOURCES = $(BONJOURSOURCES)
libbonjour_la_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES       = libbonjour.la
libbonjour_la_SOURCES = $(BONJOURSOURCES)

endif

libbonjour_la_LIBADD  = @PURPLE_LIBS@ $(LIBXML_LIBS) $(AVAHI_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(AVAHI_CFLAGS)
