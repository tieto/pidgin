DIST_SUBDIRS = docklet gevolution gaim-remote gestures perl ssl tcl ticker

if BUILD_GEVOLUTION
GEVOLUTION_DIR = gevolution
endif

if USE_PERL
PERL_DIR = perl
endif

if USE_TCL
TCL_DIR = tcl
endif

SUBDIRS = \
	docklet gaim-remote $(GEVOLUTION_DIR) gestures \
	$(PERL_DIR) ssl $(TCL_DIR) ticker

plugindir = $(libdir)/gaim

autorecon_la_LDFLAGS    = -module -avoid-version
extplacement_la_LDFLAGS = -module -avoid-version
gaimrc_la_LDFLAGS       = -module -avoid-version
history_la_LDFLAGS      = -module -avoid-version
iconaway_la_LDFLAGS     = -module -avoid-version
idle_la_LDFLAGS         = -module -avoid-version
notify_la_LDFLAGS       = -module -avoid-version
relnot_la_LDFLAGS       = -module -avoid-version
spellchk_la_LDFLAGS     = -module -avoid-version
statenotify_la_LDFLAGS  = -module -avoid-version
timestamp_la_LDFLAGS    = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = \
	autorecon.la     \
	extplacement.la  \
	gaimrc.la        \
	history.la       \
	iconaway.la      \
	idle.la          \
	notify.la        \
	relnot.la        \
	spellchk.la      \
	statenotify.la   \
	timestamp.la

autorecon_la_SOURCES    = autorecon.c
extplacement_la_SOURCES = extplacement.c
gaimrc_la_SOURCES       = gaimrc.c
history_la_SOURCES      = history.c
iconaway_la_SOURCES     = iconaway.c
idle_la_SOURCES         = idle.c
notify_la_SOURCES       = notify.c
relnot_la_SOURCES       = relnot.c
spellchk_la_SOURCES     = spellchk.c
statenotify_la_SOURCES  = statenotify.c
timestamp_la_SOURCES    = timestamp.c

endif # PLUGINS

EXTRA_DIST = \
	ChangeLog HOWTO \
	ChangeLog.API \
	filectl.c \
	mailchk.c \
	pluginpref_example.c \
	raw.c \
	signals-test.c \
	simple.c \
	gaim.pl \
	fortuneprofile.pl \
	Makefile.mingw

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/src \
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
