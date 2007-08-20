if PURPLE_AVAILABLE
# These custom wms depend on libpurple
purple_wms = s.la irssi.la
else
purple_wms =
endif

s_la_LDFLAGS             = -module -avoid-version
irssi_la_LDFLAGS         = -module -avoid-version

plugin_LTLIBRARIES = \
	$(purple_wms)
 
plugindir = $(libdir)/gnt

irssi_la_SOURCES = irssi.c
irssi_la_LIBADD =  \
  $(GLIB_LIBS) \
  $(top_builddir)/finch/libgnt/libgnt.la

s_la_SOURCES = s.c
s_la_LIBADD =  \
  $(GLIB_LIBS) \
  $(top_builddir)/finch/libgnt/libgnt.la \
  $(top_builddir)/libpurple/libpurple.la

EXTRA_DIST = 

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-I$(top_srcdir)/libpurple \
	-I$(top_srcdir)/finch \
	-I$(top_srcdir)/finch/libgnt \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GNT_CFLAGS) \
	$(PLUGIN_CFLAGS)

