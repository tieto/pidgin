DIST_SUBDIRS = cap disco gestures gevolution musicmessaging perl ticker

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

if ENABLE_GESTURES
GESTURE_DIR = gestures
endif

SUBDIRS = \
	$(CAP_DIR) \
	$(GESTURE_DIR) \
	$(GEVOLUTION_DIR) \
	$(MUSICMESSAGING_DIR) \
	$(PERL_DIR) \
	disco \
	ticker

plugindir = $(libdir)/pidgin

convcolors_la_LDFLAGS       = -module -avoid-version
contact_priority_la_LDFLAGS = -module -avoid-version
extplacement_la_LDFLAGS     = -module -avoid-version
gtk_signals_test_la_LDFLAGS = -module -avoid-version
gtkbuddynote_la_LDFLAGS     = -module -avoid-version
history_la_LDFLAGS          = -module -avoid-version
iconaway_la_LDFLAGS         = -module -avoid-version
markerline_la_LDFLAGS       = -module -avoid-version
notify_la_LDFLAGS           = -module -avoid-version
pidginrc_la_LDFLAGS         = -module -avoid-version
relnot_la_LDFLAGS           = -module -avoid-version
sendbutton_la_LDFLAGS       = -module -avoid-version
spellchk_la_LDFLAGS         = -module -avoid-version
themeedit_la_LDFLAGS        = -module -avoid-version
timestamp_la_LDFLAGS        = -module -avoid-version
timestamp_format_la_LDFLAGS = -module -avoid-version
vvconfig_la_LDFLAGS         = -module -avoid-version
xmppconsole_la_LDFLAGS      = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = \
	convcolors.la       \
	extplacement.la     \
	gtkbuddynote.la     \
	history.la          \
	iconaway.la         \
	markerline.la       \
	notify.la           \
	pidginrc.la         \
	relnot.la           \
	sendbutton.la       \
	spellchk.la         \
	themeedit.la         \
	timestamp.la        \
	timestamp_format.la \
	xmppconsole.la

if USE_VV
plugin_LTLIBRARIES +=	vvconfig.la
endif

noinst_LTLIBRARIES = \
	contact_priority.la \
	gtk_signals_test.la

convcolors_la_SOURCES       = convcolors.c
contact_priority_la_SOURCES = contact_priority.c
extplacement_la_SOURCES     = extplacement.c
gtk_signals_test_la_SOURCES = gtk-signals-test.c
gtkbuddynote_la_SOURCES     = gtkbuddynote.c
history_la_SOURCES          = history.c
iconaway_la_SOURCES         = iconaway.c
markerline_la_SOURCES       = markerline.c
notify_la_SOURCES           = notify.c
pidginrc_la_SOURCES         = pidginrc.c
relnot_la_SOURCES           = relnot.c
sendbutton_la_SOURCES       = sendbutton.c
spellchk_la_SOURCES         = spellchk.c
themeedit_la_SOURCES        = themeedit.c themeedit-icon.c themeedit-icon.h
timestamp_la_SOURCES        = timestamp.c
timestamp_format_la_SOURCES = timestamp_format.c
vvconfig_la_SOURCES         = vvconfig.c
xmppconsole_la_SOURCES      = xmppconsole.c

convcolors_la_LIBADD        = $(GTK_LIBS)
contact_priority_la_LIBADD  = $(GTK_LIBS)
extplacement_la_LIBADD      = $(GTK_LIBS)
gtk_signals_test_la_LIBADD  = $(GTK_LIBS)
gtkbuddynote_la_LIBADD      = $(GTK_LIBS)
history_la_LIBADD           = $(GTK_LIBS)
iconaway_la_LIBADD          = $(GTK_LIBS)
markerline_la_LIBADD        = $(GTK_LIBS)
notify_la_LIBADD            = $(GTK_LIBS)
pidginrc_la_LIBADD          = $(GTK_LIBS)
relnot_la_LIBADD            = $(GLIB_LIBS)
sendbutton_la_LIBADD        = $(GTK_LIBS)
spellchk_la_LIBADD          = $(GTK_LIBS)
themeedit_la_LIBADD         = $(GTK_LIBS)
timestamp_la_LIBADD         = $(GTK_LIBS)
timestamp_format_la_LIBADD  = $(GTK_LIBS)
vvconfig_la_LIBADD          = $(GTK_LIBS) $(GSTREAMER_LIBS)
xmppconsole_la_LIBADD       = $(GTK_LIBS)

endif # PLUGINS

EXTRA_DIST = \
	Makefile.mingw \
	mailchk.c \
	pidgininc.c \
	raw.c \
	win32/transparency/Makefile.mingw \
	win32/transparency/win2ktrans.c \
	win32/winprefs/gtkappbar.c \
	win32/winprefs/gtkappbar.h \
	win32/winprefs/Makefile.mingw \
	win32/winprefs/winprefs.c

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
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
	@cp .libs/libtmp$@*.so $@
	@rm -rf .libs/libtmp$@.*
