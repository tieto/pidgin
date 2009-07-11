#include <string.h>

#include "tests.h"
#include "../util.h"

START_TEST(test_util_base16_encode)
{
	assert_string_equal_free("68656c6c6f2c20776f726c642100", purple_base16_encode((const unsigned char *)"hello, world!", 14));
}
END_TEST

START_TEST(test_util_base16_decode)
{
	gsize sz = 0;
	guchar *out = purple_base16_decode("21646c726f77202c6f6c6c656800", &sz);
	fail_unless(sz == 14, NULL);
	assert_string_equal_free("!dlrow ,olleh", (char *)out);
}
END_TEST

START_TEST(test_util_base64_encode)
{
	assert_string_equal_free("Zm9ydHktdHdvAA==", purple_base64_encode((const unsigned char *)"forty-two", 10));
}
END_TEST

START_TEST(test_util_base64_decode)
{
	gsize sz;
	guchar *out = purple_base64_decode("b3d0LXl0cm9mAA==", &sz);
	fail_unless(sz == 10, NULL);
	assert_string_equal_free("owt-ytrof", (char *)out);
}
END_TEST

START_TEST(test_util_escape_filename)
{
	assert_string_equal("foo", purple_escape_filename("foo"));
	assert_string_equal("@oo", purple_escape_filename("@oo"));
	assert_string_equal("#oo", purple_escape_filename("#oo"));
	assert_string_equal("-oo", purple_escape_filename("-oo"));
	assert_string_equal("_oo", purple_escape_filename("_oo"));
	assert_string_equal(".oo", purple_escape_filename(".oo"));
	assert_string_equal("%25oo", purple_escape_filename("%oo"));
	assert_string_equal("%21oo", purple_escape_filename("!oo"));
}
END_TEST

START_TEST(test_util_unescape_filename)
{
	assert_string_equal("bar", purple_unescape_filename("bar"));
	assert_string_equal("@ar", purple_unescape_filename("@ar"));
	assert_string_equal("!ar", purple_unescape_filename("!ar"));
	assert_string_equal("!ar", purple_unescape_filename("%21ar"));
	assert_string_equal("%ar", purple_unescape_filename("%25ar"));
}
END_TEST


START_TEST(test_util_text_strip_mnemonic)
{
	assert_string_equal_free("", purple_text_strip_mnemonic(""));
	assert_string_equal_free("foo", purple_text_strip_mnemonic("foo"));
	assert_string_equal_free("foo", purple_text_strip_mnemonic("_foo"));

}
END_TEST

START_TEST(test_util_email_is_valid)
{
	fail_unless(purple_email_is_valid("purple-devel@lists.sf.net"));
	fail_if(purple_email_is_valid("purple-devel@@lists.sf.net"));
	fail_if(purple_email_is_valid("purple@devel@lists.sf.net"));
	fail_if(purple_email_is_valid("purple-devel@list..sf.net"));
	fail_if(purple_email_is_valid("purple-devel"));
	fail_if(purple_email_is_valid("purple-devel@"));
	fail_if(purple_email_is_valid("@lists.sf.net"));
	fail_if(purple_email_is_valid(""));
	fail_if(purple_email_is_valid("totally bogus"));
}
END_TEST

START_TEST(test_util_ipv6_is_valid)
{
	fail_unless(purple_ipv6_address_is_valid("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
	fail_unless(purple_ipv6_address_is_valid("2001:db8:85a3:0:0:8a2e:370:7334"));
	fail_unless(purple_ipv6_address_is_valid("2001:db8:85a3::8a2e:370:7334"));
	fail_unless(purple_ipv6_address_is_valid("2001:0db8:0:0::1428:57ab"));
	fail_unless(purple_ipv6_address_is_valid("::1"));
	fail_unless(purple_ipv6_address_is_valid("1::"));
	fail_unless(purple_ipv6_address_is_valid("1::1"));
	fail_unless(purple_ipv6_address_is_valid("::"));
	fail_if(purple_ipv6_address_is_valid(""));
	fail_if(purple_ipv6_address_is_valid(":"));
	fail_if(purple_ipv6_address_is_valid("1.2.3.4"));
	fail_if(purple_ipv6_address_is_valid("2001::FFD3::57ab"));
	fail_if(purple_ipv6_address_is_valid("200000000::1"));
	fail_if(purple_ipv6_address_is_valid("QWERTY::1"));
}
END_TEST

START_TEST(test_util_str_to_time)
{
	fail_unless(377182200 == purple_str_to_time("19811214T12:50:00", TRUE, NULL, NULL, NULL));
	fail_unless(1175919261 == purple_str_to_time("20070407T04:14:21", TRUE, NULL, NULL, NULL));
}
END_TEST

START_TEST(test_markup_html_to_xhtml)
{
	gchar *xhtml = NULL;
	gchar *plaintext = NULL;
	purple_markup_html_to_xhtml("<a>", &xhtml, &plaintext);
	assert_string_equal_free("<a href=\"\"></a>", xhtml);
	assert_string_equal_free("", plaintext);
}
END_TEST

START_TEST(test_mime_decode_field)
{
	gchar *result = purple_mime_decode_field("=?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?=");
	assert_string_equal_free("Keld Jørn Simonsen", result);
}
END_TEST

Suite *
util_suite(void)
{
	Suite *s = suite_create("Utility Functions");

	TCase *tc = tcase_create("Base16");
	tcase_add_test(tc, test_util_base16_encode);
	tcase_add_test(tc, test_util_base16_decode);
	suite_add_tcase(s, tc);

	tc = tcase_create("Base64");
	tcase_add_test(tc, test_util_base64_encode);
	tcase_add_test(tc, test_util_base64_decode);
	suite_add_tcase(s, tc);

	tc = tcase_create("Filenames");
	tcase_add_test(tc, test_util_escape_filename);
	tcase_add_test(tc, test_util_unescape_filename);
	suite_add_tcase(s, tc);

	tc = tcase_create("Strip Mnemonic");
	tcase_add_test(tc, test_util_text_strip_mnemonic);
	suite_add_tcase(s, tc);

	tc = tcase_create("Email");
	tcase_add_test(tc, test_util_email_is_valid);
	suite_add_tcase(s, tc);

	tc = tcase_create("IPv6");
	tcase_add_test(tc, test_util_ipv6_is_valid);
	suite_add_tcase(s, tc);

	tc = tcase_create("Time");
	tcase_add_test(tc, test_util_str_to_time);
	suite_add_tcase(s, tc);

	tc = tcase_create("Markup");
	tcase_add_test(tc, test_markup_html_to_xhtml);
	suite_add_tcase(s, tc);

	tc = tcase_create("MIME");
	tcase_add_test(tc, test_mime_decode_field);
	suite_add_tcase(s, tc);

	return s;
}
