plugindir = $(libdir)/gaim

ssl_la_LDFLAGS        = -module -avoid-version
ssl_gnutls_la_LDFLAGS = -module -avoid-version
ssl_nss_la_LDFLAGS    = -module -avoid-version

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
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(NSS_CFLAGS) \
	$(GNUTLS_CFLAGS)
