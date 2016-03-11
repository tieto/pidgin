#include <glib.h>

#include <purple.h>

#include "ciphers/des3cipher.h"

/******************************************************************************
 * DES3 Tests
 * See http://csrc.nist.gov/groups/ST/toolkit/examples.html
 * and some NULL things I made up
 *****************************************************************************/
static void
test_des3_cipher(const gchar *in, const gchar *key, const gchar *iv,
	             const gchar *out, size_t len, PurpleCipherBatchMode mode)
{
	PurpleCipher *cipher = NULL;
	guchar *decrypt = NULL, *encrypt = NULL, *answer = NULL;
	size_t ret = 0;

	decrypt = g_memdup(in, len + 1);
	encrypt = g_memdup(out, len + 1);

	cipher = purple_des3_cipher_new();

	purple_cipher_set_key(cipher, (const guchar *)key, 24);
	purple_cipher_set_batch_mode(cipher, mode);
	purple_cipher_set_iv(cipher, (guchar *)iv, 8);

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
test_des3_cipher_ecb_nist1(void) {
	test_des3_cipher(
		"\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
		"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51",
		"\x01\x23\x45\x67\x89\xAB\xCD\xEF"
		"\x23\x45\x67\x89\xAB\xCD\xEF\x01"
		"\x45\x67\x89\xAB\xCD\xEF\x01\x23",
		"00000000", /* ignored */
		"\x71\x47\x72\xF3\x39\x84\x1D\x34\x26\x7F\xCC\x4B\xD2\x94\x9C\xC3"
		"\xEE\x11\xC2\x2A\x57\x6A\x30\x38\x76\x18\x3F\x99\xC0\xB6\xDE\x87",
		32,
		PURPLE_CIPHER_BATCH_MODE_ECB
	);
}

static void
test_des3_cipher_ecb_nist2(void) {
	test_des3_cipher(
		"\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
		"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51",
		"\x01\x23\x45\x67\x89\xAB\xCD\xEF"
		"\x23\x45\x67\x89\xAB\xCD\xEF\x01"
		"\x01\x23\x45\x67\x89\xAB\xCD\xEF",
		"00000000", /* ignored */
		"\x06\xED\xE3\xD8\x28\x84\x09\x0A\xFF\x32\x2C\x19\xF0\x51\x84\x86"
		"\x73\x05\x76\x97\x2A\x66\x6E\x58\xB6\xC8\x8C\xF1\x07\x34\x0D\x3D",
		32,
		PURPLE_CIPHER_BATCH_MODE_ECB
	);
}

static void
test_des3_cipher_ecb_null_key(void) {
	test_des3_cipher(
		"\x16\xf4\xb3\x77\xfd\x4b\x9e\xca",
		"\x38\x00\x88\x6a\xef\xcb\x00\xad"
		"\x5d\xe5\x29\x00\x7d\x98\x64\x4c"
		"\x86\x00\x7b\xd3\xc7\x00\x7b\x32",
		"00000000", /* ignored */
		"\xc0\x60\x30\xa1\xb7\x25\x42\x44",
		8,
		PURPLE_CIPHER_BATCH_MODE_ECB
	);
}

static void
test_des3_cipher_ecb_null_text(void) {
	test_des3_cipher(
		"\x65\x73\x34\xc1\x19\x00\x79\x65",
		"\x32\x64\xda\x10\x13\x6a\xfe\x1e"
		"\x37\x54\xd1\x2c\x41\x04\x10\x40"
		"\xaf\x1c\x75\x2b\x51\x3a\x03\xf5",
		"00000000", /* ignored */
		"\xe5\x80\xf6\x12\xf8\x4e\xd9\x6c",
		8,
		PURPLE_CIPHER_BATCH_MODE_ECB
	);
}

static void
test_des3_cipher_ecb_null_key_and_text(void) {
	test_des3_cipher(
		"\xdf\x7f\x00\x92\xe7\xc1\x49\xd2",
		"\x0e\x41\x00\xc4\x8b\xf0\x6e\xa1"
		"\x66\x49\x42\x63\x22\x00\xf0\x99"
		"\x6b\x22\xc1\x37\x9c\x00\xe4\x8f",
		"00000000", /* ignored */
		"\x73\xd8\x1f\x1f\x50\x01\xe4\x79",
		8,
		PURPLE_CIPHER_BATCH_MODE_ECB
	);
}

static void
test_des3_cipher_cbc_nist1(void) {
	test_des3_cipher(
		"\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
		"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51",
		"\x01\x23\x45\x67\x89\xAB\xCD\xEF"
		"\x23\x45\x67\x89\xAB\xCD\xEF\x01"
		"\x45\x67\x89\xAB\xCD\xEF\x01\x23",
		"\xF6\x9F\x24\x45\xDF\x4F\x9B\x17",
		"\x20\x79\xC3\xD5\x3A\xA7\x63\xE1\x93\xB7\x9E\x25\x69\xAB\x52\x62"
		"\x51\x65\x70\x48\x1F\x25\xB5\x0F\x73\xC0\xBD\xA8\x5C\x8E\x0D\xA7",
		32,
		PURPLE_CIPHER_BATCH_MODE_CBC
	);
}

static void
test_des3_cipher_cbc_nist2(void) {
	test_des3_cipher(
		"\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
		"\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51",
		"\x01\x23\x45\x67\x89\xAB\xCD\xEF"
		"\x23\x45\x67\x89\xAB\xCD\xEF\x01"
		"\x01\x23\x45\x67\x89\xAB\xCD\xEF",
		"\xF6\x9F\x24\x45\xDF\x4F\x9B\x17",
		"\x74\x01\xCE\x1E\xAB\x6D\x00\x3C\xAF\xF8\x4B\xF4\x7B\x36\xCC\x21"
		"\x54\xF0\x23\x8F\x9F\xFE\xCD\x8F\x6A\xCF\x11\x83\x92\xB4\x55\x81",
		32,
		PURPLE_CIPHER_BATCH_MODE_CBC
	);
}

static void
test_des3_cipher_cbc_null_key(void) {
	test_des3_cipher(
		"\x16\xf4\xb3\x77\xfd\x4b\x9e\xca",
		"\x38\x00\x88\x6a\xef\xcb\x00\xad"
		"\x5d\xe5\x29\x00\x7d\x98\x64\x4c"
		"\x86\x00\x7b\xd3\xc7\x00\x7b\x32",
		"\x31\x32\x33\x34\x35\x36\x37\x38",
		"\x52\xe7\xde\x96\x39\x87\x87\xdb",
		8,
		PURPLE_CIPHER_BATCH_MODE_CBC
	);
}

static void
test_des3_cipher_cbc_null_text(void) {
	test_des3_cipher(
		"\x65\x73\x34\xc1\x19\x00\x79\x65",
		"\x32\x64\xda\x10\x13\x6a\xfe\x1e"
		"\x37\x54\xd1\x2c\x41\x04\x10\x40"
		"\xaf\x1c\x75\x2b\x51\x3a\x03\xf5",
		"\x7C\xAF\x0D\x57\x1E\x57\x10\xDA",
		"\x40\x12\x0e\x00\x85\xff\x6c\xc2",
		8,
		PURPLE_CIPHER_BATCH_MODE_CBC
	);
}

static void
test_des3_cipher_cbc_null_key_and_text(void) {
	test_des3_cipher(
		"\xdf\x7f\x00\x92\xe7\xc1\x49\xd2",
		"\x0e\x41\x00\xc4\x8b\xf0\x6e\xa1"
		"\x66\x49\x42\x63\x22\x00\xf0\x99"
		"\x6b\x22\xc1\x37\x9c\x00\xe4\x8f",
		"\x01\x19\x0D\x2c\x40\x67\x89\x67",
		"\xa7\xc1\x10\xbe\x9b\xd5\x8a\x67",
		8,
		PURPLE_CIPHER_BATCH_MODE_CBC
	);
}


gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/cipher/des3/ecb nist1",
	                test_des3_cipher_ecb_nist1);
	g_test_add_func("/cipher/des3/ecb nist2",
	                test_des3_cipher_ecb_nist2);
	g_test_add_func("/cipher/des3/ecb null key",
	                test_des3_cipher_ecb_null_key);
	g_test_add_func("/cipher/des3/ecb null text",
	                test_des3_cipher_ecb_null_text);
	g_test_add_func("/cipher/des3/ebc null key and text",
	                test_des3_cipher_ecb_null_key_and_text);

	g_test_add_func("/cipher/des3/cbc nist1",
	                test_des3_cipher_cbc_nist1);
	g_test_add_func("/cipher/des3/cbc nist2",
	                test_des3_cipher_cbc_nist2);
	g_test_add_func("/cipher/des3/cbc null key",
	                test_des3_cipher_cbc_null_key);
	g_test_add_func("/cipher/des3/cbc null text",
	                test_des3_cipher_cbc_null_text);
	g_test_add_func("/cipher/des3/cbc null key and text",
	                test_des3_cipher_cbc_null_key_and_text);

	return g_test_run();
}
