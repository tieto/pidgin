s_la_LDFLAGS             = -module -avoid-version $(GLIB_LIBS) $(GNT_LIBS)

plugin_LTLIBRARIES = \
	s.la

plugindir = $(libdir)/gaim

s_la_SOURCES             = s.c

EXTRA_DIST = 

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/libgaim \
	-I$(top_srcdir)/console \
	-I$(top_srcdir)/console/libgnt \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GNT_CFLAGS) \
	$(PLUGIN_CFLAGS)

