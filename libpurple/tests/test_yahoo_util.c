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
	assert_string_equal_free("plain",
			yahoo_codes_to_html("plain"));
	assert_string_equal_free("unknown  ansi code",
			yahoo_codes_to_html("unknown \x1B[12345m ansi code"));
	assert_string_equal_free("plain &lt;peanut&gt;",
			yahoo_codes_to_html("plain <peanut>"));
	assert_string_equal_free("plain &lt;peanut",
			yahoo_codes_to_html("plain <peanut"));
	assert_string_equal_free("plain&gt; peanut",
			yahoo_codes_to_html("plain> peanut"));

	/* bold/italic/underline */
	assert_string_equal_free("<b>bold</b>",
			yahoo_codes_to_html("\x1B[1mbold"));
	assert_string_equal_free("<i>italic</i>",
			yahoo_codes_to_html("\x1B[2mitalic"));
	assert_string_equal_free("<u>underline</u>",
			yahoo_codes_to_html("\x1B[4munderline"));
	assert_string_equal_free("no markup",
			yahoo_codes_to_html("no\x1B[x4m markup"));
	assert_string_equal_free("<b>bold</b> <i>italic</i> <u>underline</u>",
			yahoo_codes_to_html("\x1B[1mbold\x1B[x1m \x1B[2mitalic\x1B[x2m \x1B[4munderline"));
	assert_string_equal_free("<b>bold <i>bolditalic</i></b><i> italic</i>",
			yahoo_codes_to_html("\x1B[1mbold \x1B[2mbolditalic\x1B[x1m italic"));
	assert_string_equal_free("<b>bold <i>bolditalic</i></b><i> <u>italicunderline</u></i>",
			yahoo_codes_to_html("\x1B[1mbold \x1B[2mbolditalic\x1B[x1m \x1B[4mitalicunderline"));
	assert_string_equal_free("<b>bold <i>bolditalic <u>bolditalicunderline</u></i><u> boldunderline</u></b>",
			yahoo_codes_to_html("\x1B[1mbold \x1B[2mbolditalic \x1B[4mbolditalicunderline\x1B[x2m boldunderline"));
	assert_string_equal_free("<b>bold <i>bolditalic <u>bolditalicunderline</u></i></b><i><u> italicunderline</u></i>",
			yahoo_codes_to_html("\x1B[1mbold \x1B[2mbolditalic \x1B[4mbolditalicunderline\x1B[x1m italicunderline"));

	/* link */
	assert_string_equal_free("http://pidgin.im/",
			yahoo_codes_to_html("\x1B[lmhttp://pidgin.im/\x1B[xlm"));

#ifdef USE_CSS_FORMATTING
	/* font color */
	assert_string_equal_free("<span style='color: #0000FF'>blue</span>",
			yahoo_codes_to_html("\x1B[31mblue"));
	assert_string_equal_free("<span style='color: #70ea15'>custom color</span>",
			yahoo_codes_to_html("\x1B[#70ea15mcustom color"));

	/* font face */
	assert_string_equal_free("<font face='Georgia'>test</font>",
			yahoo_codes_to_html("<font face='Georgia'>test</font>"));

	/* font size */
	assert_string_equal_free("<font><span style='font-size: 15pt'>test</span></font>",
			yahoo_codes_to_html("<font size='15'>test"));
	assert_string_equal_free("<font><span style='font-size: 32pt'>size 32</span></font>",
			yahoo_codes_to_html("<font size='32'>size 32"));

	/* combinations */
	assert_string_equal_free("<font face='Georgia'><span style='font-size: 32pt'>test</span></font>",
			yahoo_codes_to_html("<font face='Georgia' size='32'>test"));
	assert_string_equal_free("<span style='color: #FF0080'><font><span style='font-size: 15pt'>test</span></font></span>",
			yahoo_codes_to_html("\x1B[35m<font size='15'>test"));
#else
	/* font color */
	assert_string_equal_free("<font color='#0000FF'>blue</font>",
			yahoo_codes_to_html("\x1B[31mblue"));
	assert_string_equal_free("<font color='#70ea15'>custom color</font>",
			yahoo_codes_to_html("\x1B[#70ea15mcustom color"));
	assert_string_equal_free("test",
			yahoo_codes_to_html("<ALT #ff0000,#00ff00,#0000ff>test</ALT>"));

	/* font face */
	assert_string_equal_free("<font face='Georgia'>test</font>",
			yahoo_codes_to_html("<font face='Georgia'>test"));

	/* font size */
	assert_string_equal_free("<font size='4' absz='15'>test</font>",
			yahoo_codes_to_html("<font size='15'>test"));
	assert_string_equal_free("<font size='6' absz='32'>size 32</font>",
			yahoo_codes_to_html("<font size='32'>size 32"));

	/* combinations */
	assert_string_equal_free("<font face='Georgia' size='6' absz='32'>test</font>",
			yahoo_codes_to_html("<font face='Georgia' size='32'>test"));
	assert_string_equal_free("<font color='#FF0080'><font size='4' absz='15'>test</font></font>",
			yahoo_codes_to_html("\x1B[35m<font size='15'>test"));
#endif /* !USE_CSS_FORMATTING */
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
