#EXTRA_DIST = \
#		Makefile.mingw

plugindir = $(libdir)/gaim

musicmessaging_la_LDFLAGS = -module -avoid-version $(GTK_LIBS)

if PLUGINS

plugin_LTLIBRARIES = musicmessaging.la

ticker_la_SOURCES = \
	musicmessaging.c

endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS)
