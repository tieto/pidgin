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

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/plugins \
	-I$(top_srcdir)/src \
	$(GTK_CFLAGS) \
	$(DEBUG_CFLAGS)
