#include <glib.h>
#include <check.h>
#include <stdlib.h>
#include <string.h>

#undef HAVE_DBUS

#include "../cipher.h"
#include "../signal.h"

/******************************************************************************
 * MD4 Tests
 *****************************************************************************/
#define MD4_TEST(data, digest) { \
	GaimCipher *cipher = NULL; \
	GaimCipherContext *context = NULL; \
	gchar cdigest[33]; \
	gchar *sdigest = NULL; \
	gboolean ret = FALSE; \
	\
	cipher = gaim_ciphers_find_cipher("md4"); \
	context = gaim_cipher_context_new(cipher, NULL); \
	gaim_cipher_context_append(context, (guchar *)(data), strlen((data))); \
	\
	ret = gaim_cipher_context_digest_to_str(context, sizeof(cdigest), cdigest, \
	                                        NULL); \
	\
	fail_unless(ret == TRUE, NULL); \
	\
	fail_unless(strcmp((digest), cdigest) == 0, NULL); \
	\
	gaim_cipher_context_destroy(context); \
}

START_TEST(test_md4_empty_string) {
	MD4_TEST("", "31d6cfe0d16ae931b73c59d7e0c089c0");
}
END_TEST

START_TEST(test_md4_a) {
	MD4_TEST("a", "bde52cb31de33e46245e05fbdbd6fb24");
}
END_TEST

START_TEST(test_md4_abc) {
	MD4_TEST("abc", "a448017aaf21d8525fc10ae87aa6729d");
}
END_TEST

START_TEST(test_md4_message_digest) {
	MD4_TEST("message digest", "d9130a8164549fe818874806e1c7014b");
}
END_TEST

START_TEST(test_md4_a_to_z) {
	MD4_TEST("abcdefghijklmnopqrstuvwxyz",
			 "d79e1c308aa5bbcdeea8ed63df412da9");
}
END_TEST

START_TEST(test_md4_A_to_Z_a_to_z_0_to_9) {
	MD4_TEST("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
			 "043f8582f241db351ce627e153e7f0e4");
}
END_TEST

START_TEST(test_md4_1_to_0_8_times) {
	MD4_TEST("123456789012345678901234567890123456789012345678901234567890"
			 "12345678901234567890",
			 "e33b4ddc9c38f2199c3e7b164fcc0536");
}
END_TEST


/******************************************************************************
 * MD5 Tests
 *****************************************************************************/
#define MD5_TEST(data, digest) { \
	GaimCipher *cipher = NULL; \
	GaimCipherContext *context = NULL; \
	gchar cdigest[33]; \
	gchar *sdigest = NULL; \
	gboolean ret = FALSE; \
	\
	cipher = gaim_ciphers_find_cipher("md5"); \
	context = gaim_cipher_context_new(cipher, NULL); \
	gaim_cipher_context_append(context, (guchar *)(data), strlen((data))); \
	\
	ret = gaim_cipher_context_digest_to_str(context, sizeof(cdigest), cdigest, \
	                                        NULL); \
	\
	fail_unless(ret == TRUE, NULL); \
	\
	fail_unless(strcmp((digest), cdigest) == 0, NULL); \
	\
	gaim_cipher_context_destroy(context); \
}

START_TEST(test_md5_empty_string) {
	MD5_TEST("", "d41d8cd98f00b204e9800998ecf8427e");
}
END_TEST

START_TEST(test_md5_a) {
	MD5_TEST("a", "0cc175b9c0f1b6a831c399e269772661");
}
END_TEST

START_TEST(test_md5_abc) {
	MD5_TEST("abc", "900150983cd24fb0d6963f7d28e17f72");
}
END_TEST

START_TEST(test_md5_message_digest) {
	MD5_TEST("message digest", "f96b697d7cb7938d525a2f31aaf161d0");
}
END_TEST

START_TEST(test_md5_a_to_z) {
	MD5_TEST("abcdefghijklmnopqrstuvwxyz",
			 "c3fcd3d76192e4007dfb496cca67e13b");
}
END_TEST

START_TEST(test_md5_A_to_Z_a_to_z_0_to_9) {
	MD5_TEST("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
			 "d174ab98d277d9f5a5611c2c9f419d9f");
}
END_TEST

START_TEST(test_md5_1_to_0_8_times) {
	MD5_TEST("123456789012345678901234567890123456789012345678901234567890"
			 "12345678901234567890",
			 "57edf4a22be3c955ac49da2e2107b67a");
}
END_TEST

/******************************************************************************
 * SHA-1 Tests
 *****************************************************************************/
#define SHA1_TEST(data, digest) { \
	GaimCipher *cipher = NULL; \
	GaimCipherContext *context = NULL; \
	gchar cdigest[41]; \
	gchar *sdigest = NULL; \
	gboolean ret = FALSE; \
	\
	cipher = gaim_ciphers_find_cipher("sha1"); \
	context = gaim_cipher_context_new(cipher, NULL); \
	\
	if((data)) { \
		gaim_cipher_context_append(context, (guchar *)(data), strlen((data))); \
	} else { \
		gint j; \
		guchar buff[1000]; \
		\
		memset(buff, 'a', 1000); \
		\
		for(j = 0; j < 1000; j++) \
			gaim_cipher_context_append(context, buff, 1000); \
	} \
	\
	ret = gaim_cipher_context_digest_to_str(context, sizeof(cdigest), cdigest, \
	                                        NULL); \
	\
	fail_unless(ret == TRUE, NULL); \
	\
	fail_unless(strcmp((digest), cdigest) == 0, NULL); \
	\
	gaim_cipher_context_destroy(context); \
}

START_TEST(test_sha1_a) {
	SHA1_TEST("a", "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");
}
END_TEST

START_TEST(test_sha1_abc) {
	SHA1_TEST("abc", "a9993e364706816aba3e25717850c26c9cd0d89d");
}
END_TEST

START_TEST(test_sha1_abcd_gibberish) {
	SHA1_TEST("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
			  "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}
END_TEST

START_TEST(test_sha1_1000_as_1000_times) {
	SHA1_TEST(NULL, "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}
END_TEST

/******************************************************************************
 * Suite
 *****************************************************************************/
Suite *
cipher_suite(void) {
	Suite *s = suite_create("Cipher Suite");
	TCase *tc = NULL;
	
	/* md5 tests */
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
	tcase_add_test(tc, test_sha1_a);
	tcase_add_test(tc, test_sha1_abc);
	tcase_add_test(tc, test_sha1_abcd_gibberish);
	tcase_add_test(tc, test_sha1_1000_as_1000_times);
	suite_add_tcase(s, tc);

	return s;
}


