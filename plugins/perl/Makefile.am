EXTRA_DIST = \
		Makefile.mingw

plugindir = $(libdir)/gaim

perl_la_LDFLAGS = -module -avoid-version $(PERL_LIBS)

plugin_LTLIBRARIES = perl.la

perl_la_SOURCES = perl.c

AM_CPPFLAGS = \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(PERL_CFLAGS)
