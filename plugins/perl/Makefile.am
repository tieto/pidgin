plugindir = $(libdir)/gaim

perl_la_LDFLAGS = -module -avoid-version $(PERL_LIBS)

plugin_LTLIBRARIES = perl.la

perl_la_SOURCES = perl.c

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-DVERSION=\"$(VERSION)\" \
	$(DEBUG_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(PERL_CFLAGS)
