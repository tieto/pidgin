#include <glib.h>

#include "../util.h"

/******************************************************************************
 * base 16 tests
 *****************************************************************************/
static void
test_util_base_16_encode(void) {
	gchar *in = purple_base16_encode((const guchar *)"hello, world!", 14);
	g_assert_cmpstr("68656c6c6f2c20776f726c642100", ==, in);
}

static void
test_util_base_16_decode(void) {
	gsize sz = 0;
	guchar *out = purple_base16_decode("21646c726f77202c6f6c6c656800", &sz);

	g_assert_cmpint(sz, ==, 14);
	g_assert_cmpstr("!dlrow ,olleh", ==, (const gchar *)out);
}

/******************************************************************************
 * base 64 tests
 *****************************************************************************/
static void
test_util_base_64_encode(void) {
	gchar *in = purple_base64_encode((const unsigned char *)"forty-two", 10);
	g_assert_cmpstr("Zm9ydHktdHdvAA==", ==, in);
}

static void
test_util_base_64_decode(void) {
	gsize sz = 0;
	guchar *out = purple_base64_decode("b3d0LXl0cm9mAA==", &sz);

	g_assert_cmpint(sz, ==, 10);
	g_assert_cmpstr("owt-ytrof", ==, (gchar *)out);
}

/******************************************************************************
 * filename escape tests
 *****************************************************************************/
static void
test_util_filename_escape(void) {
	g_assert_cmpstr("foo", ==, purple_escape_filename("foo"));
	g_assert_cmpstr("@oo", ==, purple_escape_filename("@oo"));
	g_assert_cmpstr("#oo", ==, purple_escape_filename("#oo"));
	g_assert_cmpstr("-oo", ==, purple_escape_filename("-oo"));
	g_assert_cmpstr("_oo", ==, purple_escape_filename("_oo"));
	g_assert_cmpstr(".oo", ==, purple_escape_filename(".oo"));
	g_assert_cmpstr("%25oo", ==, purple_escape_filename("%oo"));
	g_assert_cmpstr("%21oo", ==, purple_escape_filename("!oo"));
}

static void
test_util_filename_unescape(void) {
	g_assert_cmpstr("bar", ==, purple_unescape_filename("bar"));
	g_assert_cmpstr("@ar", ==, purple_unescape_filename("@ar"));
	g_assert_cmpstr("!ar", ==, purple_unescape_filename("!ar"));
	g_assert_cmpstr("!ar", ==, purple_unescape_filename("%21ar"));
	g_assert_cmpstr("%ar", ==, purple_unescape_filename("%25ar"));
}

/******************************************************************************
 * text_strip tests
 *****************************************************************************/
static void
test_util_text_strip_mnemonic(void) {
	g_assert_cmpstr("", ==, purple_text_strip_mnemonic(""));
	g_assert_cmpstr("foo", ==, purple_text_strip_mnemonic("foo"));
	g_assert_cmpstr("foo", ==, purple_text_strip_mnemonic("_foo"));

}

/******************************************************************************
 * email tests
 *****************************************************************************/
/*
 * Many of the valid and invalid email addresses lised below are from
 * http://fightingforalostcause.net/misc/2006/compare-email-regex.php
 */
