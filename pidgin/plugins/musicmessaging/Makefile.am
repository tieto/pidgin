EXTRA_DIST = \
	music.png

musicmessagingdir = $(libdir)/gaim

musicmessaging_la_LDFLAGS = -module -avoid-version

if PLUGINS
if ENABLE_DBUS

musicmessagingpixdir = $(datadir)/pixmaps/gaim/buttons
musicmessagingpix_DATA = music.png

musicmessaging_LTLIBRARIES = musicmessaging.la

musicmessaging_la_SOURCES = \
	musicmessaging.c

musicmessaging_la_LIBADD = $(GTK_LIBS) $(DBUS_LIBS)

CLEANFILES              = music-messaging-bindings.c

.PHONY: always

$(top_builddir)/libpurple/dbus-types.h: always
	cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

music-messaging-bindings.c: $(top_srcdir)/libpurple/dbus-analyze-functions.py $(musicmessaging_la_SOURCES)
	cat $(srcdir)/$(musicmessaging_la_SOURCES) | \
	$(PYTHON) $(top_srcdir)/libpurple/dbus-analyze-functions.py --export-only > $@

$(musicmessaging_la_OBJECTS) musicmessaging.so: music-messaging-bindings.c $(top_builddir)/libpurple/dbus-types.h

endif
endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_CFLAGS)
