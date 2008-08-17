EXTRA_DIST = \
		Makefile.mingw

plugindir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)


if ENABLE_GNOMEKEYRING

gnomekeyring_la_LDFLAGS     = -module -avoid-version
GKRSOURCES = gnomekeyring.c
#GNOME_KEYRING_CFLAGS = -I/usr/include/gnome-keyring-1
#GNOME_KEYRING_LIBS   = -lgnome-keyring
GNOME_KEYRING_LA = internalkeyring.la

endif #ENABLE_GNOMEKEYRING

internalkeyring_la_LDFLAGS  = -module -avoid-version
IKSOURCES = internalkeyring.c

if PLUGINS

plugin_LTLIBRARIES = \
	$(GNOME_KEYRING_LA)\
	internalkeyring.la


if ENABLE_GNOMEKEYRING

gnomekeyring_la_SOURCES     = $(GKRSOURCES)
gnomekeyring_la_LIBADD  = $(GLIB_LIBS) $(GNOMEKEYRING_LIBS)

endif #ENABLE_GNOMEKEYRING

internalkeyring_la_SOURCES  = $(IKSOURCES)
internalkeyring_la_LIBADD   = $(GLIB_LIBS)

endif #PLUGINS

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(GNOME_KEYRING_CFLAGS)

if ENABLE_GNOMEKEYRING
gnomekeyring_la_CFLAGS = $(AM_CPPFLAGS) $(GNOMEKEYRING_CFLAGS)
endif #ENABLE_GNOMEKEYRING
internalkeyring_la_CFLAGS = $(AM_CPPFLAGS)



