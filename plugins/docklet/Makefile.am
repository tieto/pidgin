plugindir = $(libdir)/gaim

docklet_la_LDFLAGS = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = docklet.la

docklet_la_SOURCES = \
	docklet.c \
	eggtrayicon.h \
	eggtrayicon.c

endif

INCLUDES = \
	-I$(top_srcdir)/src \
	-DVERSION=\"$(VERSION)\" \
	-DDATADIR=\"$(datadir)\" \
	$(DEBUG_CFLAGS)
