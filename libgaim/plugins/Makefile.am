DIST_SUBDIRS = mono perl ssl tcl

if USE_PERL
PERL_DIR = perl
endif

if USE_TCL
TCL_DIR = tcl
endif

if ENABLE_DBUS
DBUS_LTLIB = dbus-example.la
endif

if USE_MONO
MONO_DIR = mono
endif

SUBDIRS = \
	$(MONO_DIR) \
	$(PERL_DIR) \
	ssl \
	$(TCL_DIR)

plugindir = $(libdir)/gaim

idle_la_LDFLAGS             = -module -avoid-version
log_reader_la_LDFLAGS       = -module -avoid-version
psychic_la_LDFLAGS          = -module -avoid-version
statenotify_la_LDFLAGS      = -module -avoid-version

# this can't be in a conditional otherwise automake 1.4 yells
dbus_example_la_LDFLAGS     = -module -avoid-version

if PLUGINS

plugin_LTLIBRARIES = \
	idle.la             \
	log_reader.la       \
	psychic.la          \
	statenotify.la      \
	$(DBUS_LTLIB)


idle_la_SOURCES             = idle.c
log_reader_la_SOURCES       = log_reader.c
psychic_la_SOURCES          = psychic.c
statenotify_la_SOURCES      = statenotify.c

idle_la_LIBADD              = $(GLIB_LIBS)
log_reader_la_LIBADD        = $(GLIB_LIBS)
psychic_la_LIBADD           = $(GLIB_LIBS)
statenotify_la_LIBADD       = $(GLIB_LIBS)

if ENABLE_DBUS

CLEANFILES              = dbus-example-bindings.c
dbus_example_la_SOURCES = dbus-example.c

dbus_example_la_LIBADD      = $(GLIB_LIBS) $(DBUS_LIBS)

.PHONY: always

$(top_builddir)/libgaim/dbus-types.h: always
	cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

dbus-example-bindings.c: $(top_srcdir)/libgaim/dbus-analyze-functions.py $(dbus_example_la_SOURCES)
	cat $(srcdir)/$(dbus_example_la_SOURCES) | \
	$(PYTHON) $(top_srcdir)/libgaim/dbus-analyze-functions.py --export-only > $@

$(dbus_example_la_OBJECTS) dbus-example.so: dbus-example-bindings.c $(top_builddir)/libgaim/dbus-types.h


endif # ENABLE_DBUS

endif # PLUGINS

EXTRA_DIST = \
	Makefile.mingw \
	dbus-buddyicons-example.py \
	filectl.c \
	fortuneprofile.pl \
	gaim.pl \
	ipc-test-client.c \
	ipc-test-server.c \
	pluginpref_example.c \
	signals-test.c \
	simple.c

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_builddir)/libgaim \
	-I$(top_srcdir)/libgaim \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
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
