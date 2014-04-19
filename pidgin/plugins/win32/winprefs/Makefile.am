plugindir = $(libdir)/pidgin

plugin_LTLIBRARIES = winprefs.la

winprefs_la_LDFLAGS = -module @PLUGIN_LDFLAGS@
winprefs_la_SOURCES = \
	gtkappbar.c \
	gtkappbar.h \
	winprefs.c
winprefs_la_LIBADD = @PIDGIN_LIBS@

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CPPFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS)
