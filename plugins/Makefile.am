if ENABLE_GTK
	
if BUILD_GEVOLUTION
GEVOLUTION_DIR = gevolution
endif # GEvolution

if USE_PERL
PERL_DIR = perl
endif # Perl

gtk_dirs = docklet $(GEVOLUTION_DIR) gestures musicmessaging $(PERL_DIR) ticker
endif # GTK

DIST_SUBDIRS = mono ssl tcl $(gtk_dirs)

if USE_TCL
TCL_DIR = tcl
endif

if ENABLE_DBUS
DBUS_LTLIB = dbus-example.la

if ENABLE_GTK
# Only use music messaging if dbus is enabled
MUSICMESSAGING_DIR = musicmessaging
endif
endif

if USE_MONO
MONO_DIR = mono
endif

SUBDIRS = \
	$(MONO_DIR) \
	ssl \
	$(TCL_DIR) \
	$(gtk_dirs)

plugindir = $(libdir)/gaim

idle_la_LDFLAGS             = -module -avoid-version $(GLIB_LIBS)
psychic_la_LDFLAGS          = -module -avoid-version $(GLIB_LIBS)
relnot_la_LDFLAGS           = -module -avoid-version $(GLIB_LIBS)
statenotify_la_LDFLAGS      = -module -avoid-version $(GLIB_LIBS)

# this can't be in a conditional otherwise automake 1.4 yells
dbus_example_la_LDFLAGS     = -module -avoid-version $(GLIB_LIBS) $(DBUS_LIBS)

if PLUGINS

if ENABLE_GTK
gtk_plugins = \
	extplacement.la     \
	gaimrc.la           \
	history.la          \
	iconaway.la         \
	notify.la           \
	spellchk.la         \
	timestamp.la        \
	timestamp_format.la
	
extplacement_la_SOURCES     = extplacement.c
gaimrc_la_SOURCES           = gaimrc.c
history_la_SOURCES          = history.c
iconaway_la_SOURCES         = iconaway.c
notify_la_SOURCES           = notify.c
spellchk_la_SOURCES         = spellchk.c
timestamp_la_SOURCES        = timestamp.c
timestamp_format_la_SOURCES = timestamp_format.c

extplacement_la_LDFLAGS     = -module -avoid-version $(GLIB_LIBS)
gaimrc_la_LDFLAGS           = -module -avoid-version $(GTK_LIBS)
history_la_LDFLAGS          = -module -avoid-version $(GTK_LIBS)
iconaway_la_LDFLAGS         = -module -avoid-version $(GTK_LIBS)
notify_la_LDFLAGS           = -module -avoid-version $(GTK_LIBS)
spellchk_la_LDFLAGS         = -module -avoid-version $(GTK_LIBS)
timestamp_la_LDFLAGS        = -module -avoid-version $(GTK_LIBS)
timestamp_format_la_LDFLAGS = -module -avoid-version $(GTK_LIBS)

endif	# ENABLE_GTK

plugin_LTLIBRARIES = \
	idle.la             \
	psychic.la          \
	relnot.la           \
	statenotify.la      \
	$(gtk_plugins)		\
	$(DBUS_LTLIB)

idle_la_SOURCES             = idle.c
psychic_la_SOURCES          = psychic.c
relnot_la_SOURCES           = relnot.c
statenotify_la_SOURCES      = statenotify.c

if ENABLE_DBUS

CLEANFILES              = dbus-example-bindings.c
dbus_example_la_SOURCES = dbus-example.c

.PHONY: always

$(top_builddir)/src/dbus-types.h: always
	cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

dbus-example-bindings.c: $(top_srcdir)/src/dbus-analyze-functions.py $(dbus_example_la_SOURCES)
	cat $(srcdir)/$(dbus_example_la_SOURCES) | \
	$(PYTHON) $(top_srcdir)/src/dbus-analyze-functions.py --export-only > $@

$(dbus_example_la_OBJECTS) dbus-example.so: dbus-example-bindings.c $(top_builddir)/src/dbus-types.h


endif # ENABLE_DBUS

endif # PLUGINS

EXTRA_DIST = \
	ChangeLog HOWTO \
	ChangeLog.API \
	Makefile.mingw \
	contact_priority.c \
	dbus-buddyicons-example.py \
	filectl.c \
	fortuneprofile.pl \
	gaim.pl \
	gaiminc.c \
	gtk-signals-test.c \
	ipc-test-client.c \
	ipc-test-server.c \
	mailchk.c \
	pluginpref_example.c \
	raw.c \
	signals-test.c \
	simple.c

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(DBUS_CFLAGS)

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
