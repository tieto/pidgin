DIST_SUBDIRS = mono perl ssl tcl keyrings

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
	$(TCL_DIR) \
	keyrings

plugindir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

autoaccept_la_LDFLAGS         = -module @PLUGIN_LDFLAGS@
buddynote_la_LDFLAGS          = -module @PLUGIN_LDFLAGS@
ciphertest_la_LDFLAGS         = -module @PLUGIN_LDFLAGS@
codeinline_la_LDFLAGS         = -module @PLUGIN_LDFLAGS@
debug_example_la_LDFLAGS      = -module @PLUGIN_LDFLAGS@
helloworld_la_LDFLAGS         = -module @PLUGIN_LDFLAGS@
idle_la_LDFLAGS               = -module @PLUGIN_LDFLAGS@
joinpart_la_LDFLAGS           = -module @PLUGIN_LDFLAGS@
log_reader_la_LDFLAGS         = -module @PLUGIN_LDFLAGS@
notify_example_la_LDFLAGS     = -module @PLUGIN_LDFLAGS@
offlinemsg_la_LDFLAGS         = -module @PLUGIN_LDFLAGS@
one_time_password_la_LDFLAGS  = -module @PLUGIN_LDFLAGS@
pluginpref_example_la_LDFLAGS = -module @PLUGIN_LDFLAGS@
psychic_la_LDFLAGS            = -module @PLUGIN_LDFLAGS@
signals_test_la_LDFLAGS       = -module @PLUGIN_LDFLAGS@
simple_la_LDFLAGS             = -module @PLUGIN_LDFLAGS@
statenotify_la_LDFLAGS        = -module @PLUGIN_LDFLAGS@

# this can't be in a conditional otherwise automake 1.4 yells
dbus_example_la_LDFLAGS       = -module @PLUGIN_LDFLAGS@

if PLUGINS

plugin_LTLIBRARIES = \
	autoaccept.la       \
	buddynote.la        \
	idle.la             \
	joinpart.la         \
	log_reader.la       \
	offlinemsg.la       \
	psychic.la          \
	statenotify.la      \
	$(DBUS_LTLIB)

noinst_LTLIBRARIES = \
	ciphertest.la \
	codeinline.la \
	debug_example.la \
	helloworld.la \
	notify_example.la \
	one_time_password.la \
	pluginpref_example.la \
	signals_test.la \
	simple.la

autoaccept_la_SOURCES       = autoaccept.c
buddynote_la_SOURCES        = buddynote.c
ciphertest_la_SOURCES		= ciphertest.c
codeinline_la_SOURCES		= codeinline.c
debug_example_la_SOURCES = debug_example.c
helloworld_la_SOURCES       = helloworld.c
idle_la_SOURCES             = idle.c
joinpart_la_SOURCES         = joinpart.c
log_reader_la_SOURCES       = log_reader.c
notify_example_la_SOURCES   = notify_example.c
offlinemsg_la_SOURCES       = offlinemsg.c
one_time_password_la_SOURCES	= one_time_password.c
pluginpref_example_la_SOURCES = pluginpref_example.c
psychic_la_SOURCES          = psychic.c
signals_test_la_SOURCES		= signals-test.c
simple_la_SOURCES			= simple.c
statenotify_la_SOURCES      = statenotify.c

autoaccept_la_LIBADD         = @PURPLE_LIBS@
buddynote_la_LIBADD          = @PURPLE_LIBS@
ciphertest_la_LIBADD         = @PURPLE_LIBS@
codeinline_la_LIBADD         = @PURPLE_LIBS@
idle_la_LIBADD               = @PURPLE_LIBS@
joinpart_la_LIBADD           = @PURPLE_LIBS@
log_reader_la_LIBADD         = @PURPLE_LIBS@
notify_example_la_LIBADD     = @PURPLE_LIBS@
offlinemsg_la_LIBADD         = @PURPLE_LIBS@
one_time_password_la_LIBADD  = @PURPLE_LIBS@
pluginpref_example_la_LIBADD = @PURPLE_LIBS@
psychic_la_LIBADD            = @PURPLE_LIBS@
signals_test_la_LIBADD       = @PURPLE_LIBS@
simple_la_LIBADD             = @PURPLE_LIBS@
statenotify_la_LIBADD        = @PURPLE_LIBS@

if ENABLE_DBUS

CLEANFILES              = dbus-example-bindings.c
dbus_example_la_SOURCES = dbus-example.c

dbus_example_la_LIBADD       = @PURPLE_LIBS@ $(DBUS_LIBS)

.PHONY: always

$(top_builddir)/libpurple/dbus-types.h: always
	$(AM_V_GEN)cd $(@D) && $(MAKE) $(AM_MAKEFLAGS) $(@F)

dbus-example-bindings.c: $(top_srcdir)/libpurple/dbus-analyze-functions.py $(dbus_example_la_SOURCES)
	$(AM_V_GEN)cat $(srcdir)/$(dbus_example_la_SOURCES) | \
	$(PYTHON) $(top_srcdir)/libpurple/dbus-analyze-functions.py --export-only > $@

$(dbus_example_la_OBJECTS) dbus-example.so: dbus-example-bindings.c $(top_builddir)/libpurple/dbus-types.h


endif # ENABLE_DBUS

endif # PLUGINS

EXTRA_DIST = \
	Makefile.mingw \
	dbus-buddyicons-example.py \
	filectl.c \
	fortuneprofile.pl \
	ipc-test-client.c \
	ipc-test-server.c \
	startup.py

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(NSS_CFLAGS)

PLUGIN_LIBS = \
	$(NSS_LIBS)

#
# This part allows people to build their own plugins in here.
# Yes, it's a mess.
#
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) -DHAVE_CONFIG_H -I$(top_builddir) $(AM_CPPFLAGS) $(CFLAGS) -c $< -o tmp$@.lo $(PLUGIN_CFLAGS)
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) -module @PLUGIN_LDFLAGS@ $(PLUGIN_LIBS)
	@rm -f tmp$@.lo tmp$@.o libtmp$@.la
	@cp .libs/libtmp$@*.so $@
	@rm -rf .libs/libtmp$@.*
