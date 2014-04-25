gntclipboard_la_LDFLAGS = -module @PLUGIN_LDFLAGS@
gntgf_la_LDFLAGS        = -module @PLUGIN_LDFLAGS@
gnthistory_la_LDFLAGS   = -module @PLUGIN_LDFLAGS@
gntlastlog_la_LDFLAGS   = -module @PLUGIN_LDFLAGS@
gnttinyurl_la_LDFLAGS   = -module @PLUGIN_LDFLAGS@
grouping_la_LDFLAGS     = -module @PLUGIN_LDFLAGS@

if PLUGINS

plugin_LTLIBRARIES = \
	gntclipboard.la \
	gntgf.la \
	gnthistory.la \
	gntlastlog.la \
	gnttinyurl.la \
	grouping.la

plugindir = @FINCH_PLUGINDIR@

gntclipboard_la_SOURCES = gntclipboard.c
gntgf_la_SOURCES      = gntgf.c
gnthistory_la_SOURCES = gnthistory.c
gntlastlog_la_SOURCES = lastlog.c
gnttinyurl_la_SOURCES = gnttinyurl.c
grouping_la_SOURCES   = grouping.c

gntclipboard_la_CFLAGS = $(X11_CFLAGS)
gntgf_la_CFLAGS = $(X11_CFLAGS)

gntclipboard_la_LIBADD = @FINCH_LIBS@ $(X11_LIBS)
gntgf_la_LIBADD        = @FINCH_LIBS@ $(X11_LIBS)
gnthistory_la_LIBADD   = @FINCH_LIBS@
gntlastlog_la_LIBADD   = @FINCH_LIBS@
gnttinyurl_la_LIBADD   = @FINCH_LIBS@
grouping_la_LIBADD     = @FINCH_LIBS@

endif # PLUGINS

EXTRA_DIST = pietray.py

AM_CPPFLAGS = \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/libpurple \
	-I$(top_srcdir) \
	-I$(top_srcdir)/finch \
	-I$(top_srcdir)/finch/libgnt \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(GNT_CFLAGS) \
	$(PLUGIN_CFLAGS)

#
# This part allows people to build their own plugins in here.
# Yes, it's a mess.
#
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) -DHAVE_CONFIG_H -I$(top_builddir) $(AM_CPPFLAGS) $(CFLAGS) -c $< -o tmp$@.lo $(PLUGIN_CFLAGS)
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) -module @PLUGIN_LDFLAGS@ $(PLUGIN_LIBS)
	@rm -f tmp$@.lo tmp$@.o libtmp$@.la
	@cp .libs/libtmp$@.so.so $@
	@rm -rf .libs/libtmp$@.*
