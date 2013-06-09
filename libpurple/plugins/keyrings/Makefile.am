EXTRA_DIST = \
		Makefile.mingw \
		wincred.c
CLEANFILES =

plugindir = $(libdir)/purple-$(PURPLE_MAJOR_VERSION)

internalkeyring_la_CFLAGS  = $(AM_CPPFLAGS)
internalkeyring_la_LDFLAGS = -module -avoid-version
internalkeyring_la_SOURCES = internalkeyring.c
internalkeyring_la_LIBADD  = $(GLIB_LIBS)

if ENABLE_SECRETSERVICE

secretservice_la_CFLAGS  = $(AM_CPPFLAGS) $(SECRETSERVICE_CFLAGS)
secretservice_la_LDFLAGS = -module -avoid-version
secretservice_la_SOURCES = secretservice.c
secretservice_la_LIBADD  = $(GLIB_LIBS) $(SECRETSERVICE_LIBS)

endif

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

kwallet.cpp: kwallet.moc

kwallet.moc:
	$(AM_V_GEN)$(MOC) $(kwallet_la_CXXFLAGS) -i kwallet.cpp -o $@

CLEANFILES += kwallet.moc

endif

if PLUGINS

plugin_LTLIBRARIES = \
	internalkeyring.la

if ENABLE_SECRETSERVICE
plugin_LTLIBRARIES += \
	secretservice.la
endif

if ENABLE_GNOMEKEYRING
plugin_LTLIBRARIES += \
	gnomekeyring.la
endif

if ENABLE_KWALLET
plugin_LTLIBRARIES += \
	kwallet.la
endif

endif

#XXX: that might be done better than adding DEBUG_CPPFLAGS to all objects (not only C++ ones)
AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(GLIB_CFLAGS) \
	$(DEBUG_CPPFLAGS) \
	$(PLUGIN_CFLAGS)
