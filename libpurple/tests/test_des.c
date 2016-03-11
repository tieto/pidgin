#include <glib.h>

#include <purple.h>

#include "ciphers/descipher.h"

static void
test_des_cipher(const gchar *in, const gchar *key, const gchar *out, size_t len) {
	PurpleCipher *cipher = NULL;
	guchar *decrypt = NULL, *encrypt = NULL, *answer = NULL;
	size_t ret = 0;

	decrypt = g_memdup(in, len + 1);
	encrypt = g_memdup(out, len + 1);

	cipher = purple_des_cipher_new();

	purple_cipher_set_key(cipher, (const guchar *)key, 8);

	answer = g_new0(guchar, len + 1);
	ret = purple_cipher_encrypt(cipher, decrypt, len, answer, len);
	g_assert(ret == len);
	g_assert_cmpmem(encrypt, len, answer, len);
	g_free(answer);

	answer = g_new0(guchar, len + 1);
	ret = purple_cipher_decrypt(cipher, encrypt, len, answer, len);
	g_assert(ret == len);
	g_assert_cmpmem(decrypt, len, answer, len);
	g_free(answer);

	g_free(encrypt);
	g_free(decrypt);

	g_object_unref(G_OBJECT(cipher));
}

static void
test_des_cipher_12345678(void) {
	test_des_cipher(
		"12345678",
		"\x3b\x38\x98\x37\x15\x20\xf7\x5e",
		"\x06\x22\x05\xac\x6a\x0d\x55\xdd",
		8
	);
}

static void
test_des_cipher_abcdefgh(void) {
	test_des_cipher(
		"abcdefgh",
		"\x3b\x38\x98\x37\x15\x20\xf7\x5e",
		"\x62\xe0\xc6\x8c\x48\xe4\x75\xed",
		8
	);
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/cipher/des/12345678",
	                test_des_cipher_12345678);

	g_test_add_func("/cipher/des/abcdefgh",
	                test_des_cipher_abcdefgh);

	return g_test_run();
}
