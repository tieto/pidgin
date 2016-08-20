EXTRA_DIST = \
	mdns_avahi.c \
	mdns_win32.c \
	mdns_dns_sd.c \
	dns_sd_proxy.c \
	dns_sd_proxy.h \
	Makefile.mingw

pkgdir = @PURPLE_PLUGINDIR@

BONJOURSOURCES = \
	bonjour.c \
	bonjour.h \
	buddy.c \
	buddy.h \
	jabber.c \
	jabber.h \
	mdns_common.c \
	mdns_common.h \
	mdns_interface.h \
	mdns_types.h \
	parser.c \
	parser.h \
	bonjour_ft.c \
	bonjour_ft.h 

if IS_WIN32
BONJOURSOURCES += \
	dns_sd_proxy.c \
	mdns_win32.c
else
BONJOURSOURCES += \
	mdns_avahi.c
endif

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

if IS_WIN32
libbonjour_la_LIBADD += -lnetapi32
endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(AVAHI_CFLAGS)
