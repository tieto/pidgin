plugindir = $(libdir)/gaim

tcl_la_LDFLAGS = -module -avoid-version $(TCL_LIBS) $(TK_LIBS)

plugin_LTLIBRARIES = tcl.la

tcl_la_SOURCES = tcl.c tcl_glib.c tcl_glib.h tcl_cmds.c tcl_signals.c tcl_gaim.h

EXTRA_DIST = signal-test.tcl Makefile.mingw tcl_win32.c

AM_CPPFLAGS = \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(TK_CFLAGS) \
	$(TCL_CFLAGS)
