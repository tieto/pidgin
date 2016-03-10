#include <glib.h>
#include <check.h>
#include <stdlib.h>
#include <string.h>

#undef HAVE_DBUS

#include "tests.h"

#include "../ciphers/des3cipher.h"
#include "../ciphers/descipher.h"
#include "../ciphers/hmaccipher.h"
#include "../ciphers/md4hash.h"
#include "../ciphers/md5hash.h"
#include "../ciphers/sha1hash.h"
#include "../ciphers/sha256hash.h"

/******************************************************************************
 * DES Tests
 *****************************************************************************/
#define DES_TEST(in, keyz, out, len) { \
	PurpleCipher *cipher = NULL; \
	guchar answer[len+1]; \
	gint ret = 0; \
	guchar decrypt[len+1] = in; \
	guchar key[8+1] = keyz;\
	guchar encrypt[len+1] = out;\
	\
	cipher = purple_des_cipher_new(); \
	purple_cipher_set_key(cipher, key, 8); \
	\
	ret = purple_cipher_encrypt(cipher, decrypt, len, answer, len); \
	fail_unless(ret == len, NULL); \
	fail_unless(memcmp(encrypt, answer, len) == 0, NULL); \
	\
	ret = purple_cipher_decrypt(cipher, encrypt, len, answer, len); \
	fail_unless(ret == len, NULL); \
	fail_unless(memcmp(decrypt, answer, len) == 0, NULL); \
	\
	g_object_unref(cipher); \
}

START_TEST(test_des_12345678) {
	DES_TEST("12345678",
	         "\x3b\x38\x98\x37\x15\x20\xf7\x5e",
	         "\x06\x22\x05\xac\x6a\x0d\x55\xdd",
	         8);
}
END_TEST

START_TEST(test_des_abcdefgh) {
	DES_TEST("abcdefgh",
	         "\x3b\x38\x98\x37\x15\x20\xf7\x5e",
	         "\x62\xe0\xc6\x8c\x48\xe4\x75\xed",
	         8);
}
END_TEST

/******************************************************************************
 * DES3 Tests
 * See http://csrc.nist.gov/groups/ST/toolkit/examples.html
 * and some NULL things I made up
 *****************************************************************************/

#define DES3_TEST(in, key, iv, out, len, mode) { \
	PurpleCipher *cipher = NULL; \
	guchar answer[len+1]; \
	guchar decrypt[len+1] = in; \
	guchar encrypt[len+1] = out; \
	gint ret = 0; \
	\
	cipher = purple_des3_cipher_new(); \
	purple_cipher_set_key(cipher, (guchar *)key, 24); \
	purple_cipher_set_batch_mode(cipher, (mode)); \
	purple_cipher_set_iv(cipher, (guchar *)iv, 8); \
	\
	ret = purple_cipher_encrypt(cipher, decrypt, len, answer, len); \
	fail_unless(ret == len, NULL); \
	fail_unless(memcmp(encrypt, answer, len) == 0, NULL); \
	\
	ret = purple_cipher_decrypt(cipher, encrypt, len, answer, len); \
	fail_unless(ret == len, NULL); \
	fail_unless(memcmp(decrypt, answer, len) == 0, NULL); \
	\
	g_object_unref(cipher); \
}

START_TEST(test_des3_ecb_nist1) {
	DES3_TEST(
	          "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
	          "\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51",
	          "\x01\x23\x45\x67\x89\xAB\xCD\xEF"
	          "\x23\x45\x67\x89\xAB\xCD\xEF\x01"
	          "\x45\x67\x89\xAB\xCD\xEF\x01\x23",
	          "00000000", /* ignored */
	          "\x71\x47\x72\xF3\x39\x84\x1D\x34\x26\x7F\xCC\x4B\xD2\x94\x9C\xC3"
	          "\xEE\x11\xC2\x2A\x57\x6A\x30\x38\x76\x18\x3F\x99\xC0\xB6\xDE\x87",
	          32,
	          PURPLE_CIPHER_BATCH_MODE_ECB);
}
END_TEST

START_TEST(test_des3_ecb_nist2) {
	DES3_TEST(
	          "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
	          "\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51",
	          "\x01\x23\x45\x67\x89\xAB\xCD\xEF"
	          "\x23\x45\x67\x89\xAB\xCD\xEF\x01"
	          "\x01\x23\x45\x67\x89\xAB\xCD\xEF",
	          "00000000", /* ignored */
	          "\x06\xED\xE3\xD8\x28\x84\x09\x0A\xFF\x32\x2C\x19\xF0\x51\x84\x86"
	          "\x73\x05\x76\x97\x2A\x66\x6E\x58\xB6\xC8\x8C\xF1\x07\x34\x0D\x3D",
	          32,
	          PURPLE_CIPHER_BATCH_MODE_ECB);
}
END_TEST

