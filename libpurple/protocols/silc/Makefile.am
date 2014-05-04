EXTRA_DIST = \
	Makefile.mingw \
	README \
	TODO

pkgdir = @PURPLE_PLUGINDIR@

SILCSOURCES = \
	buddy.c \
	chat.c \
	ft.c \
	ops.c \
	pk.c \
	silc.c \
	silcpurple.h \
	util.c \
	wb.c \
	wb.h

AM_CFLAGS = $(st)

libsilcpurple_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

if STATIC_SILC

st = -DPURPLE_STATIC_PRPL $(SILC_CFLAGS)
noinst_LTLIBRARIES       = libsilcpurple.la
libsilcpurple_la_SOURCES = $(SILCSOURCES)
libsilcpurple_la_CFLAGS  = $(AM_CFLAGS)

else

st = $(SILC_CFLAGS)
pkg_LTLIBRARIES          = libsilcpurple.la
libsilcpurple_la_SOURCES = $(SILCSOURCES)

endif

libsilcpurple_la_LIBADD  = @PURPLE_LIBS@ $(SILC_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(SILC_CFLAGS)
