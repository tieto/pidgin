EXTRA_DIST = \
		Makefile.mingw

plugindir = $(libdir)/pidgin

ticker_la_LDFLAGS = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = ticker.la

ticker_la_SOURCES = \
	gtkticker.c \
	gtkticker.h \
	ticker.c

ticker_la_LIBADD = $(GTK_LIBS)

endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPLUGIN_CFLAGS)