START_TEST(test_des3_ecb_null_key) {
	DES3_TEST(
	          "\x16\xf4\xb3\x77\xfd\x4b\x9e\xca",
	          "\x38\x00\x88\x6a\xef\xcb\x00\xad"
	          "\x5d\xe5\x29\x00\x7d\x98\x64\x4c"
	          "\x86\x00\x7b\xd3\xc7\x00\x7b\x32",
	          "00000000", /* ignored */
	          "\xc0\x60\x30\xa1\xb7\x25\x42\x44",
	          8,
	          PURPLE_CIPHER_BATCH_MODE_ECB);
}
END_TEST

START_TEST(test_des3_ecb_null_text) {
	DES3_TEST(
	          "\x65\x73\x34\xc1\x19\x00\x79\x65",
	          "\x32\x64\xda\x10\x13\x6a\xfe\x1e"
	          "\x37\x54\xd1\x2c\x41\x04\x10\x40"
	          "\xaf\x1c\x75\x2b\x51\x3a\x03\xf5",
	          "00000000", /* ignored */
	          "\xe5\x80\xf6\x12\xf8\x4e\xd9\x6c",
	          8,
	          PURPLE_CIPHER_BATCH_MODE_ECB);
}
END_TEST

START_TEST(test_des3_ecb_null_key_and_text) {
	DES3_TEST(
	          "\xdf\x7f\x00\x92\xe7\xc1\x49\xd2",
	          "\x0e\x41\x00\xc4\x8b\xf0\x6e\xa1"
	          "\x66\x49\x42\x63\x22\x00\xf0\x99"
	          "\x6b\x22\xc1\x37\x9c\x00\xe4\x8f",
	          "00000000", /* ignored */
	          "\x73\xd8\x1f\x1f\x50\x01\xe4\x79",
	          8,
	          PURPLE_CIPHER_BATCH_MODE_ECB);
}
END_TEST

START_TEST(test_des3_cbc_nist1) {
	DES3_TEST(
	          "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
	          "\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51",
	          "\x01\x23\x45\x67\x89\xAB\xCD\xEF"
	          "\x23\x45\x67\x89\xAB\xCD\xEF\x01"
	          "\x45\x67\x89\xAB\xCD\xEF\x01\x23",
	          "\xF6\x9F\x24\x45\xDF\x4F\x9B\x17",
	          "\x20\x79\xC3\xD5\x3A\xA7\x63\xE1\x93\xB7\x9E\x25\x69\xAB\x52\x62"
	          "\x51\x65\x70\x48\x1F\x25\xB5\x0F\x73\xC0\xBD\xA8\x5C\x8E\x0D\xA7",
	          32,
	          PURPLE_CIPHER_BATCH_MODE_CBC);
}
END_TEST

START_TEST(test_des3_cbc_nist2) {
	DES3_TEST(
	          "\x6B\xC1\xBE\xE2\x2E\x40\x9F\x96\xE9\x3D\x7E\x11\x73\x93\x17\x2A"
	          "\xAE\x2D\x8A\x57\x1E\x03\xAC\x9C\x9E\xB7\x6F\xAC\x45\xAF\x8E\x51",
	          "\x01\x23\x45\x67\x89\xAB\xCD\xEF"
	          "\x23\x45\x67\x89\xAB\xCD\xEF\x01"
	          "\x01\x23\x45\x67\x89\xAB\xCD\xEF",
	          "\xF6\x9F\x24\x45\xDF\x4F\x9B\x17",
	          "\x74\x01\xCE\x1E\xAB\x6D\x00\x3C\xAF\xF8\x4B\xF4\x7B\x36\xCC\x21"
	          "\x54\xF0\x23\x8F\x9F\xFE\xCD\x8F\x6A\xCF\x11\x83\x92\xB4\x55\x81",
	          32,
	          PURPLE_CIPHER_BATCH_MODE_CBC);
}
END_TEST

