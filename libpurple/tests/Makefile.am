include $(top_srcdir)/glib-tap.mk

COMMON_LIBS=\
	$(top_builddir)/libpurple/libpurple.la \
	$(GLIB_LIBS) \
	$(GPLUGIN_LIBS)

test_programs=\
	test_des \
	test_des3 \
	test_hmac \
	test_image \
	test_md4 \
	test_md5 \
	test_sha1 \
	test_sha256 \
	test_smiley \
	test_trie \
	test_util \
	test_xmlnode

test_des_SOURCES=test_des.c
test_des_LDADD=$(COMMON_LIBS)

test_des3_SOURCES=test_des3.c
test_des3_LDADD=$(COMMON_LIBS)

test_hmac_SOURCES=test_hmac.c
test_hmac_LDADD=$(COMMON_LIBS)

test_image_SOURCES=test_image.c
test_image_LDADD=$(COMMON_LIBS)

test_md4_SOURCES=test_md4.c
test_md4_LDADD=$(COMMON_LIBS)

test_md5_SOURCES=test_md5.c
test_md5_LDADD=$(COMMON_LIBS)

test_sha1_SOURCES=test_sha1.c
test_sha1_LDADD=$(COMMON_LIBS)

test_sha256_SOURCES=test_sha256.c
test_sha256_LDADD=$(COMMON_LIBS)

test_smiley_SOURCES=test_smiley.c
test_smiley_LDADD=$(COMMON_LIBS)

test_trie_SOURCES=test_trie.c
test_trie_LDADD=$(COMMON_LIBS)

test_util_SOURCES=test_util.c
test_util_LDADD=$(COMMON_LIBS)

test_xmlnode_SOURCES=test_xmlnode.c
test_xmlnode_LDADD=$(COMMON_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-DTEST_DATA_DIR=\"$(srcdir)/data\" \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GPLUGIN_CFLAGS) \
	$(PLUGIN_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(NSS_CFLAGS)

EXTRA_DIST = \
	data/test-image.png
