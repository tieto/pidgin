DIST_SUBDIRS = cap docklet gestures gevolution musicmessaging perl ticker

if BUILD_GEVOLUTION
GEVOLUTION_DIR = gevolution
endif

if ENABLE_DBUS
MUSICMESSAGING_DIR = musicmessaging
endif

if ENABLE_CAP
CAP_DIR = cap
endif

if USE_PERL
PERL_DIR = perl
endif

SUBDIRS = \
	$(CAP_DIR) \
	docklet \
	gestures \
	$(GEVOLUTION_DIR) \
	$(MUSICMESSAGING_DIR) \
	$(PERL_DIR) \
	ticker

plugindir = $(libdir)/gaim

extplacement_la_LDFLAGS     = -module -avoid-version $(GLIB_LIBS)
gaimrc_la_LDFLAGS           = -module -avoid-version $(GTK_LIBS)
history_la_LDFLAGS          = -module -avoid-version $(GTK_LIBS)
iconaway_la_LDFLAGS         = -module -avoid-version $(GTK_LIBS)
notify_la_LDFLAGS           = -module -avoid-version $(GTK_LIBS)
relnot_la_LDFLAGS           = -module -avoid-version $(GLIB_LIBS)
spellchk_la_LDFLAGS         = -module -avoid-version $(GTK_LIBS)
timestamp_la_LDFLAGS        = -module -avoid-version $(GTK_LIBS)
timestamp_format_la_LDFLAGS = -module -avoid-version $(GTK_LIBS)

if PLUGINS

plugin_LTLIBRARIES = \
	extplacement.la     \
	gaimrc.la           \
	history.la          \
	iconaway.la         \
	notify.la           \
	relnot.la           \
	spellchk.la         \
	timestamp.la        \
	timestamp_format.la


extplacement_la_SOURCES     = extplacement.c
gaimrc_la_SOURCES           = gaimrc.c
history_la_SOURCES          = history.c
iconaway_la_SOURCES         = iconaway.c
notify_la_SOURCES           = notify.c
relnot_la_SOURCES           = relnot.c
spellchk_la_SOURCES         = spellchk.c
timestamp_la_SOURCES        = timestamp.c
timestamp_format_la_SOURCES = timestamp_format.c

endif # PLUGINS

EXTRA_DIST = \
	Makefile.mingw \
	contact_priority.c \
	gaiminc.c \
	gtk-signals-test.c \
	mailchk.c \
	raw.c

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/libgaim \
	-I$(top_srcdir)/gtk \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
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
