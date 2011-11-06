EXTRA_DIST = \
		Makefile.mingw

plugindir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

internalkeyring_la_CFLAGS  = $(AM_CPPFLAGS)
internalkeyring_la_LDFLAGS = -module -avoid-version
internalkeyring_la_SOURCES = internalkeyring.c
internalkeyring_la_LIBADD  = $(GLIB_LIBS)

if ENABLE_GNOMEKEYRING

gnomekeyring_la_CFLAGS  = $(AM_CPPFLAGS) $(GNOMEKEYRING_CFLAGS)
gnomekeyring_la_LDFLAGS = -module -avoid-version
gnomekeyring_la_SOURCES = gnomekeyring.c
gnomekeyring_la_LIBADD  = $(GLIB_LIBS) $(GNOMEKEYRING_LIBS)

endif

if PLUGINS

plugin_LTLIBRARIES = \
	internalkeyring.la

if ENABLE_GNOMEKEYRING

plugin_LTLIBRARIES += \
	gnomekeyring.la

endif

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(PLUGIN_CFLAGS)

