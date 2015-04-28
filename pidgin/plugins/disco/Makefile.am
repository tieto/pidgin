plugindir = @PIDGIN_PLUGINDIR@

xmppdisco_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

EXTRA_DIST = \
        Makefile.mingw

if PLUGINS

plugin_LTLIBRARIES = xmppdisco.la

xmppdisco_la_SOURCES = \
	gtkdisco.c \
	gtkdisco.h \
	xmppdisco.c \
	xmppdisco.h

xmppdisco_la_LIBADD = @PIDGIN_LIBS@

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPLUGIN_CFLAGS)
