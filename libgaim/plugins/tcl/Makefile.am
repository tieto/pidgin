plugindir = $(libdir)/gaim

tcl_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS) $(TCL_LIBS) $(TK_LIBS)

plugin_LTLIBRARIES = tcl.la

tcl_la_SOURCES = tcl.c tcl_glib.c tcl_glib.h tcl_cmds.c tcl_signals.c tcl_gaim.h \
                 tcl_ref.c tcl_cmd.c

EXTRA_DIST = signal-test.tcl Makefile.mingw

AM_CPPFLAGS = \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgaim \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(TK_CFLAGS) \
	$(TCL_CFLAGS)
