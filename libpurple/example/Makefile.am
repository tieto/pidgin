bin_PROGRAMS = nullclient

nullclient_SOURCES = nullclient.c
nullclient_DEPENDENCIES = 
nullclient_LDFLAGS = -export-dynamic
nullclient_LDADD = \
	$(DBUS_LIBS) \
	$(INTLLIBS) \
	$(GLIB_LIBS) \
	$(LIBXML_LIBS) \
	$(top_builddir)/libpurple/libpurple.la

AM_CPPFLAGS = \
	-DSTANDALONE \
	-DBR_PTHREADS=0 \
	-DDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)/libpurple/\" \
	-DLOCALEDIR=\"$(datadir)/locale\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_srcdir)/libpurple/ \
	-I$(top_srcdir) \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS)