START_TEST(test_des3_cbc_null_key) {
	DES3_TEST(
	          "\x16\xf4\xb3\x77\xfd\x4b\x9e\xca",
	          "\x38\x00\x88\x6a\xef\xcb\x00\xad"
	          "\x5d\xe5\x29\x00\x7d\x98\x64\x4c"
	          "\x86\x00\x7b\xd3\xc7\x00\x7b\x32",
	          "\x31\x32\x33\x34\x35\x36\x37\x38",
	          "\x52\xe7\xde\x96\x39\x87\x87\xdb",
	          8,
	          PURPLE_CIPHER_BATCH_MODE_CBC);
}
END_TEST

START_TEST(test_des3_cbc_null_text) {
	DES3_TEST(
	          "\x65\x73\x34\xc1\x19\x00\x79\x65",
	          "\x32\x64\xda\x10\x13\x6a\xfe\x1e"
	          "\x37\x54\xd1\x2c\x41\x04\x10\x40"
	          "\xaf\x1c\x75\x2b\x51\x3a\x03\xf5",
	          "\x7C\xAF\x0D\x57\x1E\x57\x10\xDA",
	          "\x40\x12\x0e\x00\x85\xff\x6c\xc2",
	          8,
	          PURPLE_CIPHER_BATCH_MODE_CBC);
}
END_TEST

START_TEST(test_des3_cbc_null_key_and_text) {
	DES3_TEST(
	          "\xdf\x7f\x00\x92\xe7\xc1\x49\xd2",
	          "\x0e\x41\x00\xc4\x8b\xf0\x6e\xa1"
	          "\x66\x49\x42\x63\x22\x00\xf0\x99"
	          "\x6b\x22\xc1\x37\x9c\x00\xe4\x8f",
	          "\x01\x19\x0D\x2c\x40\x67\x89\x67",
	          "\xa7\xc1\x10\xbe\x9b\xd5\x8a\x67",
	          8,
	          PURPLE_CIPHER_BATCH_MODE_CBC);
}
END_TEST

/******************************************************************************
 * HMAC Tests
 * See RFC2202 and some other NULL tests I made up
 *****************************************************************************/

#define HMAC_TEST(data, data_len, key, key_len, type, digest) { \
	PurpleCipher *cipher = NULL; \
	PurpleHash *hash = NULL; \
	gchar cdigest[41]; \
	gboolean ret = FALSE; \
	\
	hash = purple_##type##_hash_new(); \
	cipher = purple_hmac_cipher_new(hash); \
	purple_cipher_set_key(cipher, (guchar *)key, (key_len)); \
	\
	purple_cipher_append(cipher, (guchar *)(data), (data_len)); \
	ret = purple_cipher_digest_to_str(cipher, cdigest, sizeof(cdigest)); \
	\
	fail_unless(ret == TRUE, NULL); \
	fail_unless(strcmp((digest), cdigest) == 0, NULL); \
	\
	g_object_unref(cipher); \
	g_object_unref(hash); \
}

/* HMAC MD5 */

START_TEST(test_hmac_md5_Hi) {
	HMAC_TEST("Hi There",
	          8,
	          "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
	          16,
	          md5,
	          "9294727a3638bb1c13f48ef8158bfc9d");
}
END_TEST

START_TEST(test_hmac_md5_what) {
	HMAC_TEST("what do ya want for nothing?",
	          28,
	          "Jefe",
	          4,
	          md5,
	          "750c783e6ab0b503eaa86e310a5db738");
}
END_TEST

START_TEST(test_hmac_md5_dd) {
	HMAC_TEST("\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	          "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	          "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	          "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	          "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
	          50,
	          "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
	          16,
	          md5,
	          "56be34521d144c88dbb8c733f0e8b3f6");
}
END_TEST

START_TEST(test_hmac_md5_cd) {
	HMAC_TEST("\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	          "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	          "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	          "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	          "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
	          50,
	          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a"
	          "\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
	          "\x15\x16\x17\x18\x19",
	          25,
	          md5,
	          "697eaf0aca3a3aea3a75164746ffaa79");
}
END_TEST

START_TEST(test_hmac_md5_truncation) {
	HMAC_TEST("Test With Truncation",
	          20,
	          "\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c",
	          16,
	          md5,
	          "56461ef2342edc00f9bab995690efd4c");
}
END_TEST

START_TEST(test_hmac_md5_large_key) {
	HMAC_TEST("Test Using Larger Than Block-Size Key - Hash Key First",
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
	          md5,
	          "6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd");
}
END_TEST

START_TEST(test_hmac_md5_large_key_and_data) {
	HMAC_TEST("Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
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
	          md5,
	          "6f630fad67cda0ee1fb1f562db3aa53e");
}
END_TEST

