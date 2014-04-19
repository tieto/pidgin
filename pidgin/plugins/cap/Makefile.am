capdir = $(libdir)/pidgin

cap_la_LDFLAGS = -module @PLUGIN_LDFLAGS@

if PLUGINS

cappixdir = $(datadir)/pixmaps/pidgin/buttons

cap_LTLIBRARIES = cap.la

cap_la_SOURCES = \
	cap.h \
	cap.c \
	cap_statistics.h

endif

cap_la_LIBADD = @PIDGIN_LIBS@ $(SQLITE3_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(SQLITE3_CFLAGS)

EXTRA_DIST = Makefile.mingw
