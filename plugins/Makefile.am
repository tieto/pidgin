SUBDIRS = docklet gestures ticker

plugindir = $(libdir)/gaim

#CFLAGS += -I\$(top_srcdir) -I\$(top_srcdir)/src -DVERSION=\"$(VERSION)\" $(DEBUG_CFLAGS)


autorecon_la_LDFLAGS = -module -avoid-version
chatlist_la_LDFLAGS  = -module -avoid-version
iconaway_la_LDFLAGS  = -module -avoid-version
notify_la_LDFLAGS    = -module -avoid-version
spellchk_la_LDFLAGS  = -module -avoid-version
history_la_LDFLAGS   = -module -avoid-version
timestamp_la_LDFLAGS = -module -avoid-version
idle_la_LDFLAGS      = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = \
	autorecon.la   \
	chatlist.la    \
	iconaway.la    \
	notify.la      \
	spellchk.la    \
	history.la     \
	timestamp.la   \
	idle.la

autorecon_la_SOURCES = autorecon.c
chatlist_la_SOURCES  = chatlist.c
iconaway_la_SOURCES  = iconaway.c
notify_la_SOURCES    = notify.c
spellchk_la_SOURCES  = spellchk.c
history_la_SOURCES   = history.c
timestamp_la_SOURCES = timestamp.c
idle_la_SOURCES      = idle.c

endif

EXTRA_DIST = \
	ChangeLog PERL-HOWTO HOWTO SIGNALS \
	filectl.c mailchk.c gtik.c error.c \
	gaim.pl fortuneprofile.pl

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-DVERSION=\"$(VERSION)\" \
	$(DEBUG_CFLAGS) \
	$(PLUGIN_CFLAGS)

#
# This part allows people to build their own plugins in here.
# Yes, it's a mess.
#
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) $(AM_CPPFLAGS) $(CFLAGS) -c $< -o tmp$@.lo $(PLUGIN_CFLAGS)
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) -module -avoid-version $(PLUGIN_LIBS)
	@rm -f tmp$@.lo tmp$@.o libtmp$@.la
	@cp .libs/libtmp$@.so* $@
	@rm -f .libs/libtmp$@.*

