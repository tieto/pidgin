EXTRA_DIST = \
		Makefile.mingw

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

endif

INCLUDES = \
	-I$(top_srcdir)/src \
	-DVERSION=\"$(VERSION)\" \
	-DDATADIR=\"$(datadir)\" \
	$(DEBUG_CFLAGS)
