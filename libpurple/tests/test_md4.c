#include <glib.h>

#include <purple.h>

#include "ciphers/md4hash.h"

static void
test_md4hash(gchar *data, gchar *digest) {
	PurpleHash *hash = NULL;
	gchar cdigest[33];
	gboolean ret = FALSE;

	hash = purple_md4_hash_new();

	purple_hash_append(hash, (guchar *)data, strlen(data));

	ret = purple_hash_digest_to_str(hash, cdigest, sizeof(cdigest));

	g_assert(ret);
	g_assert_cmpstr(digest, ==, cdigest);
}

static void
test_md4hash_empty_string(void) {
	test_md4hash("",
	             "31d6cfe0d16ae931b73c59d7e0c089c0");
}

static void
test_md4hash_a(void) {
	test_md4hash("a",
	             "bde52cb31de33e46245e05fbdbd6fb24");
}

static void
test_md4hash_abc(void) {
	test_md4hash("abc",
	             "a448017aaf21d8525fc10ae87aa6729d");
}

static void
test_md4hash_message_digest(void) {
	test_md4hash("message digest",
	             "d9130a8164549fe818874806e1c7014b");
}

static void
test_md4hash_a_to_z(void) {
	test_md4hash("abcdefghijklmnopqrstuvwxyz",
	             "d79e1c308aa5bbcdeea8ed63df412da9");
}

static void
test_md4hash_A_to_Z_a_to_z_0_to_9(void) {
	test_md4hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
	             "043f8582f241db351ce627e153e7f0e4");
}

static void
test_md4hash_1_to_0_eight_times(void) {
	test_md4hash("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
	             "e33b4ddc9c38f2199c3e7b164fcc0536");
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/hash/md4/empty-string",
	                test_md4hash_empty_string);
	g_test_add_func("/hash/md4/a",
	                test_md4hash_a);
	g_test_add_func("/hash/md4/abc",
	                test_md4hash_abc);
	g_test_add_func("/hash/md4/message digest",
	                test_md4hash_message_digest);
	g_test_add_func("/hash/md4/a to z",
	                test_md4hash_a_to_z);
	g_test_add_func("/hash/md4/A to Z, a to z, 0 to 9" ,
	                test_md4hash_A_to_Z_a_to_z_0_to_9);
	g_test_add_func("/hash/md4/1 to 0 eight times",
	                test_md4hash_1_to_0_eight_times);

	return g_test_run();
}
