#include <glib.h>

#include <purple.h>

#include "ciphers/hmaccipher.h"
#include "ciphers/md5hash.h"
#include "ciphers/sha1hash.h"

/******************************************************************************
 * HMAC Tests
 * See RFC2202 and some other NULL tests I made up
 *****************************************************************************/
static void
test_hmac(gchar *data, size_t data_len,
          const gchar *key, size_t key_len,
          PurpleHash *hash, const gchar *digest)
{
	PurpleCipher *cipher = NULL;
	gchar cdigest[41];
	gboolean ret = FALSE;

	cipher = purple_hmac_cipher_new(hash);
	purple_cipher_set_key(cipher, (guchar *)key, key_len);

	purple_cipher_append(cipher, (guchar *)data, data_len);
	ret = purple_cipher_digest_to_str(cipher, cdigest, sizeof(cdigest));

	g_assert(ret);
	g_assert_cmpstr(digest, ==, cdigest);

	g_object_unref(G_OBJECT(cipher));
	g_object_unref(G_OBJECT(hash));
}

static void
test_hmac_md5_hi(void) {
	test_hmac(
		"Hi There",
		8,
		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
		16,
		purple_md5_hash_new(),
		"9294727a3638bb1c13f48ef8158bfc9d"
	);
}

static void
test_hmac_md5_what(void) {
	test_hmac(
		"what do ya want for nothing?",
		28,
		"Jefe",
		4,
		purple_md5_hash_new(),
		"750c783e6ab0b503eaa86e310a5db738"
	);
}

static void
test_hmac_md5_dd(void) {
	test_hmac(
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
		50,
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
		16,
		purple_md5_hash_new(),
		"56be34521d144c88dbb8c733f0e8b3f6"
	);
}

static void
test_hmac_md5_cd(void) {
	test_hmac(
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
		50,
		"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a"
		"\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
		"\x15\x16\x17\x18\x19",
		25,
		purple_md5_hash_new(),
		"697eaf0aca3a3aea3a75164746ffaa79"
	);
}

static void
test_hmac_md5_truncation(void) {
	test_hmac(
		"Test With Truncation",
		20,
		"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c",
		16,
		purple_md5_hash_new(),
		"56461ef2342edc00f9bab995690efd4c"
	);
}

static void
test_hmac_md5_large_key(void) {
	test_hmac(
		"Test Using Larger Than Block-Size Key - Hash Key First",
		54,
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
		80,
		purple_md5_hash_new(),
		"6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd"
	);
}

static void
test_hmac_md5_large_key_and_data(void) {
	test_hmac(
		"Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
		73,
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
		80,
		purple_md5_hash_new(),
		"6f630fad67cda0ee1fb1f562db3aa53e"
	);
}

static void
test_hmac_md5_null_key(void) {
	test_hmac(
		"Hi There",
		8,
		"\x0a\x0b\x00\x0d\x0e\x0f\x1a\x2f\x0b\x0b"
		"\x0b\x00\x00\x0b\x0b\x49\x5f\x6e\x0b\x0b",
		20,
		purple_md5_hash_new(),
		"597bfd644b797a985561eeb03a169e59"
	);
}

static void
test_hmac_md5_null_text(void) {
	test_hmac(
		"Hi\x00There",
		8,
		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
		20,
		purple_md5_hash_new(),
		"70be8e1b7b50dfcc335d6cd7992c564f"
	);
}

static void
test_hmac_md5_null_key_and_text(void) {
	test_hmac(
		"Hi\x00Th\x00re",
		8,
		"\x0c\x0d\x00\x0f\x10\x1a\x3a\x3a\xe6\x34"
		"\x0b\x00\x00\x0b\x0b\x49\x5f\x6e\x0b\x0b",
		20,
		purple_md5_hash_new(),
		"b31bcbba35a33a067cbba9131cba4889"
	);
}

static void
test_hmac_sha1_hi(void) {
	test_hmac(
		"Hi There",
		8,
		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
		20,
		purple_sha1_hash_new(),
		"b617318655057264e28bc0b6fb378c8ef146be00"
	);
}

static void
test_hmac_sha1_what(void) {
	test_hmac(
		"what do ya want for nothing?",
		28,
		"Jefe",
		4,
		purple_sha1_hash_new(),
		"effcdf6ae5eb2fa2d27416d5f184df9c259a7c79"
	);
}

