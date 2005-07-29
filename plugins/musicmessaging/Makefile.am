EXTRA_DIST = \
	music.png

plugindir = $(libdir)/gaim

musicmessaging_la_LDFLAGS = -module -avoid-version $(GTK_LIBS)

if PLUGINS

musicmessagingpixdir = $(DATADIR)/pixmaps/gaim/buttons
musicmessagingpix_DATA = music.png

plugin_LTLIBRARIES = musicmessaging.la

musicmessaging_la_SOURCES = \
	musicmessaging.c

endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS)
