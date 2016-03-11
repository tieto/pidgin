include $(top_srcdir)/glib-tap.mk

COMMON_LIBS=\
	$(top_builddir)/libpurple/libpurple.la \
	$(GLIB_LIBS) \
	$(GPLUGIN_LIBS)

test_programs=\
	test_des \
	test_des3 \
	test_hmac \
	test_md4 \
	test_md5 \
	test_sha1 \
	test_sha256 \
	test_util


test_des_SOURCES=test_des.c
test_des_LDADD=$(COMMON_LIBS)

test_des3_SOURCES=test_des3.c
test_des3_LDADD=$(COMMON_LIBS)

test_hmac_SOURCES=test_hmac.c
test_hmac_LDADD=$(COMMON_LIBS)

test_md4_SOURCES=test_md4.c
test_md4_LDADD=$(COMMON_LIBS)

test_md5_SOURCES=test_md5.c
test_md5_LDADD=$(COMMON_LIBS)

test_sha1_SOURCES=test_sha1.c
test_sha1_LDADD=$(COMMON_LIBS)

test_sha256_SOURCES=test_sha256.c
test_sha256_LDADD=$(COMMON_LIBS)

test_util_SOURCES=test_util.c
test_util_LDADD=$(COMMON_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(NSS_CFLAGS)