START_TEST(test_hmac_md5_null_key) {
	HMAC_TEST("Hi There",
	          8,
	          "\x0a\x0b\x00\x0d\x0e\x0f\x1a\x2f\x0b\x0b"
	          "\x0b\x00\x00\x0b\x0b\x49\x5f\x6e\x0b\x0b",
	          20,
	          md5,
	          "597bfd644b797a985561eeb03a169e59");
}
END_TEST

START_TEST(test_hmac_md5_null_text) {
	HMAC_TEST("Hi\x00There",
	          8,
	          "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
	          "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
	          20,
	          md5,
	          "70be8e1b7b50dfcc335d6cd7992c564f");
}
END_TEST

START_TEST(test_hmac_md5_null_key_and_text) {
	HMAC_TEST("Hi\x00Th\x00re",
	          8,
	          "\x0c\x0d\x00\x0f\x10\x1a\x3a\x3a\xe6\x34"
	          "\x0b\x00\x00\x0b\x0b\x49\x5f\x6e\x0b\x0b",
	          20,
	          md5,
	          "b31bcbba35a33a067cbba9131cba4889");
}
END_TEST

/* HMAC SHA1 */

START_TEST(test_hmac_sha1_Hi) {
	HMAC_TEST("Hi There",
	          8,
	          "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
	          "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
	          20,
	          sha1,
	          "b617318655057264e28bc0b6fb378c8ef146be00");
}
END_TEST

