include $(top_srcdir)/glib-tap.mk

COMMON_LIBS=\
	$(top_builddir)/libpurple/libpurple.la \
	$(top_builddir)/libpurple/protocols/jabber/libjabber.la \
	$(GLIB_LIBS) \
	$(GPLUGIN_LIBS) \
	$(LIBXML_LIBS)

test_programs=\
	test_jabber_caps \
	test_jabber_digest_md5 \
	test_jabber_jutil \
	test_jabber_scram

test_jabber_caps_SOURCES=test_jabber_caps.c
test_jabber_caps_LDADD=$(COMMON_LIBS)

test_jabber_digest_md5_SOURCES=test_jabber_digest_md5.c
test_jabber_digest_md5_LDADD=$(COMMON_LIBS)

test_jabber_jutil_SOURCES=test_jabber_jutil.c
test_jabber_jutil_LDADD=$(COMMON_LIBS)

test_jabber_scram_SOURCES=test_jabber_scram.c
test_jabber_scram_LDADD=$(COMMON_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(LIBXML_CFLAGS)
