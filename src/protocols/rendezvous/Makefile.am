EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

RENDEZVOUSSOURCES = \
	direct.c \
	direct.h \
	mdns.c \
	mdns.h \
	mdns_cache.c \
	mdns_cache.h \
	rendezvous.c

AM_CFLAGS = $(st)

librendezvous_la_LDFLAGS = -module -avoid-version

if STATIC_RENDEZVOUS

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES     = librendezvous.a
librendezvous_a_SOURCES = $(RENDEZVOUSSOURCES)
librendezvous_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES       = librendezvous.la
librendezvous_la_SOURCES = $(RENDEZVOUSSOURCES)

endif


AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS)
