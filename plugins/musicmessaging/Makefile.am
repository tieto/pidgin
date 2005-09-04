EXTRA_DIST = \
	music.png

plugindir = $(libdir)/gaim

musicmessaging_la_LDFLAGS = -module -avoid-version $(GTK_LIBS) $(DBUS_LIBS)

if PLUGINS

musicmessagingpixdir = $(datadir)/pixmaps/gaim/buttons
musicmessagingpix_DATA = music.png

plugin_LTLIBRARIES = musicmessaging.la

musicmessaging_la_SOURCES = \
	musicmessaging.c



CLEANFILES              = music-messaging-bindings.c

.PHONY: always

$(top_builddir)/src/dbus-types.h: always
	cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

music-messaging-bindings.c: ../../src/dbus-analyze-functions.py $(musicmessaging_la_SOURCES) $(musicmessaging_la_HEADERS)
	cat $(musicmessaging_la_SOURCES) $(musicmessaging_la_HEADERS) | \
	$(PYTHON) ../../src/dbus-analyze-functions.py --export-only > $@

$(musicmessaging_la_OBJECTS) musicmessaging.so: music-messaging-bindings.c $(top_builddir)/src/dbus-types.h



endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_CFLAGS)
