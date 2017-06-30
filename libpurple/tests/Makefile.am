include $(top_srcdir)/glib-tap.mk

COMMON_LIBS=\
	$(top_builddir)/libpurple/libpurple.la \
	$(GLIB_LIBS) \
	$(GPLUGIN_LIBS)

test_programs=\
	test_image \
	test_smiley \
	test_smiley_list \
	test_trie \
	test_util \
	test_xmlnode

test_image_SOURCES=test_image.c
test_image_LDADD=$(COMMON_LIBS)

test_smiley_SOURCES=test_smiley.c
test_smiley_LDADD=$(COMMON_LIBS)

test_smiley_list_SOURCES=test_smiley_list.c
test_smiley_list_LDADD=$(COMMON_LIBS)

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
	$(DBUS_CFLAGS)

EXTRA_DIST += \
	data/test-image.png
