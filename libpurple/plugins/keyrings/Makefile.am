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

if ENABLE_KWALLET

kwallet_la_CXXFLAGS  = $(KWALLET_CXXFLAGS) $(QT4_CFLAGS)
kwallet_la_LDFLAGS = -module -avoid-version
kwallet_la_SOURCES = kwallet.cpp
kwallet_la_BUILTSOURCES = kwallet.moc
kwallet_la_LIBADD  = $(GLIB_LIBS) $(KWALLET_LIBS) $(QT4_LIBS)

kwallet.moc: kwallet.cpp
	$(AM_V_GEN)$(MOC) $(kwallet_la_CXXFLAGS) -i $< -o $@

endif

if PLUGINS

plugin_LTLIBRARIES = \
	internalkeyring.la

if ENABLE_GNOMEKEYRING
plugin_LTLIBRARIES += \
	gnomekeyring.la
endif

if ENABLE_KWALLET
plugin_LTLIBRARIES += \
	kwallet.la
endif

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(PLUGIN_CFLAGS)