START_TEST(test_hmac_sha1_what) {
	HMAC_TEST("what do ya want for nothing?",
	          28,
	          "Jefe",
	          4,
	          sha1,
	          "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
}
END_TEST

START_TEST(test_hmac_sha1_dd) {
	HMAC_TEST("\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	          "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	          "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	          "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd"
	          "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
	          50,
	          "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
	          "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
	          20,
	          sha1,
	          "125d7342b9ac11cd91a39af48aa17b4f63f175d3");
}
END_TEST

START_TEST(test_hmac_sha1_cd) {
	HMAC_TEST("\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	          "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	          "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	          "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd"
	          "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
	          50,
	          "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a"
	          "\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
	          "\x15\x16\x17\x18\x19",
	          25,
	          sha1,
	          "4c9007f4026250c6bc8414f9bf50c86c2d7235da");
}
END_TEST

START_TEST(test_hmac_sha1_truncation) {
	HMAC_TEST("Test With Truncation",
	          20,
	          "\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c"
	          "\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c",
	          20,
	          sha1,
	          "4c1a03424b55e07fe7f27be1d58bb9324a9a5a04");
}
END_TEST

START_TEST(test_hmac_sha1_large_key) {
	HMAC_TEST("Test Using Larger Than Block-Size Key - Hash Key First",
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
	          sha1,
	          "aa4ae5e15272d00e95705637ce8a3b55ed402112");
}
END_TEST

START_TEST(test_hmac_sha1_large_key_and_data) {
	HMAC_TEST("Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
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
	          sha1,
	          "e8e99d0f45237d786d6bbaa7965c7808bbff1a91");
}
END_TEST

START_TEST(test_hmac_sha1_null_key) {
	HMAC_TEST("Hi There",
	          8,
	          "\x0a\x0b\x00\x0d\x0e\x0f\x1a\x2f\x0b\x0b"
	          "\x0b\x00\x00\x0b\x0b\x49\x5f\x6e\x0b\x0b",
	          20,
	          sha1,
	          "eb62a2e0e33d300be669c52aab3f591bc960aac5");
}
END_TEST

START_TEST(test_hmac_sha1_null_text) {
	HMAC_TEST("Hi\x00There",
	          8,
	          "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
	          "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
	          20,
	          sha1,
	          "31ca58d849e971e418e3439de2c6f83144b6abb7");
}
END_TEST

START_TEST(test_hmac_sha1_null_key_and_text) {
	HMAC_TEST("Hi\x00Th\x00re",
	          8,
	          "\x0c\x0d\x00\x0f\x10\x1a\x3a\x3a\xe6\x34"
	          "\x0b\x00\x00\x0b\x0b\x49\x5f\x6e\x0b\x0b",
	          20,
	          sha1,
	          "e6b8e2fede87aa09dcb13e554df1435e056eae36");
}
END_TEST

/******************************************************************************
 * Suite
 *****************************************************************************/
Suite *
cipher_suite(void) {
	Suite *s = suite_create("Cipher Suite");
	TCase *tc = NULL;

	/* md4 tests */
	tc = tcase_create("MD4");
	tcase_add_test(tc, test_md4_empty_string);
	tcase_add_test(tc, test_md4_a);
	tcase_add_test(tc, test_md4_abc);
	tcase_add_test(tc, test_md4_message_digest);
	tcase_add_test(tc, test_md4_a_to_z);
	tcase_add_test(tc, test_md4_A_to_Z_a_to_z_0_to_9);
	tcase_add_test(tc, test_md4_1_to_0_8_times);
	suite_add_tcase(s, tc);

	/* md5 tests */
	tc = tcase_create("MD5");
	tcase_add_test(tc, test_md5_empty_string);
	tcase_add_test(tc, test_md5_a);
	tcase_add_test(tc, test_md5_abc);
	tcase_add_test(tc, test_md5_message_digest);
	tcase_add_test(tc, test_md5_a_to_z);
	tcase_add_test(tc, test_md5_A_to_Z_a_to_z_0_to_9);
	tcase_add_test(tc, test_md5_1_to_0_8_times);
	suite_add_tcase(s, tc);

	/* sha1 tests */
	tc = tcase_create("SHA1");
	tcase_add_test(tc, test_sha1_empty_string);
	tcase_add_test(tc, test_sha1_a);
	tcase_add_test(tc, test_sha1_abc);
	tcase_add_test(tc, test_sha1_abcd_gibberish);
	tcase_add_test(tc, test_sha1_1000_as_1000_times);
	suite_add_tcase(s, tc);

	/* sha256 tests */
	tc = tcase_create("SHA256");
	tcase_add_test(tc, test_sha256_empty_string);
	tcase_add_test(tc, test_sha256_a);
	tcase_add_test(tc, test_sha256_abc);
	tcase_add_test(tc, test_sha256_abcd_gibberish);
	tcase_add_test(tc, test_sha256_1000_as_1000_times);
	suite_add_tcase(s, tc);

	/* des tests */
	tc = tcase_create("DES");
	tcase_add_test(tc, test_des_12345678);
	tcase_add_test(tc, test_des_abcdefgh);
	suite_add_tcase(s, tc);

	/* des3 ecb tests */
	tc = tcase_create("DES3 ECB");
	tcase_add_test(tc, test_des3_ecb_nist1);
	tcase_add_test(tc, test_des3_ecb_nist2);
	tcase_add_test(tc, test_des3_ecb_null_key);
	tcase_add_test(tc, test_des3_ecb_null_text);
	tcase_add_test(tc, test_des3_ecb_null_key_and_text);
	suite_add_tcase(s, tc);
	/* des3 cbc tests */
	tc = tcase_create("DES3 CBC");
	tcase_add_test(tc, test_des3_cbc_nist1);
	tcase_add_test(tc, test_des3_cbc_nist2);
	tcase_add_test(tc, test_des3_cbc_null_key);
	tcase_add_test(tc, test_des3_cbc_null_text);
	tcase_add_test(tc, test_des3_cbc_null_key_and_text);
	suite_add_tcase(s, tc);

	/* hmac tests */
	tc = tcase_create("HMAC");
	tcase_add_test(tc, test_hmac_md5_Hi);
	tcase_add_test(tc, test_hmac_md5_what);
	tcase_add_test(tc, test_hmac_md5_dd);
	tcase_add_test(tc, test_hmac_md5_cd);
	tcase_add_test(tc, test_hmac_md5_truncation);
	tcase_add_test(tc, test_hmac_md5_large_key);
	tcase_add_test(tc, test_hmac_md5_large_key_and_data);
	tcase_add_test(tc, test_hmac_md5_null_key);
	tcase_add_test(tc, test_hmac_md5_null_text);
	tcase_add_test(tc, test_hmac_md5_null_key_and_text);
	tcase_add_test(tc, test_hmac_sha1_Hi);
	tcase_add_test(tc, test_hmac_sha1_what);
	tcase_add_test(tc, test_hmac_sha1_dd);
	tcase_add_test(tc, test_hmac_sha1_cd);
	tcase_add_test(tc, test_hmac_sha1_truncation);
	tcase_add_test(tc, test_hmac_sha1_large_key);
	tcase_add_test(tc, test_hmac_sha1_large_key_and_data);
	tcase_add_test(tc, test_hmac_sha1_null_key);
	tcase_add_test(tc, test_hmac_sha1_null_text);
	tcase_add_test(tc, test_hmac_sha1_null_key_and_text);
	suite_add_tcase(s, tc);

	return s;
}


