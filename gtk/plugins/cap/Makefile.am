capdir = $(libdir)/gaim

cap_la_LDFLAGS = -module -avoid-version $(GTK_LIBS)

if PLUGINS

cappixdir = $(datadir)/pixmaps/gaim/buttons

cap_LTLIBRARIES = cap.la

cap_la_SOURCES = \
	cap.h \
	cap.c \
	cap_statistics.h

endif

CAP_LIBS = -ldl -ldbi

cap_la_LIBADD = $(CAP_LIBS)

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/libgaim \
	-I$(top_srcdir)/gtk \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(CAP_CFLAGS)
