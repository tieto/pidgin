grincludedir = $(includedir)/gaim-remote

plugindir = $(libdir)/gaim

lib_LTLIBRARIES = libgaim-remote.la

grinclude_HEADERS = \
	remote-socket.h \
	remote.h

libgaim_remote_la_SOURCES = remote-socket.c


gaim_remote_la_LDFLAGS = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = gaim-remote.la

gaim_remote_la_SOURCES = remote.c
gaim_remote_la_LIBADD = -lgaim-remote

endif


INCLUDES = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/plugins \
	-DVERSION=\"$(VERSION)\" \
	-DDATADIR=\"$(datadir)\" \
	$(DEBUG_CFLAGS)