static void
test_hmac_sha1_dd(void) {
	test_hmac(
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
		"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
		50,
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
		20,
		purple_sha1_hash_new(),
		"125d7342b9ac11cd91a39af48aa17b4f63f175d3"
	);
}

static void
test_hmac_sha1_cd(void) {
	test_hmac(
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
		"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
		50,
		"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a"
		"\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
		"\x15\x16\x17\x18\x19",
		25,
		purple_sha1_hash_new(),
		"4c9007f4026250c6bc8414f9bf50c86c2d7235da"
	);
}

static void
test_hmac_sha1_truncation(void) {
	test_hmac(
		"Test With Truncation",
		20,
		"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
		"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c",
		20,
		purple_sha1_hash_new(),
		"4c1a03424b55e07fe7f27be1d58bb9324a9a5a04"
	);
}

static void
test_hmac_sha1_large_key(void) {
	test_hmac(
		"Test Using Larger Than Block-Size Key - Hash Key First",
		54,
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
		80,
		purple_sha1_hash_new(),
		"aa4ae5e15272d00e95705637ce8a3b55ed402112"
	);
}

static void
test_hmac_sha1_large_key_and_data(void) {
	test_hmac(
		"Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
		73,
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
		"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
		80,
		purple_sha1_hash_new(),
		"e8e99d0f45237d786d6bbaa7965c7808bbff1a91"
	);
}

static void
test_hmac_sha1_null_key(void) {
	test_hmac(
		"Hi There",
		8,
		"\x0a\x0b\x00\x0d\x0e\x0f\x1a\x2f\x0b\x0b"
		"\x0b\x00\x00\x0b\x0b\x49\x5f\x6e\x0b\x0b",
		20,
		purple_sha1_hash_new(),
		"eb62a2e0e33d300be669c52aab3f591bc960aac5"
	);
}

static void
test_hmac_sha1_null_text(void) {
	test_hmac(
		"Hi\x00There",
		8,
		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
		"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
		20,
		purple_sha1_hash_new(),
		"31ca58d849e971e418e3439de2c6f83144b6abb7"
	);
}

static void
test_hmac_sha1_null_key_and_text(void) {
	test_hmac(
		"Hi\x00Th\x00re",
		8,
		"\x0c\x0d\x00\x0f\x10\x1a\x3a\x3a\xe6\x34"
		"\x0b\x00\x00\x0b\x0b\x49\x5f\x6e\x0b\x0b",
		20,
		purple_sha1_hash_new(),
		"e6b8e2fede87aa09dcb13e554df1435e056eae36"
	);
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/hmac/md5/hi",
	                test_hmac_md5_hi);
	g_test_add_func("/hmac/md5/what",
	                test_hmac_md5_what);
	g_test_add_func("/hmac/md5/dd",
	                test_hmac_md5_dd);
	g_test_add_func("/hmac/md5/cd",
	                test_hmac_md5_cd);
	g_test_add_func("/hmac/md5/truncation",
	                test_hmac_md5_truncation);
	g_test_add_func("/hmac/md5/large key",
	                test_hmac_md5_large_key);
	g_test_add_func("/hmac/md5/large key and data",
	                test_hmac_md5_large_key_and_data);
	g_test_add_func("/hmac/md5/null key",
	                test_hmac_md5_null_key);
	g_test_add_func("/hmac/md5/null text",
	                test_hmac_md5_null_text);
	g_test_add_func("/hmac/md5/null key and text",
	                test_hmac_md5_null_key_and_text);

	g_test_add_func("/hmac/sha1/hi",
	                test_hmac_sha1_hi);
	g_test_add_func("/hmac/sha1/what",
	                test_hmac_sha1_what);
	g_test_add_func("/hmac/sha1/dd",
	                test_hmac_sha1_dd);
	g_test_add_func("/hmac/sha1/cd",
	                test_hmac_sha1_cd);
	g_test_add_func("/hmac/sha1/truncation",
	                test_hmac_sha1_truncation);
	g_test_add_func("/hmac/sha1/large key",
	                test_hmac_sha1_large_key);
	g_test_add_func("/hmac/sha1/large key and data",
	                test_hmac_sha1_large_key_and_data);
	g_test_add_func("/hmac/sha1/null key",
	                test_hmac_sha1_null_key);
	g_test_add_func("/hmac/sha1/null text",
	                test_hmac_sha1_null_text);
	g_test_add_func("/hmac/sha1/null key and text",
	                test_hmac_sha1_null_key_and_text);

	return g_test_run();
}
