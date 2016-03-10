include $(top_srcdir)/glib-tap.mk

COMMON_LIBS=\
	$(top_builddir)/libpurple/libpurple.la \
	$(GLIB_LIBS) \
	$(GPLUGIN_LIBS)

test_programs=\
	test_md4hash \
	test_md5hash \
	test_sha1hash

test_md4hash_SOURCES=test_md4hash.c
test_md4hash_LDADD=$(COMMON_LIBS)

test_md5hash_SOURCES=test_md5hash.c
test_md5hash_LDADD=$(COMMON_LIBS)

test_sha1hash_SOURCES=test_sha1hash.c
test_sha1hash_LDADD=$(COMMON_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(NSS_CFLAGS)
