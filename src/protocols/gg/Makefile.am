EXTRA_DIST = \
		protocol.txt \
		Makefile.mingw

pkgdir = $(libdir)/gaim

GGSOURCES = \
	libgg.c \
	libgg.h \
	common.c \
	gg.c

AM_CFLAGS = $(st)

libgg_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

if STATIC_GG

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libgg.a

libgg_a_SOURCES = $(GGSOURCES)
libgg_a_CFLAGS  = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libgg.la

libgg_la_SOURCES = $(GGSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS)
