EXTRA_DIST = \
		Makefile.mingw

plugindir = $(libdir)/gaim

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
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/libgaim \
	-I$(top_srcdir)/gtk \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS)
