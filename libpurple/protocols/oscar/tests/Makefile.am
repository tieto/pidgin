include $(top_srcdir)/glib-tap.mk

COMMON_LIBS=\
	$(top_builddir)/libpurple/libpurple.la \
	$(top_builddir)/libpurple/protocols/oscar/liboscar.la \
	$(GLIB_LIBS) \
	$(GPLUGIN_LIBS)

test_programs=\
	test_oscar_util

test_oscar_util_SOURCES=test_oscar_util.c
test_oscar_util_LDADD=$(COMMON_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(NSS_CFLAGS)
