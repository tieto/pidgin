#include <glib.h>

#include <purple.h>

#include "ciphers/sha256hash.h"

static void
test_sha256hash(gchar *data, gchar *digest) {
	PurpleHash *hash = NULL;
	gchar cdigest[65];
	gboolean ret = FALSE;

	hash = purple_sha256_hash_new();

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
test_sha256hash_empty_string(void) {
	test_sha256hash("",
	                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

static void
test_sha256hash_a(void) {
	test_sha256hash("a",
	                "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");
}

static void
test_sha256hash_abc(void) {
	test_sha256hash("abc",
	                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

static void
test_sha256hash_abcd_gibberish(void) {
	test_sha256hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
	                "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

static void
test_sha256hash_1000_as_1000_times(void) {
	test_sha256hash(NULL,
	                "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/hash/sha256/empty-string",
	                test_sha256hash_empty_string);
	g_test_add_func("/hash/sha256/a",
	                test_sha256hash_a);
	g_test_add_func("/hash/sha256/abc",
	                test_sha256hash_abc);
	g_test_add_func("/hash/sha256/abcd_gibberish",
	                test_sha256hash_abcd_gibberish);
	g_test_add_func("/hash/sha256/1000 a's 1000 times",
	                test_sha256hash_1000_as_1000_times);

	return g_test_run();
}
