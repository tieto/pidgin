EXTRA_DIST = \
	mdns_win32.c \
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
	mdns_common.c \
	mdns_common.h \
	mdns_interface.h \
	mdns_types.h \
	parser.c \
	parser.h

if MDNS_AVAHI
  BONJOURSOURCES += mdns_avahi.c
else
if MDNS_HOWL
    BONJOURSOURCES += mdns_howl.c
endif
endif

AM_CFLAGS = $(st)

libbonjour_la_LDFLAGS = -module -avoid-version

if STATIC_BONJOUR

st = -DPURPLE_STATIC_PRPL
noinst_LIBRARIES     = libbonjour.a
libbonjour_a_SOURCES = $(BONJOURSOURCES)
libbonjour_a_CFLAGS  = $(AM_CFLAGS)
libbonjour_a_LIBADD  =

if MDNS_AVAHI
  libbonjour_a_LIBADD  += $(AVAHI_LIBS)
else
if MDNS_HOWL
    libbonjour_a_LIBADD  += $(HOWL_LIBS)
endif
endif

else

st =
pkg_LTLIBRARIES       = libbonjour.la
libbonjour_la_SOURCES = $(BONJOURSOURCES)
libbonjour_la_LIBADD  = $(GLIB_LIBS) $(LIBXML_LIBS)
if MDNS_AVAHI
  libbonjour_la_LIBADD  += $(AVAHI_LIBS)
else
if MDNS_HOWL
    libbonjour_la_LIBADD  += $(HOWL_LIBS)
endif
endif

endif


AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(LIBXML_CFLAGS) \
	$(FARSIGHT_CFLAGS)
if MDNS_AVAHI
  AM_CPPFLAGS += $(AVAHI_CFLAGS)
else
if MDNS_HOWL
    AM_CPPFLAGS += $(HOWL_CFLAGS)
endif
endif

