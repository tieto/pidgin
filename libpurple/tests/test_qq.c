#include <string.h>

#include "tests.h"
#include "../protocols/qq/qq_crypt.h"

START_TEST(test_qq_encrypt)
{
	const guint8 * const key = (guint8 *)"hamburger";
	guint8 crypted[80];
	gint ret;

	ret = qq_encrypt(crypted, (const guint8 * const)"a", 1, key);
	assert_int_equal(16, ret);

	ret = qq_encrypt(crypted, (const guint8 * const)"aa", 2, key);
	assert_int_equal(16, ret);

	ret = qq_encrypt(crypted, (const guint8 * const)"aaa", 3, key);
	assert_int_equal(16, ret);

	ret = qq_encrypt(crypted, (const guint8 * const)"aaaa", 4, key);
	assert_int_equal(16, ret);

	ret = qq_encrypt(crypted, (const guint8 * const)"aaaaa", 5, key);
	assert_int_equal(16, ret);

	ret = qq_encrypt(crypted, (const guint8 * const)"aaaaaa", 6, key);
	assert_int_equal(16, ret);

	ret = qq_encrypt(crypted, (const guint8 * const)"aaaaaaa", 7, key);
	assert_int_equal(24, ret);

	ret = qq_encrypt(crypted, (const guint8 * const)"aaaaaaaa", 8, key);
	assert_int_equal(24, ret);

	ret = qq_encrypt(crypted, (const guint8 * const)"aaaaaaaaa", 9, key);
	assert_int_equal(24, ret);

	ret = qq_encrypt(crypted, (const guint8 * const)"aaaaaaaaaa", 10, key);
	assert_int_equal(24, ret);

	ret = qq_encrypt(crypted,
			(const guint8 * const)"aaaaaaaaaaa", 11, key);
	assert_int_equal(24, ret);

	ret = qq_encrypt(crypted,
			(const guint8 * const)"aaaaaaaaaaaa", 12, key);
	assert_int_equal(24, ret);

	ret = qq_encrypt(crypted,
			(const guint8 * const)"aaaaaaaaaaaaa", 13, key);
	assert_int_equal(24, ret);

	ret = qq_encrypt(crypted,
			(const guint8 * const)"aaaaaaaaaaaaaa", 14, key);
	assert_int_equal(24, ret);

	ret = qq_encrypt(crypted,
			(const guint8 * const)"aaaaaaaaaaaaaaa", 15, key);
	assert_int_equal(32, ret);

	ret = qq_encrypt(crypted,
			(const guint8 * const)"aaaaaaaaaaaaaaaa", 16, key);
	assert_int_equal(32, ret);

	ret = qq_encrypt(crypted,
			(const guint8 * const)"aaaaaaaaaaaaaaaaa", 17, key);
	assert_int_equal(32, ret);

	ret = qq_encrypt(crypted,
			(const guint8 * const)"aaaaaaaaaaaaaaaaaa", 18, key);
	assert_int_equal(32, ret);

	ret = qq_encrypt(crypted,
			(const guint8 * const)"aaaaaaaaaaaaaaaaaaa", 19, key);
	assert_int_equal(32, ret);

	/*
	fprintf(stderr, "crypted=%s\n", crypted);
	assert_string_equal_free("plain",
			yahoo_codes_to_html("plain"));
	*/
}
END_TEST

START_TEST(test_qq_decrypt)
{
}
END_TEST

Suite *
qq_suite(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("QQ");

	tc = tcase_create("QQ Crypt Functions");
	tcase_add_test(tc, test_qq_encrypt);
	tcase_add_test(tc, test_qq_decrypt);
	suite_add_tcase(s, tc);

	return s;
}
