EXTRA_DIST = \
		Makefile.mingw   \
		docklet-win32.c  \
		MinimizeToTray.c \
		MinimizeToTray.h

plugindir = $(libdir)/gaim

docklet_la_LDFLAGS = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = docklet.la

docklet_la_SOURCES = \
	eggtrayicon.h \
	eggtrayicon.c \
	docklet.h \
	docklet.c \
	docklet-x11.c

docklet_la_LIBADD = $(GTK_LIBS)

endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/libgaim \
	-I$(top_srcdir)/gtk \
	$(DEBUG_CFLAGS) \
	$(X_CFLAGS) \
	$(GTK_CFLAGS)
