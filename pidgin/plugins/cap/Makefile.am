capdir = $(libdir)/pidgin

cap_la_LDFLAGS = -module -avoid-version

if PLUGINS

cappixdir = $(datadir)/pixmaps/pidgin/buttons

cap_LTLIBRARIES = cap.la

cap_la_SOURCES = \
	cap.h \
	cap.c \
	cap_statistics.h

endif

cap_la_LIBADD = $(GTK_LIBS) $(SQLITE3_LIBS)

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(SQLITE3_CFLAGS)
