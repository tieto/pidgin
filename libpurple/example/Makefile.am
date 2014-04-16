noinst_PROGRAMS = nullclient

nullclient_SOURCES = defines.h nullclient.c
nullclient_DEPENDENCIES =
nullclient_LDFLAGS = -export-dynamic
nullclient_LDADD = \
	$(DBUS_LIBS) \
	$(INTLLIBS) \
	$(GLIB_LIBS) \
	$(GPLUGIN_LIBS) \
	$(LIBXML_LIBS) \
	$(top_builddir)/libpurple/libpurple.la

AM_CPPFLAGS = \
	-DSTANDALONE \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/libpurple \
	-I$(top_srcdir) \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS)
