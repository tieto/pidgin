gntclipboard_la_LDFLAGS = -module -avoid-version
gntgf_la_LDFLAGS      = -module -avoid-version
gnthistory_la_LDFLAGS = -module -avoid-version
gntlastlog_la_LDFLAGS = -module -avoid-version
gnttinyurl_la_LDFLAGS = -module -avoid-version
grouping_la_LDFLAGS   = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = \
	gntclipboard.la \
	gntgf.la \
	gnthistory.la \
	gntlastlog.la \
	gnttinyurl.la \
	grouping.la

plugindir = $(libdir)/finch

gntclipboard_la_SOURCES = gntclipboard.c
gntgf_la_SOURCES      = gntgf.c
gnthistory_la_SOURCES = gnthistory.c
gntlastlog_la_SOURCES = lastlog.c
gnttinyurl_la_SOURCES = gnttinyurl.c
grouping_la_SOURCES   = grouping.c

gntclipboard_la_CFLAGS = $(X11_CFLAGS)
gntgf_la_CFLAGS = $(X11_CFLAGS)

gntclipboard_la_LIBADD = $(GLIB_LIBS) $(X11_LIBS)
gntgf_la_LIBADD       = $(GLIB_LIBS) $(X11_LIBS) $(top_builddir)/finch/libgnt/libgnt.la
gnthistory_la_LIBADD  = $(GLIB_LIBS)
gntlastlog_la_LIBADD  = $(GLIB_LIBS)
gnttinyurl_la_LIBADD  = $(GLIB_LIBS)
grouping_la_LIBADD    = $(GLIB_LIBS) $(top_builddir)/finch/libgnt/libgnt.la

endif # PLUGINS

EXTRA_DIST = pietray.py

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/libpurple \
	-I$(top_srcdir) \
	-I$(top_srcdir)/finch \
	-I$(top_srcdir)/finch/libgnt \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GNT_CFLAGS) \
	$(PLUGIN_CFLAGS)

#
# This part allows people to build their own plugins in here.
# Yes, it's a mess.
#
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) -DHAVE_CONFIG_H -I$(top_builddir) $(AM_CPPFLAGS) $(CFLAGS) -c $< -o tmp$@.lo $(PLUGIN_CFLAGS)
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) -module -avoid-version $(PLUGIN_LIBS)
	@rm -f tmp$@.lo tmp$@.o libtmp$@.la
	@cp .libs/libtmp$@.so.so $@
	@rm -rf .libs/libtmp$@.*
