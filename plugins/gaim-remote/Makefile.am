plugindir = $(libdir)/gaim

gaim_remote_la_LDFLAGS = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = gaim-remote.la

gaim_remote_la_SOURCES = \
	remote.c \
	remote-socket.c \
	remote-socket.h

endif

INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/plugins \
	-DVERSION=\"$(VERSION)\" \
	-DDATADIR=\"$(datadir)\" \
	$(DEBUG_CFLAGS)
