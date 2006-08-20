gntgf_la_LDFLAGS             = -module -avoid-version $(GLIB_LIBS)
gnthistory_la_LDFLAGS             = -module -avoid-version $(GLIB_LIBS)

if PLUGINS

plugin_LTLIBRARIES = \
	gntgf.la \
	gnthistory.la

plugindir = $(libdir)/gaim

gntgf_la_SOURCES             = gntgf.c
gnthistory_la_SOURCES        = gnthistory.c

endif # PLUGINS

EXTRA_DIST = 

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_builddir)/libgaim \
	-I$(top_srcdir)/libgaim \
	-I$(top_srcdir)/console \
	-I$(top_srcdir)/console/libgnt \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(PLUGIN_CFLAGS)

#
# This part allows people to build their own plugins in here.
# Yes, it's a mess.
#
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) -DHAVE_CONFIG_H -I$(top_srcdir) $(AM_CPPFLAGS) $(CFLAGS) -c $< -o tmp$@.lo $(PLUGIN_CFLAGS)
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) -module -avoid-version $(PLUGIN_LIBS)
	@rm -f tmp$@.lo tmp$@.o libtmp$@.la
	@cp .libs/libtmp$@.so* $@
	@rm -f .libs/libtmp$@.*
