EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

BONJOURSOURCES = \
	bonjour.c \
	bonjour.h \
	buddy.c \
	buddy.h \
	dns_sd.c \
	dns_sd.h \
	jabber.c \
	jabber.h

AM_CFLAGS = $(st)

libbonjour_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS) $(HOWL_LIBS)

if STATIC_BONJOUR

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES     = libbonjour.a
libbonjour_a_SOURCES = $(BONJOURSOURCES)
libbonjour_a_CFLAGS  = $(AM_CFLAGS)
libbonjour_a_LIBADD  = $(HOWL_LIBS)

else

st =
pkg_LTLIBRARIES       = libbonjour.la
libbonjour_la_SOURCES = $(BONJOURSOURCES)

endif


AM_CPPFLAGS = \
	-I$(top_srcdir)/core \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(HOWL_CFLAGS)
