#include <string.h>

#include "tests.h"
#include "../protocols/yahoo/libymsg.h"

static void setup_codes_to_html(void)
{
	yahoo_init_colorht();
}

static void teardown_codes_to_html(void)
{
	yahoo_dest_colorht();
}

START_TEST(test_codes_to_html)
{
	assert_string_equal_free("plain", yahoo_codes_to_html("plain"));
	assert_string_equal_free("<b>bold", yahoo_codes_to_html("\x1B[1mbold"));
	assert_string_equal_free("<i>italic", yahoo_codes_to_html("\x1B[2mitalic"));
	assert_string_equal_free("<u>underline", yahoo_codes_to_html("\x1B[4munderline"));
	assert_string_equal_free("<b>bold</b> <i>italic</i> <u>underline", yahoo_codes_to_html("\x1B[1mbold\x1B[x1m \x1B[2mitalic\x1B[x2m \x1B[4munderline"));
	assert_string_equal_free("<span style=\"color: #0000FF\">blue", yahoo_codes_to_html("\x1B[31mblue"));
}
END_TEST

Suite *
yahoo_util_suite(void)
{
	Suite *s = suite_create("Yahoo Utility Functions");

	TCase *tc = tcase_create("Convert to Numeric");
	tcase_add_unchecked_fixture(tc, setup_codes_to_html, teardown_codes_to_html);
	tcase_add_test(tc, test_codes_to_html);
	suite_add_tcase(s, tc);

	return s;
}
