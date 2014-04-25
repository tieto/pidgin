EXTRA_DIST = \
		Makefile.mingw

plugindir = @PIDGIN_PLUGINDIR@

ticker_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

if PLUGINS

plugin_LTLIBRARIES = ticker.la

ticker_la_SOURCES = \
	gtkticker.c \
	gtkticker.h \
	ticker.c

ticker_la_LIBADD = @PIDGIN_LIBS@

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPLUGIN_CFLAGS)
