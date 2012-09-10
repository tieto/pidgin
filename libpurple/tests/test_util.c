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

/*
 * Many of the valid and invalid email addresses lised below are from
 * http://fightingforalostcause.net/misc/2006/compare-email-regex.php
 */
const char *valid_emails[] = {
	"purple-devel@lists.sf.net",
	"l3tt3rsAndNumb3rs@domain.com",
	"has-dash@domain.com",
	"hasApostrophe.o'leary@domain.org",
	"uncommonTLD@domain.museum",
	"uncommonTLD@domain.travel",
	"uncommonTLD@domain.mobi",
	"countryCodeTLD@domain.uk",
	"countryCodeTLD@domain.rw",
	"lettersInDomain@911.com",
	"underscore_inLocal@domain.net",
	"IPInsteadOfDomain@127.0.0.1",
	/* "IPAndPort@127.0.0.1:25", */
	"subdomain@sub.domain.com",
	"local@dash-inDomain.com",
	"dot.inLocal@foo.com",
	"a@singleLetterLocal.org",
	"singleLetterDomain@x.org",
	"&*=?^+{}'~@validCharsInLocal.net",
	"foor@bar.newTLD",
	"HenryTheGreatWhiteCricket@live.ca",
	"HenryThe__WhiteCricket@hotmail.com"
};

const char *invalid_emails[] = {
	"purple-devel@@lists.sf.net",
	"purple@devel@lists.sf.net",
	"purple-devel@list..sf.net",
	"purple-devel",
	"purple-devel@",
	"@lists.sf.net",
	"totally bogus",
	"missingDomain@.com",
	"@missingLocal.org",
	"missingatSign.net",
	"missingDot@com",
	"two@@signs.com",
	"colonButNoPort@127.0.0.1:",
	""
	/* "someone-else@127.0.0.1.26", */
	".localStartsWithDot@domain.com",
	/* "localEndsWithDot.@domain.com", */ /* I don't think this is invalid -- Stu */
	/* "two..consecutiveDots@domain.com", */ /* I don't think this is invalid -- Stu */
	"domainStartsWithDash@-domain.com",
	"domainEndsWithDash@domain-.com",
	/* "numbersInTLD@domain.c0m", */
	/* "missingTLD@domain.", */ /* This certainly isn't invalid -- Stu */
	"! \"#$%(),/;<>[]`|@invalidCharsInLocal.org",
	"invalidCharsInDomain@! \"#$%(),/;<>_[]`|.org",
	/* "local@SecondLevelDomainNamesAreInvalidIfTheyAreLongerThan64Charactersss.org" */
};

START_TEST(test_util_email_is_valid)
{
	size_t i;

	for (i = 0; i < G_N_ELEMENTS(valid_emails); i++)
		fail_unless(purple_email_is_valid(valid_emails[i]), "Email address was: %s", valid_emails[i]);

	for (i = 0; i < G_N_ELEMENTS(invalid_emails); i++)
		fail_if(purple_email_is_valid(invalid_emails[i]), "Email address was: %s", invalid_emails[i]);
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
	fail_unless(1282941722 == purple_str_to_time("2010-08-27.204202", TRUE, NULL, NULL, NULL));
	fail_unless(1282941722 == purple_str_to_time("2010-08-27.134202-0700PDT", FALSE, NULL, NULL, NULL));
}
END_TEST

