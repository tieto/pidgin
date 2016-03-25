#include <glib.h>

#include <purple.h>

#include "ciphers/md5hash.h"

static void
test_md5hash(gchar *data, gchar *digest) {
	PurpleHash *hash = NULL;
	gchar cdigest[33];
	gboolean ret = FALSE;

	hash = purple_md5_hash_new();

	purple_hash_append(hash, (guchar *)data, strlen(data));

	ret = purple_hash_digest_to_str(hash, cdigest, sizeof(cdigest));

	g_assert(ret);
	g_assert_cmpstr(digest, ==, cdigest);
}

static void
test_md5hash_empty_string(void) {
	test_md5hash("",
	             "d41d8cd98f00b204e9800998ecf8427e");
}

static void
test_md5hash_a(void) {
	test_md5hash("a",
	             "0cc175b9c0f1b6a831c399e269772661");
}

static void
test_md5hash_abc(void) {
	test_md5hash("abc",
	             "900150983cd24fb0d6963f7d28e17f72");
}

static void
test_md5hash_message_digest(void) {
	test_md5hash("message digest",
	             "f96b697d7cb7938d525a2f31aaf161d0");
}

static void
test_md5hash_a_to_z(void) {
	test_md5hash("abcdefghijklmnopqrstuvwxyz",
	             "c3fcd3d76192e4007dfb496cca67e13b");
}

static void
test_md5hash_A_to_Z_a_to_z_0_to_9(void) {
	test_md5hash("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
	             "d174ab98d277d9f5a5611c2c9f419d9f");
}

static void
test_md5hash_1_to_0_eight_times(void) {
	test_md5hash("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
	             "57edf4a22be3c955ac49da2e2107b67a");
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/hash/md5/empty-string",
	                test_md5hash_empty_string);
	g_test_add_func("/hash/md5/a",
	                test_md5hash_a);
	g_test_add_func("/hash/md5/abc",
	                test_md5hash_abc);
	g_test_add_func("/hash/md5/message digest",
	                test_md5hash_message_digest);
	g_test_add_func("/hash/md5/a to z",
	                test_md5hash_a_to_z);
	g_test_add_func("/hash/md5/A to Z, a to z, 0 to 9" ,
	                test_md5hash_A_to_Z_a_to_z_0_to_9);
	g_test_add_func("/hash/md5/1 to 0 eight times",
	                test_md5hash_1_to_0_eight_times);

	return g_test_run();
}
