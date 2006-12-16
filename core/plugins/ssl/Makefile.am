EXTRA_DIST = \
		Makefile.mingw

plugindir = $(libdir)/gaim

ssl_la_LDFLAGS        = -module -avoid-version $(GLIB_LIBS)
ssl_gnutls_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)
ssl_nss_la_LDFLAGS    = -module -avoid-version $(GLIB_LIBS)

if PLUGINS

plugin_LTLIBRARIES = \
	ssl.la           \
	ssl-gnutls.la    \
	ssl-nss.la

ssl_la_SOURCES        = ssl.c
ssl_gnutls_la_SOURCES = ssl-gnutls.c
ssl_nss_la_SOURCES    = ssl-nss.c

ssl_gnutls_la_LIBADD = $(GNUTLS_LIBS)
ssl_nss_la_LIBADD    = $(NSS_LIBS)

endif # PLUGINS

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/gaim/\" \
	-I$(top_srcdir)/core \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(NSS_CFLAGS) \
	$(GNUTLS_CFLAGS)