const gchar *valid_emails[] = {
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

static void
test_util_email_is_valid(void) {
	size_t i;

	for (i = 0; i < G_N_ELEMENTS(valid_emails); i++)
		g_assert_true(purple_email_is_valid(valid_emails[i]));
}

const gchar *invalid_emails[] = {
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
	"",
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

static void
test_util_email_is_invalid(void) {
	size_t i;

	for (i = 0; i < G_N_ELEMENTS(invalid_emails); i++)
		g_assert_false(purple_email_is_valid(invalid_emails[i]));
}

/******************************************************************************
 * ipv6 tests
 *****************************************************************************/
static void
test_util_ipv6_is_valid(void) {
	g_assert_true(purple_ipv6_address_is_valid("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
	g_assert_true(purple_ipv6_address_is_valid("2001:db8:85a3:0:0:8a2e:370:7334"));
	g_assert_true(purple_ipv6_address_is_valid("2001:db8:85a3::8a2e:370:7334"));
	g_assert_true(purple_ipv6_address_is_valid("2001:0db8:0:0::1428:57ab"));
	g_assert_true(purple_ipv6_address_is_valid("::1"));
	g_assert_true(purple_ipv6_address_is_valid("1::"));
	g_assert_true(purple_ipv6_address_is_valid("1::1"));
	g_assert_true(purple_ipv6_address_is_valid("::"));
}

static void
test_util_ipv6_is_invalid(void) {
	g_assert_false(purple_ipv6_address_is_valid(""));
	g_assert_false(purple_ipv6_address_is_valid(":"));
	g_assert_false(purple_ipv6_address_is_valid("1.2.3.4"));
	g_assert_false(purple_ipv6_address_is_valid("2001::FFD3::57ab"));
	g_assert_false(purple_ipv6_address_is_valid("200000000::1"));
	g_assert_false(purple_ipv6_address_is_valid("QWERTY::1"));
}

/******************************************************************************
 * str_to_time tests
 *****************************************************************************/
static void
test_util_str_to_time(void) {
	struct tm tm;
	glong tz_off;
	const gchar *rest;
	time_t timestamp;

	g_assert_cmpint(377182200, ==, purple_str_to_time("19811214T12:50:00", TRUE, NULL, NULL, NULL));
	g_assert_cmpint(1175919261, ==, purple_str_to_time("20070407T04:14:21", TRUE, NULL, NULL, NULL));
	g_assert_cmpint(1282941722, ==, purple_str_to_time("2010-08-27.204202", TRUE, NULL, NULL, NULL));

	timestamp = purple_str_to_time("2010-08-27.134202-0700PDT", FALSE, &tm, &tz_off, &rest);
	g_assert_cmpint(1282941722, ==, timestamp);
	g_assert_cmpint((-7 * 60 * 60), ==, tz_off);
	g_assert_cmpstr("PDT", ==, rest);
}

/******************************************************************************
 * Markup tests
 *****************************************************************************/
typedef struct {
	gchar *markup;
	gchar *xhtml;
	gchar *plaintext;
} MarkupTestData;

static void
test_util_markup_html_to_xhtml(void) {
	gint i;
	MarkupTestData data[] = {
		{
			"<a>",
			"<a href=\"\"></a>",
			"",
		}, {
			"<A href='URL'>ABOUT</a>",
			"<a href=\"URL\">ABOUT</a>",
			"ABOUT <URL>",
		}, {
			"<a href='URL'>URL</a>",
			"<a href=\"URL\">URL</a>",
			"URL",
		}, {
			"<a href='mailto:mail'>mail</a>",
			"<a href=\"mailto:mail\">mail</a>",
			"mail",
		}, {
			"<A href='\"U&apos;R&L'>ABOUT</a>",
			"<a href=\"&quot;U&apos;R&amp;L\">ABOUT</a>",
			"ABOUT <\"U'R&L>",
		}, {
			"<img src='SRC' alt='ALT'/>",
			"<img src='SRC' alt='ALT' />",
			"ALT",
		}, {
			"<img src=\"'S&apos;R&C\" alt=\"'A&apos;L&T\"/>",
			"<img src='&apos;S&apos;R&amp;C' alt='&apos;A&apos;L&amp;T' />",
			"'A'L&T",
		}, {
			"<unknown>",
			"&lt;unknown>",
			"<unknown>",
		}, {
			"&eacute;&amp;",
			"&eacute;&amp;",
			"&eacute;&",
		}, {
			"<h1>A<h2>B</h2>C</h1>",
			"<h1>A<h2>B</h2>C</h1>",
			"ABC",
		}, {
			"<h1><h2><h3><h4>",
			"<h1><h2><h3><h4></h4></h3></h2></h1>",
			"",
		}, {
			"<italic/>",
			"<em/>",
			"",
		}, {
			"</",
			"&lt;/",
			"</",
		}, {
			"</div>",
			"",
			"",
		}, {
			"<hr/>",
			"<br/>",
			"\n",
		}, {
			"<hr>",
			"<br/>",
			"\n",
		}, {
			"<br />",
			"<br/>",
			"\n",
		}, {
			"<br>INSIDE</br>",
			"<br/>INSIDE",
			"\nINSIDE",
		}, {
			"<div></div>",
			"<div></div>",
			"",
		}, {
			"<div/>",
			"<div/>",
			"",
		}, {
			"<div attr='\"&<>'/>",
			"<div attr='&quot;&amp;&lt;&gt;'/>",
			"",
		}, {
			"<div attr=\"'\"/>",
			"<div attr=\"&apos;\"/>",
			"",
		}, {
			"<div/> < <div/>",
			"<div/> &lt; <div/>",
			" < ",
		}, {
			"<div>x</div>",
			"<div>x</div>",
			"x",
		}, {
			"<b>x</b>",
			"<span style='font-weight: bold;'>x</span>",
			"x",
		}, {
			"<bold>x</bold>",
			"<span style='font-weight: bold;'>x</span>",
			"x",
		}, {
			"<strong>x</strong>",
			"<span style='font-weight: bold;'>x</span>",
			"x",
		}, {
			"<u>x</u>",
			"<span style='text-decoration: underline;'>x</span>",
			"x",
		}, {
			"<underline>x</underline>",
			"<span style='text-decoration: underline;'>x</span>",
			"x",
		}, {
			"<s>x</s>",
			"<span style='text-decoration: line-through;'>x</span>",
			"x",
		}, {
			"<strike>x</strike>",
			"<span style='text-decoration: line-through;'>x</span>",
			"x",
		}, {
			"<sub>x</sub>",
			"<span style='vertical-align:sub;'>x</span>",
			"x",
		}, {
			"<sup>x</sup>",
			"<span style='vertical-align:super;'>x</span>",
			"x",
		}, {
			"<FONT>x</FONT>",
			"x",
			"x",
		}, {
			"<font face=\"'Times&gt;New & Roman'\">x</font>",
			"<span style='font-family: \"Times&gt;New &amp; Roman\";'>x</span>",
			"x",
		}, {
			"<font back=\"'color&gt;blue&red'\">x</font>",
			"<span style='background: \"color&gt;blue&amp;red\";'>x</span>",
			"x",
		}, {
			"<font color=\"'color&gt;blue&red'\">x</font>",
			"<span style='color: \"color&gt;blue&amp;red\";'>x</span>",
			"x",
		}, {
			"<font size=1>x</font>",
			"<span style='font-size: xx-small;'>x</span>",
			"x",
		}, {
			"<font size=432>x</font>",
			"<span style='font-size: medium;'>x</span>",
			"x",
		}, {
			"<!--COMMENT-->",
			"<!--COMMENT-->",
			"COMMENT-->",
		}, {
			"<br  />",
			"&lt;br  />",
			"<br  />",
		}, {
			"<hr  />",
			"&lt;hr  />",
			"<hr  />"
		}, {
			NULL, NULL, NULL,
		}
	};

	for(i = 0; data[i].markup; i++) {
		gchar *xhtml = NULL, *plaintext = NULL;

		purple_markup_html_to_xhtml(data[i].markup, &xhtml, &plaintext);

		g_assert_cmpstr(data[i].xhtml, ==, xhtml);
		g_free(xhtml);

		g_assert_cmpstr(data[i].plaintext, ==, plaintext);
		g_free(plaintext);
	}
}

#if 0
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

#endif

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/util/base/16/encode",
	                test_util_base_16_encode);
	g_test_add_func("/util/base/16/decode",
	                test_util_base_16_decode);

	g_test_add_func("/util/base/64/encode",
	                test_util_base_64_encode);
	g_test_add_func("/util/base/64/decode",
	                test_util_base_64_decode);

	g_test_add_func("/util/filename/escape",
	                test_util_filename_escape);
	g_test_add_func("/util/filename/unescape",
	                test_util_filename_unescape);

	g_test_add_func("/util/mnemonic/strip",
	                test_util_text_strip_mnemonic);

	g_test_add_func("/util/email/is valid",
	                test_util_email_is_valid);
	g_test_add_func("/util/email/is invalid",
	                test_util_email_is_invalid);

	g_test_add_func("/util/ipv6/is valid",
	                test_util_ipv6_is_valid);
	g_test_add_func("/util/ipv6/is invalid",
	                test_util_ipv6_is_invalid);

	g_test_add_func("/util/str to time",
	                test_util_str_to_time);

	g_test_add_func("/util/markup/html to xhtml",
	                test_util_markup_html_to_xhtml);

	return g_test_run();
}

#if 0
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
#endif
