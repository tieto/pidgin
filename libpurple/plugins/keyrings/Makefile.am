EXTRA_DIST = \
		Makefile.mingw

plugindir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

gnomekeyring_la_LDFLAGS     = -module -avoid-version
internalkeyring_la_LDFLAGS  = -module -avoid-version

GNOME_KEYRING_CFLAGS = -I/usr/include/gnome-keyring-1

GKRSOURCES = gnomekeyring.c
IKSOURCES = internalkeyring.c

if PLUGINS

plugin_LTLIBRARIES = \
	gnomekeyring.la       \
	internalkeyring.la

gnomekeyring_la_SOURCES     = gnomekeyring.c
internalkeyring_la_SOURCES  = internalkeyring.c

gnomekeyring_la_LIBADD  = $(GLIB_LIBS) $(GNOME_KEYRING_LIBS)
internalkeyring_la_LIBADD   = $(GLIB_LIBS)

endif #PLUGINS

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(GNOME_KEYRING_CFLAGS)

gnomekeyring_la_CFLAGS = $(AM_CPPFLAGS) $(GNOME_KEYRING_CFLAGS)
internalkeyring_la_CFLAGS = $(AM_CPPFLAGS)



