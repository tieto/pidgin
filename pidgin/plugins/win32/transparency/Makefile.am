plugindir = @PIDGIN_PLUGINDIR@

plugin_LTLIBRARIES = win2ktrans.la

win2ktrans_la_LDFLAGS = -module @PLUGIN_LDFLAGS@
win2ktrans_la_SOURCES = win2ktrans.c
win2ktrans_la_LIBADD = @PIDGIN_LIBS@

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CPPFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS)