START_TEST(test_markup_html_to_xhtml)
{
	gchar *xhtml = NULL;
	gchar *plaintext = NULL;

	purple_markup_html_to_xhtml("<a>", &xhtml, &plaintext);
	assert_string_equal_free("<a href=\"\"></a>", xhtml);
	assert_string_equal_free("", plaintext);

	purple_markup_html_to_xhtml("<A href='URL'>ABOUT</a>", &xhtml, &plaintext);
	assert_string_equal_free("<a href=\"URL\">ABOUT</a>", xhtml);
	assert_string_equal_free("ABOUT <URL>", plaintext);

	purple_markup_html_to_xhtml("<a href='URL'>URL</a>", &xhtml, &plaintext);
	assert_string_equal_free("URL", plaintext);
	assert_string_equal_free("<a href=\"URL\">URL</a>", xhtml);

	purple_markup_html_to_xhtml("<a href='mailto:mail'>mail</a>", &xhtml, &plaintext);
	assert_string_equal_free("mail", plaintext);
	assert_string_equal_free("<a href=\"mailto:mail\">mail</a>", xhtml);

	purple_markup_html_to_xhtml("<A href='\"U&apos;R&L'>ABOUT</a>", &xhtml, &plaintext);
	assert_string_equal_free("<a href=\"&quot;U&apos;R&amp;L\">ABOUT</a>", xhtml);
	assert_string_equal_free("ABOUT <\"U'R&L>", plaintext);

	purple_markup_html_to_xhtml("<img src='SRC' alt='ALT'/>", &xhtml, &plaintext);
	assert_string_equal_free("<img src='SRC' alt='ALT' />", xhtml);
	assert_string_equal_free("ALT", plaintext);

	purple_markup_html_to_xhtml("<img src=\"'S&apos;R&C\" alt=\"'A&apos;L&T\"/>", &xhtml, &plaintext);
	assert_string_equal_free("<img src='&apos;S&apos;R&amp;C' alt='&apos;A&apos;L&amp;T' />", xhtml);
	assert_string_equal_free("'A'L&T", plaintext);

	purple_markup_html_to_xhtml("<unknown>", &xhtml, &plaintext);
	assert_string_equal_free("&lt;unknown>", xhtml);
	assert_string_equal_free("<unknown>", plaintext);

	purple_markup_html_to_xhtml("&eacute;&amp;", &xhtml, &plaintext);
	assert_string_equal_free("&eacute;&amp;", xhtml);
	assert_string_equal_free("&eacute;&", plaintext);

	purple_markup_html_to_xhtml("<h1>A<h2>B</h2>C</h1>", &xhtml, &plaintext);
	assert_string_equal_free("<h1>A<h2>B</h2>C</h1>", xhtml);
	assert_string_equal_free("ABC", plaintext);

	purple_markup_html_to_xhtml("<h1><h2><h3><h4>", &xhtml, &plaintext);
	assert_string_equal_free("<h1><h2><h3><h4></h4></h3></h2></h1>", xhtml);
	assert_string_equal_free("", plaintext);
        
	purple_markup_html_to_xhtml("<italic/>", &xhtml, &plaintext);
	assert_string_equal_free("<em/>", xhtml);
	assert_string_equal_free("", plaintext);

	purple_markup_html_to_xhtml("</", &xhtml, &plaintext);
	assert_string_equal_free("&lt;/", xhtml);
	assert_string_equal_free("</", plaintext);

	purple_markup_html_to_xhtml("</div>", &xhtml, &plaintext);
	assert_string_equal_free("", xhtml);
	assert_string_equal_free("", plaintext);

	purple_markup_html_to_xhtml("<hr/>", &xhtml, &plaintext);
	assert_string_equal_free("<br/>", xhtml);
	assert_string_equal_free("\n", plaintext);

	purple_markup_html_to_xhtml("<hr>", &xhtml, &plaintext);
	assert_string_equal_free("<br/>", xhtml);
	assert_string_equal_free("\n", plaintext);

	purple_markup_html_to_xhtml("<br />", &xhtml, &plaintext);
	assert_string_equal_free("<br/>", xhtml);
	assert_string_equal_free("\n", plaintext);

	purple_markup_html_to_xhtml("<br>INSIDE</br>", &xhtml, &plaintext);
	assert_string_equal_free("<br/>INSIDE", xhtml);
	assert_string_equal_free("\nINSIDE", plaintext);

	purple_markup_html_to_xhtml("<div></div>", &xhtml, &plaintext);
	assert_string_equal_free("<div></div>", xhtml);
	assert_string_equal_free("", plaintext);

	purple_markup_html_to_xhtml("<div/>", &xhtml, &plaintext);
	assert_string_equal_free("<div/>", xhtml);
	assert_string_equal_free("", plaintext);

	purple_markup_html_to_xhtml("<div attr='\"&<>'/>", &xhtml, &plaintext);
	assert_string_equal_free("<div attr='&quot;&amp;&lt;&gt;'/>", xhtml);
	assert_string_equal_free("", plaintext);

	purple_markup_html_to_xhtml("<div attr=\"'\"/>", &xhtml, &plaintext);
	assert_string_equal_free("<div attr=\"&apos;\"/>", xhtml);
	assert_string_equal_free("", plaintext);

	purple_markup_html_to_xhtml("<div/> < <div/>", &xhtml, &plaintext);
	assert_string_equal_free("<div/> &lt; <div/>", xhtml);
	assert_string_equal_free(" < ", plaintext);

	purple_markup_html_to_xhtml("<div>x</div>", &xhtml, &plaintext);
	assert_string_equal_free("<div>x</div>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<b>x</b>", &xhtml, &plaintext);
	assert_string_equal_free("<span style='font-weight: bold;'>x</span>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<bold>x</bold>", &xhtml, &plaintext);
	assert_string_equal_free("<span style='font-weight: bold;'>x</span>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<strong>x</strong>", &xhtml, &plaintext);
	assert_string_equal_free("<span style='font-weight: bold;'>x</span>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<u>x</u>", &xhtml, &plaintext);
	assert_string_equal_free("<span style='text-decoration: underline;'>x</span>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<underline>x</underline>", &xhtml, &plaintext);
	assert_string_equal_free("<span style='text-decoration: underline;'>x</span>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<s>x</s>", &xhtml, &plaintext);
	assert_string_equal_free("<span style='text-decoration: line-through;'>x</span>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<strike>x</strike>", &xhtml, &plaintext);
	assert_string_equal_free("<span style='text-decoration: line-through;'>x</span>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<sub>x</sub>", &xhtml, &plaintext);
	assert_string_equal_free("<span style='vertical-align:sub;'>x</span>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<sup>x</sup>", &xhtml, &plaintext);
	assert_string_equal_free("<span style='vertical-align:super;'>x</span>", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<FONT>x</FONT>", &xhtml, &plaintext);
	assert_string_equal_free("x", xhtml);
	assert_string_equal_free("x", plaintext);

	purple_markup_html_to_xhtml("<font face=\"'Times&gt;New & Roman'\">x</font>", &xhtml, &plaintext);
	assert_string_equal_free("x", plaintext);
	assert_string_equal_free("<span style='font-family: \"Times&gt;New &amp; Roman\";'>x</span>", xhtml);

	purple_markup_html_to_xhtml("<font back=\"'color&gt;blue&red'\">x</font>", &xhtml, &plaintext);
	assert_string_equal_free("x", plaintext);
	assert_string_equal_free("<span style='background: \"color&gt;blue&amp;red\";'>x</span>", xhtml);

	purple_markup_html_to_xhtml("<font color=\"'color&gt;blue&red'\">x</font>", &xhtml, &plaintext);
	assert_string_equal_free("x", plaintext);
	assert_string_equal_free("<span style='color: \"color&gt;blue&amp;red\";'>x</span>", xhtml);

	purple_markup_html_to_xhtml("<font size=1>x</font>", &xhtml, &plaintext);
	assert_string_equal_free("x", plaintext);
	assert_string_equal_free("<span style='font-size: xx-small;'>x</span>", xhtml);

	purple_markup_html_to_xhtml("<font size=432>x</font>", &xhtml, &plaintext);
	assert_string_equal_free("x", plaintext);
	assert_string_equal_free("<span style='font-size: medium;'>x</span>", xhtml);

        /* The following tests document a behaviour that looks suspicious */

        /* bug report http://developer.pidgin.im/ticket/13485 */
        purple_markup_html_to_xhtml("<!--COMMENT-->", &xhtml, &plaintext);
	assert_string_equal_free("<!--COMMENT-->", xhtml);
	assert_string_equal_free("COMMENT-->", plaintext);

        /* no bug report */
	purple_markup_html_to_xhtml("<br  />", &xhtml, &plaintext);
	assert_string_equal_free("&lt;br  />", xhtml);
	assert_string_equal_free("<br  />", plaintext);

        /* same code section as <br  /> */
	purple_markup_html_to_xhtml("<hr  />", &xhtml, &plaintext);
	assert_string_equal_free("&lt;hr  />", xhtml);
	assert_string_equal_free("<hr  />", plaintext);
}
END_TEST

START_TEST(test_utf8_strip_unprintables)
{
	fail_unless(NULL == purple_utf8_strip_unprintables(NULL));
	/* invalid UTF-8 */
#if 0
	/* disabled because make check fails on an assertion */
	fail_unless(NULL == purple_utf8_strip_unprintables("abc\x80\x7f"));
#endif
	/* \t, \n, \r, space */
	assert_string_equal_free("ab \tcd\nef\r   ", purple_utf8_strip_unprintables("ab \tcd\nef\r   "));
	/* ASCII control characters (stripped) */
	assert_string_equal_free(" aaaa ", purple_utf8_strip_unprintables(
				"\x01\x02\x03\x04\x05\x06\x07\x08\x0B\x0C\x0E\x0F\x10 aaaa "
				"\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"));
	/* Basic ASCII */
	assert_string_equal_free("Foobar", purple_utf8_strip_unprintables("Foobar"));
	/* 0xE000 - 0xFFFD (UTF-8 encoded) */
	/* U+F1F7 */
	assert_string_equal_free("aaaa\xef\x87\xb7", purple_utf8_strip_unprintables("aaaa\xef\x87\xb7"));
#if 0
	/* disabled because make check fails on an assertion */
	/* U+DB80 (Private Use High Surrogate, First) -- should be stripped */
	assert_string_equal_free("aaaa", purple_utf8_strip_unprintables("aaaa\xed\xa0\x80"));
	/* U+FFFE (should be stripped) */
	assert_string_equal_free("aaaa", purple_utf8_strip_unprintables("aaaa\xef\xbf\xbe"));
#endif
	/* U+FEFF (should not be stripped) */
	assert_string_equal_free("aaaa\xef\xbb\xbf", purple_utf8_strip_unprintables("aaaa\xef\xbb\xbf"));
}
END_TEST

START_TEST(test_mime_decode_field)
{
	gchar *result = purple_mime_decode_field("=?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?=");
	assert_string_equal_free("Keld Jørn Simonsen", result);
}
END_TEST

START_TEST(test_strdup_withhtml)
{
	gchar *result = purple_strdup_withhtml("hi\r\nthere\n");
	assert_string_equal_free("hi<BR>there<BR>", result);
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

	tc = tcase_create("Stripping Unparseables");
	tcase_add_test(tc, test_utf8_strip_unprintables);
	suite_add_tcase(s, tc);

	tc = tcase_create("MIME");
	tcase_add_test(tc, test_mime_decode_field);
	suite_add_tcase(s, tc);

	tc = tcase_create("strdup_withhtml");
	tcase_add_test(tc, test_strdup_withhtml);
	suite_add_tcase(s, tc);

	return s;
}
