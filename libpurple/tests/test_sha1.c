#include <glib.h>

#include <purple.h>

#include "ciphers/sha1hash.h"

static void
test_sha1hash(gchar *data, gchar *digest) {
	PurpleHash *hash = NULL;
	gchar cdigest[41];
	gboolean ret = FALSE;

	hash = purple_sha1_hash_new();

	if(data) {
		purple_hash_append(hash, (guchar *)data, strlen(data));
	} else {
		gint j;
		guchar buff[1000];

		memset(buff, 'a', 1000);

		for(j = 0; j < 1000; j++)
			purple_hash_append(hash, buff, 1000);
	}

	ret = purple_hash_digest_to_str(hash, cdigest, sizeof(cdigest));

	g_assert(ret);
	g_assert_cmpstr(digest, ==, cdigest);
}

static void
test_sha1hash_empty_string(void) {
	test_sha1hash("",
	              "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

static void
test_sha1hash_a(void) {
	test_sha1hash("a",
	              "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");
}

static void
test_sha1hash_abc(void) {
	test_sha1hash("abc",
	              "a9993e364706816aba3e25717850c26c9cd0d89d");
}

static void
test_sha1hash_abcd_gibberish(void) {
	test_sha1hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
	              "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

static void
test_sha1hash_1000_as_1000_times(void) {
	test_sha1hash(NULL,
	              "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/hash/sha1/empty-string",
	                test_sha1hash_empty_string);
	g_test_add_func("/hash/sha1/a",
	                test_sha1hash_a);
	g_test_add_func("/hash/sha1/abc",
	                test_sha1hash_abc);
	g_test_add_func("/hash/sha1/abcd_gibberish",
	                test_sha1hash_abcd_gibberish);
	g_test_add_func("/hash/sha1/1000 a's 1000 times",
	                test_sha1hash_1000_as_1000_times);

	return g_test_run();
}
