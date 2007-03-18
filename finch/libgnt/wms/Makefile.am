s_la_LDFLAGS             = -module -avoid-version

plugin_LTLIBRARIES = \
	s.la

plugindir = $(libdir)/gaim

s_la_SOURCES = s.c
s_la_LIBADD =  \
  $(GLIB_LIBS) \
  $(top_builddir)/finch/libgnt/libgnt.la \
  $(top_builddir)/libpurple/libpurple.la

EXTRA_DIST = 

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/libpurple \
	-I$(top_srcdir)/finch \
	-I$(top_srcdir)/finch/libgnt \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GNT_CFLAGS) \
	$(PLUGIN_CFLAGS)

