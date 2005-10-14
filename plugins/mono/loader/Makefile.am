plugindir = $(libdir)/gaim

plugin_LTLIBRARIES = mono.la

mono_la_SOURCES	= \
	mono.c \
	mono-helper.c \
	debug-glue.c \
	signal-glue.c \
	blist-glue.c

mono_la_LDFLAGS  = -module -avoid-version

mono_la_LIBADD = $(MONO_LIBS)

AM_CPPFLAGS = \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(MONO_CFLAGS)
